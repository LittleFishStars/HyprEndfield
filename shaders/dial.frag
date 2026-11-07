#version 300 es
precision highp float;
precision highp int;
in vec2 uv;
out vec4 result;
uniform sampler2D mainTexture;
uniform sampler2D maskTexture;
uniform sampler2D dissolveTexture;
uniform vec4 color;
uniform vec4 parameters[20];
uniform float effectTime;
uniform float stencilAlpha;
uniform bool fx;
#define scalar(offset) parameters[(offset)/16][((offset)%16)/4]
vec2 sourceUV(vec2 coord, vec4 st, vec4 speed, vec4 rotation) {
    return (mat2(rotation.xy, rotation.zw) * (coord + speed.xy * mod(effectTime,1024.0) - .5) + .5) * st.xy + st.zw;
}
float sourceSmoothstep(float low, float high, float value) {
    return low == high ? step(low,value) : smoothstep(low,high,value);
}
void main() {
    if (!fx) {
        // HGRP_UI_Default program194, with the dial's original default material.
        vec4 vertexColor = color;
        vertexColor.a = roundEven(vertexColor.a * 255.0) / 255.0;
        vec4 pixel = texture(mainTexture,uv) * vertexColor;
        pixel.a *= stencilAlpha;
        result = vec4(pixel.rgb * pixel.a,pixel.a);
        return;
    }
    // HGRP_UI_UIMeshVfxEffect program21. Uniform offsets are imported from
    // fx-shader.json; textures and all authored mip levels stay unchanged.
    vec4 tint = parameters[5] * (scalar(0) != 0.0 ? vec4(1.0) : color);
    tint.rgb *= scalar(4);
    tint.a *= scalar(8);
    vec4 main = texture(mainTexture,sourceUV(uv,parameters[2],parameters[3],parameters[4]));
    vec4 mask = texture(maskTexture,sourceUV(uv,parameters[7],parameters[8],parameters[9]));
    vec4 base = tint * mix(main,vec4(1.0,1.0,1.0,main.r),scalar(16))
                     * mix(mask,vec4(1.0,1.0,1.0,mask.r),scalar(100));
    vec2 duv = sourceUV(uv,parameters[13],parameters[14],parameters[15]);
    float dissolve = texture(dissolveTexture,duv).r - scalar(172);
    if (scalar(196) != 0.0) {
        float direction = scalar(196) == 1.0 ? dot(normalize(parameters[18].xy),duv)
                                            : length(duv-parameters[19].xy);
        dissolve += direction * clamp(scalar(196),0.0,1.0);
    }
    if(dissolve < 0.0) discard;
    float edge = sourceSmoothstep(scalar(180),scalar(180)+scalar(184),1.0-clamp(dissolve,0.0,1.0));
    float ramp = sourceSmoothstep(1.0-scalar(188),1.0-scalar(188)+scalar(192),edge);
    vec3 ink = mix(base,mix(parameters[16],parameters[17],ramp)*parameters[5].x,edge).rgb;
    ink = clamp(ink+max(ink-vec3(scalar(24)),vec3(0.0))*scalar(28),vec3(0.0),vec3(500.0));
    result = vec4(ink,clamp(base.a*mix(1.0,1.0-edge,scalar(176)),0.0,1.0));
}
