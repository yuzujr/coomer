#version 330 core
in vec2 v_uv;

uniform sampler2D u_tex;
uniform vec2 u_imageSize;
uniform vec2 u_screenSize;
uniform vec2 u_pan;
uniform float u_zoom;
uniform int u_pixelMode;
uniform vec2 u_cursor;
uniform float u_radius;
uniform vec4 u_tint;
uniform int u_spotlight;
uniform int u_uniformImage;
uniform sampler2D u_statusTex;
uniform vec2 u_statusSize;
uniform sampler2D u_inspectorTex;
uniform vec2 u_inspectorSize;
uniform int u_inspector;
uniform float u_inspectorOpacity;

out vec4 FragColor;

void main() {
    vec2 screen = gl_FragCoord.xy;
    vec2 img = (screen - u_pan) / u_zoom;
    vec2 uv = img / u_imageSize;
    uv.y = 1.0 - uv.y;
    vec4 color = texture(u_tex, uv);

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        color = vec4(0.0, 0.0, 0.0, 1.0);
    } else if (u_pixelMode == 1) {
        ivec2 pixelCoord = ivec2(floor(img.x),
                                 u_imageSize.y - 1.0 - floor(img.y));
        pixelCoord = clamp(pixelCoord, ivec2(0), ivec2(u_imageSize) - 1);
        color = texelFetch(u_tex, pixelCoord, 0);

        vec2 withinPixel = fract(img);
        vec2 edgeDistance = min(withinPixel, 1.0 - withinPixel);
        float gridDistance = min(edgeDistance.x, edgeDistance.y);
        float grid = 1.0 - smoothstep(0.0, min(0.7 / u_zoom, 0.12), gridDistance);
        color.rgb = mix(color.rgb, vec3(0.02), grid * 0.38);
    }

    if (u_spotlight == 1) {
        float dist = distance(screen, u_cursor);
        float feather = max(2.0, u_radius * 0.08);
        float edge = smoothstep(u_radius, u_radius + feather, dist);
        float tintAmount = u_tint.a * edge;
        color.rgb = mix(color.rgb, u_tint.rgb, tintAmount);
    }

    // Keep the captured pixels intact. When the source really is one color,
    // add a labelled status card instead of inventing replacement content.
    if (u_uniformImage == 1) {
        float statusScale = min(1.0, (u_screenSize.x - 32.0) / u_statusSize.x);
        statusScale = max(statusScale, 0.25);
        vec2 displaySize = u_statusSize * statusScale;
        vec2 origin = vec2((u_screenSize.x - displaySize.x) * 0.5,
                           u_screenSize.y - displaySize.y - 20.0);
        vec2 local = screen - origin;
        if (local.x >= 0.0 && local.y >= 0.0 &&
            local.x < displaySize.x && local.y < displaySize.y) {
            vec2 statusUv = local / displaySize;
            statusUv.y = 1.0 - statusUv.y;
            vec4 status = texture(u_statusTex, statusUv);
            color.rgb = mix(color.rgb, status.rgb, status.a);
        }
    }

    if (u_inspector == 1) {
        vec2 origin = u_cursor + vec2(18.0);
        if (origin.x + u_inspectorSize.x > u_screenSize.x - 8.0) {
            origin.x = u_cursor.x - u_inspectorSize.x - 18.0;
        }
        if (origin.y + u_inspectorSize.y > u_screenSize.y - 8.0) {
            origin.y = u_cursor.y - u_inspectorSize.y - 18.0;
        }
        origin = clamp(origin, vec2(8.0),
                       max(vec2(8.0), u_screenSize - u_inspectorSize - 8.0));
        vec2 local = screen - origin;
        if (local.x >= 0.0 && local.y >= 0.0 &&
            local.x < u_inspectorSize.x && local.y < u_inspectorSize.y) {
            vec2 inspectorUv = local / u_inspectorSize;
            inspectorUv.y = 1.0 - inspectorUv.y;
            vec4 inspector = texture(u_inspectorTex, inspectorUv);
            color.rgb = mix(color.rgb, inspector.rgb,
                            inspector.a * u_inspectorOpacity);
        }
    }

    FragColor = color;
}
