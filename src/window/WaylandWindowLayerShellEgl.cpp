#include "window/WaylandWindowLayerShellEgl.hpp"

#include <EGL/egl.h>
#include <poll.h>
#include <wayland-client.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "fractional-scale-v1-client-protocol.h"
#include "platform/Log.hpp"
#include "window/WaylandEglContext.hpp"
#include "window/WaylandInput.hpp"
#include "viewporter-client-protocol.h"
#define namespace wl_namespace
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#undef namespace

namespace coomer {

class WaylandWindowLayerShellEgl final : public IWindow {
public:
    explicit WaylandWindowLayerShellEgl(const WindowConfig& config) {
        surfaceWidth_ = std::max(1, config.width);
        surfaceHeight_ = std::max(1, config.height);
        width_ = surfaceWidth_;
        height_ = surfaceHeight_;

        display_ = wl_display_connect(nullptr);
        if (!display_) {
            LOG_ERROR("failed to connect to Wayland display");
            return;
        }

        registry_ = wl_display_get_registry(display_);
        wl_registry_add_listener(registry_, &registryListener_, this);
        wl_display_roundtrip(display_);

        if (!compositor_ || !layerShell_) {
            LOG_ERROR("Wayland compositor or layer-shell missing");
            return;
        }

        surface_ = wl_compositor_create_surface(compositor_);
        if (!surface_) {
            LOG_ERROR("failed to create Wayland surface");
            return;
        }
        if (viewporter_) {
            viewport_ = wp_viewporter_get_viewport(viewporter_, surface_);
        }
        if (fractionalScaleManager_) {
            fractionalScale_ = wp_fractional_scale_manager_v1_get_fractional_scale(
                fractionalScaleManager_, surface_);
            wp_fractional_scale_v1_add_listener(fractionalScale_,
                                                &fractionalScaleListener_, this);
        }

        layerSurface_ = zwlr_layer_shell_v1_get_layer_surface(
            layerShell_, surface_, nullptr, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
            "coomer");
        if (!layerSurface_) {
            LOG_ERROR("failed to create layer surface");
            return;
        }
        zwlr_layer_surface_v1_add_listener(layerSurface_,
                                           &layerSurfaceListener_, this);
        // Let the compositor choose the full output size when anchored to all
        // edges.
        zwlr_layer_surface_v1_set_size(layerSurface_, 0, 0);
        zwlr_layer_surface_v1_set_anchor(
            layerSurface_, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                               ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                               ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                               ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        // Extend underneath panels (e.g. waybar) instead of avoiding their
        // exclusive zone.
        zwlr_layer_surface_v1_set_exclusive_zone(layerSurface_, -1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(
            layerSurface_,
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);

        wl_surface_commit(surface_);
        wl_display_roundtrip(display_);
        if (!configured_) {
            wl_display_roundtrip(display_);
        }
        if (!configured_) {
            LOG_WARN("layer-shell: no initial configure received");
        }
        updateBufferGeometry();

        if (!egl_.init(display_, surface_, width_, height_)) {
            return;
        }

        // CRITICAL: Commit an initial frame to ensure the compositor receives a
        // buffer. Without this, some compositors (e.g., niri) may not schedule
        // frame callbacks, causing the surface to appear "stuck". We skip
        // glClear to avoid black flash.
        if (surface_) {
            // Damage and request frame callback before swap
            wl_surface_damage_buffer(surface_, 0, 0, width_, height_);
            frameCallback_ = wl_surface_frame(surface_);
            wl_callback_add_listener(frameCallback_, &frameListener_, this);

            // Swap without clearing - EGL provides a valid buffer, first real
            // frame from main loop will immediately overwrite this
            if (!egl_.swap()) {
                EGLint err = eglGetError();
                LOG_WARN("layer-shell initial eglSwapBuffers failed: 0x%x",
                         err);
            }
            wl_display_flush(display_);
            LOG_DEBUG("layer-shell: initial frame committed");
        }

        valid_ = true;
    }

    ~WaylandWindowLayerShellEgl() override {
        if (frameCallback_) {
            wl_callback_destroy(frameCallback_);
        }
        if (fractionalScale_) {
            wp_fractional_scale_v1_destroy(fractionalScale_);
        }
        if (viewport_) {
            wp_viewport_destroy(viewport_);
        }
        input_.reset();
        egl_.reset();
        if (layerSurface_) {
            zwlr_layer_surface_v1_destroy(layerSurface_);
        }
        if (surface_) {
            wl_surface_destroy(surface_);
        }
        if (layerShell_) {
            zwlr_layer_shell_v1_destroy(layerShell_);
        }
        if (fractionalScaleManager_) {
            wp_fractional_scale_manager_v1_destroy(fractionalScaleManager_);
        }
        if (viewporter_) {
            wp_viewporter_destroy(viewporter_);
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
        input_.beginFrame();

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
        return input_.state();
    }
    int width() const override {
        return width_;
    }
    int height() const override {
        return height_;
    }

    void swap() override {
        if (valid_) {
            if (surface_) {
                wl_surface_damage_buffer(surface_, 0, 0, width_, height_);
                if (!frameCallback_) {
                    frameCallback_ = wl_surface_frame(surface_);
                    wl_callback_add_listener(frameCallback_, &frameListener_,
                                             this);
                }
            }
            if (!egl_.swap()) {
                EGLint err = eglGetError();
                LOG_WARN("layer-shell eglSwapBuffers failed: 0x%x", err);
            }
            if (display_) {
                wl_display_flush(display_);
            }
        }
    }

    void* glGetProcAddress(const char* name) override {
        return WaylandEglContext::getProcAddress(name);
    }

private:
    static int scaleSurfaceToBuffer(int size, uint32_t scale120) {
        long long scaled = static_cast<long long>(size) *
                           static_cast<long long>(std::max(1u, scale120));
        return std::max(1, static_cast<int>((scaled + 60) / 120));
    }

    void updateBufferGeometry() {
        surfaceWidth_ = std::max(1, surfaceWidth_);
        surfaceHeight_ = std::max(1, surfaceHeight_);

        int newWidth = surfaceWidth_;
        int newHeight = surfaceHeight_;
        if (viewport_) {
            wp_viewport_set_destination(viewport_, surfaceWidth_, surfaceHeight_);
            newWidth = scaleSurfaceToBuffer(surfaceWidth_, preferredScale120_);
            newHeight = scaleSurfaceToBuffer(surfaceHeight_, preferredScale120_);
        }

        width_ = newWidth;
        height_ = newHeight;
        input_.setGeometry(surfaceWidth_, surfaceHeight_, width_, height_);
        egl_.resize(width_, height_);
    }

    static void handleGlobal(void* data, wl_registry* registry, uint32_t name,
                             const char* interface, uint32_t version) {
        auto* self = static_cast<WaylandWindowLayerShellEgl*>(data);
        if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
            self->compositor_ = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface,
                                 std::min(version, 4u)));
        } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
            self->input_.bind(registry, name, version);
        } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) ==
                   0) {
            self->layerShell_ =
                static_cast<zwlr_layer_shell_v1*>(wl_registry_bind(
                    registry, name, &zwlr_layer_shell_v1_interface, 1));
        } else if (std::strcmp(interface, wp_viewporter_interface.name) == 0) {
            self->viewporter_ = static_cast<wp_viewporter*>(
                wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
        } else if (std::strcmp(interface,
                               wp_fractional_scale_manager_v1_interface.name) ==
                   0) {
            self->fractionalScaleManager_ =
                static_cast<wp_fractional_scale_manager_v1*>(wl_registry_bind(
                    registry, name, &wp_fractional_scale_manager_v1_interface,
                    1));
        }
    }

    static void handleGlobalRemove(void* data, wl_registry*, uint32_t name) {
        static_cast<WaylandWindowLayerShellEgl*>(data)->input_.removeSeat(name);
    }

    static void handleLayerSurfaceConfigure(void* data,
                                            zwlr_layer_surface_v1* surface,
                                            uint32_t serial, uint32_t width,
                                            uint32_t height) {
        auto* self = static_cast<WaylandWindowLayerShellEgl*>(data);
        zwlr_layer_surface_v1_ack_configure(surface, serial);
        self->configured_ = true;
        LOG_DEBUG("layer-shell configure: %ux%u", width, height);
        if (width > 0 && height > 0) {
            self->surfaceWidth_ = static_cast<int>(width);
            self->surfaceHeight_ = static_cast<int>(height);
            self->updateBufferGeometry();
        }
    }

    static void handleLayerSurfaceClosed(void* data, zwlr_layer_surface_v1*) {
        auto* self = static_cast<WaylandWindowLayerShellEgl*>(data);
        self->shouldClose_ = true;
    }

    static void handleFrameDone(void* data, wl_callback* callback, uint32_t) {
        auto* self = static_cast<WaylandWindowLayerShellEgl*>(data);
        if (callback) {
            wl_callback_destroy(callback);
        }
        self->frameCallback_ = nullptr;
    }

    static void handlePreferredScale(void* data, wp_fractional_scale_v1*,
                                     uint32_t scale) {
        auto* self = static_cast<WaylandWindowLayerShellEgl*>(data);
        if (scale == 0) {
            return;
        }
        self->preferredScale120_ = scale;
        self->updateBufferGeometry();
    }

    static inline wl_registry_listener registryListener_ = {handleGlobal,
                                                            handleGlobalRemove};
    static inline zwlr_layer_surface_v1_listener layerSurfaceListener_ = {
        handleLayerSurfaceConfigure, handleLayerSurfaceClosed};
    static inline wl_callback_listener frameListener_ = {handleFrameDone};
    static inline wp_fractional_scale_v1_listener fractionalScaleListener_ = {
        handlePreferredScale};

    wl_display* display_ = nullptr;
    wl_registry* registry_ = nullptr;
    wl_compositor* compositor_ = nullptr;
    zwlr_layer_shell_v1* layerShell_ = nullptr;
    wp_viewporter* viewporter_ = nullptr;
    wp_viewport* viewport_ = nullptr;
    wp_fractional_scale_manager_v1* fractionalScaleManager_ = nullptr;
    wp_fractional_scale_v1* fractionalScale_ = nullptr;
    wl_surface* surface_ = nullptr;
    zwlr_layer_surface_v1* layerSurface_ = nullptr;

    WaylandEglContext egl_;
    WaylandInput input_;
    bool valid_ = false;
    bool shouldClose_ = false;
    int width_ = 0;
    int height_ = 0;
    int surfaceWidth_ = 0;
    int surfaceHeight_ = 0;
    uint32_t preferredScale120_ = 120;
    bool configured_ = false;
    wl_callback* frameCallback_ = nullptr;

};

std::unique_ptr<IWindow> CreateWaylandWindowLayerShellEgl(
    const WindowConfig& config) {
    auto window = std::make_unique<WaylandWindowLayerShellEgl>(config);
    if (!window->isValid()) {
        return nullptr;
    }
    return window;
}

}  // namespace coomer
