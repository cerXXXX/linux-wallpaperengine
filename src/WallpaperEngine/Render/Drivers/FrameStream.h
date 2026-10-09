#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

namespace WallpaperEngine::Render::Drivers {
/**
 * Encodes the rendered frames into a live H.264 stream (VA-API, MPEG-TS) through an ffmpeg child process, so
 * something else can show the wallpaper while the engine's own surface can't be seen (e.g. a lock screen)
 */
class FrameStream {
public:
    FrameStream (std::string target, int fps);
    ~FrameStream ();

    /** Reads the current draw framebuffer and sends it to the encoder, restarting it when the size changes */
    void pushFrame (int width, int height);

private:
    void start (int width, int height);
    void stop ();

    std::string m_target;
    int m_fps;
    int m_width = 0;
    int m_height = 0;
    int m_pipe = -1;
    pid_t m_child = -1;
    std::vector<uint8_t> m_pixels;
};
} // namespace WallpaperEngine::Render::Drivers
