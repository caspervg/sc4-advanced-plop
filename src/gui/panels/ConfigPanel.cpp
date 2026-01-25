#include "ConfigPanel.hpp"

#include <imgui.h>
#include <ImGuiFileDialog.h>

namespace fs = std::filesystem;

ConfigPanel::ConfigPanel()
{
    // Get default paths
    const char* userProfile = std::getenv("USERPROFILE");
    const char* programFiles = std::getenv("PROGRAMFILES(x86)");

    if (programFiles) {
        auto gameRoot = fs::path(programFiles) / "SimCity 4 Deluxe Edition";
        std::string gameRootStr = gameRoot.string();
        std::copy(gameRootStr.begin(), gameRootStr.end(), gameRootBuf_.begin());
        gameRootBuf_[gameRootStr.length()] = '\0';
    }

    if (userProfile) {
        auto userPlugins = fs::path(userProfile) / "Documents" / "SimCity 4" / "Plugins";
        std::string userPluginsStr = userPlugins.string();
        std::copy(userPluginsStr.begin(), userPluginsStr.end(), userPluginsBuf_.begin());
        userPluginsBuf_[userPluginsStr.length()] = '\0';
    }

    // Default locale
    std::string defaultLocale = "English";
    std::copy(defaultLocale.begin(), defaultLocale.end(), localeBuf_.begin());
    localeBuf_[defaultLocale.length()] = '\0';
}

void ConfigPanel::Render(AppState& state)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));

    // Title
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "SimCity 4 Plugin Scanner");
    ImGui::Separator();
    ImGui::Spacing();

        // Game Root
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Game Root:");
        ImGui::SameLine(150);
        ImGui::SetNextItemWidth(-90);
        ImGui::InputText("##gameroot", gameRootBuf_.data(), gameRootBuf_.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("Browse##game", ImVec2(80, 0))) {
            IGFD::FileDialogConfig config;
            config.path = std::string(gameRootBuf_.data());
            ImGuiFileDialog::Instance()->OpenDialog(
                "ChooseGameRootDlg",
                "Choose Game Root Directory",
                nullptr,
                config);
        }

        // Handle game root dialog
        if (ImGuiFileDialog::Instance()->Display("ChooseGameRootDlg")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                auto selectedPath = ImGuiFileDialog::Instance()->GetCurrentPath();
                std::copy(selectedPath.begin(), selectedPath.end(), gameRootBuf_.begin());
                if (selectedPath.length() < gameRootBuf_.size()) {
                    gameRootBuf_[selectedPath.length()] = '\0';
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::Spacing();

        // User Plugins
        ImGui::AlignTextToFramePadding();
        ImGui::Text("User Plugins:");
        ImGui::SameLine(150);
        ImGui::SetNextItemWidth(-90);
        ImGui::InputText("##userdir", userPluginsBuf_.data(), userPluginsBuf_.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("Browse##user", ImVec2(80, 0))) {
            IGFD::FileDialogConfig config;
            config.path = std::string(userPluginsBuf_.data());
            ImGuiFileDialog::Instance()->OpenDialog(
                "ChooseUserPluginsDlg",
                "Choose User Plugins Directory",
                nullptr,
                config);
        }

        // Handle user plugins dialog
        if (ImGuiFileDialog::Instance()->Display("ChooseUserPluginsDlg")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                auto selectedPath = ImGuiFileDialog::Instance()->GetCurrentPath();
                std::copy(selectedPath.begin(), selectedPath.end(), userPluginsBuf_.begin());
                if (selectedPath.length() < userPluginsBuf_.size()) {
                    userPluginsBuf_[selectedPath.length()] = '\0';
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::Spacing();

        // Locale
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Locale:");
        ImGui::SameLine(150);
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##locale", localeBuf_.data(), localeBuf_.size());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Options
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Options");
        ImGui::Spacing();
        ImGui::Checkbox("Render 3D Thumbnails (slower)", &renderThumbnails_);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Validate paths
        bool gameRootValid = fs::exists(gameRootBuf_.data());
        bool userPluginsValid = fs::exists(userPluginsBuf_.data());
        bool validPaths = gameRootValid && userPluginsValid;

        // Status indicators
        if (!gameRootValid) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  Game root directory not found");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "  Game root OK");
        }

        if (!userPluginsValid) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  User plugins directory not found");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "  User plugins OK");
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Start scan button - centered and larger
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availWidth - 200) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));

        ImGui::BeginDisabled(!validPaths);
        if (ImGui::Button("Start Scan", ImVec2(200, 40))) {
            startButtonClicked_ = true;
            state.pluginConfig.gameRoot = gameRootBuf_.data();
            state.pluginConfig.gamePluginsRoot = fs::path(gameRootBuf_.data()) / "Plugins";
            state.pluginConfig.localeDir = fs::path(localeBuf_.data());
            state.pluginConfig.userPluginsRoot = userPluginsBuf_.data();
            state.renderThumbnails = renderThumbnails_;
        }
        ImGui::EndDisabled();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
}
