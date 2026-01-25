#include "LogPanel.hpp"

#include <imgui.h>

void LogPanel::Render(AppState& state)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));

    // Header with controls
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Application Log");

    float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(availWidth - 220);
    if (ImGui::Button("Clear Log", ImVec2(100, 0))) {
        std::lock_guard lock(state.logMutex);
        state.logMessages.clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll_);

    ImGui::Separator();
    ImGui::Spacing();

    // Log display area with monospace font
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

        {
            std::lock_guard lock(state.logMutex);
            for (const auto& msg : state.logMessages) {
                // Color-code log levels
                if (msg.find("[error]") != std::string::npos || msg.find("[ERROR]") != std::string::npos) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", msg.c_str());
                } else if (msg.find("[warn]") != std::string::npos || msg.find("[WARN]") != std::string::npos) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s", msg.c_str());
                } else if (msg.find("[info]") != std::string::npos || msg.find("[INFO]") != std::string::npos) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", msg.c_str());
                } else if (msg.find("[debug]") != std::string::npos || msg.find("[DEBUG]") != std::string::npos) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", msg.c_str());
                } else {
                    ImGui::TextUnformatted(msg.c_str());
                }
            }
        }

        if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::PopStyleVar();
}
