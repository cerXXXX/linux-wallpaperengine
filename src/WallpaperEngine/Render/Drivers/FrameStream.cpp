#include "FrameStream.h"

#include "WallpaperEngine/Logging/Log.h"

#include <GL/glew.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

using namespace WallpaperEngine::Render::Drivers;

FrameStream::FrameStream (std::string target, int fps) : m_target (std::move (target)), m_fps (std::max (1, fps)) {
    // a dead encoder must not take the engine down with it, write () reports EPIPE instead
    signal (SIGPIPE, SIG_IGN);
}

FrameStream::~FrameStream () { this->stop (); }

void FrameStream::start (int width, int height) {
    int fds[2];

    if (pipe (fds) != 0) {
	sLog.exception ("Cannot create the stream pipe");
    }

    const std::string size = std::to_string (width) + "x" + std::to_string (height);
    const std::string fps = std::to_string (this->m_fps);
    // frames come bottom-up from glReadPixels; timestamps follow the wall clock so a slow frame doesn't make the
    // stream run fast, the player keeps up with a live source
    std::vector<std::string> args = {
	"ffmpeg", "-hide_banner", "-loglevel", "error", "-use_wallclock_as_timestamps", "1", "-f", "rawvideo",
	"-pix_fmt", "rgba", "-s", size, "-i", "-", "-vaapi_device", "/dev/dri/renderD128", "-vf",
	"vflip,format=nv12,hwupload", "-c:v", "h264_vaapi", "-g", fps, "-bf", "0", "-bsf:v", "dump_extra", "-fps_mode", "passthrough",
	"-f", "mpegts", this->m_target,
    };
    std::vector<char*> argv;
    for (auto& arg : args) {
	argv.push_back (arg.data ());
    }
    argv.push_back (nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init (&actions);
    posix_spawn_file_actions_adddup2 (&actions, fds[0], STDIN_FILENO);
    posix_spawn_file_actions_addclose (&actions, fds[1]);

    const int result = posix_spawnp (&this->m_child, "ffmpeg", &actions, nullptr, argv.data (), environ);
    posix_spawn_file_actions_destroy (&actions);
    close (fds[0]);

    if (result != 0) {
	close (fds[1]);
	this->m_child = -1;
	sLog.exception ("Cannot start ffmpeg for --stream: ", strerror (result));
    }

    this->m_pipe = fds[1];
    this->m_width = width;
    this->m_height = height;
    this->m_pixels.resize (static_cast<size_t> (width) * height * 4);
    sLog.out ("Streaming ", size, " to ", this->m_target);
}

void FrameStream::stop () {
    if (this->m_pipe != -1) {
	close (this->m_pipe);
	this->m_pipe = -1;
    }

    if (this->m_child != -1) {
	waitpid (this->m_child, nullptr, 0);
	this->m_child = -1;
    }
}

void FrameStream::pushFrame (int width, int height) {
    if (width <= 0 || height <= 0) {
	return;
    }

    if (width != this->m_width || height != this->m_height) {
	this->stop ();
	this->start (width, height);
    }

    glPixelStorei (GL_PACK_ALIGNMENT, 1);
    glReadPixels (0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, this->m_pixels.data ());

    const uint8_t* data = this->m_pixels.data ();
    size_t left = this->m_pixels.size ();

    while (left > 0) {
	const ssize_t written = write (this->m_pipe, data, left);

	if (written < 0) {
	    if (errno == EINTR) {
		continue;
	    }

	    sLog.exception ("The stream encoder stopped: ", strerror (errno));
	}

	data += written;
	left -= static_cast<size_t> (written);
    }
}
