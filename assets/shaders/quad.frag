#version 330 core
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

    if (u_spotlight == 1) {
        float dist = distance(screen, u_cursor);
        float feather = max(2.0, u_radius * 0.08);
        float edge = smoothstep(u_radius, u_radius + feather, dist);
        float tintAmount = u_tint.a * edge;
        color.rgb = mix(color.rgb, u_tint.rgb, tintAmount);
    }

    FragColor = color;
}
