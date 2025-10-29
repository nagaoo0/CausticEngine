#include "WalnutGraphics.h"
#include "utilities.h"
#include "uniform_transformations.h"
#include "vertex.h"
#include "Walnut/Application.h"

#include <iostream>
#include <fstream>
#include <set>
#include <cstring>
#include <chrono>
#include <windows.h>
#include <filesystem>

// Small debug helper to print 4x4 matrices (glm is column-major: m[col][row])
static void LogMat4(const glm::mat4& m, const char* name) {
    std::cout << name << ":\n";
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            std::cout << m[col][row] << (col == 3 ? "" : " ");
        }
        std::cout << "\n";
    }
}

namespace veng {

WalnutGraphics::WalnutGraphics() {
}

WalnutGraphics::~WalnutGraphics() {
    Shutdown();
}

bool WalnutGraphics::Initialize() {
    if (m_Initialized) {
        return true;
    }

    // Get Vulkan objects from Walnut
    auto& app = Walnut::Application::Get();
    m_Instance = app.GetInstance();
    m_PhysicalDevice = app.GetPhysicalDevice();
    m_Device = app.GetDevice();
    
    // Find graphics queue family
    uint32_t queueFamilyIndex = FindGraphicsQueueFamily();
    vkGetDeviceQueue(m_Device, queueFamilyIndex, 0, &m_GraphicsQueue);

    // Create our rendering resources
    try {
        CreateRenderTargets();
        CreateRenderPass();
        CreateDescriptorSetLayout();
        CreateGraphicsPipeline();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffer();
    CreateSignals();
    // Create uniform buffers before allocating/updating descriptor sets so
    // the descriptor can point to a valid buffer object.
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSet();
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize WalnutGraphics: " << e.what() << std::endl;
        return false;
    }

    m_Initialized = true;
    // Record start time for debug overlay (no-cull period)
    m_StartTime = std::chrono::steady_clock::now();
    return true;
}

void WalnutGraphics::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    // Ensure all operations are complete before cleanup
    if (m_Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_Device);
    }

    // Cleanup in reverse order
    if (m_UniformBuffer.buffer != VK_NULL_HANDLE) {
        DestroyBuffer(m_UniformBuffer);
    }

    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    if (m_RenderFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_Device, m_RenderFence, nullptr);
        m_RenderFence = VK_NULL_HANDLE;
    }

    if (m_RenderFinishedSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_Device, m_RenderFinishedSemaphore, nullptr);
        m_RenderFinishedSemaphore = VK_NULL_HANDLE;
    }

    if (m_CommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;
    }

    if (m_Framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_Device, m_Framebuffer, nullptr);
        m_Framebuffer = VK_NULL_HANDLE;
    }

    if (m_Pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }
    if (m_PipelineNoCull != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_Device, m_PipelineNoCull, nullptr);
        m_PipelineNoCull = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }

    // Cleanup render targets
    if (m_DepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
        m_DepthImageView = VK_NULL_HANDLE;
    }
    if (m_DepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_Device, m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
    }
    if (m_DepthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);
        m_DepthImageMemory = VK_NULL_HANDLE;
    }

    if (m_ColorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_Device, m_ColorImageView, nullptr);
        m_ColorImageView = VK_NULL_HANDLE;
    }
    if (m_ColorImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_Device, m_ColorImage, nullptr);
        m_ColorImage = VK_NULL_HANDLE;
    }
    if (m_ColorImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_Device, m_ColorImageMemory, nullptr);
        m_ColorImageMemory = VK_NULL_HANDLE;
    }

    m_RenderedImage.reset();
    m_Initialized = false;
}

bool WalnutGraphics::BeginFrame() {
    if (!m_Initialized) {
        return false;
    }

    // Wait for previous frame
    vkWaitForFences(m_Device, 1, &m_RenderFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_Device, 1, &m_RenderFence);

    BeginCommands();
    // Increment frame count for our limited logging
    ++m_FrameCount;
    return true;
}

