#include "render/RendererGL.hpp"

#include <glad/gl.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "platform/Log.hpp"
#include "render/ShaderSources.hpp"

namespace coomer {

namespace {

using Glyph = std::array<uint8_t, 7>;

Glyph glyphFor(char c) {
    switch (c) {
        case '0': return {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e};
        case '1': return {0x04, 0x0c, 0x14, 0x04, 0x04, 0x04, 0x1f};
        case '2': return {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f};
        case '3': return {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e};
        case '4': return {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02};
        case '5': return {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e};
        case '6': return {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e};
        case '7': return {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
        case '8': return {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e};
        case '9': return {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e};
        case '#': return {0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a};
        case ',': return {0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x08};
        case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c};
        case 'A': return {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
        case 'B': return {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e};
        case 'C': return {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e};
        case 'D': return {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e};
        case 'E': return {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f};
        case 'F': return {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10};
        case 'G': return {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f};
        case 'H': return {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
        case 'I': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f};
        case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
        case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
        case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
        case 'O': return {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
        case 'P': return {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10};
        case 'Q': return {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d};
        case 'R': return {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
        case 'S': return {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e};
        case 'T': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
        case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
        case 'X': return {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11};
        default: return {};
    }
}

void putPixel(std::vector<uint8_t>& pixels, int width, int height, int x,
              int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    const size_t offset = static_cast<size_t>((y * width + x) * 4);
    pixels[offset] = r;
    pixels[offset + 1] = g;
    pixels[offset + 2] = b;
    pixels[offset + 3] = a;
}

void drawPanel(std::vector<uint8_t>& pixels, int width, int height) {
    constexpr int radius = 10;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int cornerX =
                x < radius
                    ? radius
                    : (x >= width - radius ? width - radius - 1 : x);
            const int cornerY =
                y < radius
                    ? radius
                    : (y >= height - radius ? height - radius - 1 : y);
            const int dx = x - cornerX;
            const int dy = y - cornerY;
            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }

            const bool border = x < 2 || y < 2 || x >= width - 2 ||
                                y >= height - 2;
            if (border) {
                putPixel(pixels, width, height, x, y, 96, 165, 250, 245);
            } else {
                putPixel(pixels, width, height, x, y, 17, 24, 39, 235);
            }
        }
    }
}

void drawText(std::vector<uint8_t>& pixels, int width, int height,
              const std::string& text, int originX, int originY, int scale,
              uint8_t r, uint8_t g, uint8_t b) {
    int x = originX;
    for (char c : text) {
        const Glyph glyph = glyphFor(c);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((glyph[row] & (1u << (4 - col))) == 0) {
                    continue;
                }
                for (int dy = 0; dy < scale; ++dy) {
                    for (int dx = 0; dx < scale; ++dx) {
                        putPixel(pixels, width, height, x + col * scale + dx,
                                 originY + row * scale + dy, r, g, b, 255);
                    }
                }
            }
        }
        x += 6 * scale;
    }
}

std::vector<uint8_t> makeUniformCaptureStatus(int width, int height) {
    std::vector<uint8_t> pixels(
        static_cast<size_t>(width * height * 4), 0);
    drawPanel(pixels, width, height);

    for (int y = 12; y < height - 12; ++y) {
        for (int x = 14; x < 18; ++x) {
            putPixel(pixels, width, height, x, y, 96, 165, 250, 255);
        }
    }

    drawText(pixels, width, height, "CAPTURED SCREEN IS A SINGLE COLOR", 28,
             13, 2, 248, 250, 252);
    drawText(pixels, width, height, "Q OR RIGHT CLICK TO EXIT", 28, 43, 2,
             147, 197, 253);
    return pixels;
}

std::vector<uint8_t> makeInspectorHud(const std::string& text, int width,
                                      int height) {
    std::vector<uint8_t> pixels(
        static_cast<size_t>(width * height * 4), 0);
    drawPanel(pixels, width, height);
    drawText(pixels, width, height, text, 16, 13, 2, 248, 250, 252);
    return pixels;
}

}  // namespace

static GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        LOG_ERROR("shader compile failed: %s",
                  log.empty() ? "unknown" : log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool RendererGL::compileShaders() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShaderSource);
    if (!vs) return false;
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetProgramInfoLog(program_, len, nullptr, log.data());
        LOG_ERROR("shader link failed: %s",
                  log.empty() ? "unknown" : log.data());
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    return true;
}

