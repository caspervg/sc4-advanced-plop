#include <args.hxx>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "DBPFReader.h"
#include "ExemplarParser.hpp"
#include "PropertyMapper.hpp"
#include "services/PluginLocator.hpp"
#include "services/DbpfIndexService.hpp"
#include "../shared/index.hpp"

#ifndef SC4_ADVANCED_LOT_PLOP_VERSION
#define SC4_ADVANCED_LOT_PLOP_VERSION "0.0.0"
#endif

namespace fs = std::filesystem;

namespace {

PluginConfiguration GetDefaultPluginConfiguration()
{
#ifdef _WIN32
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        return PluginConfiguration{
            .gameRoot = fs::path(userProfile) / "Documents" / "SimCity 4",
            .gamePluginsRoot = fs::path(userProfile) / "Documents" / "SimCity 4" / "Plugins",
            .userPluginsRoot = fs::path(userProfile) / "Documents" / "SimCity 4" / "Plugins"
        };
    }
#endif

    if (const char* home = std::getenv("HOME")) {
        return PluginConfiguration{
            .gameRoot = fs::path(home) / ".simcity4",
            .gamePluginsRoot = fs::path(home) / ".simcity4" / "plugins",
            .userPluginsRoot = fs::path(home) / ".simcity4" / "plugins"
        };
    }

    return PluginConfiguration{
        .gameRoot = fs::current_path(),
        .gamePluginsRoot = fs::current_path(),
        .userPluginsRoot = fs::current_path()
    };
}

void ScanAndAnalyzeExemplars(const PluginConfiguration& config, spdlog::logger& logger)
{
    try {
        logger.info("Initializing plugin scanner...");

        // Create locator to discover plugin files
        PluginLocator locator(config);

        // Create the index service to scan and index all plugins
        DbpfIndexService indexService(locator);
        logger.info("Starting background indexing service...");
        indexService.start();

        // Wait a bit for initial indexing
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(500ms);

        // Get the indexed DBPF files
        const auto& pluginFiles = indexService.dbpfFiles();
        logger.info("Found {} plugin files", pluginFiles.size());

        if (pluginFiles.empty()) {
            logger.warn("No plugin files found");
            indexService.shutdown();
            return;
        }

        uint32_t buildingsFound = 0;
        uint32_t lotsFound = 0;
        uint32_t parseErrors = 0;

        // Try to load property mapping XML file for better exemplar parsing
        PropertyMapper propertyMapper;
        bool mapperLoaded = false;

        // Try common locations for the property mapper XML
        std::vector<fs::path> mapperLocations{
            fs::path("PropertyMapper.xml"),
            fs::current_path() / "PropertyMapper.xml",
            config.gameRoot / "PropertyMapper.xml"
        };

        for (const auto& loc : mapperLocations) {
            if (fs::exists(loc)) {
                if (propertyMapper.loadFromXml(loc)) {
                    logger.info("Loaded property mapper from: {}", loc.string());
                    mapperLoaded = true;
                    break;
                }
            }
        }

        if (!mapperLoaded) {
            logger.warn("Could not load PropertyMapper XML - some features may be limited");
        }

        ExemplarParser parser(propertyMapper);

        // Process each indexed plugin file
        for (const auto& pluginFile : pluginFiles) {
            try {
                logger.info("Processing: {}", pluginFile.filename().string());

                DBPF::Reader reader;
                if (!reader.LoadFile(pluginFile.string())) {
                    logger.warn("  Failed to load file");
                    parseErrors++;
                    continue;
                }

                const auto& index = reader.GetIndex();

                // Iterate through all entries looking for Exemplars
                // Exemplar type is 0x6534284A
                for (const auto& entry : index) {
                    if (entry.tgi.type != 0x6534284Au) {
                        continue;
                    }

                    auto exemplarResult = reader.LoadExemplar(entry);
                    if (!exemplarResult.has_value()) {
                        continue;
                    }

                    auto exemplarType = parser.getExemplarType(*exemplarResult);
                    if (!exemplarType) {
                        continue;
                    }

                    if (*exemplarType == ExemplarType::Building) {
                        auto building = parser.parseBuilding(reader, entry);
                        if (building) {
                            logger.info("  Building: {} (0x{:08X})", building->name, building->tgi.instance);
                            buildingsFound++;
                        }
                    } else if (*exemplarType == ExemplarType::LotConfig) {
                        auto lot = parser.parseLotConfig(reader, entry);
                        if (lot) {
                            logger.info("  Lot: {} ({}x{}, 0x{:08X})",
                                       lot->name, lot->lotSize.first, lot->lotSize.second,
                                       lot->tgi.instance);
                            lotsFound++;
                        }
                    }
                }

            } catch (const std::exception& error) {
                logger.warn("  Error processing file: {}", error.what());
                parseErrors++;
            }
        }

        logger.info("Scan complete: {} buildings, {} lots, {} errors",
                   buildingsFound, lotsFound, parseErrors);

        // Shutdown the indexing service
        indexService.shutdown();
        logger.info("Index service shutdown");

    } catch (const std::exception& error) {
        logger.error("Error during exemplar scan: {}", error.what());
    }
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        auto logger = spdlog::stdout_color_mt("lotplop-cli");
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
        logger->info("SC4AdvancedLotPlop CLI {}", SC4_ADVANCED_LOT_PLOP_VERSION);

        args::ArgumentParser parser("SC4AdvancedLotPlop CLI", "Inspect and extract Lot and Building exemplars from SimCity 4 plugins.");
        args::HelpFlag helpFlag(parser, "help", "Show this help message", {'h', "help"});
        args::Flag versionFlag(parser, "version", "Print version", {"version"});
        args::Flag scanFlag(parser, "scan", "Scan plugins and extract exemplars", {"scan"});

        try {
            parser.ParseCLI(argc, argv);
        } catch (const args::Completion& e) {
            std::cout << e.what();
        } catch (const args::Help&) {
            std::cout << parser.Help() << std::endl;
            return 0;
        } catch (const args::ParseError& error) {
            std::cerr << error.what() << std::endl;
            std::cerr << parser.Help() << std::endl;
            return 1;
        }

        if (versionFlag) {
            logger->info("Version: {}", SC4_ADVANCED_LOT_PLOP_VERSION);
            return 0;
        }

        if (scanFlag) {
            auto config = GetDefaultPluginConfiguration();
            logger->info("Using plugin configuration:");
            logger->info("  Game Root: {}", config.gameRoot.string());
            logger->info("  Game Plugins: {}", config.gamePluginsRoot.string());
            logger->info("  User Plugins: {}", config.userPluginsRoot.string());

            ScanAndAnalyzeExemplars(config, *logger);
            return 0;
        }

        // Default behavior - show plugin paths
        auto config = GetDefaultPluginConfiguration();
        logger->info("Default plugin directories:");
        logger->info("  Game Root: {}", config.gameRoot.string());
        logger->info("  Game Plugins: {}", config.gamePluginsRoot.string());
        logger->info("  User Plugins: {}", config.userPluginsRoot.string());
        logger->info("Use --scan to scan and extract exemplars");

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }
}
