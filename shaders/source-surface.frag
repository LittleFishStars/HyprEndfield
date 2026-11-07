#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 tint;
    float additive;
    float alphaOnly;
};
layout(binding = 1) uniform sampler2D source;
void main() {
    vec4 pixel = texture(source, qt_TexCoord0);
    // Desktop Report artwork is tinted white using its original alpha.
    vec3 ink = mix(pixel.rgb, vec3(pixel.a), alphaOnly) * tint.rgb;
    // QSG's premultiplied One/OneMinusSrcAlpha blend also represents the
    // source additive Light pass: premultiplied RGB with zero output alpha.
    fragColor = vec4(ink, pixel.a * (1.0 - additive)) * tint.a * qt_Opacity;
}
