#version 330 core

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D uBloom;
uniform float uStrength;

void main() {
    vec3 bloom = texture(uBloom, TexCoord).rgb;
    FragColor = vec4(bloom * uStrength, 1.0);
}
