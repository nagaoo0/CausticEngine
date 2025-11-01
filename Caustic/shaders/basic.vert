#version 450
#include "common.glsl"

layout(location = 0) in vec3 input_position;
layout(location = 1) in vec3 input_color;
layout(location = 2) in vec2 input_texcoord;

layout(location = 0) out vec4 vertex_color;
layout(location = 1) out vec2 v_TexCoord;

layout(push_constant) uniform Model {
    mat4 transformation;
} model;

void main() {
    // Restore proper transformations now that triangle is visible
    gl_Position = camera.projection * camera.view * model.transformation * vec4(input_position, 1.0);
    vertex_color = vec4(input_color, 1.0);
    v_TexCoord = input_texcoord;
}