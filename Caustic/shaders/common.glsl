// #extension GL_KHR_vulkan_glsl: enable
// Extension commented out: glslangValidator on some systems warns about this extension.

layout(set = 0, binding = 0) uniform UniformTransformations {
    mat4 view;
    mat4 projection;
} camera;