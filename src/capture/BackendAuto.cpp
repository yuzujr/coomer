#include <cstdlib>
#include <memory>

#include "capture/BackendFactory.hpp"
#include "platform/Log.hpp"

#if defined(COOMER_HAS_X11)
#include "capture/BackendX11.hpp"
#endif
#if defined(COOMER_HAS_WAYLAND)
#include "capture/BackendWlrScreencopy.hpp"
#endif
#if defined(COOMER_HAS_PORTAL)
#include "capture/BackendPortalScreenshot.hpp"
#endif

namespace coomer {

namespace {

std::unique_ptr<ICaptureBackend> createX11() {
#if defined(COOMER_HAS_X11)
    return CreateBackendX11();
#else
    return nullptr;
#endif
}

std::unique_ptr<ICaptureBackend> createWlr() {
#if defined(COOMER_HAS_WAYLAND)
    return CreateBackendWlrScreencopy();
#else
    return nullptr;
#endif
}

std::unique_ptr<ICaptureBackend> createPortal() {
#if defined(COOMER_HAS_PORTAL)
    return CreateBackendPortalScreenshot();
#else
    return nullptr;
#endif
}

}  // namespace

class BackendAuto final : public ICaptureBackend {
public:
    std::string name() const override {
        auto backend = selectBackend();
        return backend ? backend->name() : "auto";
    }

    bool isAvailable() const override {
        auto selected = selectBackend();
        return selected != nullptr;
    }

    std::vector<MonitorInfo> listMonitors() override {
        auto backend = selectBackend();
        if (!backend) {
            return {};
        }
        return backend->listMonitors();
    }

    CaptureResult captureOnce(
        std::optional<std::string> monitorNameHint) override {
        auto backend = selectBackend();
        if (!backend) {
            return {};
        }
        return backend->captureOnce(monitorNameHint);
    }

private:
    ICaptureBackend* selectBackend() const {
        if (selected_) {
            return selected_.get();
        }
        bool hasWayland = std::getenv("WAYLAND_DISPLAY") != nullptr;
        bool hasX11 = std::getenv("DISPLAY") != nullptr;

        if (hasX11 && !hasWayland) {
            auto backend = createX11();
            if (backend && backend->isAvailable()) {
                selected_ = std::move(backend);
                selectedKind_ = BackendKind::X11;
                LOG_DEBUG("auto backend selected: x11");
                return selected_.get();
            }
        }

        if (hasWayland) {
            auto backend = createWlr();
            if (backend && backend->isAvailable()) {
                selected_ = std::move(backend);
                selectedKind_ = BackendKind::Wlr;
                LOG_DEBUG("auto backend selected: wlr-screencopy");
                return selected_.get();
            }
            LOG_INFO("compositor 未提供 wlr-screencopy，尝试 portal");

            auto portal = createPortal();
            if (portal && portal->isAvailable()) {
                selected_ = std::move(portal);
                selectedKind_ = BackendKind::Portal;
                LOG_DEBUG("auto backend selected: portal");
                return selected_.get();
            }
            LOG_WARN("Neither wlr-screencopy nor portal backend is available");
        }

        if (hasX11) {
            auto backend = createX11();
            if (backend && backend->isAvailable()) {
                selected_ = std::move(backend);
                selectedKind_ = BackendKind::X11;
                LOG_DEBUG("auto backend selected: x11 (fallback)");
                return selected_.get();
            }
        }

        LOG_ERROR("auto backend selection failed: no available backend");
        return nullptr;
    }

    mutable std::unique_ptr<ICaptureBackend> selected_;
    mutable BackendKind selectedKind_ = BackendKind::Auto;
};

std::unique_ptr<ICaptureBackend> CreateBackend(BackendKind kind) {
    switch (kind) {
        case BackendKind::Auto:
            return std::make_unique<BackendAuto>();
        case BackendKind::X11: {
            auto backend = createX11();
            if (!backend) {
                LOG_ERROR("x11 backend disabled at build time");
            }
            return backend;
        }
        case BackendKind::Wlr: {
            auto backend = createWlr();
            if (!backend) {
                LOG_ERROR("wlr backend disabled at build time");
            }
            return backend;
        }
        case BackendKind::Portal: {
            auto backend = createPortal();
            if (!backend) {
                LOG_ERROR("portal backend disabled at build time");
            }
            return backend;
        }
    }
    return nullptr;
}

}  // namespace coomer
