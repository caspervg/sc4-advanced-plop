#include "ProgressPanel.hpp"

#include <imgui.h>

const char* ProgressPanel::GetSpinnerChar_() const
{
    static const char* chars[] = {"|", "/", "-", "\\"};
    return chars[(spinnerFrame_ / 10) % 4];
}

void ProgressPanel::Render(AppState& state)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));

    // Status text with spinner
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Scanning plugins %s", GetSpinnerChar_());
    spinnerFrame_++;

        ImGui::Separator();
        ImGui::Spacing();

        // Progress bar with overlay text
        float fileProgress = state.totalFiles > 0
                              ? static_cast<float>(state.processedFiles) / state.totalFiles
                              : 0.0f;

        char progressOverlay[64];
        snprintf(progressOverlay, sizeof(progressOverlay), "%u / %u files (%.0f%%)",
                state.processedFiles, state.totalFiles, fileProgress * 100.0f);

        ImGui::Text("File Progress:");
        ImGui::ProgressBar(fileProgress, ImVec2(-1, 30), progressOverlay);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Statistics in a nice grid
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Statistics");
        ImGui::Spacing();

        ImGui::Columns(2, nullptr, false);

        ImGui::Text("Entries Indexed:");
        ImGui::NextColumn();
        ImGui::Text("%u", state.entriesIndexed);
        ImGui::NextColumn();

        ImGui::Text("Buildings Found:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%u", state.buildingsFound);
        ImGui::NextColumn();

        ImGui::Text("Lots Found:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%u", state.lotsFound);
        ImGui::NextColumn();

        ImGui::Text("Parse Errors:");
        ImGui::NextColumn();
        if (state.parseErrors > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "%u", state.parseErrors);
        } else {
            ImGui::Text("%u", state.parseErrors);
        }
        ImGui::NextColumn();

        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Cancel button - centered
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availWidth - 120) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

        if (ImGui::Button("Cancel Scan", ImVec2(120, 30))) {
            cancelRequested_ = true;
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
}
