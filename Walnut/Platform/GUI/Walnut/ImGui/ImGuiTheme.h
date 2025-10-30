#pragma once

#include <imgui.h>
#include <glm/glm.hpp>

#include <algorithm>

namespace Walnut::UI {

	namespace Colors
	{
		static inline float Convert_sRGB_FromLinear(float theLinearValue);
		static inline float Convert_sRGB_ToLinear(float thesRGBValue);
		ImVec4 ConvertFromSRGB(ImVec4 colour);
		ImVec4 ConvertToSRGB(ImVec4 colour);

		// To experiment with editor theme live you can change these constexpr into static
		// members of a static "Theme" class and add a quick ImGui window to adjust the colour values
		namespace Theme
		{
			constexpr auto accent = IM_COL32(30, 200, 96, 255); // Slightly Less Saturated Spotify Green
			constexpr auto highlight = IM_COL32(29, 170, 84, 255); // Slightly Less Saturated Darker Green
			constexpr auto niceBlue = IM_COL32(25, 20, 20, 255); // Unchanged
			constexpr auto compliment = IM_COL32(40, 40, 40, 255); // Unchanged
			constexpr auto background = IM_COL32(18, 18, 18, 255); // Unchanged
			constexpr auto backgroundDark = IM_COL32(13, 13, 13, 255); // Unchanged
			constexpr auto titlebar = IM_COL32(24, 24, 24, 255); // Unchanged
			constexpr auto propertyField = IM_COL32(28, 28, 28, 255); // Unchanged
			constexpr auto text = IM_COL32(255, 255, 255, 255); // Unchanged
			constexpr auto textBrighter = IM_COL32(255, 255, 255, 255); // Unchanged
			constexpr auto textDarker = IM_COL32(179, 179, 179, 255); // Unchanged
			constexpr auto textError = IM_COL32(255, 69, 58, 255); // Unchanged
			constexpr auto muted = IM_COL32(105, 105, 105, 255); // Unchanged
			constexpr auto groupHeader = IM_COL32(55, 60, 65, 255); // Unchanged
			constexpr auto selection = IM_COL32(255, 200, 0, 255); // Slightly Less Saturated Gold
			constexpr auto selectionMuted = IM_COL32(255, 210, 128, 128); // Slightly Less Saturated Muted Gold
			constexpr auto backgroundPopup = IM_COL32(60, 63, 72, 255); // Unchanged
			constexpr auto error = IM_COL32(255, 90, 71, 255); // Slightly Less Saturated Tomato Red
			constexpr auto validPrefab = IM_COL32(50, 190, 50, 255); // Slightly Less Saturated Lime Green
			constexpr auto invalidPrefab = IM_COL32(255, 69, 58, 255); // Unchanged
			constexpr auto missingMesh = IM_COL32(255, 130, 0, 255); // Slightly Less Saturated Dark Orange
			constexpr auto meshNotSet = IM_COL32(255, 150, 0, 255); // Slightly Less Saturated Orange
			constexpr auto tabActive = IM_COL32(30, 200, 96, 255); // Slightly Less Saturated Spotify Green
			constexpr auto tabInactive = IM_COL32(40, 40, 40, 128); // Unchanged
			constexpr auto tabHover = IM_COL32(30, 200, 96, 255); // Slightly Less Saturated Spotify Green
		}
	}

	namespace Colors
	{
		inline float Convert_sRGB_FromLinear(float theLinearValue)
		{
			return theLinearValue <= 0.0031308f
				? theLinearValue * 12.92f
				: glm::pow<float>(theLinearValue, 1.0f / 2.2f) * 1.055f - 0.055f;
		}

		inline float Convert_sRGB_ToLinear(float thesRGBValue)
		{
			return thesRGBValue <= 0.04045f
				? thesRGBValue / 12.92f
				: glm::pow<float>((thesRGBValue + 0.055f) / 1.055f, 2.2f);
		}

		inline ImVec4 ConvertFromSRGB(ImVec4 colour)
		{
			return ImVec4(Convert_sRGB_FromLinear(colour.x),
				Convert_sRGB_FromLinear(colour.y),
				Convert_sRGB_FromLinear(colour.z),
				colour.w);
		}

		inline ImVec4 ConvertToSRGB(ImVec4 colour)
		{
			return ImVec4(std::pow(colour.x, 2.2f),
				glm::pow<float>(colour.y, 2.2f),
				glm::pow<float>(colour.z, 2.2f),
				colour.w);
		}

		inline ImU32 ColorWithValue(const ImColor& color, float value)
		{
			const ImVec4& colRow = color.Value;
			float hue, sat, val;
			ImGui::ColorConvertRGBtoHSV(colRow.x, colRow.y, colRow.z, hue, sat, val);
			return ImColor::HSV(hue, sat, std::min(value, 1.0f));
		}

		inline ImU32 ColorWithSaturation(const ImColor& color, float saturation)
		{
			const ImVec4& colRow = color.Value;
			float hue, sat, val;
			ImGui::ColorConvertRGBtoHSV(colRow.x, colRow.y, colRow.z, hue, sat, val);
			return ImColor::HSV(hue, std::min(saturation, 1.0f), val);
		}

		inline ImU32 ColorWithHue(const ImColor& color, float hue)
		{
			const ImVec4& colRow = color.Value;
			float h, s, v;
			ImGui::ColorConvertRGBtoHSV(colRow.x, colRow.y, colRow.z, h, s, v);
			return ImColor::HSV(std::min(hue, 1.0f), s, v);
		}

		inline ImU32 ColorWithMultipliedValue(const ImColor& color, float multiplier)
		{
			const ImVec4& colRow = color.Value;
			float hue, sat, val;
			ImGui::ColorConvertRGBtoHSV(colRow.x, colRow.y, colRow.z, hue, sat, val);
			return ImColor::HSV(hue, sat, std::min(val * multiplier, 1.0f));
		}

		inline ImU32 ColorWithMultipliedSaturation(const ImColor& color, float multiplier)
		{
			const ImVec4& colRow = color.Value;
			float hue, sat, val;
			ImGui::ColorConvertRGBtoHSV(colRow.x, colRow.y, colRow.z, hue, sat, val);
			return ImColor::HSV(hue, std::min(sat * multiplier, 1.0f), val);
		}

		inline ImU32 ColorWithMultipliedHue(const ImColor& color, float multiplier)
		{
			const ImVec4& colRow = color.Value;
			float hue, sat, val;
			ImGui::ColorConvertRGBtoHSV(colRow.x, colRow.y, colRow.z, hue, sat, val);
			return ImColor::HSV(std::min(hue * multiplier, 1.0f), sat, val);
		}
	}

	void SetHazelTheme();

}