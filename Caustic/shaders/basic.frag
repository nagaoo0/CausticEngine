#version 450
#include "common.glsl"

layout(location = 0) in vec4 vertex_color;
layout(location = 1) in vec2 v_TexCoord;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform sampler2D u_Texture;

void main() {
    vec4 tex = texture(u_Texture, v_TexCoord);
    // If texture is fully transparent/invalid, fallback to vertex color
    if (tex.a == 0.0) {
        out_color = vertex_color;
    } else {
        out_color = tex * vertex_color;
    }
}