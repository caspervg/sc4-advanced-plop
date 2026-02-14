#pragma once
#include "LotFilterHelper.hpp"
#include "PanelTab.hpp"
#include "ThumbnailCache.hpp"
#include "public/ImGuiTexture.h"

/**
 * Buildings tab with master-detail layout:
 * - Top: Buildings table with virtualized scrolling
 * - Bottom: Lots detail table for selected building
 */
class BuildingsPanelTab : public PanelTab {
public:
    explicit BuildingsPanelTab(SC4AdvancedLotPlopDirector* director, cIGZImGuiService* imguiService)
        : PanelTab(director, imguiService) {}

    ~BuildingsPanelTab() override = default;

    [[nodiscard]] const char* GetTabName() const override;
    void OnRender() override;
    void OnDeviceReset(uint32_t deviceGeneration) override;
    void OnShutdown() override { thumbnailCache_.Clear(); }

private:
    ImGuiTexture LoadBuildingTexture_(uint64_t buildingKey);

    void RenderFilterUI_();
    void RenderBuildingsTable_(float tableHeight);
    void RenderLotsDetailTable_(float tableHeight);
    void RenderBuildingRow_(const Building& building, bool isSelected);
    void RenderLotRow_(const Lot& lot);
    void RenderOccupantGroupFilter_();
    void RenderFavButton_(uint32_t lotInstanceId) const;

    void ApplyFilters_();

private:
    ThumbnailCache<uint64_t> thumbnailCache_;
    uint32_t lastDeviceGeneration_{0};

    const Building* selectedBuilding_ = nullptr;
    std::vector<const Building*> filteredBuildings_;

    // Filter state (similar to LotFilterHelper but for buildings)
    std::string searchBuffer_;
    std::unordered_set<uint32_t> selectedOccupantGroups_;
    std::optional<uint8_t> selectedZoneType_;
    std::optional<uint8_t> selectedWealthType_;
    std::optional<uint8_t> selectedGrowthStage_;
    bool favoritesOnly_ = false;

    // Size filters
    int minSizeX_ = LotSize::kMinSize;
    int maxSizeX_ = LotSize::kMaxSize;
    int minSizeZ_ = LotSize::kMinSize;
    int maxSizeZ_ = LotSize::kMaxSize;

    // Sorting
    bool sortDescending_ = false;
};