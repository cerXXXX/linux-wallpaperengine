#include "VideoDriver.h"

#include <poll.h>

using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

VideoDriver::VideoDriver (WallpaperApplication& app, MouseInput& mouseInput) :
    m_app (app), m_inputContext (mouseInput) { }

wl_display* VideoDriver::getWaylandDisplay () const { return nullptr; }

WallpaperApplication& VideoDriver::getApp () const { return this->m_app; }

InputContext& VideoDriver::getInputContext () { return this->m_inputContext; }

float VideoDriver::getRenderTime () const {
    return (this->m_timeHeld ? this->m_heldAt : this->getClockTime ()) - this->m_heldFor;
}

void VideoDriver::holdTime (bool hold) {
    if (hold == this->m_timeHeld) {
	return;
    }

    if (hold) {
	this->m_heldAt = this->getClockTime ();
    } else {
	this->m_heldFor += this->getClockTime () - this->m_heldAt;
    }

    this->m_timeHeld = hold;
}

void VideoDriver::waitForEvents (const std::vector<int>& fds, int timeoutMs) {
    std::vector<pollfd> pfds;

    for (const int fd : fds) {
	pfds.push_back ({ .fd = fd, .events = POLLIN, .revents = 0 });
    }

    poll (pfds.data (), pfds.size (), timeoutMs);
}

void VideoDriver::renderHeldFrame () { this->dispatchEventQueue (); }

void VideoDriver::forgetHeldFrames () { }

void VideoDriver::resumeRendering () { }
