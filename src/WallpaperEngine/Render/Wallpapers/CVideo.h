#pragma once

#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/GainRamp.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/VideoPlayback/MPV/GLPlayer.h"

namespace WallpaperEngine::Render::Wallpapers {
using namespace WallpaperEngine::VideoPlayback::MPV;

class CVideo final : public CWallpaper {
public:
    CVideo (
	const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
	const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode,
	const glm::ivec2& outputSize = { 0, 0 }
    );

    ~CVideo () override;

    const Data::Model::Video& getVideo () const;

    [[nodiscard]] int getWidth () const override;
    [[nodiscard]] int getHeight () const override;
    [[nodiscard]] double getContentFrameRate () const override;
    [[nodiscard]] bool hasContent () const override;

    void setPause (bool newState) override;

protected:
    void renderFrame (const glm::ivec4& viewport) override;

    friend class CWallpaper;

private:
    GLPlayerUniquePtr m_player;

    /** Fade of the video's sound, see GainRamp */
    Audio::Drivers::Detectors::GainRamp m_ramp;
    /** Volume set on the player, and the volume to fade from */
    double m_baseVolume = 0.0;
    double m_appliedVolume = -1.0;
    float m_lastRampTime = 0.0f;
};
} // namespace WallpaperEngine::Render::Wallpapers
