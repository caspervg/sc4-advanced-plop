#pragma once

#include <memory>

#include "AppState.hpp"
#include "panels/ConfigPanel.hpp"
#include "panels/LogPanel.hpp"
#include "panels/ProgressPanel.hpp"

class ScanService;

class Application {
public:
    Application();
    ~Application();

    void Render();
    void Update();

private:
    AppState state_;
    std::unique_ptr<ScanService> scanService_;

    ConfigPanel configPanel_;
    ProgressPanel progressPanel_;
    LogPanel logPanel_;

    void StartScan_();
    void UpdateProgress_();
    void RenderResultsSummary_();
};