void WalnutGraphics::EndFrame() {
    if (!m_Initialized) {
        return;
    }

    EndCommands();

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_RenderFinishedSemaphore;

    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_RenderFence);
    
    // Wait for rendering to complete and copy image data to Walnut::Image
    vkWaitForFences(m_Device, 1, &m_RenderFence, VK_TRUE, UINT64_MAX);
    
    // Copy the rendered Vulkan image to Walnut::Image for display
    if (m_RenderedImage && m_ColorImage != VK_NULL_HANDLE) {
        // Create a staging buffer to read the image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        VkDeviceSize imageSize = m_RenderWidth * m_RenderHeight * 4; // RGBA
        
        // Create staging buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &stagingBuffer) == VK_SUCCESS) {
            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_Device, stagingBuffer, &memRequirements);
            
            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            
            if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &stagingBufferMemory) == VK_SUCCESS) {
                vkBindBufferMemory(m_Device, stagingBuffer, stagingBufferMemory, 0);
                
                // Copy image to staging buffer
                VkCommandBuffer copyCmd = BeginTransientCommandBuffer();
                
                // Transition image layout for copying
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = m_ColorImage;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                
                vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
                    VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                
                // Copy image to buffer
                VkBufferImageCopy region{};
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = 1;
                region.imageOffset = {0, 0, 0};
                region.imageExtent = {m_RenderWidth, m_RenderHeight, 1};
                
                vkCmdCopyImageToBuffer(copyCmd, m_ColorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
                    stagingBuffer, 1, &region);
                
                // Transition back
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                
                vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, 
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                
                EndTransientCommandBuffer(copyCmd);
                
                // Map and copy data
                void* data;
                if (vkMapMemory(m_Device, stagingBufferMemory, 0, imageSize, 0, &data) == VK_SUCCESS) {
                    // Quick debug: scan the first N pixels and report how many are non-zero
                    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
                    size_t pixelCount = static_cast<size_t>(m_RenderWidth) * static_cast<size_t>(m_RenderHeight);
                    size_t samples = std::min<size_t>(pixelCount, 1024);
                    size_t nonZero = 0;
                    for (size_t i = 0; i < samples; ++i) {
                        const uint8_t* p = bytes + i * 4;
                        if (p[0] != 0 || p[1] != 0 || p[2] != 0 || p[3] != 0) {
                            ++nonZero;
                        }
                    }

                    m_RenderedImage->SetData(data);
                    vkUnmapMemory(m_Device, stagingBufferMemory);
                }
                
                // Cleanup
                vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
                vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
            }
        }
    }
}

// Placeholder implementations - you'll need to implement these based on your original graphics.cpp
void WalnutGraphics::CreateRenderTargets() {
    // Create color image
    VkImageCreateInfo colorImageInfo{};
    colorImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorImageInfo.imageType = VK_IMAGE_TYPE_2D;
    colorImageInfo.extent.width = m_RenderWidth;
    colorImageInfo.extent.height = m_RenderHeight;
    colorImageInfo.extent.depth = 1;
    colorImageInfo.mipLevels = 1;
    colorImageInfo.arrayLayers = 1;
    colorImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    colorImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_Device, &colorImageInfo, nullptr, &m_ColorImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create color image!");
    }

    // Allocate memory for color image
    VkMemoryRequirements colorMemRequirements;
    vkGetImageMemoryRequirements(m_Device, m_ColorImage, &colorMemRequirements);

    VkMemoryAllocateInfo colorAllocInfo{};
    colorAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    colorAllocInfo.allocationSize = colorMemRequirements.size;
    colorAllocInfo.memoryTypeIndex = FindMemoryType(colorMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_Device, &colorAllocInfo, nullptr, &m_ColorImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate color image memory!");
    }

    vkBindImageMemory(m_Device, m_ColorImage, m_ColorImageMemory, 0);

    // Create color image view
    VkImageViewCreateInfo colorViewInfo{};
    colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorViewInfo.image = m_ColorImage;
    colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.baseMipLevel = 0;
    colorViewInfo.subresourceRange.levelCount = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_Device, &colorViewInfo, nullptr, &m_ColorImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create color image view!");
    }

    // Create depth image (similar process)
    VkImageCreateInfo depthImageInfo = colorImageInfo;
    depthImageInfo.format = VK_FORMAT_D32_SFLOAT;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (vkCreateImage(m_Device, &depthImageInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image!");
    }

    VkMemoryRequirements depthMemRequirements;
    vkGetImageMemoryRequirements(m_Device, m_DepthImage, &depthMemRequirements);

    VkMemoryAllocateInfo depthAllocInfo{};
    depthAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    depthAllocInfo.allocationSize = depthMemRequirements.size;
    depthAllocInfo.memoryTypeIndex = FindMemoryType(depthMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_Device, &depthAllocInfo, nullptr, &m_DepthImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate depth image memory!");
    }

    vkBindImageMemory(m_Device, m_DepthImage, m_DepthImageMemory, 0);

    // Create depth image view
    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = m_DepthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_Device, &depthViewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view!");
    }

    // Create Walnut::Image wrapper for the color attachment
    m_RenderedImage = std::make_shared<Walnut::Image>(m_RenderWidth, m_RenderHeight, Walnut::ImageFormat::RGBA);
}

