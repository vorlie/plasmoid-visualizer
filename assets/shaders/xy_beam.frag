#version 330 core
out vec4 FragColor;

in vec2 vLocal;
in vec4 vParams;

uniform vec4 uColor;
uniform float uBloom;

// Abramowitz & Stegun 7.1.26 error-function approximation (max error < 1.5e-7).
float erfApprox(float x) {
    float ax = abs(x);
    float t = 1.0 / (1.0 + 0.3275911 * ax);
    float poly = (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t
                  - 0.284496736) * t + 0.254829592) * t;
    float res = 1.0 - poly * exp(-ax * ax);
    return x < 0.0 ? -res : res;
}

void main() {
    float L = vParams.x;
    float sigma = max(vParams.y, 0.45);
    float tAlong = L > 1e-4 ? clamp(vLocal.x / L, 0.0, 1.0) : 0.5;
    float segIntensity = max(mix(vParams.z, vParams.w, tAlong), 0.0);
    if (segIntensity <= 1e-5) discard;

    float clampedX = clamp(vLocal.x, 0.0, L);
    float distToSeg = length(vec2(vLocal.x - clampedX, vLocal.y));
    float pad = sigma * 5.5;
    float outerMask = 1.0 - smoothstep(pad * 0.72, pad, distToSeg);
    if (outerMask <= 1e-5) discard;

    float coreEnergy = 0.0;
    float haloEnergy = 0.0;
    float haloSigma = sigma * 1.9;

    if (L < 0.15) {
        // Stationary / dwell CRT beam spot or explicit beam head: radial 2D Gaussian.
        float r2 = dot(vLocal, vLocal);
        coreEnergy = exp(-r2 / (2.0 * sigma * sigma));
        haloEnergy = exp(-r2 / (2.0 * haloSigma * haloSigma));
    } else {
        // Analytical Gaussian integral along segment [0, L]:
        // adjacent segments sum to 0.5 + 0.5 = 1.0 at vertices (no corner fold hotspots)
        // and blanked ends form smooth circular caps.
        float invSqrt2Sigma = 0.70710678 / sigma;
        float transverse = exp(-(vLocal.y * vLocal.y) / (2.0 * sigma * sigma));
        float along = 0.5 * (erfApprox(vLocal.x * invSqrt2Sigma)
                           - erfApprox((vLocal.x - L) * invSqrt2Sigma));
        coreEnergy = transverse * along;

        float invSqrt2Halo = 0.70710678 / haloSigma;
        float haloTransverse = exp(-(vLocal.y * vLocal.y) / (2.0 * haloSigma * haloSigma));
        float haloAlong = 0.5 * (erfApprox(vLocal.x * invSqrt2Halo)
                               - erfApprox((vLocal.x - L) * invSqrt2Halo));
        haloEnergy = haloTransverse * haloAlong;
    }

    float coreEmission = (0.52 + uBloom * 0.16) * segIntensity;
    float haloEmission = (0.09 + uBloom * 0.04) * segIntensity;
    vec3 coreColor = mix(uColor.rgb, vec3(1.0), 0.15) * coreEmission * coreEnergy;
    vec3 haloColor = uColor.rgb * haloEmission * haloEnergy;
    vec3 finalRgb = (coreColor + haloColor) * uColor.a * outerMask;
    FragColor = vec4(finalRgb, 1.0);
}
