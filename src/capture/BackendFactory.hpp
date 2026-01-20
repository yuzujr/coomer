#pragma once

#include <memory>

#include "capture/ICaptureBackend.hpp"

namespace coomer {

enum class BackendKind { Auto, X11, Wlr, Portal };

std::unique_ptr<ICaptureBackend> CreateBackend(BackendKind kind,
                                               bool portalInteractive = false);

}  // namespace coomer
