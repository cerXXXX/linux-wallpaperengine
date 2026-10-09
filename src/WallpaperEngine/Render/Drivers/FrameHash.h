#pragma once

#include <GL/glew.h>
#include <cstdint>
#include <glm/vec4.hpp>
#include <vector>

namespace WallpaperEngine::Render::Drivers {
/**
 * Reads the pixels of an area of a framebuffer (RGBA, bottom row first) and hashes them, to tell if a held
 * wallpaper's frame changed. Every pixel counts: a clock's seconds are a few pixels of a big frame.
 *
 * @param framebuffer
 * @param area x, y, width, height
 * @param pixels Where the pixels are read to (reused between calls)
 * @return The hash, never 0 (0 means "no frame shown yet")
 */
inline uint64_t hashFramebuffer (GLuint framebuffer, const glm::ivec4& area, std::vector<uint8_t>& pixels) {
    // whole 64-bit words, the tail of an odd-sized area stays zero
    pixels.assign ((static_cast<size_t> (area.z) * area.w * 4 + 7) / 8 * 8, 0);

    glBindFramebuffer (GL_READ_FRAMEBUFFER, framebuffer);
    glPixelStorei (GL_PACK_ALIGNMENT, 1);
    glReadPixels (area.x, area.y, area.z, area.w, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());

    // FNV-1a over 64-bit words
    uint64_t hash = 14695981039346656037ull;
    const auto* words = reinterpret_cast<const uint64_t*> (pixels.data ());

    for (size_t i = 0, count = pixels.size () / 8; i < count; i++) {
	hash = (hash ^ words[i]) * 1099511628211ull;
    }

    return hash | 1;
}
} // namespace WallpaperEngine::Render::Drivers
