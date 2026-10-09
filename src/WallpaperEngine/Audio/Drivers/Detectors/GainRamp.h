#pragma once

#include <algorithm>

namespace WallpaperEngine::Audio::Drivers::Detectors {
/**
 * Fades the wallpaper's sound in and out instead of cutting it: the gain moves towards 0 (silence) or 1 over
 * FADE_SECONDS. Each consumer (the audio callback, every video) keeps its own ramp.
 */
class GainRamp {
public:
    static constexpr float FADE_SECONDS = 0.5f;

    /**
     * @param silence If the sound should be faded out
     * @param seconds The time since the previous step
     * @return The gain to play at, 0 to 1
     */
    float step (bool silence, float seconds) {
	const float target = silence ? 0.0f : 1.0f;

	// the first step starts at the target, so a wallpaper started muted never plays a bit of sound
	if (!this->m_started) {
	    this->m_started = true;
	    this->m_gain = target;
	}

	const float change = seconds / FADE_SECONDS;
	this->m_gain = target > this->m_gain ? std::min (target, this->m_gain + change)
					     : std::max (target, this->m_gain - change);
	return this->m_gain;
    }

    [[nodiscard]] float gain () const { return this->m_gain; }

private:
    bool m_started = false;
    float m_gain = 1.0f;
};
} // namespace WallpaperEngine::Audio::Drivers::Detectors
