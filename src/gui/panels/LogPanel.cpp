#include "LogPanel.hpp"

#include <imgui.h>

void LogPanel::Render(AppState& state)
{
    ImGui::SetNextWindowPos(ImVec2(600, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(680, 460), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Log", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        if (ImGui::Button("Clear", ImVec2(80, 0))) {
            std::lock_guard lock(state.logMutex);
            state.logMessages.clear();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &autoScroll_);

        ImGui::Separator();

        ImGui::BeginChild("LogScrollRegion", ImVec2(0, -30), true, ImGuiWindowFlags_HorizontalScrollbar);

        {
            std::lock_guard lock(state.logMutex);
            for (const auto& msg : state.logMessages) {
                ImGui::TextUnformatted(msg.c_str());
            }
        }

        if (autoScroll_) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        ImGui::End();
    }
}
