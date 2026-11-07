#version 300 es
precision highp float;
precision highp int;
in vec2 uv;
out vec4 result;
uniform sampler2D source;
vec3 encoded(vec3 c) {
    return mix(12.92*c,1.055*pow(max(c,vec3(0.0)),vec3(1.0/2.4))-.055,greaterThan(c,vec3(.0031308)));
}
void main() {
    vec4 pixel = texture(source,uv);
    float alpha = clamp(pixel.a,0.0,1.0);
    // Source blending happens in linear light. Qt expects encoded-space
    // premultiplied RGB when compositing over the translucent desktop.
    result = vec4(encoded(pixel.rgb/max(alpha,.00001))*alpha,alpha);
}
