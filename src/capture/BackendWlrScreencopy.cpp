#include "capture/BackendWlrScreencopy.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "platform/Log.hpp"
#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace coomer {

namespace {

struct OutputInfo {
    wl_output* output = nullptr;
    zxdg_output_v1* xdg = nullptr;
    MonitorInfo info;
    bool gotMode = false;
};

struct WlrContext {
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    wl_shm* shm = nullptr;
    zwlr_screencopy_manager_v1* manager = nullptr;
    zxdg_output_manager_v1* xdgOutputManager = nullptr;
    std::vector<std::unique_ptr<OutputInfo>> outputs;
};

int createShmFile(size_t size) {
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::string name = "/coomer-shm-" + std::to_string(getpid()) + "-" +
                           std::to_string(rand());
        int fd = shm_open(name.c_str(), O_CREAT | O_RDWR | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name.c_str());
            if (ftruncate(fd, static_cast<off_t>(size)) == 0) {
                return fd;
            }
            close(fd);
        }
    }
    return -1;
}

struct ShmBuffer {
    wl_buffer* buffer = nullptr;
    void* data = nullptr;
    size_t size = 0;
    int width = 0;
    int height = 0;
    int stride = 0;
    uint32_t format = 0;
};

bool createShmBuffer(wl_shm* shm, int width, int height, int stride,
                     uint32_t format, ShmBuffer& out) {
    size_t size = static_cast<size_t>(stride) * static_cast<size_t>(height);
    int fd = createShmFile(size);
    if (fd < 0) {
        LOG_ERROR("wlr: failed to create shm file");
        return false;
    }

    void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        LOG_ERROR("wlr: failed to mmap shm");
        close(fd);
        return false;
    }

    wl_shm_pool* pool = wl_shm_create_pool(shm, fd, static_cast<int>(size));
    wl_buffer* buffer =
        wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
    wl_shm_pool_destroy(pool);
    close(fd);

    if (!buffer) {
        munmap(data, size);
        LOG_ERROR("wlr: failed to create wl_buffer");
        return false;
    }

    out.buffer = buffer;
    out.data = data;
    out.size = size;
    out.width = width;
    out.height = height;
    out.stride = stride;
    out.format = format;
    return true;
}

struct FrameCapture {
    wl_shm* shm = nullptr;
    zwlr_screencopy_frame_v1* frame = nullptr;
    ShmBuffer buffer{};
    bool bufferInfoReceived = false;
    bool bufferDone = false;
    bool ready = false;
    bool failed = false;
    bool yInvert = false;
    uint32_t format = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
};

ImageRGBA scaleImageBilinear(const ImageRGBA& src, int dstW, int dstH) {
    ImageRGBA dst;
    dst.w = dstW;
    dst.h = dstH;
    if (dstW <= 0 || dstH <= 0 || src.w <= 0 || src.h <= 0 ||
        src.rgba.empty()) {
        return dst;
    }
    dst.rgba.resize(static_cast<size_t>(dstW) * static_cast<size_t>(dstH) *
                    4u);

    float scaleX = 0.0f;
    float scaleY = 0.0f;
    if (dstW > 1 && src.w > 1) {
        scaleX = static_cast<float>(src.w - 1) / static_cast<float>(dstW - 1);
    }
    if (dstH > 1 && src.h > 1) {
        scaleY = static_cast<float>(src.h - 1) / static_cast<float>(dstH - 1);
    }

    for (int y = 0; y < dstH; ++y) {
        float srcYf = scaleY * static_cast<float>(y);
        int y0 = static_cast<int>(srcYf);
        int y1 = std::min(y0 + 1, src.h - 1);
        float fy = srcYf - static_cast<float>(y0);
        for (int x = 0; x < dstW; ++x) {
            float srcXf = scaleX * static_cast<float>(x);
            int x0 = static_cast<int>(srcXf);
            int x1 = std::min(x0 + 1, src.w - 1);
            float fx = srcXf - static_cast<float>(x0);

            size_t dstIdx =
                (static_cast<size_t>(y) * static_cast<size_t>(dstW) +
                 static_cast<size_t>(x)) *
                4u;
            size_t idx00 =
                (static_cast<size_t>(y0) * static_cast<size_t>(src.w) +
                 static_cast<size_t>(x0)) *
                4u;
            size_t idx10 =
                (static_cast<size_t>(y0) * static_cast<size_t>(src.w) +
                 static_cast<size_t>(x1)) *
                4u;
            size_t idx01 =
                (static_cast<size_t>(y1) * static_cast<size_t>(src.w) +
                 static_cast<size_t>(x0)) *
                4u;
            size_t idx11 =
                (static_cast<size_t>(y1) * static_cast<size_t>(src.w) +
                 static_cast<size_t>(x1)) *
                4u;

            for (int c = 0; c < 4; ++c) {
                float v00 = static_cast<float>(src.rgba[idx00 + c]);
                float v10 = static_cast<float>(src.rgba[idx10 + c]);
                float v01 = static_cast<float>(src.rgba[idx01 + c]);
                float v11 = static_cast<float>(src.rgba[idx11 + c]);
                float v0 = v00 + (v10 - v00) * fx;
                float v1 = v01 + (v11 - v01) * fx;
                float v = v0 + (v1 - v0) * fy;
                int vi = static_cast<int>(v + 0.5f);
                dst.rgba[dstIdx + c] =
                    static_cast<uint8_t>(std::clamp(vi, 0, 255));
            }
        }
    }
    return dst;
}

