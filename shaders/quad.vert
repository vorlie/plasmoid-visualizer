#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform float uScale = 1.0;
uniform vec2 uOffset = vec2(0.0, 0.0);

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos * uScale + uOffset, 0.0, 1.0);
    TexCoord = aTexCoord;
}
