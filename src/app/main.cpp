#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "app/cli.hpp"
#include "capture/BackendFactory.hpp"
#include "capture/CaptureTypes.hpp"
#include "platform/Log.hpp"
#include "platform/Time.hpp"
#include "render/RendererGL.hpp"
#include "window/IWindow.hpp"

#if defined(COOMER_HAS_X11)
#include "window/X11WindowGlx.hpp"
#endif
#if defined(COOMER_HAS_WAYLAND)
#include "window/WaylandWindowLayerShellEgl.hpp"
#include "window/WaylandWindowXdgEgl.hpp"
#endif

namespace coomer {

namespace {

void printMonitorList(const std::string& backendName,
                      const std::vector<MonitorInfo>& monitors) {
    std::cout << "Backend: " << backendName << "\n";
    if (monitors.empty()) {
        std::cout << "(no monitors reported)\n";
        return;
    }
    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& m = monitors[i];
        std::cout << "[" << i << "] " << m.name << " " << m.x << "," << m.y
                  << " " << m.w << "x" << m.h << " scale=" << m.scale;
        if (m.primary) {
            std::cout << " primary";
        }
        std::cout << "\n";
    }
}

std::unique_ptr<IWindow> createWindowForSession(const WindowConfig& cfg,
                                                const std::string& backendName,
                                                bool overlay) {
    bool waylandSession = std::getenv("WAYLAND_DISPLAY") != nullptr;
    bool forceX11 = (backendName == "x11");
    if (waylandSession && !forceX11) {
#if defined(COOMER_HAS_WAYLAND)
        if (overlay) {
            LOG_DEBUG("window: requested layer-shell overlay");
            auto layer = CreateWaylandWindowLayerShellEgl(cfg);
            if (layer) {
                LOG_DEBUG("window: layer-shell surface created");
                return layer;
            }
            LOG_WARN("layer-shell unavailable, falling back to xdg-shell");
        }
        LOG_DEBUG("window: using xdg-shell fullscreen");
        return CreateWaylandWindowXdgEgl(cfg);
#else
        LOG_ERROR("Wayland window support disabled at build time");
        return nullptr;
#endif
    }

#if defined(COOMER_HAS_X11)
    if (overlay) {
        LOG_WARN("overlay ignored on X11");
    }
    LOG_DEBUG("window: using X11 fullscreen");
    return CreateX11WindowGlx(cfg);
#else
    LOG_ERROR("X11 window support disabled at build time");
    return nullptr;
#endif
}

}  // namespace

}  // namespace coomer

