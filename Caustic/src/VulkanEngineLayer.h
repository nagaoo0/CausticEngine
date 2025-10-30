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
};