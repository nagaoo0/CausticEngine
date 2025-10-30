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
    
    // Create a quad face using two triangles in world space
    std::array<veng::Vertex, 4> vertices = {
        veng::Vertex{glm::vec3{-0.5f, -0.5f, 0.0f}, glm::vec3{0.0f, 0.0f, 0.0f}},  
        veng::Vertex{glm::vec3{0.5f, -0.5f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}},   
        veng::Vertex{glm::vec3{0.5f, 0.5f, 0.0f}, glm::vec3{1.0f, 1.0f, 0.0f}},    
        veng::Vertex{glm::vec3{-0.5f, 0.5f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}},   
    };

    m_VertexBuffer = m_Graphics->CreateVertexBuffer(vertices);

    // Define indices for two triangles forming the quad
    std::array<std::uint32_t, 6> indices = {
        0, 1, 2,  // First triangle (Bottom-left, Bottom-right, Top-right)
        2, 3, 0   // Second triangle (Bottom-left, Top-right, Top-left)
    };

    m_IndexBuffer = m_Graphics->CreateIndexBuffer(indices);
    
    // Proper camera setup for normal 3D rendering
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(m_Graphics->GetRenderWidth()) / static_cast<float>(m_Graphics->GetRenderHeight()),0.1f,10.0f);
    projection[1][1] *= -1; // Flip Y-axis for Vulkan
    glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    m_Graphics->SetViewProjection(view, projection);

    // Set the model matrix to position the quad in world space
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    m_Graphics->SetModelMatrix(model);
    
    // Log the model matrix for debugging
    LogMat4(model, "Model Matrix");
    
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

    // Set background color (can be made user-configurable)
    static glm::vec4 bgColor = glm::vec4(0.0f,0.0f,0.0f,1.0f);
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

    static uint32_t lastWidth =0, lastHeight =0;
    uint32_t newWidth = static_cast<uint32_t>(viewportSize.x);
    uint32_t newHeight = static_cast<uint32_t>(viewportSize.y);

    if (m_Graphics && newWidth >0 && newHeight >0) {
        if (newWidth != lastWidth || newHeight != lastHeight) {
            m_Graphics->Resize(newWidth, newHeight);
            ResetCamera(); // Reset camera on resize to maintain aspect ratio
            lastWidth = newWidth;
            lastHeight = newHeight;
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
        
        ImGui::End();
    }
}


void VulkanEngineLayer::ResetCamera()
{
    if (!m_EngineInitialized)
        return;

    // Use the current viewport dimensions to calculate the aspect ratio
    uint32_t renderWidth = m_Graphics->GetRenderHeight();
    uint32_t renderHeight = m_Graphics->GetRenderWidth();

    if (renderWidth ==0 || renderHeight ==0)
        return; // Avoid division by zero

    float aspectRatio = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
    std::cout << "Aspect Ratio: " << aspectRatio << std::endl;

    // Use a fixed vertical FOV and adjust the width based on the aspect ratio
    float verticalFOV = glm::radians(45.0f); // Fixed vertical FOV
    glm::mat4 projection = glm::perspective(verticalFOV, aspectRatio,0.1f,100.0f);
    projection[1][1] *= -1; // Flip Y-axis for Vulkan

    // Dynamically adjust the camera position based on viewport dimensions
    glm::vec3 cameraPosition = glm::vec3(0.0f,0.0f, -5.0f); // Fixed distance from the object
    glm::vec3 cameraTarget = glm::vec3(0.0f,0.0f,0.0f); // Center of the view
    glm::vec3 cameraUp = glm::vec3(0.0f,1.0f,0.0f); // Up direction

    glm::mat4 view = glm::lookAt(cameraPosition, cameraTarget, cameraUp);

    // Log matrices for debugging
    LogMat4(projection, "Projection Matrix");
    LogMat4(view, "View Matrix");

    // Update the graphics engine with the new matrices
    if (m_Graphics)
        m_Graphics->SetViewProjection(view, projection);
}

ImVec2 VulkanEngineLayer::GetViewportResolution() const
{
    if (!m_EngineInitialized)
        return ImVec2(0.0f, 0.0f);

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    return ImVec2(viewportSize.x, viewportSize.y);
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
