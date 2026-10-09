#pragma once

#include <map>
#include <vector>

#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Audio/Drivers/AudioDriver.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/GainRamp.h"

#include <atomic>

#include <SDL.h>

#define MAX_AUDIO_FRAME_SIZE 192000

namespace WallpaperEngine::Audio::Drivers {
/**
 * Audio output buffers for streams being played under SDL
 */
struct SDLAudioBuffer {
    AudioStream* stream = nullptr;
    uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2] = { 0 };
    unsigned int audio_buf_size = 0;
    unsigned int audio_buf_index = 0;
};

/**
 * SDL's audio driver implementation
 */
class SDLAudioDriver final : public AudioDriver {
public:
    SDLAudioDriver (
	Application::ApplicationContext& applicationContext, Detectors::AudioPlayingDetector& detector,
	Recorders::PlaybackRecorder& recorder
    );
    ~SDLAudioDriver () override;

    /** @inheritdoc */
    int addStream (AudioStream* stream) override;
    /** @inheritdoc */
    void removeStream (int streamId) override;
    /**
     * @return All the registered audio streams
     */
    const std::map<int, SDLAudioBuffer*>& getStreams ();

    /** @inheritdoc */
    [[nodiscard]] AVSampleFormat getFormat () const override;
    /** @inheritdoc */
    [[nodiscard]] int getSampleRate () const override;
    /** @inheritdoc */
    [[nodiscard]] int getChannels () const override;
    /**
     * @return The SDL's audio driver settings
     */
    [[nodiscard]] const SDL_AudioSpec& getSpec () const;

    [[nodiscard]] SDL_mutex* getStreamMutex () const;

    /**
     * Stops the audio device while there's nothing to hear (no sounds, or faded out) so the sound card can go idle
     */
    void update () override;

    /** Fade of the wallpaper's sound, only stepped by the audio callback */
    Detectors::GainRamp m_ramp;
    /** The ramp's gain, for the main thread */
    std::atomic<float> m_gain = 1.0f;

private:
    /** The mutex lock used to access the stream list mutex */
    SDL_mutex* m_streamListMutex;
    /** The last streamID used */
    int m_lastStreamID = 0;
    /** The device's ID */
    SDL_AudioDeviceID m_deviceID;
    /** If the driver is initialized or not */
    bool m_initialized = false;
    bool m_devicePaused = false;
    /** The sound output configuration */
    SDL_AudioSpec m_audioSpec {};
    /** All the playable steams */
    std::map<int, SDLAudioBuffer*> m_streams {};
};
} // namespace WallpaperEngine::Audio::Drivers