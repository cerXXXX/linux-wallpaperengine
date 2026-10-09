#include "ScriptableObjectAdapter.h"

#include <cstdint>
#include <string_view>
#include <utility>

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Scripting::Adapters;

#define SCRIPTABLE_OPAQUE_MAGIC 0xdeadbeef

struct OpaqueScriptableObjectAdapter {
    unsigned int magic;
    ScriptableObjectAdapter& adapter;
    WallpaperEngine::Scripting::ScriptableObject& object;
};

namespace {
enum TextureAnimationCall {
    TextureAnimation_Get = 0,
    TextureAnimation_SetFrame,
    TextureAnimation_GetFrame,
    TextureAnimation_Play,
    TextureAnimation_Pause,
    TextureAnimation_Stop,
    TextureAnimation_IsPlaying,
};

WallpaperEngine::Scripting::ScriptableObject* textureAnimationTarget (JSContext* ctx, JSValueConst* data) {
    int64_t pointer = 0;

    if (JS_ToInt64 (ctx, &pointer, data[0]) != 0) {
	return nullptr;
    }

    return reinterpret_cast<WallpaperEngine::Scripting::ScriptableObject*> (static_cast<intptr_t> (pointer));
}

// thisLayer.getTextureAnimation () and the methods of the object it returns
JSValue textureanimation_call (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic, JSValue* data) {
    auto* object = textureAnimationTarget (ctx, data);

    if (object == nullptr) {
	return JS_UNDEFINED;
    }

    switch (magic) {
	case TextureAnimation_Get: {
	    JSValue result = JS_NewObject (ctx);
	    const struct {
		const char* name;
		int length;
		int magic;
	    } methods[] = {
		{ "setFrame", 1, TextureAnimation_SetFrame }, { "getFrame", 0, TextureAnimation_GetFrame },
		{ "play", 0, TextureAnimation_Play },	      { "pause", 0, TextureAnimation_Pause },
		{ "stop", 0, TextureAnimation_Stop },	      { "isPlaying", 0, TextureAnimation_IsPlaying },
	    };

	    for (const auto& method : methods) {
		JS_SetPropertyStr (
		    ctx, result, method.name, JS_NewCFunctionData (ctx, textureanimation_call, method.length, method.magic, 1, data)
		);
	    }

	    JS_SetPropertyStr (ctx, result, "frameCount", JS_NewInt32 (ctx, object->getTextureAnimationFrameCount ()));
	    return result;
	}
	case TextureAnimation_SetFrame: {
	    int32_t frame = 0;
	    if (argc > 0 && JS_ToInt32 (ctx, &frame, argv[0]) == 0) {
		object->setTextureAnimationFrame (frame);
	    }
	    return JS_UNDEFINED;
	}
	case TextureAnimation_GetFrame: return JS_NewInt32 (ctx, object->getTextureAnimationFrame ());
	case TextureAnimation_Play: object->setTextureAnimationPlaying (true); return JS_UNDEFINED;
	case TextureAnimation_Pause: object->setTextureAnimationPlaying (false); return JS_UNDEFINED;
	case TextureAnimation_Stop:
	    object->setTextureAnimationPlaying (false);
	    object->setTextureAnimationFrame (0);
	    return JS_UNDEFINED;
	case TextureAnimation_IsPlaying: return JS_NewBool (ctx, object->isTextureAnimationPlaying ());
	default: return JS_UNDEFINED;
    }
}
} // namespace

JSValue scriptableobject_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return JS_EXCEPTION;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return JS_EXCEPTION;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

    if (std::string_view (name) == "getTextureAnimation") {
	JSValue data[] = { JS_NewInt64 (ctx, static_cast<int64_t> (reinterpret_cast<intptr_t> (&container->object))) };
	return JS_NewCFunctionData (ctx, textureanimation_call, 0, TextureAnimation_Get, 1, data);
    }

    // find the property inside, otherwise return undefined
    const auto& properties = container->object.getProperties ();
    const auto property = properties.find (name);

    if (property == properties.end ()) {
	return JS_UNDEFINED;
    }

    return container->adapter.getEngine ().dynamicToJs (property->second.value);
}

int scriptableobject_property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return -1;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return -1;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

    // scripts move layers around by assigning to them (thisLayer.origin = ...), unknown properties are ignored
    const auto& properties = container->object.getProperties ();
    const auto property = properties.find (name);

    if (property != properties.end ()) {
	container->adapter.getEngine ().jsToDynamic (val, property->second.value);
    }

    return 1;
}

ScriptableObjectAdapter::ScriptableObjectAdapter (ScriptEngine& engine, std::string name) :
    ObjectAdapter (engine), m_exoticMethods (), m_name (std::move (name)) {
    this->m_exoticMethods = {
	.get_property = scriptableobject_property_get,
	.set_property = scriptableobject_property_set,
    };
    this->registerType (
	{
	    .class_name = m_name.c_str (),
	    .exotic = &m_exoticMethods,
	}
    );
}

JSValue ScriptableObjectAdapter::instantiate (ScriptableObject& object) {
    JSValue result = this->ObjectAdapter::instantiate (object);
    JS_SetOpaque (
	result,
	new OpaqueScriptableObjectAdapter { .magic = SCRIPTABLE_OPAQUE_MAGIC, .adapter = *this, .object = object }
    );

    return result;
}

JSValue ScriptableObjectAdapter::instantiate (DynamicValue& value) {
    throw std::runtime_error ("Cannot create a ScriptableObject instance from a DynamicValue");
}