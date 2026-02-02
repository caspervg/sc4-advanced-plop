#pragma once

#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

#include "../shared/index.hpp"
#include "services/ScanService.hpp"

namespace fs = std::filesystem;

enum class ScanState { Idle, Scanning, Complete, Error };

struct AppState {
    // Configuration
    PluginConfiguration pluginConfig;
    bool renderThumbnails = false;

    // Runtime state
    ScanState scanState = ScanState::Idle;
    uint32_t totalFiles = 0;
    uint32_t processedFiles = 0;
    uint32_t entriesIndexed = 0;
    uint32_t buildingsFound = 0;
    uint32_t lotsFound = 0;
    uint32_t parseErrors = 0;
    std::string currentFile;
    ScanResults scanResults;

    // Logs (thread-safe)
    std::mutex logMutex;
    std::deque<std::string> logMessages;  // Max 1000 entries

    void AppendLog(const std::string& msg)
    {
        std::lock_guard lock(logMutex);
        logMessages.push_back(msg);
    }
};
