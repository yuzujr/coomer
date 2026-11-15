#include "window/WaylandEglContext.hpp"

#include <EGL/eglext.h>
#include <wayland-egl.h>

#include "platform/Log.hpp"

namespace coomer {

WaylandEglContext::~WaylandEglContext() {
    reset();
}

bool WaylandEglContext::init(wl_display* display, wl_surface* surface,
                             int width, int height) {
    reset();
    window_ = wl_egl_window_create(surface, width, height);
    if (!window_) {
        LOG_ERROR("failed to create wl_egl_window");
        return false;
    }
    display_ = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display));
    if (display_ == EGL_NO_DISPLAY ||
        !eglInitialize(display_, nullptr, nullptr)) {
        LOG_ERROR("failed to initialize EGL display: 0x%x", eglGetError());
        return false;
    }
    if (!eglBindAPI(EGL_OPENGL_API)) {
        LOG_ERROR("failed to select OpenGL API: 0x%x", eglGetError());
        return false;
    }
    const EGLint attributes[] = {EGL_SURFACE_TYPE,
                                 EGL_WINDOW_BIT,
                                 EGL_RED_SIZE,
                                 8,
                                 EGL_GREEN_SIZE,
                                 8,
                                 EGL_BLUE_SIZE,
                                 8,
                                 EGL_ALPHA_SIZE,
                                 8,
                                 EGL_RENDERABLE_TYPE,
                                 EGL_OPENGL_BIT,
                                 EGL_NONE};
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!eglChooseConfig(display_, attributes, &config, 1, &count) ||
        count == 0) {
        LOG_ERROR("failed to choose EGL config: 0x%x", eglGetError());
        return false;
    }
    const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                                        EGL_CONTEXT_MINOR_VERSION, 3, EGL_NONE};
    context_ =
        eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttributes);
    if (context_ == EGL_NO_CONTEXT) {
        LOG_ERROR("failed to create OpenGL 3.3 context: 0x%x", eglGetError());
        return false;
    }
    surface_ = eglCreateWindowSurface(
        display_, config, reinterpret_cast<EGLNativeWindowType>(window_),
        nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOG_ERROR("failed to create EGL window surface: 0x%x", eglGetError());
        return false;
    }
    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        LOG_ERROR("eglMakeCurrent failed: 0x%x", eglGetError());
        return false;
    }
    return true;
}

void WaylandEglContext::reset() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        eglTerminate(display_);
    }
    if (window_) wl_egl_window_destroy(window_);
    window_ = nullptr;
    display_ = EGL_NO_DISPLAY;
    context_ = EGL_NO_CONTEXT;
    surface_ = EGL_NO_SURFACE;
}

void WaylandEglContext::resize(int width, int height) {
    if (window_) wl_egl_window_resize(window_, width, height, 0, 0);
}

bool WaylandEglContext::swap() {
    return display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE &&
           eglSwapBuffers(display_, surface_);
}

void* WaylandEglContext::getProcAddress(const char* name) {
    return reinterpret_cast<void*>(eglGetProcAddress(name));
}

}  // namespace coomer
