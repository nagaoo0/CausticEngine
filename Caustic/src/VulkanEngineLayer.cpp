#include "VulkanEngineLayer.h"
#include "Walnut/UI/UI.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

// Include the LogMat4 function from WalnutGraphics.cpp
extern void LogMat4(const glm::mat4& m, const char* name);

VulkanEngineLayer::VulkanEngineLayer()
{
}

VulkanEngineLayer::~VulkanEngineLayer()
{
 try {
 CleanupEngine();
 } catch (...) {
 // Ignore cleanup errors in destructor to prevent exceptions
 // during application shutdown
 }
}

void VulkanEngineLayer::OnAttach()
{
 InitializeEngine();
}

void VulkanEngineLayer::OnDetach()
{
 try {
 CleanupEngine();
 } catch (const std::exception& e) {
 std::cout << "Warning during OnDetach cleanup: " << e.what() << std::endl;
 }
}

void VulkanEngineLayer::OnUpdate(float ts)
{
 m_LastFrameTime = ts;

 if (!m_EngineInitialized)
 return;
}

void VulkanEngineLayer::OnUIRender()
{
 RenderEngine();
 RenderUI();
}

void VulkanEngineLayer::InitializeEngine()
{
 m_Graphics = std::make_unique<veng::WalnutGraphics>();

 if (!m_Graphics->Initialize()) {
 throw std::runtime_error("Failed to initialize Vulkan graphics engine");
 }

 // Create a quad face using two triangles in world space, with texcoords
 std::array<veng::Vertex,4> vertices = {
 veng::Vertex{glm::vec3{-0.5f, -0.5f,0.0f}, glm::vec3{1.0f,1.0f,1.0f}, glm::vec2{0.0f,1.0f}},
 veng::Vertex{glm::vec3{0.5f, -0.5f,0.0f}, glm::vec3{1.0f,1.0f,1.0f}, glm::vec2{1.0f,1.0f}},
 veng::Vertex{glm::vec3{0.5f,0.5f,0.0f}, glm::vec3{1.0f,1.0f,1.0f}, glm::vec2{1.0f,0.0f}},
 veng::Vertex{glm::vec3{-0.5f,0.5f,0.0f}, glm::vec3{1.0f,1.0f,1.0f}, glm::vec2{0.0f,0.0f}},
 };

 m_VertexBuffer = m_Graphics->CreateVertexBuffer(vertices);

 // Define indices for two triangles forming the quad
 std::array<std::uint32_t,6> indices = {
0,1,2, // First triangle (Bottom-left, Bottom-right, Top-right)
2,3,0 // Second triangle (Bottom-left, Top-right, Top-left)
 };

 m_IndexBuffer = m_Graphics->CreateIndexBuffer(indices);

 // Load default texture from textures/texture.png
 try {
 m_Graphics->LoadTextureFromFile("textures/texture.png");
 } catch (const std::exception& e) {
 std::cout << "Warning: failed to load texture: " << e.what() << std::endl;
 }

 // Proper camera setup for normal3D rendering
 glm::mat4 projection = glm::perspective(glm::radians(m_CameraSettings.fovDegrees), static_cast<float>(m_Graphics->GetRenderWidth()) / static_cast<float>(m_Graphics->GetRenderHeight()), m_CameraSettings.nearClip,10.0f);
 projection[1][1] *= -1; // Flip Y-axis for Vulkan
 glm::mat4 view = glm::lookAt(glm::vec3(2.0f,2.0f,2.0f), glm::vec3(0.0f,0.0f,0.0f), glm::vec3(0.0f,0.0f,1.0f));
 m_Graphics->SetViewProjection(view, projection);

 // Set the model matrix to position the quad in world space
 glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f,0.0f,1.0f));
 m_Graphics->SetModelMatrix(model);

 // Log the model matrix for debugging
 LogMat4(model, "Model Matrix");

 // Initialize runtime camera state
 m_CurrentCameraPosition = glm::vec3(2.0f,1.0f,2.0f);
 m_CurrentCameraTarget = glm::vec3(0.0f,0.0f,0.0f);

 m_EngineInitialized = true;
}

