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
            float delta = max(fwidth(dist), 1e-5);
            alpha = 1.0 - smoothstep(-delta, delta, dist);
        }
    } else if (uShape == 2) { // Dots (Quads)
        float dist = length(LocalPos * 2.0 - 1.0);
        float delta = max(fwidth(dist), 1e-5);
        alpha = 1.0 - smoothstep(1.0 - delta, 1.0, dist);
    } else if (uShape == 10) { // Circular Points (for Beam Head)
        float dist = length(gl_PointCoord * 2.0 - 1.0);
        float delta = max(fwidth(dist), 1e-5);
        alpha = 1.0 - smoothstep(1.0 - delta, 1.0, dist);
        alpha *= (1.0 - smoothstep(0.4, 1.0, dist));
        FragColor = vec4(uColor.rgb * vIntensity, uColor.a * alpha);
        return;
    } else if (uShape == 4 || uShape == 6) { // XY traces
        // The ribbon supplies 0..1 across its width. A soft bell-shaped beam
        // avoids the stacked rectangular bands produced by hard-edged passes.
        float across = abs(LocalPos.y * 2.0 - 1.0);
        float feather = max(fwidth(across) * 1.5, 0.025);
        float edgeMask = 1.0 - smoothstep(1.0 - feather, 1.0, across);
        alpha = exp(-3.0 * across * across) * edgeMask;
    }

    // Keep velocity brightness linear. Multiplying the complete color by
    // vIntensity also reduced source alpha, which squared the dimming during
    // blending and created visible gaps between XY samples.
    FragColor = vec4(uColor.rgb * vIntensity, uColor.a * alpha);
}