void frameBuffer(void* data, zwlr_screencopy_frame_v1*, uint32_t format,
                 uint32_t width, uint32_t height, uint32_t stride) {
    auto* capture = static_cast<FrameCapture*>(data);
    // The compositor tells us the shm format/size; we allocate a matching
    // wl_buffer.
    capture->format = format;
    capture->width = width;
    capture->height = height;
    capture->stride = stride;
    capture->bufferInfoReceived = true;
    if ((capture->bufferDone ||
         zwlr_screencopy_frame_v1_get_version(capture->frame) < 3) &&
        !capture->buffer.buffer) {
        if (createShmBuffer(capture->shm, static_cast<int>(width),
                            static_cast<int>(height), static_cast<int>(stride),
                            format, capture->buffer)) {
            zwlr_screencopy_frame_v1_copy(capture->frame,
                                          capture->buffer.buffer);
        }
    }
}

void frameFlags(void* data, zwlr_screencopy_frame_v1*, uint32_t flags) {
    auto* capture = static_cast<FrameCapture*>(data);
    capture->yInvert = (flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT) != 0;
}

void frameReady(void* data, zwlr_screencopy_frame_v1*, uint32_t, uint32_t,
                uint32_t) {
    auto* capture = static_cast<FrameCapture*>(data);
    capture->ready = true;
}

void frameFailed(void* data, zwlr_screencopy_frame_v1*) {
    auto* capture = static_cast<FrameCapture*>(data);
    capture->failed = true;
}

void frameDamage(void*, zwlr_screencopy_frame_v1*, uint32_t, uint32_t, uint32_t,
                 uint32_t) {}

void frameLinuxDmabuf(void*, zwlr_screencopy_frame_v1*, uint32_t, uint32_t,
                      uint32_t) {}

void frameBufferDone(void* data, zwlr_screencopy_frame_v1*) {
    auto* capture = static_cast<FrameCapture*>(data);
    capture->bufferDone = true;
    if (capture->bufferInfoReceived && !capture->buffer.buffer) {
        if (createShmBuffer(capture->shm, static_cast<int>(capture->width),
                            static_cast<int>(capture->height),
                            static_cast<int>(capture->stride), capture->format,
                            capture->buffer)) {
            zwlr_screencopy_frame_v1_copy(capture->frame,
                                          capture->buffer.buffer);
        }
    }
}

const zwlr_screencopy_frame_v1_listener kFrameListener = {
    frameBuffer, frameFlags,       frameReady,     frameFailed,
    frameDamage, frameLinuxDmabuf, frameBufferDone};

void outputGeometry(void* data, wl_output*, int32_t x, int32_t y, int32_t,
                    int32_t, int32_t, const char*, const char*, int32_t) {
    auto* output = static_cast<OutputInfo*>(data);
    output->info.x = x;
    output->info.y = y;
}

void outputMode(void* data, wl_output*, uint32_t flags, int32_t width,
                int32_t height, int32_t) {
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        auto* output = static_cast<OutputInfo*>(data);
        output->info.w = width;
        output->info.h = height;
        output->gotMode = true;
    }
}

void outputDone(void* data, wl_output*) {
    auto* output = static_cast<OutputInfo*>(data);
    if (output->info.name.empty()) {
        output->info.name = "wl_output";
    }
}

