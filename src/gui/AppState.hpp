#pragma once

#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

#include "../shared/index.hpp"

namespace fs = std::filesystem;

enum class ScanState { Idle, Scanning, Complete, Error };

struct ScanProgress {
    uint32_t totalFiles = 0;
    uint32_t processedFiles = 0;
    uint32_t entriesIndexed = 0;
    uint32_t buildingsFound = 0;
    uint32_t lotsFound = 0;
    uint32_t parseErrors = 0;
    bool done = false;
};

struct ScanResults {
    uint32_t buildingsFound = 0;
    uint32_t lotsFound = 0;
    uint32_t parseErrors = 0;
    fs::path outputPath;
    std::string errorMessage;
};

struct AppState {
    // Configuration
    PluginConfiguration pluginConfig;
    bool renderThumbnails = false;

    // Runtime state
    ScanState scanState = ScanState::Idle;
    ScanProgress scanProgress;
    ScanResults scanResults;

    // Logs (thread-safe)
    std::mutex logMutex;
    std::deque<std::string> logMessages;  // Max 1000 entries

    void AppendLog(const std::string& msg)
    {
        std::lock_guard lock(logMutex);
        if (logMessages.size() >= 1000) {
            logMessages.pop_front();
        }
        logMessages.push_back(msg);
    }
};
