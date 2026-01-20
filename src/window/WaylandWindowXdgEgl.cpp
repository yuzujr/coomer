#include "window/WaylandWindowXdgEgl.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "platform/Log.hpp"
#include "xdg-shell-client-protocol.h"

#if __has_include(<linux/input-event-codes.h>)
#include <linux/input-event-codes.h>
#else
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#endif

namespace coomer {

class WaylandWindowXdgEgl final : public IWindow {
public:
    explicit WaylandWindowXdgEgl(const WindowConfig& config) {
        width_ = std::max(1, config.width);
        height_ = std::max(1, config.height);

        display_ = wl_display_connect(nullptr);
        if (!display_) {
            LOG_ERROR("failed to connect to Wayland display");
            return;
        }

        registry_ = wl_display_get_registry(display_);
        wl_registry_add_listener(registry_, &registryListener_, this);
        wl_display_roundtrip(display_);

        if (!compositor_ || !wmBase_) {
            LOG_ERROR("Wayland compositor or xdg_wm_base missing");
            return;
        }

        surface_ = wl_compositor_create_surface(compositor_);
        if (!surface_) {
            LOG_ERROR("failed to create Wayland surface");
            return;
        }

        xdgSurface_ = xdg_wm_base_get_xdg_surface(wmBase_, surface_);
        xdg_surface_add_listener(xdgSurface_, &xdgSurfaceListener_, this);

        xdgToplevel_ = xdg_surface_get_toplevel(xdgSurface_);
        xdg_toplevel_add_listener(xdgToplevel_, &xdgToplevelListener_, this);
        xdg_toplevel_set_title(xdgToplevel_, config.title.c_str());
        xdg_toplevel_set_fullscreen(xdgToplevel_, nullptr);

        wl_surface_commit(surface_);
        wl_display_roundtrip(display_);

        if (!configured_) {
            wl_display_roundtrip(display_);
        }

        eglWindow_ = wl_egl_window_create(surface_, width_, height_);
        if (!eglWindow_) {
            LOG_ERROR("failed to create wl_egl_window");
            return;
        }

        eglDisplay_ =
            eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display_));
        if (eglDisplay_ == EGL_NO_DISPLAY) {
            LOG_ERROR("failed to get EGL display");
            return;
        }
        if (!eglInitialize(eglDisplay_, nullptr, nullptr)) {
            LOG_ERROR("failed to initialize EGL");
            return;
        }
        eglBindAPI(EGL_OPENGL_API);

        EGLint attribs[] = {EGL_SURFACE_TYPE,
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

        EGLConfig eglConfig = nullptr;
        EGLint numConfigs = 0;
        if (!eglChooseConfig(eglDisplay_, attribs, &eglConfig, 1,
                             &numConfigs) ||
            numConfigs == 0) {
            LOG_ERROR("failed to choose EGL config");
            return;
        }

        EGLint ctxAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                               EGL_CONTEXT_MINOR_VERSION, 3, EGL_NONE};
        eglContext_ = eglCreateContext(eglDisplay_, eglConfig, EGL_NO_CONTEXT,
                                       ctxAttribs);
        if (eglContext_ == EGL_NO_CONTEXT) {
            LOG_ERROR("failed to create EGL context");
            return;
        }

        eglSurface_ = eglCreateWindowSurface(
            eglDisplay_, eglConfig,
            reinterpret_cast<EGLNativeWindowType>(eglWindow_), nullptr);
        if (eglSurface_ == EGL_NO_SURFACE) {
            LOG_ERROR("failed to create EGL window surface");
            return;
        }