void outputScale(void* data, wl_output*, int32_t factor) {
    auto* output = static_cast<OutputInfo*>(data);
    output->info.scale = static_cast<float>(factor);
}

void outputName(void* data, wl_output*, const char* name) {
    auto* output = static_cast<OutputInfo*>(data);
    if (name) {
        output->info.name = name;
    }
}

void outputDescription(void*, wl_output*, const char*) {}

const wl_output_listener kOutputListener = {outputGeometry, outputMode,
                                            outputDone,     outputScale,
                                            outputName,     outputDescription};

void xdgOutputLogicalPosition(void* data, zxdg_output_v1*, int32_t x,
                              int32_t y) {
    auto* output = static_cast<OutputInfo*>(data);
    output->info.x = x;
    output->info.y = y;
}

void xdgOutputLogicalSize(void* data, zxdg_output_v1*, int32_t width,
                          int32_t height) {
    auto* output = static_cast<OutputInfo*>(data);
    output->info.w = width;
    output->info.h = height;
}

void xdgOutputDone(void*, zxdg_output_v1*) {}

void xdgOutputName(void* data, zxdg_output_v1*, const char* name) {
    auto* output = static_cast<OutputInfo*>(data);
    if (name) {
        output->info.name = name;
    }
}

void xdgOutputDescription(void*, zxdg_output_v1*, const char*) {}

const zxdg_output_v1_listener kXdgOutputListener = {
    xdgOutputLogicalPosition, xdgOutputLogicalSize, xdgOutputDone,
    xdgOutputName, xdgOutputDescription};

void registryGlobal(void* data, wl_registry* registry, uint32_t name,
                    const char* interface, uint32_t version) {
    auto* ctx = static_cast<WlrContext*>(data);
    if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        ctx->shm = static_cast<wl_shm*>(
            wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
        auto output = std::make_unique<OutputInfo>();
        output->output = static_cast<wl_output*>(wl_registry_bind(
            registry, name, &wl_output_interface, std::min(version, 4u)));
        output->info.scale = 1.0f;
        wl_output_add_listener(output->output, &kOutputListener, output.get());
        ctx->outputs.push_back(std::move(output));
    } else if (std::strcmp(interface,
                           zwlr_screencopy_manager_v1_interface.name) == 0) {
        ctx->manager =
            static_cast<zwlr_screencopy_manager_v1*>(wl_registry_bind(
                registry, name, &zwlr_screencopy_manager_v1_interface,
                std::min(version, 3u)));
    } else if (std::strcmp(interface, zxdg_output_manager_v1_interface.name) ==
               0) {
        ctx->xdgOutputManager = static_cast<zxdg_output_manager_v1*>(
            wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface,
                             std::min(version, 3u)));
    }
}

void registryGlobalRemove(void*, wl_registry*, uint32_t) {}

const wl_registry_listener kRegistryListener = {registryGlobal,
                                                registryGlobalRemove};

bool initContext(WlrContext& ctx) {
    ctx.display = wl_display_connect(nullptr);
    if (!ctx.display) {
        LOG_ERROR("wlr: failed to connect to Wayland display");
        return false;
    }
    ctx.registry = wl_display_get_registry(ctx.display);
    wl_registry_add_listener(ctx.registry, &kRegistryListener, &ctx);
    wl_display_roundtrip(ctx.display);

    if (ctx.xdgOutputManager) {
        // xdg-output provides stable names and logical coordinates for
        // wl_output.
        for (auto& outputPtr : ctx.outputs) {
            auto& output = *outputPtr;
            output.xdg = zxdg_output_manager_v1_get_xdg_output(
                ctx.xdgOutputManager, output.output);
            zxdg_output_v1_add_listener(output.xdg, &kXdgOutputListener,
                                        &output);
        }
        wl_display_roundtrip(ctx.display);
    }

    if (!ctx.outputs.empty()) {
        ctx.outputs[0]->info.primary = true;
    }
    return true;
}

void cleanupContext(WlrContext& ctx) {
    for (auto& outputPtr : ctx.outputs) {
        auto& output = *outputPtr;
        if (output.xdg) {
            zxdg_output_v1_destroy(output.xdg);
        }
        if (output.output) {
            wl_output_destroy(output.output);
        }
    }
    if (ctx.xdgOutputManager) {
        zxdg_output_manager_v1_destroy(ctx.xdgOutputManager);
    }
    if (ctx.manager) {
        zwlr_screencopy_manager_v1_destroy(ctx.manager);
    }
    if (ctx.shm) {
        wl_shm_destroy(ctx.shm);
    }
    if (ctx.registry) {
        wl_registry_destroy(ctx.registry);
    }
    if (ctx.display) {
        wl_display_disconnect(ctx.display);
    }
}