void VulkanEngineLayer::CleanupEngine()
{
 if (!m_EngineInitialized)
 return;

 // Cleanup in proper order
 if (m_Graphics) {
 // Wait for all operations to complete first
 try {
 // Only destroy buffers if they're valid
 if (m_VertexBuffer.buffer != VK_NULL_HANDLE) {
 m_Graphics->DestroyBuffer(m_VertexBuffer);
 m_VertexBuffer.buffer = VK_NULL_HANDLE;
 }
 if (m_IndexBuffer.buffer != VK_NULL_HANDLE) {
 m_Graphics->DestroyBuffer(m_IndexBuffer);
 m_IndexBuffer.buffer = VK_NULL_HANDLE;
 }

 // Then shutdown the graphics system
 m_Graphics->Shutdown();
 } catch (const std::exception& e) {
 std::cout << "Warning during cleanup: " << e.what() << std::endl;
 }

 m_Graphics.reset();
 }

 m_EngineInitialized = false;
}

void VulkanEngineLayer::RenderEngine()
{
 if (!m_EngineInitialized)
 return;

 glm::vec4 bgColor = glm::vec4(0.0f, 0.0f, 0.0f,1.0f);
 m_Graphics->SetClearColor(bgColor);

 try {
 if (m_Graphics->BeginFrame()) {
 // Render the entire quad by using the correct index count (6)
 m_Graphics->RenderIndexedBuffer(m_VertexBuffer, m_IndexBuffer,6);
 m_Graphics->EndFrame();
 }
 } catch (const std::exception& e) {
 std::cout << "Rendering error: " << e.what() << std::endl;
 // Continue running but skip this frame
 }

 // Display the rendered triangle viewport with live image
 ImGui::Begin("Viewport");

 // Detect viewport size and trigger resize if needed
 ImVec2 viewportSize = ImGui::GetContentRegionAvail();
 viewportSize.y -=0; // Leave space for controls

 uint32_t newWidth = static_cast<uint32_t>(viewportSize.x);
 uint32_t newHeight = static_cast<uint32_t>(viewportSize.y);

 if (m_Graphics && newWidth >0 && newHeight >0) {
 if (newWidth != m_LastViewportWidth || newHeight != m_LastViewportHeight) {
 // Resize the GPU render target to match the ImGui viewport size
 m_Graphics->Resize(newWidth, newHeight);
 // Use the explicit viewport to compute projection to avoid mismatch
 ResetCamera(newWidth, newHeight);
 m_LastViewportWidth = newWidth;
 m_LastViewportHeight = newHeight;
 }
 }

 // Display the rendered image from the graphics engine
 if (auto renderedImage = m_Graphics->GetRenderedImage()) {
 ImGui::Image(renderedImage->GetDescriptorSet(), viewportSize);
 }
 ImGui::End();
}

void VulkanEngineLayer::RenderUI()
{

 // ImGui demo window for reference
 if (m_ShowDemoWindow)
 {
 ImGui::ShowDemoWindow(&m_ShowDemoWindow);
 }

 // Advanced Engine Controls with real-time parameters
 if (ImGui::Begin("Debug"))
 {
 ImGui::Separator();
 ImGui::Text("DEBUG WINDOWS");
 ImGui::Checkbox("Show ImGui Demo", &m_ShowDemoWindow);

 // Camera controls
 ImGui::Separator();
 ImGui::Text("Camera Settings");
 ImGui::SliderFloat("FOV", &m_CameraSettings.fovDegrees,10.0f,120.0f);
 ImGui::SliderFloat("Fit Margin", &m_CameraSettings.fitMargin,1.0f,2.0f);
 ImGui::InputFloat("Near Clip", &m_CameraSettings.nearClip,0.01f,0.1f, "%.3f");
 ImGui::SliderFloat("Far Multiplier", &m_CameraSettings.farMultiplier,1.0f,50.0f);
 ImGui::InputFloat3("Preferred Dir", &m_CameraSettings.preferredDir[0]);
 if (ImGui::Button("Normalize Preferred Dir")) {
 if (glm::length(m_CameraSettings.preferredDir) >0.0001f)
 m_CameraSettings.preferredDir = glm::normalize(m_CameraSettings.preferredDir);
 }
 ImGui::SameLine();
 if (ImGui::Button("Reset Camera"))
 ResetCamera();

 // Show current camera debug info
 ImGui::Separator();
 ImGui::Text("Camera Debug");
 ImGui::Text("Position: (%.2f, %.2f, %.2f)", m_CurrentCameraPosition.x, m_CurrentCameraPosition.y, m_CurrentCameraPosition.z);
 glm::vec3 viewDir = glm::normalize(m_CurrentCameraTarget - m_CurrentCameraPosition);
 ImGui::Text("View Dir: (%.2f, %.2f, %.2f)", viewDir.x, viewDir.y, viewDir.z);

 // Show viewport / render target sizes to diagnose aspect mismatches
 if (m_Graphics) {
 ImGui::Separator();
 ImGui::Text("Viewport (ImGui): %u x %u", m_LastViewportWidth, m_LastViewportHeight);
 ImGui::Text("Render target (GPU): %u x %u", m_Graphics->GetRenderWidth(), m_Graphics->GetRenderHeight());
 float guiAspect = m_LastViewportHeight ==0 ?0.0f : static_cast<float>(m_LastViewportWidth) / static_cast<float>(m_LastViewportHeight);
 float gpuAspect = m_Graphics->GetRenderHeight() ==0 ?0.0f : static_cast<float>(m_Graphics->GetRenderWidth()) / static_cast<float>(m_Graphics->GetRenderHeight());
 ImGui::Text("Aspect (ImGui): %.4f", guiAspect);
 ImGui::Text("Aspect (GPU): %.4f", gpuAspect);
 }

 ImGui::End();
 }
}

