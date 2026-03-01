#pragma once

#include <cstdint>

// Detects the running SimCity 4 executable version (e.g., 641).
class VersionDetection {
public:
    static VersionDetection& GetInstance();

    [[nodiscard]] uint16_t GetGameVersion() const noexcept;

private:
    VersionDetection();

    uint16_t gameVersion_;
};