bool captureOutputImage(WlrContext& ctx, wl_output* output, ImageRGBA& out) {
    FrameCapture capture;
    capture.shm = ctx.shm;
    capture.frame =
        zwlr_screencopy_manager_v1_capture_output(ctx.manager, 0, output);
    zwlr_screencopy_frame_v1_add_listener(capture.frame, &kFrameListener,
                                          &capture);

    while (!capture.ready && !capture.failed) {
        wl_display_dispatch(ctx.display);
    }

    bool ok = false;
    if (capture.failed || !capture.buffer.data) {
        LOG_ERROR("wlr: capture failed");
    } else if (capture.format != WL_SHM_FORMAT_ARGB8888 &&
               capture.format != WL_SHM_FORMAT_XRGB8888) {
        LOG_ERROR("wlr: unsupported shm format %u", capture.format);
    } else {
        int width = static_cast<int>(capture.width);
        int height = static_cast<int>(capture.height);
        out.w = width;
        out.h = height;
        out.rgba.resize(static_cast<size_t>(width) *
                        static_cast<size_t>(height) * 4u);

        const uint32_t* src = static_cast<uint32_t*>(capture.buffer.data);
        for (int y = 0; y < height; ++y) {
            int srcY = capture.yInvert ? (height - 1 - y) : y;
            const uint32_t* row = reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const uint8_t*>(src) +
                static_cast<size_t>(capture.stride) * srcY);
            for (int x = 0; x < width; ++x) {
                uint32_t pixel = row[x];
                uint8_t a = (capture.format == WL_SHM_FORMAT_XRGB8888)
                                ? 255
                                : ((pixel >> 24) & 0xFF);
                uint8_t r = (pixel >> 16) & 0xFF;
                uint8_t g = (pixel >> 8) & 0xFF;
                uint8_t b = (pixel >> 0) & 0xFF;
                size_t idx = (static_cast<size_t>(y) *
                                  static_cast<size_t>(width) +
                              static_cast<size_t>(x)) *
                             4u;
                out.rgba[idx + 0] = r;
                out.rgba[idx + 1] = g;
                out.rgba[idx + 2] = b;
                out.rgba[idx + 3] = a;
            }
        }
        ok = true;
    }

    if (capture.buffer.buffer) {
        wl_buffer_destroy(capture.buffer.buffer);
    }
    if (capture.buffer.data) {
        munmap(capture.buffer.data, capture.buffer.size);
    }
    if (capture.frame) {
        zwlr_screencopy_frame_v1_destroy(capture.frame);
    }
    return ok;
}

}  // namespace

class WlrScreencopyBackend final : public ICaptureBackend {
public:
    std::string name() const override {
        return "wlr-screencopy";
    }

    bool isAvailable() const override {
        if (!std::getenv("WAYLAND_DISPLAY")) {
            return false;
        }
        WlrContext ctx;
        if (!initContext(ctx)) {
            return false;
        }
        bool ok = ctx.manager != nullptr;
        cleanupContext(ctx);
        return ok;
    }

    std::vector<MonitorInfo> listMonitors() override {
        std::vector<MonitorInfo> result;
        WlrContext ctx;
        if (!initContext(ctx)) {
            return result;
        }
        for (auto& output : ctx.outputs) {
            result.push_back(output->info);
        }
        cleanupContext(ctx);
        return result;
    }

