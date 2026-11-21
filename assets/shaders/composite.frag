#version 330 core

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uStrength;
uniform bool uUseBloom;

// Exact 1:1 SDR passthrough when peak <= 1.0 (preserving backgrounds, UI & bars),
// with smooth CRT hot-white phosphor core rolloff for HDR energy > 1.0.
vec3 crtToneMap(vec3 hdr) {
    vec3 c = max(hdr, vec3(0.0));
    float peak = max(max(c.r, c.g), c.b);
    if (peak <= 1.0) {
        return c;
    }
    vec3 base = c / peak;
    float excess = peak - 1.0;
    float coreWhite = 1.0 - exp(-excess * 0.18);
    return mix(base, vec3(1.0), coreWhite * 0.72);
}

void main() {
    vec3 scene = texture(uScene, TexCoord).rgb;
    vec3 bloom = uUseBloom ? texture(uBloom, TexCoord).rgb * uStrength : vec3(0.0);
    FragColor = vec4(crtToneMap(scene + bloom), 1.0);
}