void WalnutGraphics::CreateRenderPass() {
    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // We intentionally omit a depth attachment here to temporarily disable
    // depth testing during debugging. The pipeline's depth/stencil state
    // already has depth tests disabled, but removing the depth attachment
    // ensures the render pass won't perform depth operations.

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 1> attachments = {colorAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void WalnutGraphics::CreateGraphicsPipeline() {
    // For now, create a simple pipeline without shaders to test the infrastructure
    // TODO: Load and compile shaders
    
    // Vertex input
    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) m_RenderWidth;
    viewport.height = (float) m_RenderHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_RenderWidth, m_RenderHeight};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;  // Re-enable back face culling
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth and stencil testing
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;  // DISABLE depth testing for debugging
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Push constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // Load shaders
    auto vertShaderCode = ReadFile("shaders/basic.vert.spv");
    auto fragShaderCode = ReadFile("shaders/basic.frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = m_RenderPass;
    pipelineInfo.subpass = 0;

    // Create the regular pipeline (back-face culling)
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineInfo.pRasterizationState = &rasterizer;
    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    // Create a second pipeline variant with culling disabled for early-debug overlay
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    pipelineInfo.pRasterizationState = &rasterizer;
    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_PipelineNoCull) != VK_SUCCESS) {
        // If creating a no-cull pipeline fails, keep running without it
        m_PipelineNoCull = VK_NULL_HANDLE;
        std::cout << "Warning: failed to create no-cull debug pipeline; continuing without it." << std::endl;
    }

    // Clean up shader modules
    vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
    
    std::cout << "WalnutGraphics pipeline layout created successfully!" << std::endl;
    std::cout << "Graphics pipeline with shaders created successfully!" << std::endl;
}

void WalnutGraphics::CreateFramebuffers() {
    std::array<VkImageView, 1> attachments = {
        m_ColorImageView
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_RenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = m_RenderWidth;
    framebufferInfo.height = m_RenderHeight;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_Framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create framebuffer!");
    }
}

void WalnutGraphics::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = FindGraphicsQueueFamily();

    if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

void WalnutGraphics::CreateCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(m_Device, &allocInfo, &m_CommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }
}

void WalnutGraphics::CreateSignals() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(m_Device, &fenceInfo, nullptr, &m_RenderFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create synchronization objects");
    }
}

void WalnutGraphics::CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

void WalnutGraphics::CreateDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void WalnutGraphics::CreateDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorSetLayout;

    if (vkAllocateDescriptorSets(m_Device, &allocInfo, &m_DescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_UniformBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformTransformations);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_DescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
}

void WalnutGraphics::CreateUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformTransformations);

    m_UniformBuffer = CreateBuffer(bufferSize, 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkMapMemory(m_Device, m_UniformBuffer.memory, 0, bufferSize, 0, &m_UniformBufferLocation) != VK_SUCCESS) {
        m_UniformBufferLocation = nullptr;
        std::cout << "ERROR: Failed to map uniform buffer memory" << std::endl;
    }
}

void WalnutGraphics::BeginCommands() {
    vkResetCommandBuffer(m_CommandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_Framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {m_RenderWidth, m_RenderHeight};

    std::array<VkClearValue, 1> clearValues{};
    // Clear to transparent black so only triangle pixels are visible in the final image
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    VkViewport viewport = GetViewport();
    vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

    VkRect2D scissor = GetScissor();
    vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);

    // Note: pipeline will be bound per-draw so a debug no-cull pipeline
    // can be selected for the first few seconds. Do not bind a global
    // pipeline here.
}

