#pragma once

#include <memory>
#include <string>
#include <vector>

namespace WallpaperEngine::Application {
/**
 * Commands for a running engine, one per line on the standard input (--control). A thread reads them so the
 * render loop never blocks on the input; fd () becomes readable when commands are waiting.
 */
class ControlChannel {
public:
    ControlChannel ();

    /**
     * @return An eventfd that is readable while commands are waiting to be taken
     */
    [[nodiscard]] int fd () const;
    /**
     * @return The commands received since the last call, oldest first
     */
    [[nodiscard]] std::vector<std::string> take ();

private:
    struct State;
    /** Shared with the reader thread, which can outlive this object (it blocks on the input until exit) */
    std::shared_ptr<State> m_state;
};
} // namespace WallpaperEngine::Application