bool RendererGL::initGL(std::function<void*(const char*)> loaderProc) {
    if (!loaderProc) {
        LOG_ERROR("GL loader proc not provided");
        return false;
    }

    // Store the loader in a static variable for the wrapper to access
    static std::function<void*(const char*)> staticLoader;
    staticLoader = loaderProc;

    auto wrapper = [](const char* name) -> GLADapiproc {
        // EGL/GLX loaders return void*; reinterpret to function pointer.
        return reinterpret_cast<GLADapiproc>(staticLoader(name));
    };

    if (!gladLoadGL(+wrapper)) {
        LOG_ERROR("gladLoadGL failed");
        return false;
    }
    if (!GLAD_GL_VERSION_3_3) {
        LOG_ERROR("OpenGL 3.3 core not available");
        return false;
    }

    if (!compileShaders()) {
        return false;
    }

    float verts[] = {
        // pos     // uv
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,  -1.0f, 1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f, -1.0f, 1.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    statusW_ = 448;
    statusH_ = 72;
    const std::vector<uint8_t> statusPixels =
        makeUniformCaptureStatus(statusW_, statusH_);
    glGenTextures(1, &statusTex_);
    glBindTexture(GL_TEXTURE_2D, statusTex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, statusW_, statusH_, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, statusPixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    inspectorW_ = 364;
    inspectorH_ = 40;
    const std::vector<uint8_t> inspectorPixels(
        static_cast<size_t>(inspectorW_ * inspectorH_ * 4), 0);
    glGenTextures(1, &inspectorTex_);
    glBindTexture(GL_TEXTURE_2D, inspectorTex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, inspectorW_, inspectorH_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, inspectorPixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool RendererGL::uploadScreenshotTexture(const ImageRGBA& image) {
    if (image.w <= 0 || image.h <= 0 || image.rgba.empty()) {
        LOG_ERROR("invalid screenshot image");
        return false;
    }
    imageW_ = image.w;
    imageH_ = image.h;
    imagePixels_ = image.rgba;
    uniformImage_ = true;
    for (size_t i = 4; i < image.rgba.size(); i += 4) {
        if (image.rgba[i] != image.rgba[0] ||
            image.rgba[i + 1] != image.rgba[1] ||
            image.rgba[i + 2] != image.rgba[2] ||
            image.rgba[i + 3] != image.rgba[3]) {
            uniformImage_ = false;
            break;
        }
    }
    if (uniformImage_) {
        LOG_INFO("captured screen is a single color; showing status overlay");
    }

    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.w, image.h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, image.rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool RendererGL::updateInspectorTexture(const CameraState& camera,
                                        const SpotlightState& spotlight,
                                        float opacity) {
    if (opacity <= 0.001f || imagePixels_.empty()) {
        return false;
    }

    const float imageX = (spotlight.cursorX - camera.panX) / camera.zoom;
    const float imageYFromBottom =
        (spotlight.cursorY - camera.panY) / camera.zoom;
    if (imageX < 0.0f || imageYFromBottom < 0.0f ||
        imageX >= static_cast<float>(imageW_) ||
        imageYFromBottom >= static_cast<float>(imageH_)) {
        return false;
    }

    const int x = static_cast<int>(std::floor(imageX));
    const int y = imageH_ - 1 -
                  static_cast<int>(std::floor(imageYFromBottom));
    const size_t offset = static_cast<size_t>((y * imageW_ + x) * 4);

    char text[96];
    std::snprintf(text, sizeof(text), "%.2fX  %d,%d  #%02X%02X%02X",
                  camera.zoom, x, y,
                  static_cast<unsigned int>(imagePixels_[offset]),
                  static_cast<unsigned int>(imagePixels_[offset + 1]),
                  static_cast<unsigned int>(imagePixels_[offset + 2]));
    if (inspectorText_ != text) {
        inspectorText_ = text;
        const std::vector<uint8_t> pixels =
            makeInspectorHud(inspectorText_, inspectorW_, inspectorH_);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, inspectorTex_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, inspectorW_, inspectorH_,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    return true;
}

void RendererGL::renderFrame(const CameraState& camera,
                             const SpotlightState& spotlight,
                             float inspectorOpacity) {
    if (!program_ || !tex_) {
        return;
    }

    glViewport(0, 0, camera.screenW, camera.screenH);
    glDisable(GL_DEPTH_TEST);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const bool inspectorVisible =
        updateInspectorTexture(camera, spotlight, inspectorOpacity);

    glUseProgram(program_);

    GLint locTex = glGetUniformLocation(program_, "u_tex");
    GLint locImageSize = glGetUniformLocation(program_, "u_imageSize");
    GLint locScreenSize = glGetUniformLocation(program_, "u_screenSize");
    GLint locPan = glGetUniformLocation(program_, "u_pan");
    GLint locZoom = glGetUniformLocation(program_, "u_zoom");
    GLint locCursor = glGetUniformLocation(program_, "u_cursor");
    GLint locRadius = glGetUniformLocation(program_, "u_radius");
    GLint locTint = glGetUniformLocation(program_, "u_tint");
    GLint locSpotlight = glGetUniformLocation(program_, "u_spotlight");
    GLint locUniformImage = glGetUniformLocation(program_, "u_uniformImage");
    GLint locStatusTex = glGetUniformLocation(program_, "u_statusTex");
    GLint locStatusSize = glGetUniformLocation(program_, "u_statusSize");
    GLint locInspectorTex = glGetUniformLocation(program_, "u_inspectorTex");
    GLint locInspectorSize = glGetUniformLocation(program_, "u_inspectorSize");
    GLint locInspector = glGetUniformLocation(program_, "u_inspector");
    GLint locInspectorOpacity =
        glGetUniformLocation(program_, "u_inspectorOpacity");

    glUniform1i(locTex, 0);
    glUniform1i(locStatusTex, 1);
    glUniform1i(locInspectorTex, 2);
    glUniform2f(locImageSize, static_cast<float>(imageW_),
                static_cast<float>(imageH_));
    glUniform2f(locScreenSize, static_cast<float>(camera.screenW),
                static_cast<float>(camera.screenH));
    glUniform2f(locPan, camera.panX, camera.panY);
    glUniform1f(locZoom, camera.zoom);
    glUniform2f(locCursor, spotlight.cursorX, spotlight.cursorY);
    glUniform1f(locRadius, spotlight.radiusPx);
    glUniform4f(locTint, spotlight.tintR, spotlight.tintG, spotlight.tintB,
                spotlight.tintA);
    glUniform1i(locSpotlight, spotlight.enabled ? 1 : 0);
    glUniform1i(locUniformImage, uniformImage_ ? 1 : 0);
    glUniform2f(locStatusSize, static_cast<float>(statusW_),
                static_cast<float>(statusH_));
    glUniform2f(locInspectorSize, static_cast<float>(inspectorW_),
                static_cast<float>(inspectorH_));
    glUniform1i(locInspector, inspectorVisible ? 1 : 0);
    glUniform1f(locInspectorOpacity, inspectorOpacity);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, inspectorTex_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, statusTex_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);
}

}  // namespace coomer
