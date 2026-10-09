#include <iostream>

#include <GL/glew.h>

#include "CWallpaper.h"
#include "RenderContext.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/FrameHash.h"

namespace WallpaperEngine::Render {
RenderContext::RenderContext (
    Drivers::VideoDriver& driver, WallpaperApplication& app, Media::MediaSource& mediaSource
) :
    m_driver (driver), m_app (app), m_mediaSource (mediaSource),
    m_textureCache (std::make_unique<TextureCache> (*this)) { }

void RenderContext::render (Drivers::Output::OutputViewport* viewport) {
    viewport->makeCurrent ();

#if !NDEBUG
    const std::string str = "Rendering to output " + viewport->name;

    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */

    // search the background in the viewport selection

    // render the background
    const auto ref = this->m_wallpapers.find (viewport->name);

    if (ref != this->m_wallpapers.end ()) {
	ref->second->render (
	    viewport->viewport, this->getOutput ().renderVFlip (), viewport->globalPosition, viewport->logicalSize
	);
    }

    viewport->swappingContent = ref != this->m_wallpapers.end () && ref->second->hasContent ();

#if !NDEBUG
    glPopDebugGroup ();
#endif /* DEBUG */

    viewport->swapOutput ();
}

void RenderContext::framePresented (const Drivers::Output::OutputViewport* viewport) {
    if (this->m_firstFrameReported) {
	return;
    }

    this->m_presented.insert (viewport->name);

    for (const auto& screen : this->m_wallpapers | std::views::keys) {
	if (!this->m_presented.contains (screen)) {
	    return;
	}
    }

    this->m_firstFrameReported = true;
    this->m_firstFrameAt = std::chrono::steady_clock::now ();
    sLog.out ("First frame presented");
}

std::optional<std::chrono::steady_clock::time_point> RenderContext::firstFrameAt () const {
    return this->m_firstFrameAt;
}

void RenderContext::renderHeld (Drivers::Output::OutputViewport* viewport) {
    viewport->makeCurrent ();

    const auto ref = this->m_wallpapers.find (viewport->name);

    if (ref == this->m_wallpapers.end ()) {
	return;
    }

    ref->second->render (
	viewport->viewport, this->getOutput ().renderVFlip (), viewport->globalPosition, viewport->logicalSize
    );

    // nothing changed (the usual case: a clock without seconds, or no clock at all), the screen keeps the
    // frame it has and the compositor has nothing to do
    const uint64_t hash
	= Drivers::hashFramebuffer (this->m_app.getDestinationFramebuffer (), viewport->viewport, this->m_heldPixels);

    if (hash == viewport->heldFrameHash) {
	return;
    }

    viewport->heldFrameHash = hash;
    viewport->swappingContent = ref->second->hasContent ();
    viewport->swapOutput ();
}

void RenderContext::setWallpaper (const std::string& display, std::shared_ptr<CWallpaper> wallpaper) {
    wallpaper->setDestinationFramebuffer (this->m_app.getDestinationFramebuffer ());
    this->m_wallpapers.insert_or_assign (display, wallpaper);
}

void RenderContext::setPause (const bool newState) const {
    for (const auto& wallpaper : this->m_wallpapers | std::views::values) {
	wallpaper->setPause (newState);
    }
}

Input::InputContext& RenderContext::getInputContext () const { return this->m_driver.getInputContext (); }

const WallpaperApplication& RenderContext::getApp () const { return this->m_app; }

const Drivers::VideoDriver& RenderContext::getDriver () const { return this->m_driver; }

const Drivers::Output::Output& RenderContext::getOutput () const { return this->m_driver.getOutput (); }

std::shared_ptr<const TextureProvider> RenderContext::resolveTexture (const std::string& name) const {
    return this->m_textureCache->resolve (name);
}

const std::map<std::string, std::shared_ptr<CWallpaper>>& RenderContext::getWallpapers () const {
    return this->m_wallpapers;
}

Media::MediaSource& RenderContext::getMediaSource () const { return this->m_mediaSource; }

} // namespace WallpaperEngine::Render
