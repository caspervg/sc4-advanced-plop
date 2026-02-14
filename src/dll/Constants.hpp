#pragma once

#include <cstddef>

// UI Constants for consistent sizing across panels
namespace UI {
    constexpr auto kSearchBarWidth = 150.0f;
    constexpr auto kSliderWidth = 50.0f;
    constexpr auto kSlider2Width = 100.0f;
    constexpr auto kDropdownWidth = 100.0f;
    constexpr auto kIconColumnWidth = 45.0f;
    constexpr auto kIconSize = 44.0f;
    constexpr auto kNameColumnWidth = 100.0f;
    constexpr auto kSizeColumnWidth = 10.0f;
    constexpr auto kActionColumnWidth = 75.0f;
    constexpr auto kTableHeight = 400.0f;
    constexpr auto kMeterFloatFormat = "%.1f m";
}

constexpr auto kMaxIconsToLoadPerFrame = 50;

// Thumbnail cache configuration for virtualized scrolling
namespace ThumbnailCacheConfig {
    constexpr size_t kMaxCacheSize = 200;      // Max textures in memory (~1.5MB for 44x44 RGBA)
    constexpr size_t kMaxLoadPerFrame = 10;    // Load rate limit per frame
    constexpr int kPrefetchMargin = 5;         // Items above/below visible to prefetch
}