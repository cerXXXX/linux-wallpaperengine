#pragma once

#include <set>
#include <vector>

#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"

namespace WallpaperEngine::Data::Utils {
/**
 * Which objects of a scene can never show up during this run. Properties are only applied at startup, so an object
 * hidden by one (a language, a clock layout...) stays hidden, and so does everything inside it.
 */
struct SceneVisibility {
    /** Objects that are never drawn: hidden by the scene or by the user, or inside a group that is */
    std::set<int> hidden;
    /** Objects hidden by the user (--hide-layer), and everything inside them */
    std::set<int> hiddenByUser;
    /** Hidden objects that are still created because something that is drawn depends on them */
    std::set<int> required;

    /** Whether the object can be left out of the scene altogether */
    [[nodiscard]] bool skipCreation (int id) const { return hidden.contains (id) && !required.contains (id); }
};

/**
 * Whether the object's own "visible" hides it for the whole run: it's false (because of a property or set by the
 * author) and no script could flip it, neither its own nor one that looks layers up by name or id
 */
bool isHiddenForGood (const Model::Object& object, const std::vector<std::string>& layerScripts);

SceneVisibility computeSceneVisibility (const Model::Scene& scene, const std::vector<int>& hiddenLayers);
} // namespace WallpaperEngine::Data::Utils
