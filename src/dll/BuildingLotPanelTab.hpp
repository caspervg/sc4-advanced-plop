#pragma once
#include "LotFilterHelper.hpp"
#include "PanelTab.hpp"
#include "public/ImGuiTexture.h"


class BuildingLotPanelTab : public PanelTab {
public:
    explicit BuildingLotPanelTab(SC4AdvancedLotPlopDirector* director, cIGZImGuiService* imguiService)
        : PanelTab(director, imguiService) {}

    ~BuildingLotPanelTab() override = default;

    [[nodiscard]] const char* GetTabName() const override;
    void OnRender() override;
    void OnDeviceReset(uint32_t deviceGeneration) override;

private:
    void LoadIconTexture_(uint32_t buildingInstanceId, const Building& building);

    void RenderFilterUI_();
    void RenderTable_();

    void RenderTableInternal_(const std::vector<LotView>& filteredLots,
                              const std::unordered_set<uint32_t>& favorites);

    void RenderFavButton_(uint32_t lotInstanceId) const;
    void RenderOccupantGroupFilter_();

private:
    std::unordered_map<uint32_t, ImGuiTexture> iconCache_;
    uint32_t lastDeviceGeneration_{0};
    bool texturesLoaded_ = false;
    bool isOpen_ = false;
    std::unordered_set<uint32_t> openBuildings_;

    // Filter helper
    LotFilterHelper filterHelper_;
    std::vector<LotFilterHelper::SortSpec> sortSpecs_ = {
        {LotFilterHelper::SortColumn::Name, false}
    };
};
