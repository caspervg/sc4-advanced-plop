#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/logger.h>

namespace fs = std::filesystem;

struct PluginConfiguration;
struct Lot;

struct ScanResults {
    uint32_t buildingsFound = 0;
    uint32_t lotsFound = 0;
    uint32_t parseErrors = 0;
    fs::path outputPath;
    std::string errorMessage;
};

struct ScanServiceProgress {
    uint32_t totalFiles = 0;
    uint32_t processedFiles = 0;
    uint32_t entriesIndexed = 0;
    uint32_t buildingsFound = 0;
    uint32_t lotsFound = 0;
    uint32_t parseErrors = 0;
    bool done = false;
};

class ScanService {
public:
    ScanService(const PluginConfiguration& config,
                spdlog::logger& logger,
                bool renderThumbnails);
    ~ScanService();

    // Non-copyable, non-movable
    ScanService(const ScanService&) = delete;
    ScanService& operator=(const ScanService&) = delete;

    void start();
    void cancel();
    bool isRunning() const;
    ScanServiceProgress getProgress() const;
    ScanResults getResults() const;

private:
    const PluginConfiguration& config_;
    spdlog::logger& logger_;
    bool renderThumbnails_;

    std::unique_ptr<std::thread> scanThread_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> shouldCancel_{false};

    mutable std::mutex progressMutex_;
    ScanServiceProgress progress_;

    mutable std::mutex resultsMutex_;
    ScanResults results_;

    void ScanThread_();
};
