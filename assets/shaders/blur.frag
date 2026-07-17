#version 330 core

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D uTexture;
uniform vec2 uDirection;
uniform float uThreshold;
uniform float uSoftKnee;
uniform bool uPrefilter;

vec3 prefilter(vec3 color) {
    if (!uPrefilter) return color;

    float brightness = max(max(color.r, color.g), color.b);
    float knee = max(uSoftKnee, 0.0001);
    float soft = clamp(brightness - uThreshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);
    float contribution = max(brightness - uThreshold, soft);
    contribution /= max(brightness, 0.0001);
    return color * contribution;
}

vec3 bloomSample(vec2 uv) {
    return prefilter(texture(uTexture, uv).rgb);
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(uTexture, 0));
    vec2 offset = uDirection * texel * 1.35;

    vec3 result = bloomSample(TexCoord) * 0.227027;
    result += bloomSample(TexCoord + offset) * 0.194595;
    result += bloomSample(TexCoord - offset) * 0.194595;
    result += bloomSample(TexCoord + offset * 2.0) * 0.121622;
    result += bloomSample(TexCoord - offset * 2.0) * 0.121622;
    result += bloomSample(TexCoord + offset * 3.0) * 0.054054;
    result += bloomSample(TexCoord - offset * 3.0) * 0.054054;
    result += bloomSample(TexCoord + offset * 4.0) * 0.016216;
    result += bloomSample(TexCoord - offset * 4.0) * 0.016216;

    FragColor = vec4(result, 1.0);
}
