#include "VulkanEngineLayer.h"
#include "Walnut/UI/UI.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

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
    // Do not update the model matrix each frame — keep the initial model
    // transform (set in InitializeEngine) so the triangle stays in front of
    // the camera for debugging.
}

void VulkanEngineLayer::OnUIRender()
{
    RenderEngine();
    RenderUI();
}

void VulkanEngineLayer::InitializeEngine()
{
    // Initialize your Vulkan engine
    m_Graphics = std::make_unique<veng::WalnutGraphics>();
    
    if (!m_Graphics->Initialize()) {
        throw std::runtime_error("Failed to initialize Vulkan graphics engine");
    }
    
    // Create a simple triangle - DIRECT CLIP SPACE coordinates for testing
    std::array<veng::Vertex, 3> vertices = {
        veng::Vertex{glm::vec3{0.0f, -0.5f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}},  // Red bottom
        veng::Vertex{glm::vec3{0.5f, 0.5f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}},   // Green top-right
        veng::Vertex{glm::vec3{-0.5f, 0.5f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}},  // Blue top-left
    };
    
    m_VertexBuffer = m_Graphics->CreateVertexBuffer(vertices);
    
    std::array<std::uint32_t, 3> indices = {0, 2, 1};  // Counter-clockwise winding order
    m_IndexBuffer = m_Graphics->CreateIndexBuffer(indices);
    
    // Proper camera setup for normal 3D rendering
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 100.0f);
    m_Graphics->SetViewProjection(view, projection);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    m_Graphics->SetModelMatrix(model);
    
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
        
    // Triangle rendering is now ENABLED!
    try {
        if (m_Graphics->BeginFrame()) {
            m_Graphics->RenderIndexedBuffer(m_VertexBuffer, m_IndexBuffer, 3);
            m_Graphics->EndFrame();
        }
    } catch (const std::exception& e) {
        std::cout << "Rendering error: " << e.what() << std::endl;
        // Continue running but skip this frame
    }
    
    // Display the rendered triangle viewport with live image
    ImGui::Begin("Viewport");
    
    
    // Display the actual rendered image
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    viewportSize.y -= 1; // Leave space for controls
    
    if (auto renderedImage = m_Graphics->GetRenderedImage()) {
        ImGui::Image(renderedImage->GetDescriptorSet(), viewportSize);
    } else {
        ImGui::Text("Viewport RenderTarget size: %.0f x %.0f", viewportSize.x, viewportSize.y);
    }
    
    ImGui::End();
}

void VulkanEngineLayer::RenderUI()
{
    // Advanced Engine Statistics window
    if (m_ShowEngineStats)
    {
        ImGui::Begin("Advanced Engine Statistics", &m_ShowEngineStats);
        
        // Performance metrics
        ImGui::Text("PERFORMANCE METRICS");
        ImGui::Separator();
        ImGui::Text("Frame Time: %.3f ms", m_LastFrameTime * 1000.0f);
        ImGui::Text("FPS: %.1f", 1.0f / m_LastFrameTime);
        
        // Graphics statistics
        //ImGui::Separator();
        //ImGui::Text("GRAPHICS STATISTICS");
        //ImGui::Text("Triangles Rendered: 1");
        //ImGui::Text("Draw Calls: 1");
        //ImGui::Text("Vertices: 3");
        //ImGui::Text("Shader Stages: 2 (Vertex + Fragment)");
        
        // Engine status
        ImGui::Separator();
        ImGui::Text("ENGINE STATUS");
        ImGui::Text("Engine Initialized: %s", m_EngineInitialized ? "YES" : "NO");
        
        ImGui::Separator();
        ImGui::Checkbox("Show Demo Window", &m_ShowDemoWindow);
        
        ImGui::End();
    }
    
    // ImGui demo window for reference
    if (m_ShowDemoWindow)
    {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }
    
    // Advanced Engine Controls with real-time parameters
    if (ImGui::Begin("Advanced Engine Controls"))
    {
        ImGui::Text("RENDERING CONTROLS");
        ImGui::Separator();
        
        if (ImGui::Button("Reload Shaders"))
        {
            std::cout << "Shader reload requested (feature coming soon)" << std::endl;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Camera"))
        {
            // Reset view projection
            glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));
            glm::mat4 projection = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 100.0f);
            projection[1][1] *= -1.0f;
            m_Graphics->SetViewProjection(view, projection);
            std::cout << "Camera reset to default position" << std::endl;
        }
        
        ImGui::Separator();
        ImGui::Text("DEBUG WINDOWS");
        ImGui::Checkbox("Show Advanced Statistics", &m_ShowEngineStats);
        ImGui::Checkbox("Show ImGui Demo", &m_ShowDemoWindow);
        
        ImGui::End();
    }
}