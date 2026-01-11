#pragma once

#include <imgui.h>

namespace Fishy {

/**
 * @brief Debug settings for shader visualization
 */
struct DebugSettings {
	int debugViewInputs = 0;   // 0=off, 1=baseColor, 2=shadingNormal, 3=AO, 4=emissive, 5=metallic, 6=roughness, 7=UV,
							   // 8=geometricNormal, 9=tangent, 10=bitangent
	int debugViewEquation = 0; // 0=off, 1=diffuse, 2=F, 3=G, 4=D, 5=specular
};

/**
 * @brief ImGui panel for shader debugging
 */
class DebugPanel {
public:
	void draw() {
		if (ImGui::Begin("Shader Debug")) {
			ImGui::Text("Shader Inputs");
			ImGui::Separator();

			const char* inputItems[] = {
				"Off",		 "Base Color", "Shading Normal",   "AO",	  "Emissive",  "Metallic",
				"Roughness", "UV",		   "Geometric Normal", "Tangent", "Bitangent", "Shadow"};
			ImGui::Combo("View Input", &_settings.debugViewInputs, inputItems, IM_ARRAYSIZE(inputItems));

			ImGui::Spacing();
			ImGui::Text("PBR Equation");
			ImGui::Separator();

			const char* equationItems[] = {"Off",			"Diffuse",			"Fresnel (F)",
										   "Geometric (G)", "Distribution (D)", "Specular"};
			ImGui::Combo("View Equation", &_settings.debugViewEquation, equationItems, IM_ARRAYSIZE(equationItems));

			// If one is selected, clear the other (they're mutually exclusive)
			if (_settings.debugViewInputs > 0 && _lastInputs != _settings.debugViewInputs) {
				_settings.debugViewEquation = 0;
			}
			if (_settings.debugViewEquation > 0 && _lastEquation != _settings.debugViewEquation) {
				_settings.debugViewInputs = 0;
			}

			_lastInputs = _settings.debugViewInputs;
			_lastEquation = _settings.debugViewEquation;

			ImGui::Spacing();
			if (ImGui::Button("Reset")) {
				_settings.debugViewInputs = 0;
				_settings.debugViewEquation = 0;
			}
		}
		ImGui::End();
	}

	const DebugSettings& getSettings() const { return _settings; }
	DebugSettings& getSettings() { return _settings; }

private:
	DebugSettings _settings;
	int _lastInputs = 0;
	int _lastEquation = 0;
};

} // namespace Fishy
