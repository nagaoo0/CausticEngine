#pragma once

#include "precomp.h"
#include <vulkan/vulkan.h>
#include <vector>
#include "vertex.h"
#include "buffer_handle.h"
#include "uniform_transformations.h"
#include <glm/glm.hpp>

namespace veng {

class WalnutGraphics final {
 public:
  WalnutGraphics();
  ~WalnutGraphics();

  static constexpr int MAX_FRAMES_IN_FLIGHT =2;

  bool Initialize();
  void Shutdown();

  bool BeginFrame();
  void SetModelMatrix(glm::mat4 model);
  void SetViewProjection(glm::mat4 view, glm::mat4 projection);
  void RenderBuffer(BufferHandle handle, std::uint32_t vertex_count);
  void RenderIndexedBuffer(BufferHandle vertex_buffer, BufferHandle index_buffer, std::uint32_t count);
  void EndFrame();

  BufferHandle CreateVertexBuffer(gsl::span<Vertex> vertices);
  BufferHandle CreateIndexBuffer(gsl::span<std::uint32_t> indices);
  void DestroyBuffer(BufferHandle handle);

  // Get the rendered image for display in ImGui
  std::shared_ptr<Walnut::Image> GetRenderedImage() const { return m_RenderedImage; }

  void Resize(uint32_t width, uint32_t height);

  // Set the clear color for the background
  void SetClearColor(const glm::vec4& color) { m_ClearColor = color; }

 private:
  void CreateRenderTargets();
  void CreateRenderPass();
  void CreateGraphicsPipeline();
  void CreateFramebuffers();
  void CreateCommandPool();
  void CreateCommandBuffers();
  void CreateSyncObjects();
  void CleanupRenderTargets();
  void RecreateRenderTargets();
  void CreateDescriptorSetLayout();
  void CreateDescriptorPool();
  void CreateDescriptorSet();
  void CreateUniformBuffers();

  void BeginCommands();
  void EndCommands();

  std::vector<char> ReadFile(const std::string& filename);
  VkShaderModule CreateShaderModule(const std::vector<char>& code);
  std::uint32_t FindMemoryType(std::uint32_t type_bits_filter, VkMemoryPropertyFlags required_properties);
  BufferHandle CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
  VkCommandBuffer BeginTransientCommandBuffer();
  void EndTransientCommandBuffer(VkCommandBuffer command_buffer);
  
  uint32_t FindGraphicsQueueFamily();

  VkViewport GetViewport();
  VkRect2D GetScissor();

  // Walnut/Vulkan objects (obtained from Walnut Application)
  VkInstance m_Instance = VK_NULL_HANDLE;
  VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
  VkDevice m_Device = VK_NULL_HANDLE;
  VkQueue m_GraphicsQueue = VK_NULL_HANDLE;

  // Our render targets and pipeline
  std::shared_ptr<Walnut::Image> m_RenderedImage;
  VkImage m_ColorImage = VK_NULL_HANDLE;
  VkDeviceMemory m_ColorImageMemory = VK_NULL_HANDLE;
  VkImageView m_ColorImageView = VK_NULL_HANDLE;
  
  VkImage m_DepthImage = VK_NULL_HANDLE;
  VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
  VkImageView m_DepthImageView = VK_NULL_HANDLE;

  VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
  VkRenderPass m_RenderPass = VK_NULL_HANDLE;
  VkPipeline m_Pipeline = VK_NULL_HANDLE;
  VkPipeline m_PipelineNoCull = VK_NULL_HANDLE; // debug pipeline with culling disabled
  VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;

  VkCommandPool m_CommandPool = VK_NULL_HANDLE;
  // Per-frame command buffers
  std::vector<VkCommandBuffer> m_CommandBuffers;

  // Per-frame synchronization objects
  std::vector<VkSemaphore> m_ImageAvailableSemaphores;
  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  std::vector<VkFence> m_InFlightFences;

  // Current frame index for frame-in-flight resources
  uint32_t m_CurrentFrame =0;

  VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
  BufferHandle m_UniformBuffer;
  void* m_UniformBufferLocation = nullptr;

  // Render state
  uint32_t m_RenderWidth =800;
  uint32_t m_RenderHeight =600;
  bool m_Initialized = false;
  glm::vec4 m_ClearColor = glm::vec4(0.0f,0.0f,0.0f,1.0f); // Default black
  // Debug helpers
  std::chrono::steady_clock::time_point m_StartTime;
  float m_DebugNoCullDuration =3.0f; // seconds to force no-cull on startup
  int m_FrameCount =0;
  int m_LogFramesLimit =5; // only log matrices for first N frames
  bool m_LoggedInitialVP = false;
  // Current model matrix stored so push constants can be applied when recording
  glm::mat4 m_CurrentModel = glm::mat4(1.0f);
};

} // namespace veng