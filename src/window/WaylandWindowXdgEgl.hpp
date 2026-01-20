#pragma once

#include <memory>

#include "window/IWindow.hpp"

namespace coomer {

std::unique_ptr<IWindow> CreateWaylandWindowXdgEgl(const WindowConfig& config);

}  // namespace coomer
