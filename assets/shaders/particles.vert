#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in float aSize;

out vec4 ParticleColor;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    gl_PointSize = max(aSize, 1.0);
    ParticleColor = aColor;
}
