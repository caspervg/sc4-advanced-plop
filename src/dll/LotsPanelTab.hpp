#pragma once

#include <vector>
#include "LotFilterHelper.hpp"
#include "PanelTab.hpp"

/**
 * Lots tab with flat table of all lots.
 * Columns: Name, Building Name, Size, Actions
 * Uses ImGuiListClipper for virtualized scrolling.
 */
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
        {LotFilterHelper::SortColumn::Name, false}
    };
};
