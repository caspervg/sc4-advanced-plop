#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include <spdlog/common.h>

class Settings {
public:
    Settings();

    void Load(const std::filesystem::path& settingsFilePath);

    // Logging
    [[nodiscard]] spdlog::level::level_enum GetLogLevel() const noexcept;
    [[nodiscard]] bool GetLogToFile() const noexcept;
    [[nodiscard]] bool GetEnableDrawOverlay() const noexcept;

private:
    spdlog::level::level_enum logLevel_;
    bool logToFile_;
    bool enableDrawOverlay_;
};
