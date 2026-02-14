#pragma once
#include "LotFilterHelper.hpp"
#include "PanelTab.hpp"


class LotsPanelTab : public PanelTab {
public:
    explicit LotsPanelTab(SC4AdvancedLotPlopDirector* director, cIGZImGuiService* imguiService)
        : PanelTab(director, imguiService) {}

    ~LotsPanelTab() override = default;

    [[nodiscard]] const char* GetTabName() const override;
    void OnRender() override;
    void OnDeviceReset(uint32_t deviceGeneration) override;

private:
    void RenderFilterUI_();
    void RenderTable_();
    void RenderFavButton_(uint32_t lotInstanceId) const;
    void RenderOccupantGroupFilter_();

private:
    std::vector<LotView> filteredLots_;
    LotFilterHelper filterHelper_;
    std::vector<LotFilterHelper::SortSpec> sortSpecs_ = {
        {LotFilterHelper::SortColumn::BuildingName, false}
    };
};
