#pragma once

#include "AudioPlayingDetector.h"
#include <condition_variable>
#include <mutex>
#include <pulse/pulseaudio.h>

namespace WallpaperEngine::Audio::Drivers::Detectors {
class PulseAudioPlayingDetector final : public AudioPlayingDetector {
public:
    explicit PulseAudioPlayingDetector (
	Application::ApplicationContext& appContext, const Render::Drivers::Detectors::FullScreenDetector&
    );
    ~PulseAudioPlayingDetector () override;

    void update () override;

private:
    /** Runs the main loop until the operation is done */
    void wait (pa_operation* operation) const;

    pa_mainloop* m_mainloop = nullptr;
    pa_mainloop_api* m_mainloopApi = nullptr;
    pa_context* m_context = nullptr;
    std::chrono::steady_clock::time_point m_lastCheck = {};
};
} // namespace WallpaperEngine::Audio::Drivers::Detectors
