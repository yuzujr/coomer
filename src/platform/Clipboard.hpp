#pragma once

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace coomer {

// Clipboard helpers own the selection after coomer exits. Pass text through
// stdin, never through a shell or command-line interpolation.
inline bool copyToClipboard(const std::string& text) {
    FILE* input = std::tmpfile();
    if (!input) return false;
    if (std::fwrite(text.data(), 1, text.size(), input) != text.size() ||
        std::fflush(input) != 0 || std::fseek(input, 0, SEEK_SET) != 0) {
        std::fclose(input);
        return false;
    }
    char waylandCommand[] = "wl-copy";
    char type[] = "--type";
    char mime[] = "text/plain";
    char x11Command[] = "xclip";
    char selection[] = "-selection";
    char clipboard[] = "clipboard";
    char* waylandArgs[] = {waylandCommand, type, mime, nullptr};
    char* x11Args[] = {x11Command, selection, clipboard, nullptr};
    char** args = std::getenv("WAYLAND_DISPLAY") ? waylandArgs : x11Args;

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        std::fclose(input);
        return false;
    }
    int error = posix_spawn_file_actions_adddup2(&actions, fileno(input), STDIN_FILENO);
    pid_t child = -1;
    if (!error) error = posix_spawnp(&child, args[0], &actions, nullptr, args, environ);
    posix_spawn_file_actions_destroy(&actions);
    std::fclose(input);
    if (error) return false;
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    return waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}  // namespace coomer
