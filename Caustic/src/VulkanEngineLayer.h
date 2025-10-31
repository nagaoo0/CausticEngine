#pragma once

#include "Walnut/Layer.h"
#include "Walnut/Application.h"
#include "Walnut/Timer.h"

// Include your Vulkan engine headers
#include "Engine/WalnutGraphics.h"
#include "Engine/vertex.h"
#include "Engine/buffer_handle.h"

class VulkanEngineLayer : public Walnut::Layer
{
public:
    VulkanEngineLayer();
    virtual ~VulkanEngineLayer();

    virtual void OnUpdate(float ts) override;
    virtual void OnUIRender() override;
    virtual void OnAttach() override;
    virtual void OnDetach() override;
    void ResetCamera();
    void ResetCamera(uint32_t renderWidth, uint32_t renderHeight);

private:
    void InitializeEngine();
    void CleanupEngine();
    void RenderEngine();
    void RenderUI();
    ImVec2 GetViewportResolution() const;


    void SetClearColor(const glm::vec4& color);

private:
    // Your Vulkan engine
    std::unique_ptr<veng::WalnutGraphics> m_Graphics;
    
    // Scene objects
    veng::BufferHandle m_VertexBuffer;
    veng::BufferHandle m_IndexBuffer;
    
    // Walnut integration
    Walnut::Timer m_Timer;
    float m_LastFrameTime = 0.0f;
    
    // Engine state
    bool m_EngineInitialized = false;
    
    // UI state
    bool m_ShowDemoWindow = false;
    bool m_ShowEngineStats = true;

    // Camera/settings (moved from cpp globals)
    struct CameraSettings {
        float fovDegrees = 45.0f;
        float fitMargin = 1.15f;
        float nearClip = 0.1f;
        glm::vec3 preferredDir = glm::vec3(2.0f, 2.0f, 2.0f);
        float farMultiplier = 10.0f;
    } m_CameraSettings;

    // Runtime camera state
    glm::vec3 m_CurrentCameraPosition = glm::vec3(2.0f, 2.0f, 2.0f);
    glm::vec3 m_CurrentCameraTarget = glm::vec3(0.0f);

    // Last viewport size from ImGui
    uint32_t m_LastViewportWidth = 0;
    uint32_t m_LastViewportHeight = 0;
};