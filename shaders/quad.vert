#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform vec2 uSize = vec2(1.0, 1.0);
uniform vec2 uOffset = vec2(0.0, 0.0);
uniform float uRotation = 0.0; // Radians
uniform vec4 uTexRect = vec4(0.0, 0.0, 1.0, 1.0); // xy, wh

out vec2 TexCoord;

void main() {
    float c = cos(uRotation);
    float s = sin(uRotation);
    mat2 rot = mat2(c, -s, s, c);
    
    vec2 pos = (aPos * uSize);
    pos = rot * pos;
    gl_Position = vec4(pos + uOffset, 0.0, 1.0);
    
    // Remap TexCoord [0, 1] to sub-rect [tx, ty, tw, th]
    TexCoord = uTexRect.xy + aTexCoord * uTexRect.zw;
}
