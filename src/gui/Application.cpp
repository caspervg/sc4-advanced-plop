#include "Application.hpp"

#include <cstdlib>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "ImGuiLogSink.hpp"
#include "services/ScanService.hpp"

Application::Application()
{
    // Create and configure the logger with ImGui sink
    auto sink = std::make_shared<ImGuiLogSink_mt>(state_);
    auto logger = std::make_shared<spdlog::logger>("gui", sink);
    // Default to info to avoid thumbnail/texture debug spam; allow override via env.
    auto level = spdlog::level::info;
    if (const char* env = std::getenv("SC4_LOTPLOP_GUI_LOG_LEVEL")) {
        level = spdlog::level::from_str(env);
    }
    logger->set_level(level);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
}

Application::~Application()
{
    // Cancel and wait for scan to complete before destroying
    if (scanService_ && scanService_->isRunning()) {
        auto logger = spdlog::get("gui");
        if (logger) {
            logger->info("Waiting for scan to complete before shutdown...");
        }
        scanService_->cancel();

        // Wait up to 5 seconds for graceful shutdown
        int waitCount = 0;
        while (scanService_->isRunning() && waitCount < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waitCount++;
        }
    }
}

void Application::StartScan_()
{
    if (state_.scanState == ScanState::Scanning) {
        return;  // Already scanning
    }

    state_.scanState = ScanState::Scanning;
    state_.totalFiles = 0;
    state_.processedFiles = 0;
    state_.entriesIndexed = 0;
    state_.buildingsFound = 0;
    state_.lotsFound = 0;
    state_.parseErrors = 0;

    auto logger = spdlog::get("gui");

    scanService_ = std::make_unique<ScanService>(
        state_.pluginConfig,
        *logger,
        state_.renderThumbnails);

    scanService_->start();
}

void Application::UpdateProgress_()
{
    if (state_.scanState != ScanState::Scanning || !scanService_) {
        return;
    }

    auto progress = scanService_->getProgress();
    state_.totalFiles = progress.totalFiles;
    state_.processedFiles = progress.processedFiles;
    state_.entriesIndexed = progress.entriesIndexed;
    state_.buildingsFound = progress.buildingsFound;
    state_.lotsFound = progress.lotsFound;
    state_.parseErrors = progress.parseErrors;
    state_.currentFile = progress.currentFile;

    // Check if complete
    if (!scanService_->isRunning()) {
        state_.scanResults = scanService_->getResults();
        state_.scanState = state_.scanResults.errorMessage.empty()
                               ? ScanState::Complete
                               : ScanState::Error;
        scanService_.reset();
    }

    // Check for cancel request
    if (progressPanel_.IsCancelRequested()) {
        progressPanel_.ResetCancelRequest();
        scanService_->cancel();
        state_.scanState = ScanState::Idle;
    }
}

void Application::RenderResultsSummary_()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));

    if (state_.scanState == ScanState::Complete) {
            // Success header
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "  Scan completed successfully!");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Results in a nice grid
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Summary");
            ImGui::Spacing();

            ImGui::Columns(2, nullptr, false);

            ImGui::Text("Buildings Found:");
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%u", state_.scanResults.buildingsFound);
            ImGui::NextColumn();

            ImGui::Text("Lots Found:");
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%u", state_.scanResults.lotsFound);
            ImGui::NextColumn();

            ImGui::Text("Parse Errors:");
            ImGui::NextColumn();
            if (state_.scanResults.parseErrors > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "%u", state_.scanResults.parseErrors);
            } else {
                ImGui::Text("%u", state_.scanResults.parseErrors);
            }
            ImGui::NextColumn();

            ImGui::Columns(1);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Output file path
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Output File");
            ImGui::Spacing();
            ImGui::TextWrapped("%s", state_.scanResults.outputPath.string().c_str());

            ImGui::Spacing();
            ImGui::Spacing();

            // Scan again button - centered
            float availWidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availWidth - 150) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));

            if (ImGui::Button("Scan Again", ImVec2(150, 35))) {
                state_.scanState = ScanState::Idle;
                configPanel_.ResetStartButton();
            }

            ImGui::PopStyleColor(3);

        } else if (state_.scanState == ScanState::Error) {
            // Error header
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  Scan failed!");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Error message
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Error Details");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.05f, 0.05f, 0.5f));
            ImGui::BeginChild("ErrorRegion", ImVec2(0, 100), true);
            ImGui::TextWrapped("%s", state_.scanResults.errorMessage.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();

            // Try again button - centered
            const float availWidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availWidth - 150) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.4f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.2f, 0.0f, 1.0f));

            if (ImGui::Button("Try Again", ImVec2(150, 35))) {
                state_.scanState = ScanState::Idle;
                configPanel_.ResetStartButton();
            }

            ImGui::PopStyleColor(3);
        }

        ImGui::PopStyleVar();
}

void Application::Render()
{
    // Create a fullscreen window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    if (ImGui::Begin("MainWindow", nullptr, window_flags)) {
        ImGui::PopStyleVar(3);

        // Create main layout with left column (config + progress/results) and right column (log)
        const float leftColumnWidth = 720.0f;
        const float rightColumnWidth = ImGui::GetContentRegionAvail().x - leftColumnWidth - 10.0f;

        ImGui::Columns(2, "MainColumns", false);
        ImGui::SetColumnWidth(0, leftColumnWidth);
        ImGui::SetColumnWidth(1, rightColumnWidth);

        // Left column
        {
            // Config panel region
            ImGui::BeginChild("ConfigRegion", ImVec2(0, 350), true);
            configPanel_.Render(state_);
            ImGui::EndChild();

            ImGui::Spacing();

            // Progress/Results panel region
            ImGui::BeginChild("StatusRegion", ImVec2(0, 0), true);
            if (state_.scanState == ScanState::Scanning) {
                progressPanel_.Render(state_);
            } else if (state_.scanState == ScanState::Complete || state_.scanState == ScanState::Error) {
                RenderResultsSummary_();
            } else {
                // Idle state - show instructions
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Ready to Scan");
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextWrapped("Configure your SimCity 4 paths above and click 'Start Scan' to analyze your plugins.");
                ImGui::Spacing();
                ImGui::TextWrapped("The scanner will:");
                ImGui::BulletText("Index all DBPF files in your game and user plugin directories");
                ImGui::BulletText("Extract building and lot configuration data");
                ImGui::BulletText("Generate a CBOR file with all lot configurations");
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
        }

        ImGui::NextColumn();

        // Right column - Log
        {
            ImGui::BeginChild("LogRegion", ImVec2(0, 0), true);
            logPanel_.Render(state_);
            ImGui::EndChild();
        }

        ImGui::Columns(1);

        // Check if scan should be started
        if (configPanel_.IsStartButtonClicked()) {
            configPanel_.ResetStartButton();
            StartScan_();
        }

        ImGui::End();
    } else {
        ImGui::PopStyleVar(3);
    }
}

void Application::Update()
{
    UpdateProgress_();
}
