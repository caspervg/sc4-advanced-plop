#include "ProgressPanel.hpp"

#include <imgui.h>

const char* ProgressPanel::GetSpinnerChar_() const
{
    static const char* chars[] = {"|", "/", "-", "\\"};
    return chars[(spinnerFrame_ / 10) % 4];
}

void ProgressPanel::Render(AppState& state)
{
    ImGui::SetNextWindowPos(ImVec2(0, 260), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Progress", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Scanning plugins... %s", GetSpinnerChar_());
        spinnerFrame_++;

        ImGui::Spacing();

        // Progress bars
        ImGui::Text("Files processed:");
        float fileProgress = state.totalFiles > 0
                              ? static_cast<float>(state.processedFiles) / state.totalFiles
                              : 0.0f;
        ImGui::ProgressBar(fileProgress, ImVec2(-1, 0), "");
        ImGui::Text("%u / %u files", state.processedFiles, state.totalFiles);

        ImGui::Spacing();

        // Statistics
        ImGui::Text("Statistics:");
        ImGui::BulletText("Entries indexed: %u", state.entriesIndexed);
        ImGui::BulletText("Buildings found: %u", state.buildingsFound);
        ImGui::BulletText("Lots found: %u", state.lotsFound);
        ImGui::BulletText("Parse errors: %u", state.parseErrors);

        ImGui::Spacing();

        // Cancel button
        if (ImGui::Button("Cancel", ImVec2(100, 30))) {
            cancelRequested_ = true;
        }

        ImGui::End();
    }
}
