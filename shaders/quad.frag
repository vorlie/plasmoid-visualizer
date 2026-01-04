#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D screenTexture;
uniform bool uUseTexture;
uniform vec4 uColor;
void main() {
    if (uUseTexture) {
        vec4 texColor = texture(screenTexture, TexCoord);
        FragColor = vec4(texColor.rgb, 1.0);
    } else {
        FragColor = uColor;
    }
}
