#pragma once
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "PluginLocator.hpp"
#include "TGI.h"

struct ScanProgress {
    size_t totalFiles = 0;
    size_t processedFiles = 0;
    size_t entriesIndexed = 0;
    size_t errorCount = 0;
    std::string currentFile;
    bool done = false;
};

class DbpfIndexService {
public:
    explicit DbpfIndexService(const PluginLocator& locator);
    ~DbpfIndexService();

    auto start() -> void;
    auto shutdown() -> void;

    [[nodiscard]] auto isRunning() const -> bool;
    [[nodiscard]] auto snapshot() const -> ScanProgress;
    [[nodiscard]] auto tgiIndex() const -> const std::unordered_map<DBPF::Tgi, std::filesystem::path, DBPF::TgiHash>&;
    [[nodiscard]] auto typeInstanceIndex() const -> const std::unordered_map<uint64_t, std::vector<DBPF::Tgi>>&;
    [[nodiscard]] auto dbpfFiles() const -> const std::vector<std::filesystem::path>&;
    [[nodiscard]] auto pluginLocator() const -> const PluginLocator&;

private:
    auto worker_() -> void;
    auto publishProgress_() -> void;

private:
    PluginLocator locator_;

    mutable std::mutex mutex_;
    std::thread workerThread_;
    std::atomic_bool running_{false};
    std::atomic_bool stop_{false};
    std::atomic_bool done_{false};
    std::atomic<size_t> totalFiles_{0};
    std::atomic<size_t> processedFiles_{0};
    std::atomic<size_t> entriesIndexed_{0};
    std::atomic<size_t> errorCount_{0};

    std::string currentFile_;
    std::vector<std::filesystem::path> files_;
    std::unordered_map<DBPF::Tgi, std::vector<std::filesystem::path>, DBPF::TgiHash> tgiToFiles_;
    std::unordered_map<uint64_t, std::vector<DBPF::Tgi>> typeInstanceToTgis_;
};