void WalnutGraphics::EndCommands() {
    vkCmdEndRenderPass(m_CommandBuffer);
    vkEndCommandBuffer(m_CommandBuffer);
}

VkViewport WalnutGraphics::GetViewport() {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_RenderWidth);
    viewport.height = static_cast<float>(m_RenderHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    return viewport;
}

VkRect2D WalnutGraphics::GetScissor() {
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_RenderWidth, m_RenderHeight};
    return scissor;
}

// Stub implementations - you'll need to implement these based on your original code
void WalnutGraphics::SetModelMatrix(glm::mat4 model) {
    // Log model matrix only for first few frames to avoid spam
    if (m_FrameCount <= m_LogFramesLimit) {
        LogMat4(model, "Model matrix");
    }
    // Store the current model matrix; push constants must be issued while
    // recording the command buffer (inside BeginCommands). We'll push the
    // stored matrix immediately before drawing in Render* functions.
    m_CurrentModel = model;
}

void WalnutGraphics::SetViewProjection(glm::mat4 view, glm::mat4 projection) {
    // Log view/projection matrices only for first few frames or once on init
    if (!m_LoggedInitialVP || m_FrameCount <= m_LogFramesLimit) {
        LogMat4(view, "View matrix");
        LogMat4(projection, "Projection matrix");
        m_LoggedInitialVP = true;
    }

    UniformTransformations transformations{view, projection};
    if (!m_UniformBufferLocation) {
        std::cout << "ERROR: Uniform buffer memory not mapped (m_UniformBufferLocation == nullptr)\n";
        return;
    }

    std::memcpy(m_UniformBufferLocation, &transformations, sizeof(UniformTransformations));
}

void WalnutGraphics::RenderBuffer(BufferHandle handle, std::uint32_t vertex_count) {
    VkDeviceSize offset = 0;
    vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
    // Ensure push constants are set while recording
    vkCmdPushConstants(m_CommandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &m_CurrentModel);
    vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, &handle.buffer, &offset);
    vkCmdDraw(m_CommandBuffer, vertex_count, 1, 0, 0);
}

void WalnutGraphics::RenderIndexedBuffer(BufferHandle vertex_buffer, BufferHandle index_buffer, std::uint32_t count) {
    // Skip rendering if pipeline is not created yet
    if (m_Pipeline == VK_NULL_HANDLE) {
        std::cout << "WARNING: Skipping render - pipeline not ready" << std::endl;
        return;
    }
    
    // Debug: Verify buffers are valid
    if (vertex_buffer.buffer == VK_NULL_HANDLE || index_buffer.buffer == VK_NULL_HANDLE) {
        std::cout << "WARNING: Invalid buffers - vertex:" << (vertex_buffer.buffer != VK_NULL_HANDLE) 
                  << " index:" << (index_buffer.buffer != VK_NULL_HANDLE) << std::endl;
        return;
    }
    
    // Triangle rendering is working - debug output removed
    
    VkDeviceSize offset = 0;

    // choose which pipeline to bind: during the initial debug window, prefer the no-cull pipeline
    VkPipeline pipelineToBind = m_Pipeline;
    if (m_PipelineNoCull != VK_NULL_HANDLE) {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - m_StartTime).count();
        if (elapsed < m_DebugNoCullDuration) {
            pipelineToBind = m_PipelineNoCull;
        }
    }

    if (pipelineToBind != VK_NULL_HANDLE) {
        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineToBind);
    }

    vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
    // Push the current model matrix as a push constant now that we're recording
    vkCmdPushConstants(m_CommandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &m_CurrentModel);

    // Debug log when issuing the draw (limited frames)
    if (m_FrameCount <= m_LogFramesLimit) {
        std::cout << "DEBUG: Recording drawIndexed count=" << count << " frame=" << m_FrameCount << "\n";
    }

    vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, &vertex_buffer.buffer, &offset);
    vkCmdBindIndexBuffer(m_CommandBuffer, index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(m_CommandBuffer, count, 1, 0, 0, 0);

    // Also issue a non-indexed draw using gl_VertexIndex in the temporary
    // debug vertex shader. This helps when index/vertex buffers might be
    // misconfigured — the shader will emit a triangle regardless.
    if (m_FrameCount <= m_LogFramesLimit) {
        vkCmdDraw(m_CommandBuffer, 3, 1, 0, 0);
    }

    // Debug rectangle disabled: removed to avoid obscuring test geometry.
    // If you need the magenta debug rect for diagnostics, re-enable here.
}

