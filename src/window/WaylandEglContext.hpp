#pragma once

#include <EGL/egl.h>

struct wl_display;
struct wl_surface;
struct wl_egl_window;

namespace coomer {

// Owns the EGL objects associated with a Wayland surface. Reset before the
// surface and display are destroyed, including after partial initialization.
class WaylandEglContext {
public:
    WaylandEglContext() = default;
    ~WaylandEglContext();
    WaylandEglContext(const WaylandEglContext&) = delete;
    WaylandEglContext& operator=(const WaylandEglContext&) = delete;

    bool init(wl_display* display, wl_surface* surface, int width, int height);
    void reset();
    void resize(int width, int height);
    bool swap();
    static void* getProcAddress(const char* name);

private:
    wl_egl_window* window_ = nullptr;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
};

}  // namespace coomer