        if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_,
                            eglContext_)) {
            LOG_ERROR("eglMakeCurrent failed");
            return;
        }

        // Commit an initial frame to ensure the compositor receives a buffer
        if (surface_) {
            wl_surface_damage_buffer(surface_, 0, 0, width_, height_);
            frameCallback_ = wl_surface_frame(surface_);
            wl_callback_add_listener(frameCallback_, &frameListener_, this);

            // Swap without clearing to avoid visible flash before screenshot
            // renders
            if (!eglSwapBuffers(eglDisplay_, eglSurface_)) {
                EGLint err = eglGetError();
                LOG_WARN("xdg-shell initial eglSwapBuffers failed: 0x%x", err);
            }
            wl_display_flush(display_);
        }

        xkbContext_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

        valid_ = true;
    }

    ~WaylandWindowXdgEgl() override {
        if (frameCallback_) {
            wl_callback_destroy(frameCallback_);
        }
        if (keyboard_) {
            wl_keyboard_destroy(keyboard_);
        }
        if (pointer_) {
            wl_pointer_destroy(pointer_);
        }
        if (seat_) {
            wl_seat_destroy(seat_);
        }
        if (xkbState_) {
            xkb_state_unref(xkbState_);
        }
        if (xkbKeymap_) {
            xkb_keymap_unref(xkbKeymap_);
        }
        if (xkbContext_) {
            xkb_context_unref(xkbContext_);
        }
        if (eglDisplay_ != EGL_NO_DISPLAY) {
            eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
            if (eglContext_ != EGL_NO_CONTEXT) {
                eglDestroyContext(eglDisplay_, eglContext_);
            }
            if (eglSurface_ != EGL_NO_SURFACE) {
                eglDestroySurface(eglDisplay_, eglSurface_);
            }
            eglTerminate(eglDisplay_);
        }
        if (eglWindow_) {
            wl_egl_window_destroy(eglWindow_);
        }
        if (xdgToplevel_) {
            xdg_toplevel_destroy(xdgToplevel_);
        }
        if (xdgSurface_) {
            xdg_surface_destroy(xdgSurface_);
        }
        if (surface_) {
            wl_surface_destroy(surface_);
        }
        if (wmBase_) {
            xdg_wm_base_destroy(wmBase_);
        }
        if (compositor_) {
            wl_compositor_destroy(compositor_);
        }
        if (registry_) {
            wl_registry_destroy(registry_);
        }
        if (display_) {
            wl_display_disconnect(display_);
        }
    }

    bool isValid() const {
        return valid_;
    }

    void pollEvents() override {
        input_.deltaX = 0.0;
        input_.deltaY = 0.0;
        input_.wheelDelta = 0.0;

        if (!display_) {
            shouldClose_ = true;
            return;
        }

        wl_display_dispatch_pending(display_);
        wl_display_flush(display_);

        int fd = wl_display_get_fd(display_);
        pollfd pfd{fd, POLLIN, 0};
        if (poll(&pfd, 1, 0) > 0) {
            wl_display_dispatch(display_);
        }
    }

    bool shouldClose() const override {
        return shouldClose_;
    }
    InputState input() const override {
        return input_;
    }
    int width() const override {
        return width_;
    }
    int height() const override {
        return height_;
    }

    void swap() override {
        if (eglDisplay_ != EGL_NO_DISPLAY && eglSurface_ != EGL_NO_SURFACE) {
            if (surface_) {
                wl_surface_damage_buffer(surface_, 0, 0, width_, height_);
                if (!frameCallback_) {
                    frameCallback_ = wl_surface_frame(surface_);
                    wl_callback_add_listener(frameCallback_, &frameListener_,
                                             this);
                }
            }
            if (!eglSwapBuffers(eglDisplay_, eglSurface_)) {
                EGLint err = eglGetError();
                LOG_WARN("xdg-shell eglSwapBuffers failed: 0x%x", err);
            }
            if (display_) {
                wl_display_flush(display_);
            }
        }
    }

    void* glGetProcAddress(const char* name) override {
        return reinterpret_cast<void*>(eglGetProcAddress(name));
    }