BufferHandle WalnutGraphics::CreateVertexBuffer(gsl::span<Vertex> vertices) {
    VkDeviceSize size = sizeof(Vertex) * vertices.size();
    BufferHandle staging_handle = CreateBuffer(
        size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(m_Device, staging_handle.memory, 0, size, 0, &data);
    std::memcpy(data, vertices.data(), size);
    vkUnmapMemory(m_Device, staging_handle.memory);

    BufferHandle gpu_handle = CreateBuffer(
        size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBuffer transient_commands = BeginTransientCommandBuffer();

    VkBufferCopy copy_info = {};
    copy_info.srcOffset = 0;
    copy_info.dstOffset = 0;
    copy_info.size = size;
    vkCmdCopyBuffer(transient_commands, staging_handle.buffer, gpu_handle.buffer, 1, &copy_info);

    EndTransientCommandBuffer(transient_commands);

    DestroyBuffer(staging_handle);

    return gpu_handle;
}

BufferHandle WalnutGraphics::CreateIndexBuffer(gsl::span<std::uint32_t> indices) {
    VkDeviceSize size = sizeof(std::uint32_t) * indices.size();

    BufferHandle staging_handle = CreateBuffer(
        size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(m_Device, staging_handle.memory, 0, size, 0, &data);
    std::memcpy(data, indices.data(), size);
    vkUnmapMemory(m_Device, staging_handle.memory);

    BufferHandle gpu_handle = CreateBuffer(
        size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBuffer transient_commands = BeginTransientCommandBuffer();

    VkBufferCopy copy_info = {};
    copy_info.srcOffset = 0;
    copy_info.dstOffset = 0;
    copy_info.size = size;
    vkCmdCopyBuffer(transient_commands, staging_handle.buffer, gpu_handle.buffer, 1, &copy_info);

    EndTransientCommandBuffer(transient_commands);

    DestroyBuffer(staging_handle);

    return gpu_handle;
}

void WalnutGraphics::DestroyBuffer(BufferHandle handle) {
    if (handle.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_Device, handle.buffer, nullptr);
    }
    if (handle.memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_Device, handle.memory, nullptr);
    }
}

// Helper functions
std::uint32_t WalnutGraphics::FindMemoryType(std::uint32_t type_bits_filter, VkMemoryPropertyFlags required_properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((type_bits_filter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & required_properties) == required_properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

BufferHandle WalnutGraphics::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    BufferHandle handle = {};

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(m_Device, &buffer_info, nullptr, &handle.buffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(m_Device, handle.buffer, &memory_requirements);

    std::uint32_t chosen_memory_type = FindMemoryType(memory_requirements.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocation_info = {};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = chosen_memory_type;

    VkResult allocation_result = vkAllocateMemory(m_Device, &allocation_info, nullptr, &handle.memory);

    if (allocation_result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(m_Device, handle.buffer, handle.memory, 0);

    return handle;
}

VkCommandBuffer WalnutGraphics::BeginTransientCommandBuffer() {
    VkCommandBufferAllocateInfo allocation_info = {};
    allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation_info.commandPool = m_CommandPool;
    allocation_info.commandBufferCount = 1;

    VkCommandBuffer buffer;
    vkAllocateCommandBuffers(m_Device, &allocation_info, &buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(buffer, &begin_info);

    return buffer;
}

void WalnutGraphics::EndTransientCommandBuffer(VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(m_GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);

    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &command_buffer);
}

uint32_t WalnutGraphics::FindGraphicsQueueFamily() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find graphics queue family!");
}

std::vector<char> WalnutGraphics::ReadFile(const std::string& filename) {
    // Try opening the file as given first (relative to current working dir)
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    // If that fails, try relative to the executable directory (common when running
    // the app with a different working directory).
    if (!file.is_open()) {
        char exePath[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            std::filesystem::path p(exePath);
            std::filesystem::path alt = p.parent_path() / filename;
            file.open(alt.string(), std::ios::ate | std::ios::binary);
        }
    }

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule WalnutGraphics::CreateShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

}  // namespace veng