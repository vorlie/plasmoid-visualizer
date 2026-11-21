#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aLocal;  // (alongPx, acrossPx) relative to segment start p0
layout (location = 2) in vec4 aParams; // (segLenPx, sigmaPx, intensity0, intensity1)

out vec2 vLocal;
out vec4 vParams;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vLocal = aLocal;
    vParams = aParams;
}
