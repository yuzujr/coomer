#include "render/RendererGL.hpp"

#include <glad/glad.h>

#include <cmath>
#include <string>
#include <vector>

#include "platform/Log.hpp"
#include "render/ShaderSources.hpp"

namespace coomer {

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

    auto wrapper = [](const char* name) -> void* {
        return staticLoader(name);
    };

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(+wrapper))) {
        LOG_ERROR("gladLoadGLLoader failed");
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

    return true;
}

bool RendererGL::uploadScreenshotTexture(const ImageRGBA& image) {
    if (image.w <= 0 || image.h <= 0 || image.rgba.empty()) {
        LOG_ERROR("invalid screenshot image");
        return false;
    }
    imageW_ = image.w;
    imageH_ = image.h;

    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.w, image.h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, image.rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void RendererGL::renderFrame(const CameraState& camera,
                             const SpotlightState& spotlight) {
    if (!program_ || !tex_) {
        return;
    }

    glViewport(0, 0, camera.screenW, camera.screenH);
    glDisable(GL_DEPTH_TEST);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

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

    glUniform1i(locTex, 0);
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

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

}  // namespace coomer
