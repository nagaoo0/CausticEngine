#pragma once

#include <vulkan/vulkan.h>

namespace veng {

struct Vertex {
  glm::vec3 position;
  glm::vec3 color;

  static VkVertexInputBindingDescription GetBindingDescription() {
    VkVertexInputBindingDescription description = {};
    description.binding = 0;
    description.stride = sizeof(Vertex);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return description;
  }

  static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> descriptions = {};

    descriptions[0].binding = 0;
    descriptions[0].location = 0;
    descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    descriptions[0].offset = offsetof(Vertex, position);

    descriptions[1].binding = 0;
    descriptions[1].location = 1;
    descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    descriptions[1].offset = offsetof(Vertex, color);

    return descriptions;
  }
};

}  // namespace veng
