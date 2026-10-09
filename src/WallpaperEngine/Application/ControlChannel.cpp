#include "ControlChannel.h"

#include <mutex>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>

using namespace WallpaperEngine::Application;

struct ControlChannel::State {
    std::mutex mutex;
    std::vector<std::string> commands;
    int eventFd = -1;
};

ControlChannel::ControlChannel () : m_state (std::make_shared<State> ()) {
    this->m_state->eventFd = eventfd (0, EFD_CLOEXEC | EFD_NONBLOCK);

    // plain read () on the descriptor: std::cin would hold the stdin FILE lock while it waits, and exit () takes
    // that lock to flush stdio, so the engine would hang on its way out
    std::thread ([state = this->m_state] () {
	std::string pending;
	char buffer[256];
	ssize_t length;

	while ((length = read (STDIN_FILENO, buffer, sizeof (buffer))) > 0) {
	    pending.append (buffer, length);

	    size_t end;
	    bool received = false;

	    while ((end = pending.find ('\n')) != std::string::npos) {
		std::lock_guard lock (state->mutex);
		state->commands.push_back (pending.substr (0, end));
		pending.erase (0, end + 1);
		received = true;
	    }

	    if (received) {
		// a full counter can't happen in practice, and the commands are queued either way
		constexpr uint64_t one = 1;
		[[maybe_unused]] const ssize_t written = write (state->eventFd, &one, sizeof (one));
	    }
	}
    }).detach ();
}

int ControlChannel::fd () const { return this->m_state->eventFd; }

std::vector<std::string> ControlChannel::take () {
    // reset the eventfd; nothing to read is fine, the queue below is what counts
    uint64_t count;
    [[maybe_unused]] const ssize_t got = read (this->m_state->eventFd, &count, sizeof (count));

    std::lock_guard lock (this->m_state->mutex);
    std::vector<std::string> commands;
    commands.swap (this->m_state->commands);
    return commands;
}
