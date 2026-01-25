#include "Application.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include "ImGuiLogSink.hpp"
#include "services/ScanService.hpp"

Application::Application()
{
    // Create and configure the logger with ImGui sink
    auto sink = std::make_shared<ImGuiLogSink_mt>(state_);
    auto logger = std::make_shared<spdlog::logger>("gui", sink);
    logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
}

Application::~Application()
{
    if (scanService_) {
        scanService_->cancel();
    }
}

void Application::StartScan_()
{
    if (state_.scanState == ScanState::Scanning) {
        return;  // Already scanning
    }

    state_.scanState = ScanState::Scanning;
    state_.scanProgress = {};
    state_.scanResults = {};

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

    state_.scanProgress = scanService_->getProgress();

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
    ImGui::SetNextWindowPos(ImVec2(0, 260), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Results", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        if (state_.scanState == ScanState::Complete) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Scan completed successfully!");

            ImGui::Spacing();
            ImGui::BulletText("Buildings found: %u", state_.scanResults.buildingsFound);
            ImGui::BulletText("Lots found: %u", state_.scanResults.lotsFound);
            ImGui::BulletText("Parse errors: %u", state_.scanResults.parseErrors);

            ImGui::Spacing();
            ImGui::Text("Output file:");
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "%s", state_.scanResults.outputPath.string().c_str());

            ImGui::Spacing();
            if (ImGui::Button("Scan Again", ImVec2(120, 30))) {
                state_.scanState = ScanState::Idle;
                configPanel_.ResetStartButton();
            }
        } else if (state_.scanState == ScanState::Error) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Scan failed!");
            ImGui::Spacing();
            ImGui::TextWrapped("Error: %s", state_.scanResults.errorMessage.c_str());

            ImGui::Spacing();
            if (ImGui::Button("Try Again", ImVec2(120, 30))) {
                state_.scanState = ScanState::Idle;
                configPanel_.ResetStartButton();
            }
        }

        ImGui::End();
    }
}

void Application::Render()
{
    // Render configuration panel
    configPanel_.Render(state_);

    // Check if scan should be started
    if (configPanel_.IsStartButtonClicked()) {
        configPanel_.ResetStartButton();
        StartScan_();
    }

    // Render progress panel if scanning
    if (state_.scanState == ScanState::Scanning) {
        progressPanel_.Render(state_);
    }

    // Render results summary if complete or error
    if (state_.scanState == ScanState::Complete || state_.scanState == ScanState::Error) {
        RenderResultsSummary_();
    }

    // Render log panel
    logPanel_.Render(state_);
}

void Application::Update()
{
    UpdateProgress_();
}
