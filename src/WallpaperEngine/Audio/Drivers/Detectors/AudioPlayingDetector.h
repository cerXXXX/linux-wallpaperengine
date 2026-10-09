#pragma once

#include <atomic>
#include <chrono>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"

namespace WallpaperEngine {
namespace Application {
    class ApplicationContext;
}

namespace Render::Drivers::Detectors {
    class FullScreenDetector;
}

namespace Audio::Drivers::Detectors {
    /**
     * Base class for any implementation of audio playing detection
     */
    class AudioPlayingDetector {
    public:
	AudioPlayingDetector (
	    Application::ApplicationContext& appContext,
	    const Render::Drivers::Detectors::FullScreenDetector& fullscreenDetector
	);

	virtual ~AudioPlayingDetector () = default;

	/**
	 * @return If any kind of sound is currently playing on the default audio device
	 */
	[[nodiscard]] bool anythingPlaying () const;

	/**
	 * Updates the playing status to the specified value
	 *
	 * @param newState
	 */
	void setIsPlaying (bool newState);

	/**
	 * Fades the wallpaper's sound out (or back in) no matter what else is playing
	 *
	 * @param muted
	 */
	void setMuted (bool muted);
	/**
	 * @return If the wallpaper's sound should be faded out: it's muted, something else is playing, or something
	 * else stopped playing less than RESUME_DELAY ago
	 */
	[[nodiscard]] bool wantsSilence () const;

	/**
	 * Checks if any audio is playing and updates state accordingly
	 */
	virtual void update ();

    protected:
	/**
	 * @return The application context using this detector
	 */
	[[nodiscard]] Application::ApplicationContext& getApplicationContext () const;
	/**
	 * @return The fullscreen detector used
	 */
	[[nodiscard]] const Render::Drivers::Detectors::FullScreenDetector& getFullscreenDetector () const;

    private:
	/** Read by the audio thread */
	std::atomic<bool> m_isPlaying = false;
	std::atomic<bool> m_muted = false;
	/** When something else was last seen playing (steady clock, nanoseconds) */
	std::atomic<int64_t> m_lastPlayingAt = 0;

	Application::ApplicationContext& m_applicationContext;
	const Render::Drivers::Detectors::FullScreenDetector& m_fullscreenDetector;
    };
} // namespace Audio::Drivers::Detectors
} // namespace WallpaperEngine