    CaptureResult captureOnce(
        std::optional<std::string> monitorNameHint) override {
        CaptureResult result;
        WlrContext ctx;
        if (!initContext(ctx)) {
            return result;
        }
        if (!ctx.manager || !ctx.shm) {
            LOG_ERROR("wlr: missing screencopy manager or shm");
            cleanupContext(ctx);
            return result;
        }
        result.monitors.reserve(ctx.outputs.size());
        for (auto& output : ctx.outputs) {
            result.monitors.push_back(output->info);
        }

        int selected = -1;
        bool captureAll = monitorNameHint && (*monitorNameHint == "all");
        if (monitorNameHint && !captureAll) {
            for (size_t i = 0; i < ctx.outputs.size(); ++i) {
                if (ctx.outputs[i]->info.name == *monitorNameHint) {
                    selected = static_cast<int>(i);
                    break;
                }
            }
        }
        if (selected < 0 && !ctx.outputs.empty()) {
            selected = 0;
        }
        result.selectedMonitorIndex = selected;

        if (captureAll) {
            if (ctx.outputs.empty()) {
                LOG_ERROR("wlr: no outputs available for capture");
                cleanupContext(ctx);
                return result;
            }

            std::vector<ImageRGBA> images;
            images.resize(ctx.outputs.size());
            for (size_t i = 0; i < ctx.outputs.size(); ++i) {
                if (!captureOutputImage(ctx, ctx.outputs[i]->output,
                                        images[i])) {
                    LOG_ERROR("wlr: capture failed for output %s",
                              ctx.outputs[i]->info.name.c_str());
                }
            }

            bool hasBounds = false;
            int minX = 0;
            int minY = 0;
            int maxX = 0;
            int maxY = 0;
            std::vector<int> targetW(ctx.outputs.size(), 0);
            std::vector<int> targetH(ctx.outputs.size(), 0);
            for (size_t i = 0; i < result.monitors.size(); ++i) {
                int w = result.monitors[i].w;
                int h = result.monitors[i].h;
                if ((w <= 0 || h <= 0) && i < images.size()) {
                    w = images[i].w;
                    h = images[i].h;
                }
                if (result.monitors[i].w <= 0) {
                    result.monitors[i].w = w;
                }
                if (result.monitors[i].h <= 0) {
                    result.monitors[i].h = h;
                }
                targetW[i] = w;
                targetH[i] = h;
                if (w <= 0 || h <= 0) {
                    continue;
                }
                int left = result.monitors[i].x;
                int top = result.monitors[i].y;
                int right = left + w;
                int bottom = top + h;
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

            if (hasBounds && maxX > minX && maxY > minY) {
                int totalW = maxX - minX;
                int totalH = maxY - minY;
                result.image.w = totalW;
                result.image.h = totalH;
                result.image.rgba.assign(
                    static_cast<size_t>(totalW) * static_cast<size_t>(totalH) *
                        4u,
                    0);
                for (size_t i = 3; i < result.image.rgba.size(); i += 4u) {
                    result.image.rgba[i] = 255;
                }

                for (size_t i = 0; i < images.size(); ++i) {
                    if (images[i].rgba.empty()) {
                        continue;
                    }
                    int w = targetW[i];
                    int h = targetH[i];
                    if (w <= 0 || h <= 0) {
                        continue;
                    }
                    ImageRGBA scaled;
                    const ImageRGBA* src = &images[i];
                    if (images[i].w != w || images[i].h != h) {
                        scaled = scaleImageBilinear(images[i], w, h);
                        src = &scaled;
                    }

                    int offsetX = result.monitors[i].x - minX;
                    int offsetY = result.monitors[i].y - minY;
                    if (offsetX < 0 || offsetY < 0) {
                        continue;
                    }
                    int copyW = std::min(w, totalW - offsetX);
                    int copyH = std::min(h, totalH - offsetY);
                    if (copyW <= 0 || copyH <= 0) {
                        continue;
                    }

                    for (int y = 0; y < copyH; ++y) {
                        size_t dstIdx =
                            (static_cast<size_t>(offsetY + y) *
                                 static_cast<size_t>(totalW) +
                             static_cast<size_t>(offsetX)) *
                            4u;
                        size_t srcIdx =
                            (static_cast<size_t>(y) * static_cast<size_t>(w)) *
                            4u;
                        std::memcpy(&result.image.rgba[dstIdx],
                                    &src->rgba[srcIdx],
                                    static_cast<size_t>(copyW) * 4u);
                    }
                }
            } else {
                LOG_ERROR("wlr: failed to compute output bounds");
            }
        } else {
            if (selected < 0 ||
                selected >= static_cast<int>(ctx.outputs.size())) {
                LOG_ERROR("wlr: no output selected for capture");
                cleanupContext(ctx);
                return result;
            }

            if (!captureOutputImage(ctx, ctx.outputs[selected]->output,
                                    result.image)) {
                LOG_ERROR("wlr: capture failed");
            }
        }
        cleanupContext(ctx);
        return result;
    }
};

std::unique_ptr<ICaptureBackend> CreateBackendWlrScreencopy() {
    return std::make_unique<WlrScreencopyBackend>();
}

}  // namespace coomer
