#include "AudioPlayingDetector.h"

namespace WallpaperEngine::Audio::Drivers::Detectors {
AudioPlayingDetector::AudioPlayingDetector (
    Application::ApplicationContext& appContext,
    const Render::Drivers::Detectors::FullScreenDetector& fullscreenDetector
) : m_muted (appContext.settings.audio.muted), m_applicationContext (appContext),
    m_fullscreenDetector (fullscreenDetector) { }

/** The wallpaper's sound comes back this long after the other app went quiet */
static constexpr auto RESUME_DELAY = std::chrono::seconds (2);

bool AudioPlayingDetector::anythingPlaying () const { return this->m_isPlaying; }

Application::ApplicationContext& AudioPlayingDetector::getApplicationContext () const {
    return this->m_applicationContext;
}

const Render::Drivers::Detectors::FullScreenDetector& AudioPlayingDetector::getFullscreenDetector () const {
    return this->m_fullscreenDetector;
}

void AudioPlayingDetector::setIsPlaying (const bool newState) {
    this->m_isPlaying = newState;

    if (newState) {
	this->m_lastPlayingAt = std::chrono::steady_clock::now ().time_since_epoch ().count ();
    }
}

void AudioPlayingDetector::setMuted (const bool muted) { this->m_muted = muted; }

bool AudioPlayingDetector::wantsSilence () const {
    if (this->m_muted || this->m_isPlaying) {
	return true;
    }

    const int64_t lastPlayingAt = this->m_lastPlayingAt;

    return lastPlayingAt != 0
	&& std::chrono::steady_clock::now ().time_since_epoch ()
	    < std::chrono::steady_clock::duration (lastPlayingAt) + RESUME_DELAY;
}

void AudioPlayingDetector::update () { }

} // namespace WallpaperEngine::Audio::Drivers::Detectors