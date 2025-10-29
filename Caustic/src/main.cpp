#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/UI/UI.h"

#include "VulkanEngineLayer.h"

class ExampleLayer : public Walnut::Layer
{
public:
	virtual void OnUIRender() override
	{


		UI_DrawAboutModal();
	}

	void UI_DrawAboutModal()
	{
		if (!m_AboutModalOpen)
			return;

		ImGui::OpenPopup("About");
		m_AboutModalOpen = ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
		if (m_AboutModalOpen)
		{
			auto image = Walnut::Application::Get().GetApplicationIcon();
			ImGui::Image(image->GetDescriptorSet(), { 64, 64 });

			ImGui::SameLine();
			Walnut::UI::ShiftCursorX(20.0f);

			ImGui::BeginGroup();
			ImGui::Text("Caustic, Vulkan Render Engine");
			ImGui::Text("by Mihajlo Ciric.");
			ImGui::Text("Version 0.1.0");
			ImGui::EndGroup();

			if (Walnut::UI::ButtonCentered("Close"))
			{
				m_AboutModalOpen = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ShowAboutModal()
	{
		m_AboutModalOpen = true;
	}
private:
	bool m_AboutModalOpen = false;
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Caustic";
	spec.CustomTitlebar = true;

	Walnut::Application* app = new Walnut::Application(spec);
	
	// Create both layers - ExampleLayer for About modal, VulkanEngineLayer for the engine
	std::shared_ptr<ExampleLayer> exampleLayer = std::make_shared<ExampleLayer>();
	std::shared_ptr<VulkanEngineLayer> engineLayer = std::make_shared<VulkanEngineLayer>();
	app->PushLayer(exampleLayer);
	app->PushLayer(engineLayer);
	
	// Restore original menu structure and add new engine-specific items
	app->SetMenubarCallback([app, exampleLayer]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Engine"))
		{
			if (ImGui::MenuItem("Reload Shaders"))
			{
				// TODO: Add shader reloading functionality
			}
			if (ImGui::MenuItem("Reset Camera"))
			{
				// TODO: Add camera reset functionality
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About"))
			{
				exampleLayer->ShowAboutModal();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}