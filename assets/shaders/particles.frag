#version 330 core
out vec4 FragColor;
in vec4 ParticleColor;

void main() {
    vec2 point = gl_PointCoord * 2.0 - 1.0;
    float distanceFromCenter = length(point);
    float edgeWidth = max(fwidth(distanceFromCenter), 1e-4);
    float alpha = 1.0 - smoothstep(1.0 - edgeWidth, 1.0, distanceFromCenter);

    FragColor = vec4(ParticleColor.rgb, ParticleColor.a * alpha);
}
