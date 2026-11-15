#pragma once

#include <iosfwd>
#include <optional>
#include <string>

#include "capture/BackendFactory.hpp"

namespace coomer {

enum class CliAction { Run, Help, Version };

struct CliOptions {
    CliAction action = CliAction::Run;
    BackendKind backend = BackendKind::Auto;
    std::optional<std::string> monitor;
    bool listMonitors = false;
    bool debug = false;
    bool noSpotlight = false;
    bool overlay = false;
    bool portalInteractive = false;
};

bool parseCli(int argc, char** argv, CliOptions& out, std::string& err);
std::string backendKindToString(BackendKind kind);
void printUsage(std::ostream& output, const char* exe);
void printVersion(std::ostream& output);

}  // namespace coomer
