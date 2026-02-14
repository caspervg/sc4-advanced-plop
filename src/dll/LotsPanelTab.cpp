#include "LotsPanelTab.hpp"

#include "Constants.hpp"
#include "OccupantGroups.hpp"
#include "Utils.hpp"

const char* LotsPanelTab::GetTabName() const {
    return "Lots";
}

void LotsPanelTab::OnRender() {
    const auto& buildings = director_->GetBuildings();

    if (buildings.empty()) {
        ImGui::TextUnformatted("No lots loaded. Please ensure lot_configs.cbor exists in the Plugins directory.");
        return;
    }

    RenderFilterUI_();
    ImGui::Separator();

    // Build lot views from all buildings
    std::vector<LotView> lotViews;
    size_t totalLots = 0;
    for (const auto& b : buildings) totalLots += b.lots.size();
    lotViews.reserve(totalLots);
    for (const auto& b : buildings) {
        for (const auto& lot : b.lots) {
            lotViews.push_back(LotView{&b, &lot});
        }
    }

    // Apply filters and sort
    const auto filteredLots = filterHelper_.ApplyFiltersAndSort(
        lotViews, director_->GetFavoriteLotIds(), sortSpecs_
    );

    ImGui::Text("Showing %zu of %zu lots", filteredLots.size(), lotViews.size());

    RenderTable_();
}

void LotsPanelTab::OnDeviceReset(const uint32_t deviceGeneration) {
    (void)deviceGeneration;
    // No textures to reset in this tab
}

void LotsPanelTab::RenderFilterUI_() {
    static char searchBuf[256] = {};
    if (filterHelper_.searchBuffer != searchBuf) {
        std::strncpy(searchBuf, filterHelper_.searchBuffer.c_str(), sizeof(searchBuf) - 1);
        searchBuf[sizeof(searchBuf) - 1] = '\0';
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##SearchLots", "Search lots and buildings...", searchBuf, sizeof(searchBuf))) {
        filterHelper_.searchBuffer = searchBuf;
    }

    // Zone type filter
    const char* zoneTypes[] = {
        "Any zone", "Residential (R)", "Commercial (C)", "Industrial (I)", "Plopped", "None", "Other"
    };
    int currentZone = filterHelper_.selectedZoneType.has_value() ? (filterHelper_.selectedZoneType.value() + 1) : 0;
    ImGui::SetNextItemWidth(UI::kDropdownWidth);
    if (ImGui::Combo("##ZoneType", &currentZone, zoneTypes, 7)) {
        if (currentZone == 0) {
            filterHelper_.selectedZoneType.reset();
        }
        else {
            filterHelper_.selectedZoneType = static_cast<uint8_t>(currentZone - 1);
        }
    }

    ImGui::SameLine();

    // Wealth filter
    const char* wealthOptions[] = {"Any wealth", "$", "$$", "$$$"};
    int currentWealth = filterHelper_.selectedWealthType.has_value() ? filterHelper_.selectedWealthType.value() : 0;
    ImGui::SetNextItemWidth(UI::kDropdownWidth);
    if (ImGui::Combo("##Wealth", &currentWealth, wealthOptions, 4)) {
        if (currentWealth == 0) {
            filterHelper_.selectedWealthType.reset();
        }
        else {
            filterHelper_.selectedWealthType = static_cast<uint8_t>(currentWealth);
        }
    }

    ImGui::SameLine();

    // Growth stage filter
    const char* growthStages[] = {
        "Any stage", "Plopped (255)", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
        "15"
    };
    auto currentGrowthStageIndex = 0;
    if (filterHelper_.selectedGrowthStage.has_value()) {
        uint8_t val = filterHelper_.selectedGrowthStage.value();
        if (val == 255) {
            currentGrowthStageIndex = 1;
        }
        else if (val <= 15) {
            currentGrowthStageIndex = val + 2;
        }
    }
    ImGui::SetNextItemWidth(UI::kDropdownWidth);
    if (ImGui::Combo("##GrowthStage", &currentGrowthStageIndex, growthStages, 18)) {
        if (currentGrowthStageIndex == 0) {
            filterHelper_.selectedGrowthStage.reset();
        }
        else if (currentGrowthStageIndex == 1) {
            filterHelper_.selectedGrowthStage = 255;
        }
        else {
            filterHelper_.selectedGrowthStage = static_cast<uint8_t>(currentGrowthStageIndex - 2);
        }
    }

    ImGui::SameLine();
    ImGui::Checkbox("Favorites only", &filterHelper_.favoritesOnly);

    // Size filters
    ImGui::Text("Width:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::InputInt("##MinSizeX", &filterHelper_.minSizeX, 1, 1)) {
        filterHelper_.minSizeX = std::clamp(filterHelper_.minSizeX, LotSize::kMinSize, LotSize::kMaxSize);
    }
    ImGui::SameLine();
    ImGui::Text("to");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::InputInt("##MaxSizeX", &filterHelper_.maxSizeX, 1, 1)) {
        filterHelper_.maxSizeX = std::clamp(filterHelper_.maxSizeX, LotSize::kMinSize, LotSize::kMaxSize);
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    ImGui::Text("Depth:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::InputInt("##MinSizeZ", &filterHelper_.minSizeZ, 1, 1)) {
        filterHelper_.minSizeZ = std::clamp(filterHelper_.minSizeZ, LotSize::kMinSize, LotSize::kMaxSize);
    }
    ImGui::SameLine();
    ImGui::Text("to");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::InputInt("##MaxSizeZ", &filterHelper_.maxSizeZ, 1, 1)) {
        filterHelper_.maxSizeZ = std::clamp(filterHelper_.maxSizeZ, LotSize::kMinSize, LotSize::kMaxSize);
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    if (ImGui::Button("Clear filters")) {
        filterHelper_.ResetFilters();
        searchBuf[0] = '\0';
    }

    ImGui::Separator();
    RenderOccupantGroupFilter_();
}

