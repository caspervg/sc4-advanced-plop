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
        const auto& progress = state.scanProgress;

        ImGui::Text("Scanning plugins... %s", GetSpinnerChar_());
        spinnerFrame_++;

        ImGui::Spacing();

        // Progress bars
        ImGui::Text("Files processed:");
        float fileProgress = progress.totalFiles > 0
                              ? static_cast<float>(progress.processedFiles) / progress.totalFiles
                              : 0.0f;
        ImGui::ProgressBar(fileProgress, ImVec2(-1, 0), "");
        ImGui::Text("%u / %u files", progress.processedFiles, progress.totalFiles);

        ImGui::Spacing();

        // Statistics
        ImGui::Text("Statistics:");
        ImGui::BulletText("Entries indexed: %u", progress.entriesIndexed);
        ImGui::BulletText("Buildings found: %u", progress.buildingsFound);
        ImGui::BulletText("Lots found: %u", progress.lotsFound);
        ImGui::BulletText("Parse errors: %u", progress.parseErrors);

        ImGui::Spacing();

        // Cancel button
        if (ImGui::Button("Cancel", ImVec2(100, 30))) {
            cancelRequested_ = true;
        }

        ImGui::End();
    }
}
