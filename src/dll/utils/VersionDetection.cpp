#include "VersionDetection.h"

#include <vector>
#include <Windows.h>

namespace {
    uint64_t GetAssemblyVersion(HMODULE module) {
        wchar_t pathBuffer[MAX_PATH];
        if (!GetModuleFileNameW(module, pathBuffer, MAX_PATH)) {
            return 0;
        }

        DWORD handle = 0;
        const DWORD verSize = GetFileVersionInfoSizeW(pathBuffer, &handle);
        if (verSize == 0) {
            return 0;
        }

        std::vector<BYTE> verData(verSize);
        UINT size = 0;
        VS_FIXEDFILEINFO* verInfo = nullptr;

        if (GetFileVersionInfoW(pathBuffer, 0, verSize, verData.data()) &&
            VerQueryValueW(verData.data(), L"\\", reinterpret_cast<LPVOID*>(&verInfo), &size) &&
            size >= sizeof(VS_FIXEDFILEINFO) &&
            verInfo && verInfo->dwSignature == 0xfeef04bd) {
            uint64_t value = (static_cast<uint64_t>(verInfo->dwFileVersionMS) << 32);
            value |= verInfo->dwFileVersionLS;
            return value;
        }
        return 0;
    }

    uint16_t DetermineGameVersion() {
        const auto fileVersion = GetAssemblyVersion(nullptr);
        const auto major = static_cast<uint16_t>((fileVersion >> 48) & 0xFFFF);
        const auto minor = static_cast<uint16_t>((fileVersion >> 32) & 0xFFFF);
        const auto revision = static_cast<uint16_t>((fileVersion >> 16) & 0xFFFF);
        const auto build = static_cast<uint16_t>(fileVersion & 0xFFFF);

        uint16_t version = 0;

        // Official builds use 1.1.x.y where x is the game version we care about (e.g., 641).
        if (fileVersion != 0 && major == 1 && minor == 1) {
            version = revision;
        }

        // Fallback: sentinel byte heuristic from SC4Fix
        if (version == 0) {
            const volatile uint8_t sentinel = *reinterpret_cast<uint8_t*>(0x6E5000);
            switch (sentinel) {
            case 0x8B: version = 610; break; // 610/613 indistinguishable
            case 0xFF: version = 638; break;
            case 0x24: version = 640; break;
            case 0x0F: version = 641; break;
            default:   version = 0;   break;
            }
        }

        // Build number may disambiguate if needed later; currently unused.
        (void)build;
        return version;
    }
}

VersionDetection& VersionDetection::GetInstance() {
    static VersionDetection instance;
    return instance;
}

uint16_t VersionDetection::GetGameVersion() const noexcept {
    return gameVersion_;
}

VersionDetection::VersionDetection()
    : gameVersion_(DetermineGameVersion()) {
}

