#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

#include "app/cli.hpp"
#include "app/ViewController.hpp"
#include "platform/Clipboard.hpp"
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

    CliOptions options;
    std::string err;
    if (!parseCli(argc, argv, options, err)) {
        LOG_ERROR("%s", err.c_str());
        std::cerr << "Run '" << argv[0] << " --help' for usage.\n";
        return 1;
    }
    if (options.action == CliAction::Help) {
        printUsage(std::cout, argv[0]);
        return 0;
    }
    if (options.action == CliAction::Version) {
        printVersion(std::cout);
        return 0;
    }

    initFileLogging();
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

    if (backend->name() == "portal-screenshot" &&
        (options.monitor || options.listMonitors)) {
        LOG_ERROR("portal does not support --monitor or --list-monitors; "
                  "use --portal-interactive to choose the capture in the system dialog");
        closeFileLogging();
        return 1;
    }
    if (options.portalInteractive && backend->name() != "portal-screenshot") {
        LOG_WARN("--portal-interactive has no effect with backend '%s'; "
                 "use --backend portal to request the system dialog",
                 backend->name().c_str());
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
    if (!renderer.uploadScreenshotTexture(std::move(capture.image))) {
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

    float spotlightRadiusMulCurrent = 1.0f;
    bool prevSpotlight = false;
    bool prevP = false;
    bool prevC = false;
    bool pixelMode = false;
    ViewController view(camera);
    double copyNoticeUntil = 0.0;
    std::string copyNotice;
    bool spotlightAnimating = false;
    double spotlightAnimStart = 0.0;
    float spotlightAnimFrom = 0.0f;
    float spotlightAnimTo = 0.0f;

    double lastTime = nowSeconds();
    float inspectorOpacity = 0.0f;

    while (!window->shouldClose()) {
        window->pollEvents();
        InputState input = window->input();

        if (input.keyQ || input.keyEscape) {
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

        const bool copyPressed = input.keyC && !prevC;
        if (input.keyP && !prevP) pixelMode = !pixelMode;
        prevP = input.keyP;
        prevC = input.keyC;

        camera.screenW = window->width();
        camera.screenH = window->height();
        view.update(camera, input, capture.image.w, capture.image.h, dt);

        const float inspectorImageX =
            (cursorX - camera.panX) / camera.zoom;
        const float inspectorImageY =
            (cursorY - camera.panY) / camera.zoom;
        const bool inspectorAvailable =
            pixelMode && inspectorImageX >= 0.0f &&
            inspectorImageY >= 0.0f &&
            inspectorImageX < static_cast<float>(capture.image.w) &&
            inspectorImageY < static_cast<float>(capture.image.h);
        if (copyPressed && inspectorAvailable) {
            const std::string color = renderer.colorAt(camera, cursorX, cursorY);
            const bool copied = !color.empty() && copyToClipboard(color);
            copyNotice = copied ? "COPIED " + color : "COPY FAILED";
            copyNoticeUntil = now + 1.2;
            if (!copied) {
                LOG_WARN("clipboard copy failed; install wl-clipboard (Wayland) or xclip (X11)");
            }
        }
        const bool showInspector = inspectorAvailable;
        const float inspectorBlend =
            1.0f - std::exp(-(showInspector ? 14.0f : 5.0f) * dt);
        const float inspectorTarget = showInspector ? 1.0f : 0.0f;
        inspectorOpacity +=
            (inspectorTarget - inspectorOpacity) * inspectorBlend;

        float follow = 1.0f - std::exp(-14.0f * dt);
        spotlightRadiusMulCurrent +=
            (view.spotlightRadiusMultiplier() - spotlightRadiusMulCurrent) * follow;
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

        renderer.renderFrame(camera, spotlight, inspectorOpacity, pixelMode,
                             now < copyNoticeUntil ? copyNotice : "");
        window->swap();
    }

    closeFileLogging();
    return 0;
}
