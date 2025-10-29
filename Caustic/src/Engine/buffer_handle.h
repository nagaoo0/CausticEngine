#pragma once

#include <vulkan/vulkan.h>

namespace veng {

struct BufferHandle {
  VkBuffer buffer;
  VkDeviceMemory memory;
};

}  // namespace veng
