#version 330 core
out vec4 FragColor;
in vec2 LocalPos;
in float vIntensity;
uniform vec4 uColor;
uniform float uCornerRadius;
uniform int uShape; // 0: Bars, 1: Lines, 2: Dots

float roundedBoxSDF(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    float alpha = 1.0;
    if (uShape == 0) { // Bars
        if (uCornerRadius > 0.001) {
            vec2 p = LocalPos * 2.0 - 1.0;
            float dist = roundedBoxSDF(p, vec2(1.0), uCornerRadius);
            float delta = fwidth(dist);
            alpha = 1.0 - smoothstep(-delta, 0.0, dist);
        }
    } else if (uShape == 2) { // Dots (Quads)
        float dist = length(LocalPos * 2.0 - 1.0);
        float delta = fwidth(dist);
        alpha = 1.0 - smoothstep(1.0 - delta, 1.0, dist);
    } else if (uShape == 10) { // Circular Points (for Beam Head)
        float dist = length(gl_PointCoord * 2.0 - 1.0);
        float delta = fwidth(dist);
        alpha = 1.0 - smoothstep(1.0 - delta, 1.0, dist);
        if (uShape == 10) { // Beam Head soft falloff
            alpha *= (1.0 - smoothstep(0.4, 1.0, dist));
        }
        FragColor = uColor * vIntensity * alpha;
        return;
    }
    
    if (alpha <= 0.0) discard;
    FragColor = uColor * vIntensity * alpha;
}