int main(int argc, char** argv) {
    using namespace coomer;

    initFileLogging();

    CliOptions options;
    std::string err;
    if (!parseCli(argc, argv, options, err)) {
        LOG_ERROR("%s", err.c_str());
        closeFileLogging();
        return 1;
    }

    setDebugLogging(options.debug);

    auto backend = CreateBackend(options.backend, options.portalInteractive);
    if (!backend) {
        LOG_ERROR("failed to create backend");
        closeFileLogging();
        return 1;
    }

    if (!backend->isAvailable()) {
        if (options.backend == BackendKind::Wlr) {
            LOG_ERROR("compositor does not support wlr-screencopy");
        } else if (options.backend == BackendKind::Portal) {
            LOG_ERROR("xdg-desktop-portal is missing or unavailable");
        } else if (options.backend == BackendKind::X11) {
            LOG_ERROR(
                "X11 backend unavailable (DISPLAY missing or access denied)");
        } else {
            LOG_ERROR("backend '%s' is not available", backend->name().c_str());
        }
        closeFileLogging();
        return 1;
    }

    if (options.listMonitors) {
        auto monitors = backend->listMonitors();
        printMonitorList(backend->name(), monitors);
        closeFileLogging();
        return 0;
    }

    CaptureResult capture = backend->captureOnce(options.monitor);
    if (capture.image.rgba.empty() || capture.image.w <= 0 ||
        capture.image.h <= 0) {
        LOG_ERROR("capture failed on backend '%s'", backend->name().c_str());
        closeFileLogging();
        return 1;
    }

    if (options.debug) {
        LOG_DEBUG("capture size: %dx%d", capture.image.w, capture.image.h);
        LOG_DEBUG("monitors: %zu", capture.monitors.size());
    }

    WindowConfig cfg;
    cfg.width = capture.image.w;
    cfg.height = capture.image.h;
    cfg.overlay = options.overlay;
    cfg.title = "coomer";

    if (capture.selectedMonitorIndex >= 0 &&
        capture.selectedMonitorIndex <
            static_cast<int>(capture.monitors.size())) {
        const auto& mon = capture.monitors[capture.selectedMonitorIndex];
        cfg.x = mon.x;
        cfg.y = mon.y;
        cfg.width = mon.w;
        cfg.height = mon.h;
    }

    auto window = createWindowForSession(cfg, backend->name(), options.overlay);
    if (!window) {
        LOG_ERROR("failed to create window");
        closeFileLogging();
        return 1;
    }

    RendererGL renderer;
    if (!renderer.initGL([&window](const char* name) -> void* {
            return window->glGetProcAddress(name);
        })) {
        LOG_ERROR("failed to initialize renderer");
        closeFileLogging();
        return 1;
    }
    if (!renderer.uploadScreenshotTexture(capture.image)) {
        LOG_ERROR("failed to upload screenshot texture");
        closeFileLogging();
        return 1;
    }

    CameraState camera;
    camera.zoom = 1.0f;
    camera.panX = 0.0f;
    camera.panY = 0.0f;
    if (options.monitor && *options.monitor == "all" &&
        capture.selectedMonitorIndex >= 0 &&
        capture.selectedMonitorIndex <
            static_cast<int>(capture.monitors.size())) {
        bool hasBounds = false;
        int minX = 0;
        int minY = 0;
        int maxX = 0;
        int maxY = 0;
        for (const auto& mon : capture.monitors) {
            if (mon.w <= 0 || mon.h <= 0) {
                continue;
            }
            int left = mon.x;
            int top = mon.y;
            int right = left + mon.w;
            int bottom = top + mon.h;
            if (!hasBounds) {
                minX = left;
                minY = top;
                maxX = right;
                maxY = bottom;
                hasBounds = true;
            } else {
                minX = std::min(minX, left);
                minY = std::min(minY, top);
                maxX = std::max(maxX, right);
                maxY = std::max(maxY, bottom);
            }
        }
        int totalW = hasBounds ? (maxX - minX) : 0;
        int totalH = hasBounds ? (maxY - minY) : 0;
        if (hasBounds && totalW == capture.image.w &&
            totalH == capture.image.h) {
            const auto& mon = capture.monitors[capture.selectedMonitorIndex];
            camera.panX = static_cast<float>(minX - mon.x);
            camera.panY = static_cast<float>(minY - mon.y);
        }
    }

    float panVelX = 0.0f;
    float panVelY = 0.0f;
    float zoomVel = 0.0f;
    float spotlightRadiusMulTarget = 1.0f;
    float spotlightRadiusMulCurrent = 1.0f;
    bool prevLeft = false;
    bool prevSpotlight = false;
    bool spotlightAnimating = false;
    double spotlightAnimStart = 0.0;
    float spotlightAnimFrom = 0.0f;
    float spotlightAnimTo = 0.0f;

    double lastTime = nowSeconds();

    while (!window->shouldClose()) {
        window->pollEvents();
        InputState input = window->input();

        if (input.keyQ || input.keyA || input.mouseRight) {
            break;
        }

        double now = nowSeconds();
        float dt = static_cast<float>(now - lastTime);
        if (dt > 0.05f) {
            dt = 0.05f;
        }
        lastTime = now;

        float cursorX = static_cast<float>(input.mouseX);
        float cursorY = static_cast<float>(window->height() - input.mouseY);
        float deltaX = static_cast<float>(input.deltaX);
        float deltaY = static_cast<float>(-input.deltaY);

        if (input.mouseLeft) {
            camera.panX += deltaX;
            camera.panY += deltaY;
            if (dt > 0.0f) {
                panVelX = deltaX / dt;
                panVelY = deltaY / dt;
            }
        } else if (!prevLeft) {
            camera.panX += panVelX * dt;
            camera.panY += panVelY * dt;
            float decay = std::exp(-6.0f * dt);
            panVelX *= decay;
            panVelY *= decay;
            // Stop momentum when velocity becomes imperceptible to prevent
            // subpixel jitter
            const float minVel = 1.0f;  // px/s
            if (std::abs(panVelX) < minVel && std::abs(panVelY) < minVel) {
                panVelX = 0.0f;
                panVelY = 0.0f;
            }
        }
        prevLeft = input.mouseLeft;

        if (input.wheelDelta != 0.0) {
            float wheel = static_cast<float>(-input.wheelDelta);
            if (input.keyCtrl) {
                spotlightRadiusMulTarget += wheel * 0.35f;
                spotlightRadiusMulTarget =
                    std::clamp(spotlightRadiusMulTarget, 0.3f, 10.0f);
            } else {
                zoomVel += wheel * 2.0f;
            }
        }

        if (std::abs(zoomVel) > 0.0001f) {
            float oldZoom = camera.zoom;
            float factor = std::exp(zoomVel * dt);
            camera.zoom = std::clamp(camera.zoom * factor, 1.0f, 10.0f);
            if (camera.zoom != oldZoom) {
                float ratio = camera.zoom / oldZoom;
                camera.panX = cursorX - (cursorX - camera.panX) * ratio;
                camera.panY = cursorY - (cursorY - camera.panY) * ratio;
            } else {
                zoomVel = 0.0f;
            }
            zoomVel *= std::exp(-6.0f * dt);
            // Stop zoom momentum when velocity becomes imperceptible
            if (std::abs(zoomVel) < 0.01f) {
                zoomVel = 0.0f;
            }
        }

        camera.screenW = window->width();
        camera.screenH = window->height();

        float follow = 1.0f - std::exp(-14.0f * dt);
        spotlightRadiusMulCurrent +=
            (spotlightRadiusMulTarget - spotlightRadiusMulCurrent) * follow;
        spotlightRadiusMulCurrent =
            std::clamp(spotlightRadiusMulCurrent, 0.3f, 10.0f);

        SpotlightState spotlight;
        spotlight.enabled = (!options.noSpotlight) && input.keyCtrl;
        spotlight.cursorX = cursorX;
        spotlight.cursorY = cursorY;
        float baseRadius = std::min(camera.screenW, camera.screenH) * 0.2f;
        float targetRadius = baseRadius * spotlightRadiusMulCurrent;
        if (spotlight.enabled && !prevSpotlight) {
            spotlightAnimating = true;
            spotlightAnimStart = now;
            spotlightAnimFrom =
                std::max(targetRadius * 1.5f,
                         std::min(camera.screenW, camera.screenH) * 0.6f);
            spotlightAnimTo = targetRadius;
        }
        if (!spotlight.enabled) {
            spotlightAnimating = false;
        }
        if (spotlightAnimating) {
            spotlightAnimTo = targetRadius;
            const float duration = 0.18f;
            float t = static_cast<float>((now - spotlightAnimStart) / duration);
            if (t >= 1.0f) {
                t = 1.0f;
                spotlightAnimating = false;
            } else if (t < 0.0f) {
                t = 0.0f;
            }
            float ease = 1.0f - std::pow(1.0f - t, 3.0f);
            spotlight.radiusPx = spotlightAnimFrom +
                                 (spotlightAnimTo - spotlightAnimFrom) * ease;
        } else {
            spotlight.radiusPx = targetRadius;
        }
        spotlight.tintR = 0.0f;
        spotlight.tintG = 0.0f;
        spotlight.tintB = 0.0f;
        spotlight.tintA = 190.0f / 255.0f;
        prevSpotlight = spotlight.enabled;

        renderer.renderFrame(camera, spotlight);
        window->swap();
    }

    closeFileLogging();
    return 0;
}
