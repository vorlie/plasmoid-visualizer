#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D uTexture;
uniform bool uPrefilter;
uniform float uThreshold;
uniform float uSoftKnee;

vec3 prefilterColor(vec3 color) {
    vec3 c = max(color, vec3(0.0));
    float brightness = max(max(c.r, c.g), c.b);
    float knee = max(uSoftKnee, 0.0001);
    float soft = clamp(brightness - uThreshold + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee + 0.0001);
    float contribution = max(brightness - uThreshold, soft) / max(brightness, 0.0001);
    return c * contribution;
}

float karisWeight(vec3 c) {
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return 1.0 / (1.0 + luma);
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(uTexture, 0));

    // 13-tap Jimenez downsample filter (eliminates thin-line shimmering / fireflies).
    vec3 a = texture(uTexture, TexCoord + texel * vec2(-2.0,  2.0)).rgb;
    vec3 b = texture(uTexture, TexCoord + texel * vec2( 0.0,  2.0)).rgb;
    vec3 c = texture(uTexture, TexCoord + texel * vec2( 2.0,  2.0)).rgb;

    vec3 d = texture(uTexture, TexCoord + texel * vec2(-2.0,  0.0)).rgb;
    vec3 e = texture(uTexture, TexCoord).rgb;
    vec3 f = texture(uTexture, TexCoord + texel * vec2( 2.0,  0.0)).rgb;

    vec3 g = texture(uTexture, TexCoord + texel * vec2(-2.0, -2.0)).rgb;
    vec3 h = texture(uTexture, TexCoord + texel * vec2( 0.0, -2.0)).rgb;
    vec3 i = texture(uTexture, TexCoord + texel * vec2( 2.0, -2.0)).rgb;

    vec3 j = texture(uTexture, TexCoord + texel * vec2(-1.0,  1.0)).rgb;
    vec3 k = texture(uTexture, TexCoord + texel * vec2( 1.0,  1.0)).rgb;
    vec3 l = texture(uTexture, TexCoord + texel * vec2(-1.0, -1.0)).rgb;
    vec3 m = texture(uTexture, TexCoord + texel * vec2( 1.0, -1.0)).rgb;

    if (uPrefilter) {
        a = prefilterColor(a); b = prefilterColor(b); c = prefilterColor(c);
        d = prefilterColor(d); e = prefilterColor(e); f = prefilterColor(f);
        g = prefilterColor(g); h = prefilterColor(h); i = prefilterColor(i);
        j = prefilterColor(j); k = prefilterColor(k); l = prefilterColor(l); m = prefilterColor(m);

        vec3 g0 = (a + b + d + e) * 0.25;
        vec3 g1 = (b + c + e + f) * 0.25;
        vec3 g2 = (d + e + g + h) * 0.25;
        vec3 g3 = (e + f + h + i) * 0.25;
        vec3 g4 = (j + k + l + m) * 0.25;

        float w0 = karisWeight(g0) * 0.125;
        float w1 = karisWeight(g1) * 0.125;
        float w2 = karisWeight(g2) * 0.125;
        float w3 = karisWeight(g3) * 0.125;
        float w4 = karisWeight(g4) * 0.5;
        vec3 result = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) / max(w0 + w1 + w2 + w3 + w4, 1e-5);
        FragColor = vec4(max(result, vec3(0.0)), 1.0);
        return;
    }

    vec3 result = e * 0.125;
    result += (a + c + g + i) * 0.03125;
    result += (b + d + f + h) * 0.0625;
    result += (j + k + l + m) * 0.125;
    FragColor = vec4(max(result, vec3(0.0)), 1.0);
}
