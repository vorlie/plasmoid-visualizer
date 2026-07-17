#pragma once

#include "imgui.h"

inline void HelpMarker(const char* description) {
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
