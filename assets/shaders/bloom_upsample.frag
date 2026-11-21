#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D uTexture;
uniform float uFilterRadius;

void main() {
    vec2 texel = 1.0 / vec2(textureSize(uTexture, 0));
    vec2 r = texel * max(uFilterRadius, 0.5);

    // 9-tap 3x3 tent filter for smooth multi-scale CRT optical halation.
    vec3 a = texture(uTexture, TexCoord + vec2(-r.x,  r.y)).rgb;
    vec3 b = texture(uTexture, TexCoord + vec2( 0.0,  r.y)).rgb;
    vec3 c = texture(uTexture, TexCoord + vec2( r.x,  r.y)).rgb;

    vec3 d = texture(uTexture, TexCoord + vec2(-r.x,  0.0)).rgb;
    vec3 e = texture(uTexture, TexCoord).rgb;
    vec3 f = texture(uTexture, TexCoord + vec2( r.x,  0.0)).rgb;

    vec3 g = texture(uTexture, TexCoord + vec2(-r.x, -r.y)).rgb;
    vec3 h = texture(uTexture, TexCoord + vec2( 0.0, -r.y)).rgb;
    vec3 i = texture(uTexture, TexCoord + vec2( r.x, -r.y)).rgb;

    vec3 result = e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i);
    result *= (1.0 / 16.0);
    FragColor = vec4(result, 1.0);
}
