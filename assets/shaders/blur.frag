#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D uTexture;
uniform vec2 uDirection; // (1,0) horizontal or (0,1) vertical

// 9-tap gaussian kernel (center + 4 pairs)
void main() {
    vec2 texel = 1.0 / textureSize(uTexture, 0);
    vec3 result = vec3(0.0);
    float kernel[5] = float[](0.227027, 0.194595, 0.121622, 0.054054, 0.016216);
    vec2 off = uDirection * texel;
    result += texture(uTexture, TexCoord).rgb * kernel[0];
    for (int i = 1; i <= 4; ++i) {
        result += texture(uTexture, TexCoord + off * float(i)).rgb * kernel[i];
        result += texture(uTexture, TexCoord - off * float(i)).rgb * kernel[i];
    }
    FragColor = vec4(result, 1.0);
}
