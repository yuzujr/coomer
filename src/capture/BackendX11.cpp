#include "capture/BackendX11.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>

#include "platform/Log.hpp"

namespace coomer {

class X11CaptureBackend final : public ICaptureBackend {
public:
    std::string name() const override {
        return "x11";
    }

    bool isAvailable() const override {
        const char* displayEnv = std::getenv("DISPLAY");
        if (!displayEnv) {
            return false;
        }
        Display* display = XOpenDisplay(nullptr);
        if (!display) {
            return false;
        }
        XCloseDisplay(display);
        return true;
    }

    std::vector<MonitorInfo> listMonitors() override {
        std::vector<MonitorInfo> result;
        Display* display = XOpenDisplay(nullptr);
        if (!display) {
            LOG_ERROR("X11: failed to open display for monitor list");
            return result;
        }

        Window root = DefaultRootWindow(display);
        XRRScreenResources* resources =
            XRRGetScreenResourcesCurrent(display, root);
        if (!resources) {
            XCloseDisplay(display);
            LOG_ERROR("X11: failed to get screen resources");
            return result;
        }

        RROutput primary = XRRGetOutputPrimary(display, root);

        for (int i = 0; i < resources->noutput; ++i) {
            RROutput output = resources->outputs[i];
            XRROutputInfo* info = XRRGetOutputInfo(display, resources, output);
            if (!info) {
                continue;
            }
            if (info->connection == RR_Connected && info->crtc) {
                XRRCrtcInfo* crtc =
                    XRRGetCrtcInfo(display, resources, info->crtc);
                if (crtc) {
                    MonitorInfo mon;
                    mon.name.assign(info->name, info->nameLen);
                    mon.x = crtc->x;
                    mon.y = crtc->y;
                    mon.w = crtc->width;
                    mon.h = crtc->height;
                    mon.scale = 1.0f;
                    mon.primary = (output == primary);
                    result.push_back(mon);
                    XRRFreeCrtcInfo(crtc);
                }
            }
            XRRFreeOutputInfo(info);
        }

        XRRFreeScreenResources(resources);
        XCloseDisplay(display);
        return result;
    }

    CaptureResult captureOnce(
        std::optional<std::string> monitorNameHint) override {
        CaptureResult result;
        Display* display = XOpenDisplay(nullptr);
        if (!display) {
            LOG_ERROR("X11: failed to open display for capture");
            return result;
        }

        Window root = DefaultRootWindow(display);
        XRRScreenResources* resources =
            XRRGetScreenResourcesCurrent(display, root);
        if (!resources) {
            LOG_ERROR("X11: failed to get screen resources");
            XCloseDisplay(display);
            return result;
        }

        auto monitors = listMonitorsFromResources(display, root, resources);
        result.monitors = monitors;

        int chosen = -1;
        if (monitorNameHint) {
            for (size_t i = 0; i < monitors.size(); ++i) {
                if (monitors[i].name == *monitorNameHint) {
                    chosen = static_cast<int>(i);
                    break;
                }
            }
        }
        if (chosen < 0 && !monitors.empty()) {
            chosen = 0;
        }
        result.selectedMonitorIndex = chosen;

        int x = 0;
        int y = 0;
        int w = DisplayWidth(display, DefaultScreen(display));
        int h = DisplayHeight(display, DefaultScreen(display));
        if (chosen >= 0 && chosen < static_cast<int>(monitors.size())) {
            x = monitors[chosen].x;
            y = monitors[chosen].y;
            w = monitors[chosen].w;
            h = monitors[chosen].h;
        }

        XImage* image =
            XGetImage(display, root, x, y, static_cast<unsigned int>(w),
                      static_cast<unsigned int>(h), AllPlanes, ZPixmap);
        if (!image) {
            LOG_ERROR("X11: XGetImage failed (permissions or remote session?)");
            XRRFreeScreenResources(resources);
            XCloseDisplay(display);
            return result;
        }

        result.image.w = w;
        result.image.h = h;
        result.image.rgba.resize(static_cast<size_t>(w) *
                                 static_cast<size_t>(h) * 4u);

        const unsigned long rmask = image->red_mask;
        const unsigned long gmask = image->green_mask;
        const unsigned long bmask = image->blue_mask;
        const int rshift = rmask ? __builtin_ctzl(rmask) : 0;
        const int gshift = gmask ? __builtin_ctzl(gmask) : 0;
        const int bshift = bmask ? __builtin_ctzl(bmask) : 0;
        const unsigned long rmax = rmask >> rshift;
        const unsigned long gmax = gmask >> gshift;
        const unsigned long bmax = bmask >> bshift;

        for (int iy = 0; iy < h; ++iy) {
            for (int ix = 0; ix < w; ++ix) {
                unsigned long pixel = XGetPixel(image, ix, iy);
                unsigned long r = (pixel & rmask) >> rshift;
                unsigned long g = (pixel & gmask) >> gshift;
                unsigned long b = (pixel & bmask) >> bshift;
                std::uint8_t rr =
                    static_cast<std::uint8_t>(rmax ? (r * 255ul / rmax) : 0);
                std::uint8_t gg =
                    static_cast<std::uint8_t>(gmax ? (g * 255ul / gmax) : 0);
                std::uint8_t bb =
                    static_cast<std::uint8_t>(bmax ? (b * 255ul / bmax) : 0);
                size_t idx = (static_cast<size_t>(iy) * static_cast<size_t>(w) +
                              static_cast<size_t>(ix)) *
                             4u;
                result.image.rgba[idx + 0] = rr;
                result.image.rgba[idx + 1] = gg;
                result.image.rgba[idx + 2] = bb;
                result.image.rgba[idx + 3] = 255;
            }
        }

        XDestroyImage(image);
        XRRFreeScreenResources(resources);
        XCloseDisplay(display);
        return result;
    }

private:
    static std::vector<MonitorInfo> listMonitorsFromResources(
        Display* display, Window root, XRRScreenResources* resources) {
        std::vector<MonitorInfo> result;
        if (!display || !resources) {
            return result;
        }
        RROutput primary = XRRGetOutputPrimary(display, root);
        for (int i = 0; i < resources->noutput; ++i) {
            RROutput output = resources->outputs[i];
            XRROutputInfo* info = XRRGetOutputInfo(display, resources, output);
            if (!info) {
                continue;
            }
            if (info->connection == RR_Connected && info->crtc) {
                XRRCrtcInfo* crtc =
                    XRRGetCrtcInfo(display, resources, info->crtc);
                if (crtc) {
                    MonitorInfo mon;
                    mon.name.assign(info->name, info->nameLen);
                    mon.x = crtc->x;
                    mon.y = crtc->y;
                    mon.w = crtc->width;
                    mon.h = crtc->height;
                    mon.scale = 1.0f;
                    mon.primary = (output == primary);
                    result.push_back(mon);
                    XRRFreeCrtcInfo(crtc);
                }
            }
            XRRFreeOutputInfo(info);
        }
        return result;
    }
};

std::unique_ptr<ICaptureBackend> CreateBackendX11() {
    return std::make_unique<X11CaptureBackend>();
}

}  // namespace coomer
