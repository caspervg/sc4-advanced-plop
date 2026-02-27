#pragma once
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "rfl/Bytestring.hpp"
#include "rfl/Hex.hpp"
#include "rfl/TaggedUnion.hpp"
#include "rfl/Timestamp.hpp"

struct PreRendered {
    rfl::Bytestring data;  // RGBA32 pixel data (width * height * 4 bytes)
    uint32_t width;
    uint32_t height;
};

struct Icon {
    rfl::Bytestring data;  // RGBA32 pixel data (width * height * 4 bytes)
    uint32_t width;
    uint32_t height;
};

using Thumbnail = rfl::TaggedUnion<"type", PreRendered, Icon>;

struct Lot {
    rfl::Hex<uint32_t> instanceId;
    rfl::Hex<uint32_t> groupId;

    std::string name;

    uint8_t sizeX;
    uint8_t sizeZ;

    uint16_t minCapacity;
    uint16_t maxCapacity;

    uint8_t growthStage;

    std::optional<uint8_t> zoneType;      // LotConfigPropertyZoneTypes (0x88edc793)
    std::optional<uint8_t> wealthType;    // LotConfigPropertyWealthTypes (0x88edc795)
    std::optional<uint8_t> purposeType;   // LotConfigPropertyPurposeTypes (0x88edc796)
};

struct Building {
    rfl::Hex<uint32_t> instanceId;
    rfl::Hex<uint32_t> groupId;

    std::string name;
    std::string description;

    std::unordered_set<uint32_t> occupantGroups;

    std::optional<Thumbnail> thumbnail;

    std::vector<Lot> lots;
};

struct Prop {
    rfl::Hex<uint32_t> groupId;
    rfl::Hex<uint32_t> instanceId;

    std::string exemplarName;
    std::string visibleName;

    float width;
    float height;
    float depth;
    std::vector<rfl::Hex<uint32_t>> familyIds;

    std::optional<Thumbnail> thumbnail;
};

struct PropFamilyInfo {
    rfl::Hex<uint32_t> familyId;
    std::string displayName;
};

struct PropsCache {
    uint32_t version = 1;
    std::vector<Prop> props;
    std::vector<PropFamilyInfo> propFamilies;
};

// Used internally at runtime to represent a resolved prop for painting.
struct PaletteEntry {
    rfl::Hex<uint32_t> propID;
    float weight = 1.0f;
};

// Per-prop configuration stored inside a FamilyEntry.
// For game-family entries: overrides (excluded, weight, pinned).
// For manual palette entries: defines the full prop list (pinned unused).
struct FamilyPropConfig {
    rfl::Hex<uint32_t> propID;
    float weight = 1.0f;
    bool excluded = false;  // game-family: exclude this prop from painting
    bool pinned = false;    // game-family: force-include even if not in the family
};

// A Families-tab entry. Covers both live game families and user-created manual palettes.
struct FamilyEntry {
    std::string name;
    bool starred = false;
    // Set for game-family entries; absent for manual palettes.
    std::optional<rfl::Hex<uint32_t>> familyId;
    // Game-family: per-prop overrides (weight, exclude, pin).
    // Manual palette: the full prop list.
    std::vector<FamilyPropConfig> propConfigs;
    float densityVariation = 0.0f;
};

struct TabFavorites {
    std::vector<rfl::Hex<uint64_t>> items;
};

struct AllFavorites {
    uint32_t version = 3;
    TabFavorites lots;
    std::optional<TabFavorites> props;  // Future: prop favorites
    std::optional<TabFavorites> flora;  // Future: flora favorites
    // Game-family entries with overrides + user-created manual palettes.
    // Unmodified game families are not stored; they are derived from the props cache.
    std::optional<std::vector<FamilyEntry>> families;
    rfl::Timestamp<"%Y-%m-%dT%H:%M:%S"> lastModified;
};
