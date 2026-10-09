#include "PulseAudioPlayingDetector.h"
#include "WallpaperEngine/Logging/Log.h"

#include <unistd.h>
#include <vector>

namespace WallpaperEngine::Audio::Drivers::Detectors {
/** What one scan of the playing streams found */
struct PlayingScan {
    bool playing = false;
    /** Clients of audible streams whose process isn't known from the stream itself */
    std::vector<uint32_t> clients = {};
};

/**
 * @return If the stream belongs to another app: not this process, and not another wallpaper either (two
 * wallpapers with sound on two screens would otherwise silence each other for good)
 */
static bool isOtherApp (const pa_proplist* proplist) {
    const char* process = pa_proplist_gets (proplist, PA_PROP_APPLICATION_PROCESS_ID);
    const char* binary = pa_proplist_gets (proplist, PA_PROP_APPLICATION_PROCESS_BINARY);

    return process != nullptr && strtol (process, nullptr, 10) != getpid ()
	&& (binary == nullptr || strcmp (binary, "linux-wallpaperengine") != 0);
}

void sinkInputInfoCallback (pa_context* context, const pa_sink_input_info* info, int eol, void* userdata) {
    auto* scan = static_cast<PlayingScan*> (userdata);

    if (info == nullptr || info->proplist == nullptr) {
	return;
    }

    // paused (corked) and muted streams don't count, players keep them around while paused
    if (info->corked || info->mute || pa_cvolume_avg (&info->volume) == PA_VOLUME_MUTED) {
	return;
    }

    // native PipeWire streams (SDL, browsers) only carry the process id on their client
    if (pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_PROCESS_ID) != nullptr) {
	scan->playing |= isOtherApp (info->proplist);
    } else if (info->client != PA_INVALID_INDEX) {
	scan->clients.push_back (info->client);
    }
}

void clientInfoCallback (pa_context* context, const pa_client_info* info, int eol, void* userdata) {
    auto* scan = static_cast<PlayingScan*> (userdata);

    if (info != nullptr && info->proplist != nullptr) {
	scan->playing |= isOtherApp (info->proplist);
    }
}

PulseAudioPlayingDetector::PulseAudioPlayingDetector (
    Application::ApplicationContext& appContext,
    const Render::Drivers::Detectors::FullScreenDetector& fullscreenDetector
) : AudioPlayingDetector (appContext, fullscreenDetector) {
    this->m_mainloop = pa_mainloop_new ();
    this->m_mainloopApi = pa_mainloop_get_api (this->m_mainloop);
    this->m_context = pa_context_new (this->m_mainloopApi, "wallpaperengine");

    pa_context_connect (this->m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    // lock until pulseaudio allows connection
    while (pa_context_get_state (this->m_context) != PA_CONTEXT_READY) {
	pa_mainloop_iterate (this->m_mainloop, 1, nullptr);
    }
}

PulseAudioPlayingDetector::~PulseAudioPlayingDetector () {
    if (this->m_context) {
	pa_context_disconnect (this->m_context);
	pa_context_unref (this->m_context);
    }

    if (this->m_mainloop) {
	pa_mainloop_free (this->m_mainloop);
    }
}

void PulseAudioPlayingDetector::update () {
    if (!this->getApplicationContext ().settings.audio.automute) {
	return this->setIsPlaying (false);
    }
    if (this->getFullscreenDetector ().anythingFullscreen ()) {
	return this->setIsPlaying (true);
    }

    // a server round trip every frame is wasted work, sound starting a few frames late doesn't matter
    const auto now = std::chrono::steady_clock::now ();
    if (now - this->m_lastCheck < std::chrono::milliseconds (250)) {
	return;
    }
    this->m_lastCheck = now;

    PlayingScan scan;

    this->wait (pa_context_get_sink_input_info_list (this->m_context, sinkInputInfoCallback, &scan));

    for (const uint32_t client : scan.clients) {
	if (scan.playing) {
	    break;
	}

	this->wait (pa_context_get_client_info (this->m_context, client, clientInfoCallback, &scan));
    }

    this->setIsPlaying (scan.playing);
}

void PulseAudioPlayingDetector::wait (pa_operation* operation) const {
    if (operation == nullptr) {
	return;
    }

    while (pa_operation_get_state (operation) == PA_OPERATION_RUNNING) {
	pa_mainloop_iterate (this->m_mainloop, 1, nullptr);
    }

    pa_operation_unref (operation);
}
} // namespace WallpaperEngine::Audio::Drivers::Detectors