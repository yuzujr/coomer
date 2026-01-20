#pragma once

#include <optional>
#include <string>
#include <vector>

#include "capture/BackendFactory.hpp"

namespace coomer {

struct CliOptions {
    BackendKind backend = BackendKind::Auto;
    std::optional<std::string> monitor;
    bool listMonitors = false;
    bool debug = false;
    bool noSpotlight = false;
    bool overlay = false;
};

bool parseCli(int argc, char** argv, CliOptions& out, std::string& err);
std::string backendKindToString(BackendKind kind);

}  // namespace coomer
