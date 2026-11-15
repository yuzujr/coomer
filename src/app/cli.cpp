#include "app/cli.hpp"

#include <iostream>

namespace coomer {

#ifndef COOMER_VERSION
#define COOMER_VERSION "dev"
#endif

void printUsage(std::ostream& output, const char* exe) {
    output << "Usage: " << exe << " [options]\n"
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
           << "  --version              Show version\n"
           << "  --debug                Enable debug logging\n"
           << "  --help, -h             Show this help message\n"
           << "\n"
           << "Hotkeys:\n"
           << "  Q or Escape: quit\n"
           << "  Hold Left click: pan\n"
           << "  Scroll wheel: zoom (0.05x to 32x)\n"
           << "  Hold Space: overview; click to center the original zoom on a "
              "location\n"
           << "  P: toggle pixel inspection\n"
           << "  C: copy HEX color while inspecting\n"
           << "  1: reset view\n"
           << "  Hold Ctrl: spotlight (Ctrl + wheel to resize)\n"
           << "\n"
           << "Examples:\n"
           << "  " << exe << " --list-monitors\n"
           << "  " << exe << " --overlay --monitor HDMI-A-1\n"
           << "  " << exe << " --backend portal --portal-interactive\n"
           << "\n"
           << "Copying colors requires wl-copy on Wayland or xclip on X11.\n"
           << "Use --debug for diagnostics; COOMER_LOG_FILE saves a log.\n";
}

void printVersion(std::ostream& output) {
    output << "coomer " << COOMER_VERSION << "\n";
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
    out = CliOptions{};
    err.clear();
    // Help and version are available without a display or valid run options.
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "--version") {
            out.action =
                arg == "--version" ? CliAction::Version : CliAction::Help;
            return true;
        }
    }
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto equals = arg.find('=');
        const std::string option = arg.substr(0, equals);
        if (option == "--backend" || option == "--monitor") {
            std::string value;
            if (equals != std::string::npos) {
                value = arg.substr(equals + 1);
            } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                value = argv[++i];
            }
            if (value.empty()) {
                err = option +
                      (option == "--backend"
                           ? " requires a value: auto, x11, wlr or portal"
                           : " requires an output name or 'all'; use "
                             "--list-monitors");
                return false;
            }
            if (option == "--monitor") {
                out.monitor = value;
            } else if (value == "auto") {
                out.backend = BackendKind::Auto;
            } else if (value == "x11") {
                out.backend = BackendKind::X11;
            } else if (value == "wlr") {
                out.backend = BackendKind::Wlr;
            } else if (value == "portal") {
                out.backend = BackendKind::Portal;
            } else {
                err = "unknown backend '" + value +
                      "'; choose auto, x11, wlr or portal";
                return false;
            }
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
        } else {
            err = "unknown argument: " + arg;
            return false;
        }
    }
    if (out.backend == BackendKind::Portal &&
        (out.monitor || out.listMonitors)) {
        err =
            "portal does not support --monitor or --list-monitors; "
            "use --portal-interactive to choose the capture in the system "
            "dialog";
        return false;
    }
    if (out.portalInteractive && out.backend != BackendKind::Auto &&
        out.backend != BackendKind::Portal) {
        err = "--portal-interactive requires --backend portal or auto";
        return false;
    }
    return true;
}

}  // namespace coomer
