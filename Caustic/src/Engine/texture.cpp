#include "texture.h"
#include "WalnutGraphics.h"
#include "utilities.h"
#include "../../vendor/stb_image/stb_image.h"
#include <stdexcept>
#include <iostream>

namespace veng {

Texture::Texture(WalnutGraphics* gfx)
 : m_Graphics(gfx)
{
}

Texture::~Texture()
{
 if (m_Sampler != VK_NULL_HANDLE) {
 vkDestroySampler(m_Graphics->m_Device, m_Sampler, nullptr);
 }
 if (m_ImageView != VK_NULL_HANDLE) {
 vkDestroyImageView(m_Graphics->m_Device, m_ImageView, nullptr);
 }
 if (m_Image != VK_NULL_HANDLE) {
 vkDestroyImage(m_Graphics->m_Device, m_Image, nullptr);
 }
 if (m_ImageMemory != VK_NULL_HANDLE) {
 vkFreeMemory(m_Graphics->m_Device, m_ImageMemory, nullptr);
 }
}

void Texture::LoadFromFile(const std::string& filename)
{
 stbi_set_flip_vertically_on_load(1);
 int width, height, channels;
 unsigned char* pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
 if (!pixels) {
 std::cerr << "Failed to load texture: " << filename << "\n";
 return;
 }
 CreateImageAndUpload(pixels, width, height,4);
 stbi_image_free(pixels);
 CreateImageView();
 CreateSampler();
}

void Texture::WriteDescriptor(VkDevice device, VkDescriptorSet dstSet, uint32_t binding) const
{
 // Defensive checks
 if (device == VK_NULL_HANDLE) return;
 if (dstSet == VK_NULL_HANDLE) return;
 if (m_ImageView == VK_NULL_HANDLE || m_Sampler == VK_NULL_HANDLE) return;

 VkDescriptorImageInfo imageInfo{};
 imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
 imageInfo.imageView = m_ImageView;
 imageInfo.sampler = m_Sampler;

 VkWriteDescriptorSet write{};
 write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
 write.dstSet = dstSet;
 write.dstBinding = binding;
 write.dstArrayElement =0;
 write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
 write.descriptorCount =1;
 write.pImageInfo = &imageInfo;

 vkUpdateDescriptorSets(device,1, &write,0, nullptr);
}

void Texture::CreateImageAndUpload(const unsigned char* pixels, int width, int height, int channels)
{
 // Create staging buffer and upload
 VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height *4;
 BufferHandle staging = m_Graphics->CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
 void* data;
 vkMapMemory(m_Graphics->m_Device, staging.memory,0, imageSize,0, &data);
 memcpy(data, pixels, static_cast<size_t>(imageSize));
 vkUnmapMemory(m_Graphics->m_Device, staging.memory);

 // Determine mip levels
 m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) +1;

 VkImageCreateInfo imageInfo{};
 imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
 imageInfo.imageType = VK_IMAGE_TYPE_2D;
 imageInfo.extent.width = static_cast<uint32_t>(width);
 imageInfo.extent.height = static_cast<uint32_t>(height);
 imageInfo.extent.depth =1;
 imageInfo.mipLevels = m_MipLevels;
 imageInfo.arrayLayers =1;
 imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
 imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
 imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
 imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
 imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
 imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

 if (vkCreateImage(m_Graphics->m_Device, &imageInfo, nullptr, &m_Image) != VK_SUCCESS) {
 // cleanup staging explicitly
 vkDestroyBuffer(m_Graphics->m_Device, staging.buffer, nullptr);
 vkFreeMemory(m_Graphics->m_Device, staging.memory, nullptr);
 throw std::runtime_error("Failed to create image");
 }

 VkMemoryRequirements memReq;
 vkGetImageMemoryRequirements(m_Graphics->m_Device, m_Image, &memReq);
 VkMemoryAllocateInfo alloc{};
 alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
 alloc.allocationSize = memReq.size;
 alloc.memoryTypeIndex = m_Graphics->FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

 if (vkAllocateMemory(m_Graphics->m_Device, &alloc, nullptr, &m_ImageMemory) != VK_SUCCESS) {
 // cleanup staging explicitly
 vkDestroyBuffer(m_Graphics->m_Device, staging.buffer, nullptr);
 vkFreeMemory(m_Graphics->m_Device, staging.memory, nullptr);
 throw std::runtime_error("Failed to allocate image memory");
 }

 vkBindImageMemory(m_Graphics->m_Device, m_Image, m_ImageMemory,0);

 // Transition and copy
 VkCommandBuffer cmd = m_Graphics->BeginTransientCommandBuffer();

 VkImageMemoryBarrier barrier{};
 barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
 barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
 barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
 barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
 barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
 barrier.image = m_Image;
 barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
 barrier.subresourceRange.baseMipLevel =0;
 barrier.subresourceRange.levelCount =1; // only base level
 barrier.subresourceRange.baseArrayLayer =0;
 barrier.subresourceRange.layerCount =1;
 barrier.srcAccessMask =0;
 barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

 vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,0,0, nullptr,0, nullptr,1, &barrier);

 VkBufferImageCopy region{};
 region.bufferOffset =0;
 region.bufferRowLength =0;
 region.bufferImageHeight =0;
 region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
 region.imageSubresource.mipLevel =0;
 region.imageSubresource.baseArrayLayer =0;
 region.imageSubresource.layerCount =1;
 region.imageOffset = {0,0,0};
 region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height),1 };

 vkCmdCopyBufferToImage(cmd, staging.buffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1, &region);

 // Do NOT transition all mips to SHADER_READ here - leave base level in TRANSFER_DST for mip generation
 m_Graphics->EndTransientCommandBuffer(cmd);

 // cleanup staging buffer explicitly
 vkDestroyBuffer(m_Graphics->m_Device, staging.buffer, nullptr);
 vkFreeMemory(m_Graphics->m_Device, staging.memory, nullptr);

 // Generate mipmaps using GPU
 GenerateMipmaps(width, height);
}

void Texture::CreateImageView()
{
 VkImageViewCreateInfo view{};
 view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
 view.image = m_Image;
 view.viewType = VK_IMAGE_VIEW_TYPE_2D;
 view.format = VK_FORMAT_R8G8B8A8_UNORM;
 view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
 view.subresourceRange.baseMipLevel =0;
 view.subresourceRange.levelCount = m_MipLevels;
 view.subresourceRange.baseArrayLayer =0;
 view.subresourceRange.layerCount =1;

 if (vkCreateImageView(m_Graphics->m_Device, &view, nullptr, &m_ImageView) != VK_SUCCESS) {
 throw std::runtime_error("Failed to create image view");
 }
}

void Texture::CreateSampler()
{
 VkSamplerCreateInfo samplerInfo{};
 samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
 samplerInfo.magFilter = VK_FILTER_LINEAR;
 samplerInfo.minFilter = VK_FILTER_LINEAR;
 samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
 samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
 samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
 samplerInfo.anisotropyEnable = VK_TRUE;
 // Query device properties for max anisotropy
 VkPhysicalDeviceProperties props{};
 vkGetPhysicalDeviceProperties(m_Graphics->m_PhysicalDevice, &props);
 samplerInfo.maxAnisotropy = props.limits.maxSamplerAnisotropy >1.0f ? props.limits.maxSamplerAnisotropy :1.0f;
 samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
 samplerInfo.unnormalizedCoordinates = VK_FALSE;
 samplerInfo.compareEnable = VK_FALSE;
 samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
 samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
 samplerInfo.minLod =0.0f;
 samplerInfo.maxLod = static_cast<float>(m_MipLevels);
 samplerInfo.mipLodBias =0.0f;

 if (vkCreateSampler(m_Graphics->m_Device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
 throw std::runtime_error("Failed to create texture sampler");
 }
}

void Texture::GenerateMipmaps(int width, int height)
{
 VkFormatProperties formatProps;
 vkGetPhysicalDeviceFormatProperties(m_Graphics->m_PhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
 if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
 throw std::runtime_error("Device does not support linear blitting for mipmap generation");
 }

 VkCommandBuffer cmd = m_Graphics->BeginTransientCommandBuffer();

 VkImageMemoryBarrier barrier{};
 barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
 barrier.image = m_Image;
 barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
 barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
 barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
 barrier.subresourceRange.baseArrayLayer =0;
 barrier.subresourceRange.layerCount =1;
 barrier.subresourceRange.levelCount =1;

 int32_t mipWidth = width;
 int32_t mipHeight = height;

 for (uint32_t i =1; i < m_MipLevels; ++i) {
 // Transition previous level (i-1) from TRANSFER_DST_OPTIMAL to TRANSFER_SRC_OPTIMAL
 barrier.subresourceRange.baseMipLevel = i -1;
 barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
 barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
 barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
 barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

 vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,0,0, nullptr,0, nullptr,1, &barrier);

 // Transition current level (i) from UNDEFINED to TRANSFER_DST_OPTIMAL
 barrier.subresourceRange.baseMipLevel = i;
 barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
 barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
 barrier.srcAccessMask =0;
 barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

 vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,0,0, nullptr,0, nullptr,1, &barrier);

 VkImageBlit blit{};
 blit.srcOffsets[0] = {0,0,0};
 blit.srcOffsets[1] = { mipWidth, mipHeight,1 };
 blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
 blit.srcSubresource.mipLevel = i -1;
 blit.srcSubresource.baseArrayLayer =0;
 blit.srcSubresource.layerCount =1;

 blit.dstOffsets[0] = {0,0,0};
 blit.dstOffsets[1] = { mipWidth >1 ? mipWidth/2 :1, mipHeight >1 ? mipHeight/2 :1,1 };
 blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
 blit.dstSubresource.mipLevel = i;
 blit.dstSubresource.baseArrayLayer =0;
 blit.dstSubresource.layerCount =1;

 vkCmdBlitImage(cmd, m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1, &blit, VK_FILTER_LINEAR);

 // Transition previous level (i-1) from TRANSFER_SRC_OPTIMAL to SHADER_READ_ONLY_OPTIMAL
 barrier.subresourceRange.baseMipLevel = i -1;
 barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
 barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
 barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
 barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

 vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0, nullptr,0, nullptr,1, &barrier);

 if (mipWidth >1) mipWidth /=2;
 if (mipHeight >1) mipHeight /=2;
 }

 // Transition last mip level to SHADER_READ_ONLY_OPTIMAL
 barrier.subresourceRange.baseMipLevel = m_MipLevels -1;
 barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
 barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
 barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
 barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

 vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0, nullptr,0, nullptr,1, &barrier);

 m_Graphics->EndTransientCommandBuffer(cmd);
}

} // namespace veng
