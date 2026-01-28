#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D screenTexture;
uniform bool uUseTexture;
uniform bool uIsFont = false;
uniform vec4 uColor;
uniform float uCornerRadius = 0.0; // Normalized 0-1 (e.g. 0.1)
uniform float uAspect = 1.0;      // width/height

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
        vec4 texColor = texture(screenTexture, TexCoord);
        if (uIsFont) {
            FragColor = vec4(uColor.rgb, texColor.r * uColor.a);
        } else {
            FragColor = vec4(texColor.rgb * uColor.rgb, texColor.a * uColor.a);
        }
    } else {
        FragColor = uColor;
    }
}