// New overload: explicit viewport dimensions
void VulkanEngineLayer::ResetCamera(uint32_t renderWidth, uint32_t renderHeight)
{
 if (!m_EngineInitialized || !m_Graphics)
 return;

 if (renderWidth ==0 || renderHeight ==0)
 return; // Avoid division by zero

 const float aspectRatio = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);

 // Camera / framing parameters (use UI-controlled settings)
 const float verticalFOV = glm::radians(m_CameraSettings.fovDegrees);
 const float fitMargin = glm::max(1.0f, m_CameraSettings.fitMargin);
 const float nearClip = glm::max(0.001f, m_CameraSettings.nearClip);

 // Approximate scene bounds: the default quad is ~1x1 centered at origin.
 // Replace this with real scene bounds when available.
 const float approxHalfWidth =0.5f;
 const float approxHalfHeight =0.5f;
 const float sceneRadius = glm::sqrt(approxHalfWidth * approxHalfWidth + approxHalfHeight * approxHalfHeight);

 // Compute required distance so the bounding sphere fits the frustum.
 const float halfV = verticalFOV *0.5f;
 const float tanHalfV = glm::tan(halfV);

 const float distanceV = sceneRadius / tanHalfV;
 const float distanceH = sceneRadius / (tanHalfV * aspectRatio);
 const float requiredDistance = (distanceV > distanceH ? distanceV : distanceH) * fitMargin;

 // Use the preferred direction from UI (direction from target to camera). If zero, fallback to diagonal.
 glm::vec3 preferredDir = m_CameraSettings.preferredDir;
 if (glm::length(preferredDir) <1e-4f)
 preferredDir = glm::vec3(2.0f,2.0f,2.0f);
 preferredDir = glm::normalize(preferredDir);

 // Set camera in world space (Z-up)
 const glm::vec3 cameraTarget = glm::vec3(0.0f,0.0f,0.0f);
 const glm::vec3 cameraPosition = cameraTarget + preferredDir * requiredDistance;
 const glm::vec3 cameraUp = glm::vec3(0.0f,0.0f,1.0f); // Z-up consistent with InitializeEngine

 // Update global runtime camera state for UI / background color
 m_CurrentCameraPosition = cameraPosition;
 m_CurrentCameraTarget = cameraTarget;

 const float farClip = glm::max(100.0f, requiredDistance * m_CameraSettings.farMultiplier);

 glm::mat4 projection = glm::perspective(verticalFOV, aspectRatio, nearClip, farClip);
 projection[1][1] *= -1.0f; // Flip Y for Vulkan (unless GLM_FORCE_DEPTH_ZERO_TO_ONE is set globally)

 glm::mat4 view = glm::lookAt(cameraPosition, cameraTarget, cameraUp);

 // Debug logging only in debug builds
#ifndef NDEBUG
 std::cout << "ResetCamera: " << renderWidth << "x" << renderHeight
 << " aspect=" << aspectRatio
 << " distance=" << requiredDistance
 << " near=" << nearClip << " far=" << farClip << std::endl;
 LogMat4(projection, "Projection Matrix");
 LogMat4(view, "View Matrix");
#endif

 m_Graphics->SetViewProjection(view, projection);
}

// Keep no-arg overload for compatibility: forward to explicit version
void VulkanEngineLayer::ResetCamera()
{
 if (!m_Graphics)
 return;
 ResetCamera(m_Graphics->GetRenderWidth(), m_Graphics->GetRenderHeight());
}

// Add a helper function to log4x4 matrices for debugging
static void LogMat4(const glm::mat4& m, const char* name) {
 std::cout << name << ":\n";
 for (int row =0; row <4; ++row) {
 for (int col =0; col <4; ++col) {
 std::cout << m[col][row] << (col ==3 ? "" : " ");
 }
 std::cout << "\n";
 }
}
