#pragma once

namespace coomer {

static const char* kVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;

out vec2 v_uv;

void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* kFragmentShaderSource = R"(#version 330 core
in vec2 v_uv;

uniform sampler2D u_tex;
uniform vec2 u_imageSize;
uniform vec2 u_screenSize;
uniform vec2 u_pan;
uniform float u_zoom;
uniform vec2 u_cursor;
uniform float u_radius;
uniform vec4 u_tint;
uniform int u_spotlight;
uniform int u_uniformImage;

out vec4 FragColor;

void main() {
    vec2 screen = gl_FragCoord.xy;
    vec2 img = (screen - u_pan) / u_zoom;
    vec2 uv = img / u_imageSize;
    uv.y = 1.0 - uv.y;
    vec4 color = texture(u_tex, uv);

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        color = vec4(0.0, 0.0, 0.0, 1.0);
    }

    // A completely uniform capture gives users no visual confirmation that
    // the magnifier is running. Add a subtle placeholder only in that edge
    // case; normal screenshots remain pixel-for-pixel unchanged.
    if (u_uniformImage == 1) {
        vec2 tile = floor(screen / 48.0);
        float checker = mod(tile.x + tile.y, 2.0);
        vec3 shadeA = mix(color.rgb, vec3(0.12, 0.16, 0.22), 0.18);
        vec3 shadeB = mix(color.rgb, vec3(0.24, 0.38, 0.58), 0.28);
        color.rgb = mix(shadeA, shadeB, checker);

        float scale = min(u_screenSize.x, u_screenSize.y);
        vec2 p = (screen - u_screenSize * 0.5) / scale;
        vec2 lensCenter = vec2(-0.05, 0.04);
        float ringDistance = abs(length(p - lensCenter) - 0.14);
        float ring = 1.0 - smoothstep(0.008, 0.016, ringDistance);

        vec2 handleStart = vec2(0.05, -0.06);
        vec2 handleEnd = vec2(0.20, -0.21);
        vec2 pa = p - handleStart;
        vec2 ba = handleEnd - handleStart;
        float alongHandle = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        float handleDistance = length(pa - ba * alongHandle);
        float handle = 1.0 - smoothstep(0.012, 0.025, handleDistance);

        color.rgb = mix(color.rgb, vec3(0.35, 0.72, 1.0), max(ring, handle));
    }

    if (u_spotlight == 1) {
        float dist = distance(screen, u_cursor);
        float feather = max(2.0, u_radius * 0.08);
        float edge = smoothstep(u_radius, u_radius + feather, dist);
        float tintAmount = u_tint.a * edge;
        color.rgb = mix(color.rgb, u_tint.rgb, tintAmount);
    }

    FragColor = color;
}
)";

}  // namespace coomer