private:
    static void handleGlobal(void* data, wl_registry* registry, uint32_t name,
                             const char* interface, uint32_t version) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
            self->compositor_ = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface,
                                 std::min(version, 4u)));
        } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
            self->seat_ = static_cast<wl_seat*>(wl_registry_bind(
                registry, name, &wl_seat_interface, std::min(version, 5u)));
            wl_seat_add_listener(self->seat_, &seatListener_, self);
        } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
            self->wmBase_ = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
            xdg_wm_base_add_listener(self->wmBase_, &wmBaseListener_, self);
        }
    }

    static void handleGlobalRemove(void* data, wl_registry* registry,
                                   uint32_t name) {
        (void)data;
        (void)registry;
        (void)name;
    }

    static void handlePing(void* data, xdg_wm_base* wm, uint32_t serial) {
        xdg_wm_base_pong(wm, serial);
        (void)data;
    }

    static void handleXdgSurfaceConfigure(void* data, xdg_surface* surface,
                                          uint32_t serial) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        xdg_surface_ack_configure(surface, serial);
        self->configured_ = true;
    }

    static void handleToplevelConfigure(void* data, xdg_toplevel*,
                                        int32_t width, int32_t height,
                                        wl_array*) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        if (width > 0 && height > 0) {
            self->width_ = width;
            self->height_ = height;
            if (self->eglWindow_) {
                wl_egl_window_resize(self->eglWindow_, width, height, 0, 0);
            }
        }
    }

    static void handleToplevelClose(void* data, xdg_toplevel*) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        self->shouldClose_ = true;
    }

    static void handleFrameDone(void* data, wl_callback* callback, uint32_t) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        if (callback) {
            wl_callback_destroy(callback);
        }
        self->frameCallback_ = nullptr;
    }

    static void handleSeatCapabilities(void* data, wl_seat* seat,
                                       uint32_t caps) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        if (caps & WL_SEAT_CAPABILITY_POINTER) {
            if (!self->pointer_) {
                self->pointer_ = wl_seat_get_pointer(seat);
                wl_pointer_add_listener(self->pointer_, &pointerListener_,
                                        self);
            }
        } else if (self->pointer_) {
            wl_pointer_destroy(self->pointer_);
            self->pointer_ = nullptr;
        }

        if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
            if (!self->keyboard_) {
                self->keyboard_ = wl_seat_get_keyboard(seat);
                wl_keyboard_add_listener(self->keyboard_, &keyboardListener_,
                                         self);
            }
        } else if (self->keyboard_) {
            wl_keyboard_destroy(self->keyboard_);
            self->keyboard_ = nullptr;
        }
    }

    static void handleSeatName(void*, wl_seat*, const char*) {}

    static void handlePointerEnter(void* data, wl_pointer*, uint32_t,
                                   wl_surface*, wl_fixed_t sx, wl_fixed_t sy) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        double x = wl_fixed_to_double(sx);
        double y = wl_fixed_to_double(sy);
        self->input_.mouseX = x;
        self->input_.mouseY = y;
        self->lastMouseX_ = x;
        self->lastMouseY_ = y;
        self->hasLastMouse_ = true;
    }

    static void handlePointerLeave(void* data, wl_pointer*, uint32_t,
                                   wl_surface*) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        self->hasLastMouse_ = false;
    }

    static void handlePointerMotion(void* data, wl_pointer*, uint32_t,
                                    wl_fixed_t sx, wl_fixed_t sy) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        double x = wl_fixed_to_double(sx);
        double y = wl_fixed_to_double(sy);
        if (!self->hasLastMouse_) {
            self->lastMouseX_ = x;
            self->lastMouseY_ = y;
            self->hasLastMouse_ = true;
        }
        self->input_.deltaX += x - self->lastMouseX_;
        self->input_.deltaY += y - self->lastMouseY_;
        self->lastMouseX_ = x;
        self->lastMouseY_ = y;
        self->input_.mouseX = x;
        self->input_.mouseY = y;
    }

    static void handlePointerButton(void* data, wl_pointer*, uint32_t, uint32_t,
                                    uint32_t button, uint32_t state) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
        if (button == BTN_LEFT) {
            self->input_.mouseLeft = pressed;
        } else if (button == BTN_RIGHT) {
            self->input_.mouseRight = pressed;
        }
    }

    static void handlePointerAxis(void* data, wl_pointer*, uint32_t,
                                  uint32_t axis, wl_fixed_t value) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
            double v = wl_fixed_to_double(value);
            self->input_.wheelDelta += v / 120.0;
        }
    }

    static void handlePointerFrame(void*, wl_pointer*) {}
    static void handlePointerAxisSource(void*, wl_pointer*, uint32_t) {}
    static void handlePointerAxisStop(void*, wl_pointer*, uint32_t, uint32_t) {}
    static void handlePointerAxisDiscrete(void*, wl_pointer*, uint32_t,
                                          int32_t) {}

    static void handleKeyboardKeymap(void* data, wl_keyboard*, uint32_t format,
                                     int fd, uint32_t size) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
            close(fd);
            return;
        }
        char* map = static_cast<char*>(
            mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
        if (map == MAP_FAILED) {
            close(fd);
            return;
        }
        if (self->xkbKeymap_) {
            xkb_keymap_unref(self->xkbKeymap_);
        }
        if (self->xkbState_) {
            xkb_state_unref(self->xkbState_);
        }
        self->xkbKeymap_ = xkb_keymap_new_from_string(
            self->xkbContext_, map, XKB_KEYMAP_FORMAT_TEXT_V1,
            XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(map, size);
        close(fd);
        if (!self->xkbKeymap_) {
            return;
        }
        self->xkbState_ = xkb_state_new(self->xkbKeymap_);
    }

    static void handleKeyboardEnter(void*, wl_keyboard*, uint32_t, wl_surface*,
                                    wl_array*) {}
    static void handleKeyboardLeave(void*, wl_keyboard*, uint32_t,
                                    wl_surface*) {}

    static void handleKeyboardKey(void* data, wl_keyboard*, uint32_t, uint32_t,
                                  uint32_t key, uint32_t state) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        if (!self->xkbState_) {
            return;
        }
        bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
        xkb_keysym_t sym = xkb_state_key_get_one_sym(self->xkbState_, key + 8);
        if (sym == XKB_KEY_q || sym == XKB_KEY_Q) {
            self->input_.keyQ = pressed;
        } else if (sym == XKB_KEY_a || sym == XKB_KEY_A) {
            self->input_.keyA = pressed;
        }
    }

    static void handleKeyboardModifiers(void* data, wl_keyboard*, uint32_t,
                                        uint32_t depressed, uint32_t latched,
                                        uint32_t locked, uint32_t group) {
        auto* self = static_cast<WaylandWindowXdgEgl*>(data);
        if (!self->xkbState_) {
            return;
        }
        xkb_state_update_mask(self->xkbState_, depressed, latched, locked, 0, 0,
                              group);
        self->input_.keyCtrl =
            xkb_state_mod_name_is_active(self->xkbState_, XKB_MOD_NAME_CTRL,
                                         XKB_STATE_MODS_EFFECTIVE) > 0;
        self->input_.keyShift =
            xkb_state_mod_name_is_active(self->xkbState_, XKB_MOD_NAME_SHIFT,
                                         XKB_STATE_MODS_EFFECTIVE) > 0;
    }

    static void handleKeyboardRepeatInfo(void*, wl_keyboard*, int32_t,
                                         int32_t) {}

    static inline wl_registry_listener registryListener_ = {handleGlobal,
                                                            handleGlobalRemove};
    static inline xdg_wm_base_listener wmBaseListener_ = {handlePing};
    static inline xdg_surface_listener xdgSurfaceListener_ = {
        handleXdgSurfaceConfigure};
    static inline xdg_toplevel_listener xdgToplevelListener_ = {
        handleToplevelConfigure, handleToplevelClose};
    static inline wl_seat_listener seatListener_ = {handleSeatCapabilities,
                                                    handleSeatName};
    static inline wl_pointer_listener pointerListener_ = {
        handlePointerEnter,       handlePointerLeave,
        handlePointerMotion,      handlePointerButton,
        handlePointerAxis,        handlePointerFrame,
        handlePointerAxisSource,  handlePointerAxisStop,
        handlePointerAxisDiscrete};
    static inline wl_keyboard_listener keyboardListener_ = {
        handleKeyboardKeymap,    handleKeyboardEnter,
        handleKeyboardLeave,     handleKeyboardKey,
        handleKeyboardModifiers, handleKeyboardRepeatInfo};
    static inline wl_callback_listener frameListener_ = {handleFrameDone};

    wl_display* display_ = nullptr;
    wl_registry* registry_ = nullptr;
    wl_compositor* compositor_ = nullptr;
    wl_surface* surface_ = nullptr;
    xdg_wm_base* wmBase_ = nullptr;
    xdg_surface* xdgSurface_ = nullptr;
    xdg_toplevel* xdgToplevel_ = nullptr;

    wl_seat* seat_ = nullptr;
    wl_pointer* pointer_ = nullptr;
    wl_keyboard* keyboard_ = nullptr;

    wl_egl_window* eglWindow_ = nullptr;
    EGLDisplay eglDisplay_ = EGL_NO_DISPLAY;
    EGLContext eglContext_ = EGL_NO_CONTEXT;
    EGLSurface eglSurface_ = EGL_NO_SURFACE;

    xkb_context* xkbContext_ = nullptr;
    xkb_keymap* xkbKeymap_ = nullptr;
    xkb_state* xkbState_ = nullptr;

    InputState input_{};
    bool valid_ = false;
    bool shouldClose_ = false;
    bool configured_ = false;
    wl_callback* frameCallback_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    bool hasLastMouse_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
};

std::unique_ptr<IWindow> CreateWaylandWindowXdgEgl(const WindowConfig& config) {
    auto window = std::make_unique<WaylandWindowXdgEgl>(config);
    if (!window->isValid()) {
        return nullptr;
    }
    return window;
}

}  // namespace coomer
