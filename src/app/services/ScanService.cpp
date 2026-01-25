#include "ScanService.hpp"

#include <chrono>
#include <exception>
#include <fstream>
#include <set>

#include <rfl/cbor.hpp>
#include <spdlog/spdlog.h>

#include "DbpfIndexService.hpp"
#include "ExemplarParser.hpp"
#include "PluginLocator.hpp"
#include "PropertyMapper.hpp"
#include "../../shared/entities.hpp"
#include "../../shared/index.hpp"

namespace fs = std::filesystem;

ScanService::ScanService(const PluginConfiguration& config,
                         spdlog::logger& logger,
                         bool renderThumbnails)
    : config_(config), logger_(logger), renderThumbnails_(renderThumbnails)
{
}

ScanService::~ScanService()
{
    cancel();
    if (scanThread_ && scanThread_->joinable()) {
        scanThread_->join();
    }
}

void ScanService::start()
{
    if (isRunning_) {
        return;  // Already running
    }

    isRunning_ = true;
    shouldCancel_ = false;

    scanThread_ = std::make_unique<std::thread>([this] { ScanThread_(); });
}

void ScanService::cancel()
{
    shouldCancel_ = true;
}

bool ScanService::isRunning() const
{
    return isRunning_.load();
}

ScanServiceProgress ScanService::getProgress() const
{
    std::lock_guard lock(progressMutex_);
    return progress_;
}

ScanResults ScanService::getResults() const
{
    std::lock_guard lock(resultsMutex_);
    return results_;
}

