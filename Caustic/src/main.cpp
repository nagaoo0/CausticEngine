#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/UI/UI.h"

#include "VulkanEngineLayer.h"

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Caustic";
	spec.CustomTitlebar = true;
	spec.UseDockspace = true;


	Walnut::Application* app = new Walnut::Application(spec);
	

	std::shared_ptr<VulkanEngineLayer> engineLayer = std::make_shared<VulkanEngineLayer>();

	app->PushLayer(engineLayer);

	
	
	app->SetMenubarCallback([app, engineLayer]()
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
				engineLayer->ResetCamera();
			}
			ImGui::EndMenu();
		}

	});
	return app;
}