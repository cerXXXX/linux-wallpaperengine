#pragma once
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Render/CObject.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
class ScriptableObject : virtual public CObject {
public:
    struct PropertyEntry {
	std::string key;
	DynamicValue& value;
    };

    ScriptableObject (Wallpapers::CScene& scene, const Object& object);
    virtual ~ScriptableObject () = default;

    DynamicValue& getProperty (const std::string& name);

    const std::map<std::string, PropertyEntry>& getProperties () const;

    /** Texture animation controls exposed to scripts through thisLayer.getTextureAnimation () */
    [[nodiscard]] virtual int getTextureAnimationFrameCount () const { return 0; }
    [[nodiscard]] virtual int getTextureAnimationFrame () const { return 0; }
    virtual void setTextureAnimationFrame (int frame) { }
    [[nodiscard]] virtual bool isTextureAnimationPlaying () const { return false; }
    virtual void setTextureAnimationPlaying (bool playing) { }

protected:
    /**
     * @param runScript Whether the property's script (if any) should run as a property script, objects that drive
     * the script themselves (text layers) register it without one
     */
    void registerProperty (const std::string& name, DynamicValue& value, bool runScript = true);

private:
    std::map<std::string, PropertyEntry> m_properties;
};
}