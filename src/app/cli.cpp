#include "app/cli.hpp"

#include <cstdlib>
#include <iostream>

namespace coomer {

static void printUsage(const char* exe) {
    std::cerr << "Usage: " << exe << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  --backend <mode>       Capture backend: "
                 "auto|x11|wlr|portal (default: auto)\n"
              << "  --monitor <name>       Select monitor/output by name "
                 "(x11/wlr only, use 'all' to capture all monitors)\n"
              << "  --list-monitors        List monitors/outputs visible to "
                 "the backend (x11/wlr only)\n"
              << "  --overlay              Wayland layer-shell overlay "
                 "(wlr/portal only)\n"
              << "  --portal-interactive   Enable interactive mode for portal "
                 "(show selection dialog)\n"
              << "  --no-spotlight         Disable spotlight mode\n"
              << "  --debug                Enable debug logging\n"
              << "  --help, -h             Show this help message\n"
              << "\n"
              << "Hotkeys:\n"
              << "  Q or A or Right click: quit\n"
              << "  Hold Left click: pan\n"
              << "  Scroll wheel: zoom\n"
              << "  Hold Ctrl: spotlight (Ctrl + wheel to resize)\n";
}

std::string backendKindToString(BackendKind kind) {
    switch (kind) {
        case BackendKind::Auto:
            return "auto";
        case BackendKind::X11:
            return "x11";
        case BackendKind::Wlr:
            return "wlr";
        case BackendKind::Portal:
            return "portal";
    }
    return "auto";
}

bool parseCli(int argc, char** argv, CliOptions& out, std::string& err) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--backend") {
            if (i + 1 >= argc) {
                err = "--backend requires a value";
                return false;
            }
            std::string val = argv[++i];
            if (val == "auto") {
                out.backend = BackendKind::Auto;
            } else if (val == "x11") {
                out.backend = BackendKind::X11;
            } else if (val == "wlr") {
                out.backend = BackendKind::Wlr;
            } else if (val == "portal") {
                out.backend = BackendKind::Portal;
            } else {
                err = "unknown backend: " + val;
                return false;
            }
        } else if (arg == "--monitor") {
            if (i + 1 >= argc) {
                err = "--monitor requires a name";
                return false;
            }
            out.monitor = argv[++i];
        } else if (arg == "--list-monitors") {
            out.listMonitors = true;
        } else if (arg == "--debug") {
            out.debug = true;
        } else if (arg == "--no-spotlight") {
            out.noSpotlight = true;
        } else if (arg == "--overlay") {
            out.overlay = true;
        } else if (arg == "--portal-interactive") {
            out.portalInteractive = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else {
            err = "unknown argument: " + arg;
            return false;
        }
    }
    return true;
}

}  // namespace coomer
