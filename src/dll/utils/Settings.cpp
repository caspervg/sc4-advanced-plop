#include "Settings.h"
#include "Logger.h"

#include "mini/ini.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <string>

namespace {
    constexpr spdlog::level::level_enum kDefaultLogLevel = spdlog::level::info;
    constexpr bool kDefaultLogToFile = true;
    constexpr bool kDefaultEnableDrawOverlay = true;

    const std::string kSectionName = "SC4PlopAndPaint";

    std::string ToLower(const std::string& value) {
        std::string normalized(value);
        std::ranges::transform(normalized, normalized.begin(),
                       [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return normalized;
    }

    spdlog::level::level_enum ParseLogLevel(const std::string& value, bool& valid) {
        const std::string normalized = ToLower(value);

        if (normalized == "trace") { valid = true; return spdlog::level::trace; }
        if (normalized == "debug") { valid = true; return spdlog::level::debug; }
        if (normalized == "info") { valid = true; return spdlog::level::info; }
        if (normalized == "warn" || normalized == "warning") { valid = true; return spdlog::level::warn; }
        if (normalized == "error") { valid = true; return spdlog::level::err; }
        if (normalized == "critical") { valid = true; return spdlog::level::critical; }
        if (normalized == "off") { valid = true; return spdlog::level::off; }

        valid = false;
        return kDefaultLogLevel;
    }

    bool ParseBool(const std::string& value, bool& valid) {
        const std::string normalized = ToLower(value);

        if (normalized == "true" || normalized == "1" || normalized == "yes") {
            valid = true;
            return true;
        }
        if (normalized == "false" || normalized == "0" || normalized == "no") {
            valid = true;
            return false;
        }

        valid = false;
        return false;
    }

} // namespace

Settings::Settings()
    : logLevel_(kDefaultLogLevel)
    , logToFile_(kDefaultLogToFile)
    , enableDrawOverlay_(kDefaultEnableDrawOverlay) {}

void Settings::Load(const std::filesystem::path& settingsFilePath) {
    // Reset to defaults
    *this = Settings();

    try {
        const mINI::INIFile file(settingsFilePath.string());
        mINI::INIStructure ini;

        if (!file.read(ini)) {
            LOG_INFO("Using default settings, no configuration file detected: {}", settingsFilePath.string());
            return;
        }

        if (!ini.has(kSectionName)) {
            LOG_INFO("Using default settings, section [{}] missing in {}", kSectionName, settingsFilePath.string());
            return;
        }
        auto section = ini.get(kSectionName);

        // LogLevel
        if (section.has("LogLevel")) {
            bool valid = false;
            const std::string text = section.get("LogLevel");
            logLevel_ = ParseLogLevel(text, valid);
            if (!valid) {
                logLevel_ = kDefaultLogLevel;
                LOG_ERROR("Invalid LogLevel value '{}' in {}. Using default info.", text, settingsFilePath.string());
            }
        }

        // LogToFile
        if (section.has("LogToFile")) {
            bool valid = false;
            const std::string text = section.get("LogToFile");
            logToFile_ = ParseBool(text, valid);
            if (!valid) {
                logToFile_ = kDefaultLogToFile;
                LOG_ERROR("Invalid LogToFile value '{}' in {}. Using default true.", text, settingsFilePath.string());
            }
        }

        // EnableDrawOverlay
        if (section.has("EnableDrawOverlay")) {
            bool valid = false;
            const std::string text = section.get("EnableDrawOverlay");
            enableDrawOverlay_ = ParseBool(text, valid);
            if (!valid) {
                enableDrawOverlay_ = kDefaultEnableDrawOverlay;
                LOG_ERROR("Invalid EnableDrawOverlay value '{}' in {}. Using default true.", text,
                          settingsFilePath.string());
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error reading settings file {}: {}", settingsFilePath.string(), e.what());
        *this = Settings();
    }
}

spdlog::level::level_enum Settings::GetLogLevel() const noexcept { return logLevel_; }
bool Settings::GetLogToFile() const noexcept { return logToFile_; }
bool Settings::GetEnableDrawOverlay() const noexcept { return enableDrawOverlay_; }
