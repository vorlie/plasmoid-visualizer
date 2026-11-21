#pragma once

#include "imgui.h"
#include <string>

inline void HelpMarker(const char* description) {
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

namespace UIHelpers {
inline int ResizeStringCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = str->data();
    }
    return 0;
}

inline bool InputText(const char* label, std::string& value, ImGuiInputTextFlags flags = 0) {
    return ImGui::InputText(
        label,
        value.data(),
        value.capacity() + 1,
        flags | ImGuiInputTextFlags_CallbackResize,
        ResizeStringCallback,
        &value);
}

inline bool InputTextMultiline(
    const char* label,
    std::string& value,
    const ImVec2& size = ImVec2(0, 0),
    ImGuiInputTextFlags flags = 0
) {
    return ImGui::InputTextMultiline(
        label,
        value.data(),
        value.capacity() + 1,
        size,
        flags | ImGuiInputTextFlags_CallbackResize,
        ResizeStringCallback,
        &value);
}
} // namespace UIHelpers
