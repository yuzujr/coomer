#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "platform/Clipboard.hpp"

int main() {
    // Helpers are fakes: this test never changes the desktop clipboard.
    char directory[] = "/tmp/coomer-clipboard-XXXXXX";
    assert(mkdtemp(directory));
    const std::filesystem::path root(directory);
    setenv("PATH", directory, 1);
    setenv("COOMER_TEST_CLIPBOARD", (root / "result").c_str(), 1);
    for (const char* helper : {"wl-copy", "xclip"}) {
        const auto script = root / helper;
        std::ofstream file(script);
        file << "#!/bin/sh\n"
                "printf '%s\\n' \"$@\" > \"$COOMER_TEST_CLIPBOARD.args\"\n"
                "IFS= read -r value || :\n"
                "printf '%s' \"$value\" > \"$COOMER_TEST_CLIPBOARD\"\n";
        file.close();
        std::filesystem::permissions(script, std::filesystem::perms::owner_all);
        if (std::string(helper) == "wl-copy") setenv("WAYLAND_DISPLAY", "test", 1);
        else unsetenv("WAYLAND_DISPLAY");
        assert(coomer::copyToClipboard("#12ABFE"));
        std::ifstream result(root / "result");
        const std::string text((std::istreambuf_iterator<char>(result)), {});
        assert(text == "#12ABFE");
        std::ifstream args(root / "result.args");
        const std::string arguments((std::istreambuf_iterator<char>(args)), {});
        assert(arguments == (std::string(helper) == "wl-copy"
            ? "--type\ntext/plain\n" : "-selection\nclipboard\n"));
    }
    std::filesystem::remove(root / "xclip");
    assert(!coomer::copyToClipboard("#FFFFFF"));
    std::ofstream failure(root / "xclip");
    failure << "#!/bin/sh\nexit 1\n";
    failure.close();
    std::filesystem::permissions(root / "xclip", std::filesystem::perms::owner_all);
    assert(!coomer::copyToClipboard("#FFFFFF"));
    std::filesystem::remove_all(root);
}
