#version 300 es
precision highp float;
precision highp int;
layout(location=0) in vec3 position;
layout(location=1) in vec2 textureUV;
uniform mat4 projection;
out vec2 uv;
void main() {
    vec4 p = projection * vec4(position, 1.0);
    // Source camera returns homogeneous 1920x1080 screen coordinates. Keep W
    // for perspective-correct interpolation and every vertex's original Z.
    gl_Position = vec4(p.x / 960.0 - p.w, p.w - p.y / 540.0, 0.0, p.w);
    uv = textureUV;
}