void ScanService::ScanThread_()
{
    try {
        logger_.info("Initializing plugin scanner...");

        // Create locator to discover plugin files
        PluginLocator locator(config_);

        // Create and start the index service immediately for parallel indexing
        DbpfIndexService indexService(locator);
        logger_.info("Starting background indexing service...");
        indexService.start();

        // While indexing happens in the background, load the property mapper
        logger_.info("Loading property mapper...");
        PropertyMapper propertyMapper;
        auto mapperLoaded = false;

        // Try common locations for the property mapper XML
        std::vector<fs::path> mapperLocations{
            fs::path("PropertyMapper.xml"),
            fs::current_path() / "PropertyMapper.xml",
            config_.gameRoot / "PropertyMapper.xml"
        };

        for (const auto& loc : mapperLocations) {
            if (fs::exists(loc)) {
                if (propertyMapper.loadFromXml(loc)) {
                    logger_.info("Loaded property mapper from: {}", loc.string());
                    mapperLoaded = true;
                    break;
                }
            }
        }

        if (!mapperLoaded) {
            logger_.warn("Could not load PropertyMapper XML - some features may be limited");
        }

        // Wait for indexing to complete, logging progress periodically
        logger_.info("Waiting for indexing to complete...");
        using namespace std::chrono_literals;
        int logIntervalCount = 0;
        while (true) {
            // Check cancellation
            if (shouldCancel_) {
                logger_.info("Scan cancelled by user");
                indexService.shutdown();
                isRunning_ = false;
                return;
            }

            auto progress = indexService.snapshot();

            // Update progress
            {
                std::lock_guard lock(progressMutex_);
                progress_.totalFiles = progress.totalFiles;
                progress_.processedFiles = progress.processedFiles;
                progress_.entriesIndexed = progress.entriesIndexed;
            }

            // Check if done
            if (progress.done) {
                break;
            }

            std::this_thread::sleep_for(100ms);

            // Log progress every 2 seconds
            if (++logIntervalCount % 20 == 0) {
                logger_.info("  Indexing progress: {}/{} files processed, {} entries indexed",
                           progress.processedFiles, progress.totalFiles, progress.entriesIndexed);
            }
        }

        // Log final indexing results
        auto finalProgress = indexService.snapshot();
        logger_.info("Indexing complete: {} files processed, {} entries indexed, {} errors",
                   finalProgress.processedFiles, finalProgress.totalFiles, finalProgress.errorCount);

        uint32_t buildingsFound = 0;
        uint32_t lotsFound = 0;
        uint32_t parseErrors = 0;
        std::set<uint32_t> missingBuildingIds;

        ExemplarParser parser(propertyMapper, &indexService, renderThumbnails_);
        std::vector<Lot> allLots;
        std::unordered_map<uint32_t, ParsedBuildingExemplar> buildingMap;

        // Use the index service to get all exemplars across all files
        logger_.info("Processing exemplars using type index...");
        const auto& tgiIndex = indexService.tgiIndex();
        const auto exemplarTgis = indexService.typeIndex(0x6534284Au);

        logger_.info("Found {} exemplars to process", exemplarTgis.size());

        // Group exemplar TGIs by file for efficient batch processing
        std::unordered_map<fs::path, std::vector<DBPF::Tgi>> fileToExemplarTgis;
        for (const auto& tgi : exemplarTgis) {
            const auto& filePaths = tgiIndex.at(tgi);
            // Add this TGI to the first file that contains it
            if (!filePaths.empty()) {
                fileToExemplarTgis[filePaths[0]].push_back(tgi);
            }
        }

        size_t filesProcessed = 0;

        // Store lot config TGIs for second pass
        std::vector<std::pair<fs::path, DBPF::Tgi>> lotConfigTgis;

        for (const auto& [filePath, tgis] : fileToExemplarTgis) {
            // Check cancellation
            if (shouldCancel_) {
                logger_.info("Scan cancelled by user");
                indexService.shutdown();
                isRunning_ = false;
                return;
            }

            try {
                // Get cached reader from index service
                auto* reader = indexService.getReader(filePath);
                if (!reader) {
                    logger_.warn("Failed to get reader for file: {}", filePath.string());
                    continue;
                }

                logger_.debug("Processing {} exemplars from {}", tgis.size(), filePath.filename().string());

                // Process all exemplars in this file
                for (const auto& tgi : tgis) {
                    try {
                        auto exemplarResult = reader->LoadExemplar(tgi);
                        if (!exemplarResult.has_value()) {
                            continue;
                        }

                        auto exemplarType = parser.getExemplarType(*exemplarResult);
                        if (!exemplarType) {
                            continue;
                        }

                        if (*exemplarType == ExemplarType::Building) {
                            auto building = parser.parseBuilding(*exemplarResult, tgi);
                            if (building) {
                                buildingMap[tgi.instance] = *building;
                                buildingsFound++;
                                logger_.trace("  Building: {} (0x{:08X})", building->name, tgi.instance);
                            }
                        } else if (*exemplarType == ExemplarType::LotConfig) {
                            // Queue for second pass
                            lotConfigTgis.emplace_back(filePath, tgi);
                        }

                    } catch (const std::exception& error) {
                        logger_.debug("Error processing TGI {}/{}/{}: {}",
                                   tgi.type, tgi.group, tgi.instance, error.what());
                        parseErrors++;
                    }
                }

                filesProcessed++;

                // Update progress
                {
                    std::lock_guard lock(progressMutex_);
                    progress_.buildingsFound = buildingsFound;
                    progress_.parseErrors = parseErrors;
                }

                // Log progress periodically
                if (filesProcessed % 100 == 0) {
                    logger_.info("  Processed {}/{} files ({} buildings found so far)",
                               filesProcessed, fileToExemplarTgis.size(), buildingsFound);
                }

            } catch (const std::exception& error) {
                logger_.warn("Error processing file {}: {}", filePath.filename().string(), error.what());
            }
        }

        // Build family-to-buildings map for resolving growable lot references
        std::unordered_map<uint32_t, std::vector<uint32_t>> familyToBuildingsMap;
        for (const auto& [instanceId, building] : buildingMap) {
            for (uint32_t familyId : building.familyIds) {
                familyToBuildingsMap[familyId].push_back(instanceId);
            }
        }

        for (const auto& [filePath, tgi] : lotConfigTgis) {
            // Check cancellation
            if (shouldCancel_) {
                logger_.info("Scan cancelled by user");
                indexService.shutdown();
                isRunning_ = false;
                return;
            }

            try {
                auto* reader = indexService.getReader(filePath);
                if (!reader) {
                    continue;
                }

                auto exemplarResult = reader->LoadExemplar(tgi);
                if (!exemplarResult.has_value()) {
                    continue;
                }

                if (auto parsedLot = parser.parseLotConfig(*exemplarResult, tgi, buildingMap, familyToBuildingsMap)) {
                    // Get building for this lot
                    auto buildingIt = buildingMap.find(parsedLot->buildingInstanceId);
                    if (buildingIt != buildingMap.end()) {
                        Building building = parser.buildingFromParsed(buildingIt->second);
                        Lot lot = parser.lotFromParsed(*parsedLot, building);
                        if (parsedLot->isFamilyReference) {
                            logger_.trace("  Lot: {} (0x{:08X}) [family 0x{:08X} -> building 0x{:08X}]",
                                       lot.name, lot.instanceId.get(),
                                       parsedLot->buildingFamilyId, parsedLot->buildingInstanceId);
                        } else {
                            logger_.trace("  Lot: {} (0x{:08X})", lot.name, lot.instanceId.get());
                        }
                        allLots.push_back(lot);
                        lotsFound++;
                    } else {
                        if (parsedLot->isFamilyReference) {
                            logger_.warn("  Lot {} references family 0x{:08X} but resolved building 0x{:08X} not found",
                                       parsedLot->name, parsedLot->buildingFamilyId, parsedLot->buildingInstanceId);
                        } else {
                            logger_.warn("  Lot {} references unknown building 0x{:08X}",
                                       parsedLot->name, parsedLot->buildingInstanceId);
                        }
                        missingBuildingIds.insert(parsedLot->buildingInstanceId);
                    }
                }
            } catch (const std::exception& error) {
                logger_.debug("Error processing lot config TGI {}/{}/{}: {}",
                           tgi.type, tgi.group, tgi.instance, error.what());
                parseErrors++;
            }
        }

        if (!missingBuildingIds.empty()) {
            logger_.warn("Missing building references for {} lots:", missingBuildingIds.size());
        }

        logger_.info("Scan complete: {} buildings, {} lots, {} parse errors",
                   buildingsFound, lotsFound, parseErrors);

        // Update final progress
        {
            std::lock_guard lock(progressMutex_);
            progress_.buildingsFound = buildingsFound;
            progress_.lotsFound = lotsFound;
            progress_.parseErrors = parseErrors;
            progress_.done = true;
        }

        // Export lot config data to CBOR file in user plugins directory
        if (!allLots.empty()) {
            try {
                auto cborPath = config_.userPluginsRoot / "lot_configs.cbor";
                fs::create_directories(config_.userPluginsRoot);

                logger_.info("Exporting {} unique lot configs to {}", allLots.size(), cborPath.string());

                if (std::ofstream file(cborPath, std::ios::binary); !file) {
                    logger_.error("Failed to open file for writing: {}", cborPath.string());
                    {
                        std::lock_guard lock(resultsMutex_);
                        results_.errorMessage = "Failed to open output file for writing";
                    }
                } else {
                    rfl::cbor::write(allLots, file);
                    file.close();
                    logger_.info("Successfully exported lot configs");

                    std::lock_guard lock(resultsMutex_);
                    results_.outputPath = cborPath;
                    results_.buildingsFound = buildingsFound;
                    results_.lotsFound = lotsFound;
                    results_.parseErrors = parseErrors;
                }
            } catch (const std::exception& error) {
                logger_.error("Error exporting lot configs: {}", error.what());
                std::lock_guard lock(resultsMutex_);
                results_.errorMessage = std::string("Error exporting lot configs: ") + error.what();
            }
        }

        // Shutdown the indexing service
        indexService.shutdown();
        isRunning_ = false;

    } catch (const std::exception& error) {
        logger_.error("Error during exemplar scan: {}", error.what());
        std::lock_guard lock(resultsMutex_);
        results_.errorMessage = std::string("Error during scan: ") + error.what();
        isRunning_ = false;
    }
}
