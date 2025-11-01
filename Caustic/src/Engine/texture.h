#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>

namespace veng {
class WalnutGraphics;

class Texture {
public:
 Texture(WalnutGraphics* gfx);
 ~Texture();

 // Load image from disk, create image, view, sampler and mipmaps
 void LoadFromFile(const std::string& filename);

 // Bind texture to a descriptor set (write descriptor)
 void WriteDescriptor(VkDevice device, VkDescriptorSet dstSet, uint32_t binding) const;

 // Accessors
 VkImageView GetImageView() const { return m_ImageView; }
 VkSampler GetSampler() const { return m_Sampler; }
 VkImage GetImage() const { return m_Image; }

private:
 WalnutGraphics* m_Graphics = nullptr;
 VkImage m_Image = VK_NULL_HANDLE;
 VkDeviceMemory m_ImageMemory = VK_NULL_HANDLE;
 VkImageView m_ImageView = VK_NULL_HANDLE;
 VkSampler m_Sampler = VK_NULL_HANDLE;
 uint32_t m_MipLevels =1;

 // helper methods
 void CreateImageAndUpload(const unsigned char* pixels, int width, int height, int channels);
 void CreateImageView();
 void CreateSampler();
 void GenerateMipmaps(int width, int height);
};

} // namespace veng
