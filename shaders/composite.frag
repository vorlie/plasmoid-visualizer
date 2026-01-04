#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D uScene;      // raw trace
uniform sampler2D uBlur;       // blurred glow
uniform sampler2D uPersistence; // previous frame persistence
uniform float uDecay;
uniform float uScanlineIntensity;
uniform float uNoiseAmount;
uniform vec3 uColor;

float rand(vec2 co) {
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
    vec3 scene = texture(uScene, TexCoord).rgb;
    vec3 blur = texture(uBlur, TexCoord).rgb;
    vec3 persist = texture(uPersistence, TexCoord).rgb;

    // Phosphor decay: previous frame fades and adds current scene
    // Use exponential-style decay for more realistic trailing
    vec3 phosphor = persist * (1.0 - uDecay) + scene;

    // Composite glow: additive of blurred with softer strength
    vec3 glow = blur * 0.9;

    // Scanlines: lower frequency and slight contrast
    float scanFreq = 600.0; // tuned for typical vertical resolution
    float scan = sin(TexCoord.y * scanFreq) * 0.5 + 0.5;
    float scanline = mix(1.0, 1.0 - uScanlineIntensity, scan);

    // Noise: subtle
    float n = (rand(TexCoord * vec2(1920,1080)) - 0.5) * uNoiseAmount;

    vec3 color = (phosphor + glow) * uColor + n;
    color *= scanline;

    // slight gamma to emulate phosphor nonlinear brightness
    color = pow(color, vec3(0.9));

    FragColor = vec4(color, 1.0);
}
