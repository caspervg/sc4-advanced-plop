#include <args.hxx>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <string_view>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fstream>

#include "services/DbpfIndexService.hpp"
#include "DBPFReader.h"
#include "services/ExemplarParser.hpp"
#include "services/PluginLocator.hpp"
#include "services/PropertyMapper.hpp"
#include "services/ScanService.hpp"
#include "../shared/entities.hpp"
#include "../shared/index.hpp"

#include <rfl/cbor.hpp>

#ifndef SC4_ADVANCED_LOT_PLOP_VERSION
#define SC4_ADVANCED_LOT_PLOP_VERSION "0.0.1"
#endif

namespace fs = std::filesystem;

namespace {

PluginConfiguration GetDefaultPluginConfiguration()
{
    const char* userProfile = std::getenv("USERPROFILE");
    const char* programFiles = std::getenv("PROGRAMFILES(x86)");
    const auto gameRoot = fs::path(programFiles) / "SimCity 4 Deluxe Edition";
    if (userProfile && programFiles) {
        return PluginConfiguration{
            .gameRoot = gameRoot,
            .localeDir = "English",
            .gamePluginsRoot = gameRoot / "Plugins",
            .userPluginsRoot = fs::path(userProfile) / "Documents" / "SimCity 4" / "Plugins"
        };
    }
    return PluginConfiguration{};
}

void ScanAndAnalyzeExemplars(const PluginConfiguration& config,
                             spdlog::logger& logger,
                             bool renderModelThumbnails)
{
    ScanService scanner(config, logger, renderModelThumbnails);
    scanner.start();

    // Wait for scanning to complete and log progress periodically
    using namespace std::chrono_literals;
    while (scanner.isRunning()) {
        auto progress = scanner.getProgress();
        std::this_thread::sleep_for(100ms);
    }

    // Report final results
    auto results = scanner.getResults();
    if (results.errorMessage.empty()) {
        logger.info("Export completed successfully at: {}", results.outputPath.string());
    } else {
        logger.error("Scan failed: {}", results.errorMessage);
    }
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        auto logger = spdlog::stdout_color_mt("lotplop-cli");
        logger->set_level(spdlog::level::debug);
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
        logger->info("SC4AdvancedLotPlop CLI {}", SC4_ADVANCED_LOT_PLOP_VERSION);

        args::ArgumentParser parser("SC4AdvancedLotPlop CLI", "Inspect and extract Lot and Building exemplars from SimCity 4 plugins.");
        args::HelpFlag helpFlag(parser, "help", "Show this help message", {'h', "help"});
        args::Flag versionFlag(parser, "version", "Print version", {"version"});
        args::Flag scanFlag(parser, "scan", "Scan plugins and extract exemplars", {"scan"});
        args::ValueFlag<std::string> gameFlag(parser, "path", "Game root directory (plugins will be in {path}/Plugins)", {"game"});
        args::ValueFlag<std::string> pluginsFlag(parser, "path", "User plugins directory", {"plugins"});
        args::ValueFlag<std::string> localeFlag(parser, "path", "Locale directory under game root (e.g., English)", {"locale"});
        args::Flag renderThumbnailsFlag(parser, "render-thumbnails", "Render 3D thumbnails for buildings without icons", {"render-thumbnails"});

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

            // Override with command-line arguments if provided
            if (gameFlag) {
                config.gameRoot = args::get(gameFlag);
                config.gamePluginsRoot = config.gameRoot / "Plugins";
            }
            if (localeFlag) {
                config.localeDir = args::get(localeFlag);
            }
            if (pluginsFlag) {
                config.userPluginsRoot = args::get(pluginsFlag);
            }

            logger->info("Using plugin configuration:");
            logger->info("  Game Root: {}", config.gameRoot.string());
            logger->info("  Game Locale: {}", (config.gameRoot / config.localeDir).string());
            logger->info("  Game Plugins: {}", config.gamePluginsRoot.string());
            logger->info("  User Plugins: {}", config.userPluginsRoot.string());

            if (renderThumbnailsFlag) {
                logger->info("3D thumbnail rendering enabled");
            }
            ScanAndAnalyzeExemplars(config, *logger, renderThumbnailsFlag);
            return 0;
        }

        // Default behavior - show plugin paths
        auto config = GetDefaultPluginConfiguration();

        // Override with command-line arguments if provided
        if (gameFlag) {
            config.gameRoot = args::get(gameFlag);
            config.gamePluginsRoot = config.gameRoot / "Plugins";
        }
        if (localeFlag) {
            config.localeDir = args::get(localeFlag);
        }
        if (pluginsFlag) {
            config.userPluginsRoot = args::get(pluginsFlag);
        }

        logger->info("Plugin directories:");
        logger->info("  Game Root: {}", config.gameRoot.string());
        logger->info("  Game Locale: {}", (config.gameRoot / config.localeDir).string());
        logger->info("  Game Plugins: {}", config.gamePluginsRoot.string());
        logger->info("  User Plugins: {}", config.userPluginsRoot.string());
        logger->info("Use --scan to scan and extract exemplars");

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }
}