void LotsPanelTab::RenderTable_() {
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_SortMulti | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("AllLotsTable", 4, tableFlags, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Lot",
                                ImGuiTableColumnFlags_NoHide |
                                ImGuiTableColumnFlags_DefaultSort |
                                ImGuiTableColumnFlags_PreferSortAscending);
        ImGui::TableSetupColumn("Building",
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size",
                                ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_PreferSortAscending,
                                UI::kSizeColumnWidth);
        ImGui::TableSetupColumn("Action",
                                ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_NoSort,
                                UI::kActionColumnWidth);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Handle sorting
        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0) {
            std::vector<LotFilterHelper::SortSpec> newSpecs;
            newSpecs.reserve(specs->SpecsCount);
            for (auto i = 0; i < specs->SpecsCount; ++i) {
                const auto& s = specs->Specs[i];
                switch (s.ColumnIndex) {
                case 0: // Lot name
                    newSpecs.push_back({
                        LotFilterHelper::SortColumn::LotName,
                        s.SortDirection == ImGuiSortDirection_Descending
                    });
                    break;
                case 1: // Building Name - both use Name sort
                    newSpecs.push_back({
                        LotFilterHelper::SortColumn::BuildingName,
                        s.SortDirection == ImGuiSortDirection_Descending
                    });
                    break;
                case 2: // Size
                    newSpecs.push_back({
                        LotFilterHelper::SortColumn::Size,
                        s.SortDirection == ImGuiSortDirection_Descending
                    });
                    break;
                default:
                    break;
                }
            }
            if (!newSpecs.empty()) {
                sortSpecs_ = std::move(newSpecs);
                specs->SpecsDirty = false;
            }
        }

        // Use clipper for virtualized scrolling
        const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filteredLots_.size()), rowHeight);

        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto& view = filteredLots_[i];

                ImGui::PushID(static_cast<int>(MakeGIKey(view.lot->groupId.value(), view.lot->instanceId.value())));
                ImGui::TableNextRow();

                // Lot Name
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(view.lot->name.c_str());

                // Building Name
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(view.building->name.c_str());

                // Size
                ImGui::TableNextColumn();
                ImGui::Text("%d x %d", view.lot->sizeX, view.lot->sizeZ);

                // Actions
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Plop")) {
                    director_->TriggerLotPlop(view.lot->instanceId.value());
                }
                ImGui::SameLine();
                RenderFavButton_(view.lot->instanceId.value());

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }
}


void LotsPanelTab::RenderFavButton_(const uint32_t lotInstanceId) const {
    const bool isFavorite = director_->IsFavorite(lotInstanceId);
    const char* label = isFavorite ? "Unstar" : "Star";

    if (ImGui::SmallButton(label)) {
        director_->ToggleFavorite(lotInstanceId);
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isFavorite ? "Remove from favorites" : "Add to favorites");
    }
}

void LotsPanelTab::RenderOccupantGroupFilter_() {
    std::string preview;
    if (filterHelper_.selectedOccupantGroups.empty()) {
        preview = "All Occupant Groups";
    } else {
        preview = std::to_string(filterHelper_.selectedOccupantGroups.size()) + " selected";
    }

    std::function<void(const OccupantGroup&)> renderOGNode = [&](const OccupantGroup& og) {
        if (!og.children.empty()) {
            bool nodeOpen = ImGui::TreeNode(reinterpret_cast<void*>(static_cast<intptr_t>(og.id)),
                                            "%s", og.name.data());
            if (nodeOpen) {
                for (const auto& child : og.children) {
                    renderOGNode(child);
                }
                ImGui::TreePop();
            }
        } else {
            bool isSelected = filterHelper_.selectedOccupantGroups.contains(og.id);
            if (ImGui::Checkbox(og.name.data(), &isSelected)) {
                if (isSelected) {
                    filterHelper_.selectedOccupantGroups.insert(og.id);
                } else {
                    filterHelper_.selectedOccupantGroups.erase(og.id);
                }
            }
        }
    };

    if (ImGui::CollapsingHeader("Occupant Groups")) {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
        ImGui::Text("%s", preview.c_str());

        if (ImGui::BeginChild("##OGTree", ImVec2(0, 150), true)) {
            for (const auto& og : OCCUPANT_GROUP_TREE) {
                renderOGNode(og);
            }
        }
        ImGui::EndChild();

        if (ImGui::SmallButton("Clear OGs")) {
            filterHelper_.selectedOccupantGroups.clear();
        }
        ImGui::PopStyleVar();
    }
}