#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform bool uIsFont = false;
uniform vec4 uColor;
uniform float uCornerRadius = 0.0; // Normalized 0-1 (e.g. 0.1)
uniform float uAspect = 1.0;      // width/height
uniform float uSaturation = 1.0;
uniform float uColorShift = 0.0;

// Luminance-preserving Rodrigues hue rotation around (1, 1, 1) for authentic CRT phosphor decay shift.
vec3 shiftHue(vec3 c, float radians) {
    float cosA = cos(radians);
    float sinA = sin(radians);
    const vec3 k = vec3(0.57735027);
    return max(c * cosA + cross(k, c) * sinA + k * dot(k, c) * (1.0 - cosA), vec3(0.0));
}

void main() {
    if (uCornerRadius > 0.0) {
        // Distance field for rounded rect
        vec2 uv = (TexCoord * 2.0 - 1.0) * vec2(uAspect, 1.0);
        vec2 size = vec2(uAspect, 1.0);
        vec2 q = abs(uv) - size + uCornerRadius;
        float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - uCornerRadius;

        if (dist > 0.0) discard;
    }

    if (uUseTexture) {
        vec4 texColor = texture(uTexture, TexCoord);
        if (uIsFont) {
            FragColor = vec4(uColor.rgb, texColor.r * uColor.a);
        } else {
            vec3 rgb = texColor.rgb * uColor.rgb;
            if (abs(uColorShift) > 0.001) {
                rgb = shiftHue(rgb, uColorShift * 1.25);
            }
            if (abs(uSaturation - 1.0) > 0.001) {
                float luma = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
                rgb = max(mix(vec3(luma), rgb, uSaturation), vec3(0.0));
            }
            FragColor = vec4(rgb, texColor.a * uColor.a);
        }
    } else {
        FragColor = uColor;
    }
}
