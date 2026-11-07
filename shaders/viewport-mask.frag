#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};
layout(binding = 1) uniform sampler2D source;
void main() {
    // right-tiles.js: linear-gradient(177deg, transparent 58px, black 78px,
    // black calc(100% - 82px), transparent calc(100% - 38px)).
    float distance = qt_TexCoord0.x * 512.0 * 0.052335956
                   + qt_TexCoord0.y * 805.0 * 0.998629535;
    float length = 512.0 * 0.052335956 + 805.0 * 0.998629535;
    float alpha = clamp((distance - 58.0) / 20.0, 0.0, 1.0)
                * clamp((length - 38.0 - distance) / 44.0, 0.0, 1.0);
    fragColor = texture(source, qt_TexCoord0) * (qt_Opacity * alpha);
}
