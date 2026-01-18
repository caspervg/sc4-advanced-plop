#include "DbpfIndexService.hpp"
#include "../../../vendor/DBPFKit/src/DBPFReader.h"
#include <spdlog/spdlog.h>

DbpfIndexService::DbpfIndexService(const PluginLocator& locator) : locator_(locator) {}

DbpfIndexService::~DbpfIndexService() { shutdown(); }

void DbpfIndexService::start() {
    if (running_) {
        return;
    }

    stop_ = false;
    done_ = false;
    totalFiles_ = 0;
    processedFiles_ = 0;
    entriesIndexed_ = 0;
    errorCount_ = 0;

    {
        std::lock_guard lock(mutex_);
        currentFile_.clear();
        files_.clear();
        tgiToFiles_.clear();
        typeInstanceToTgis_.clear();
    }

    running_ = true;
    workerThread_ = std::thread([this] { worker_(); });
}

void DbpfIndexService::shutdown() {
    stop_ = true;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    running_ = false;
}

auto DbpfIndexService::isRunning() const -> bool {
    return running_;
}

auto DbpfIndexService::snapshot() const -> ScanProgress {
    std::lock_guard lock(mutex_);
    return ScanProgress{
        .totalFiles = totalFiles_,
        .processedFiles = processedFiles_,
        .entriesIndexed = entriesIndexed_,
        .errorCount = errorCount_,
        .currentFile = currentFile_,
        .done = done_
    };
}

auto DbpfIndexService::tgiIndex() const -> const std::unordered_map<DBPF::Tgi, std::filesystem::path, DBPF::TgiHash>& {
    // Note: This assumes tgiToFiles_ maps single file per TGI
    // For actual multi-file mapping, would need to refactor
    static const std::unordered_map<DBPF::Tgi, std::filesystem::path, DBPF::TgiHash> empty;
    return empty;
}

auto DbpfIndexService::typeInstanceIndex() const -> const std::unordered_map<uint64_t, std::vector<DBPF::Tgi>>& {
    return typeInstanceToTgis_;
}

auto DbpfIndexService::dbpfFiles() const -> const std::vector<std::filesystem::path>& {
    return files_;
}

auto DbpfIndexService::pluginLocator() const -> const PluginLocator& {
    return locator_;
}

void DbpfIndexService::worker_() {
    try {
        auto pluginFiles = locator_.ListDbpfFiles();

        {
            std::lock_guard lock(mutex_);
            files_ = pluginFiles;
            totalFiles_ = pluginFiles.size();
        }

        for (const auto& filePath : pluginFiles) {
            if (stop_) {
                break;
            }

            {
                std::lock_guard lock(mutex_);
                currentFile_ = filePath.filename().string();
            }

            try {
                DBPF::Reader reader;
                if (!reader.LoadFile(filePath.string())) {
                    errorCount_++;
                    processedFiles_++;
                    continue;
                }

                const auto& index = reader.GetIndex();
                size_t entriesCount = 0;

                for (const auto& entry : index) {
                    if (stop_) break;

                    // Create a type-instance key for indexing
                    uint64_t typeInstanceKey = (static_cast<uint64_t>(entry.tgi.type) << 32) | entry.tgi.instance;

                    {
                        std::lock_guard lock(mutex_);
                        typeInstanceToTgis_[typeInstanceKey].push_back(entry.tgi);
                        tgiToFiles_[entry.tgi].push_back(filePath);
                    }

                    entriesCount++;
                }

                {
                    std::lock_guard lock(mutex_);
                    entriesIndexed_ += entriesCount;
                    processedFiles_++;
                }

            } catch (const std::exception& error) {
                errorCount_++;
                processedFiles_++;
            }
        }

        {
            std::lock_guard lock(mutex_);
            done_ = true;
            currentFile_.clear();
        }

    } catch (const std::exception& error) {
        errorCount_++;
        done_ = true;
    }
}

void DbpfIndexService::publishProgress_() {
    // Could be used to notify observers of progress
    // For now, kept simple - snapshots can be taken with snapshot()
}
