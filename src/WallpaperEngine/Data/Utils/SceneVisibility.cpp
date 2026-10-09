#include "SceneVisibility.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <regex>

using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils;

namespace {
constexpr int kMaxParentDepth = 32;

const UserSetting* typedVisible (const Object& object) {
    if (object.is<Image> ()) {
	return object.as<Image> ()->visible.get ();
    }
    if (object.is<Text> ()) {
	return object.as<Text> ()->visible.get ();
    }
    if (object.is<Particle> ()) {
	return object.as<Particle> ()->visible.get ();
    }

    return nullptr;
}

bool isScripted (const UserSetting* setting) {
    return setting != nullptr && setting->value->getScriptSource ().has_value ();
}

bool isOff (const UserSetting* setting) { return setting != nullptr && !setting->value->getBool (); }

/** Ids of the layers whose output a texture name like _rt_imageLayerComposite_123_a reads */
void collectTextureLayers (const TextureMap& textures, std::vector<int>& result) {
    static const std::regex layerTexture ("_rt_imageLayer[A-Za-z]*_(-?[0-9]+)");

    for (const auto& name : textures | std::views::values) {
	for (std::sregex_iterator it (name.begin (), name.end (), layerTexture), end; it != end; ++it) {
	    result.push_back (std::stoi ((*it)[1].str ()));
	}
    }
}

/** Everything a created object needs created as well: its parent and the layers it reads from */
std::vector<int> objectNeeds (const Object& object) {
    std::vector<int> result = object.dependencies;

    if (object.parent.has_value ()) {
	result.push_back (object.parent.value ());
    }

    if (object.is<Image> ()) {
	for (const auto& effect : object.as<Image> ()->effects) {
	    for (const auto& passOverride : effect->passOverrides) {
		collectTextureLayers (passOverride->textures, result);
		collectTextureLayers (passOverride->usertextures, result);
	    }
	}
    }

    return result;
}
} // namespace

bool WallpaperEngine::Data::Utils::isHiddenForGood (
    const Object& object, const std::vector<std::string>& layerScripts
) {
    const UserSetting* group = object.groupVisible.get ();
    const UserSetting* typed = typedVisible (object);

    if (isScripted (group) || isScripted (typed) || (!isOff (group) && !isOff (typed))) {
	return false;
    }

    // a script somewhere might fetch the layer with thisScene.getLayer () and show it again
    const std::string id = std::to_string (object.id);

    return std::ranges::none_of (layerScripts, [&] (const std::string& script) {
	return (!object.name.empty () && script.find (object.name) != std::string::npos)
	    || script.find (id) != std::string::npos;
    });
}

SceneVisibility
WallpaperEngine::Data::Utils::computeSceneVisibility (const Scene& scene, const std::vector<int>& hiddenLayers) {
    SceneVisibility result;
    std::map<int, const Object*> objects;

    for (const auto& object : scene.objects) {
	objects.emplace (object->id, object.get ());
    }

    for (const auto& object : scene.objects) {
	bool hidden = false;
	bool hiddenByUser = false;
	const Object* current = object.get ();

	for (int depth = 0; current != nullptr && depth <= kMaxParentDepth; ++depth) {
	    hiddenByUser = hiddenByUser || std::ranges::find (hiddenLayers, current->id) != hiddenLayers.end ();
	    hidden = hidden || hiddenByUser || isHiddenForGood (*current, scene.layerScripts);

	    if (!current->parent.has_value ()) {
		break;
	    }

	    const auto parent = objects.find (current->parent.value ());
	    current = parent == objects.end () ? nullptr : parent->second;
	}

	if (hidden) {
	    result.hidden.insert (object->id);
	}
	if (hiddenByUser) {
	    result.hiddenByUser.insert (object->id);
	}
    }

    // objects that get drawn pull in what they need, even when it's hidden itself
    std::vector<int> pending;

    for (const auto& object : scene.objects) {
	if (!result.hidden.contains (object->id)) {
	    std::ranges::copy (objectNeeds (*object), std::back_inserter (pending));
	}
    }

    while (!pending.empty ()) {
	const int id = pending.back ();
	pending.pop_back ();

	if (!result.hidden.contains (id) || !result.required.insert (id).second) {
	    continue;
	}

	if (const auto object = objects.find (id); object != objects.end ()) {
	    std::ranges::copy (objectNeeds (*object->second), std::back_inserter (pending));
	}
    }

    return result;
}
