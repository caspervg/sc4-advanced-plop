#include "BuildingsPanelTab.hpp"

#include "OccupantGroups.hpp"
#include "Utils.hpp"
#include "jsoncons/staj_event.hpp"
#include "rfl/visit.hpp"

const char* BuildingsPanelTab::GetTabName() const {
    return "Buildings";
}

void BuildingsPanelTab::OnRender() {
    const auto& buildings = director_->GetBuildings();

    if (buildings.empty()) {
        ImGui::TextUnformatted("No buildings loaded. Please ensure lot_configs.cbor exists in the Plugins directory.");
        return;
    }

    RenderFilterUI_();
    ImGui::Separator();

    ApplyFilters_();
    ImGui::Text("Showing %zu of %zu buildings", filteredBuildings_.size(), buildings.size());

    // Calculate available height for tables
    const float availHeight = ImGui::GetContentRegionAvail().y;
    const float buildingTableHeight = availHeight * 0.6f;
    const float lotsTableHeight = availHeight * 0.4f - ImGui::GetTextLineHeightWithSpacing();

    RenderBuildingsTable_(buildingTableHeight);

    ImGui::Separator();

    RenderLotsDetailTable_(lotsTableHeight);
}

void BuildingsPanelTab::OnDeviceReset(uint32_t deviceGeneration) {
    if (deviceGeneration != lastDeviceGeneration_) {
        thumbnailCache_.OnDeviceReset();
        lastDeviceGeneration_ = deviceGeneration;
    }
}

ImGuiTexture BuildingsPanelTab::LoadBuildingTexture_(uint64_t buildingKey) {
    ImGuiTexture texture;

    if (!imguiService_) {
        spdlog::warn("Could not load building thumbnail: imguiService_ is null");
        return texture;
    }

    const auto& buildingsById = director_->GetBuildingsById();
    if (!buildingsById.contains(buildingKey)) {
        spdlog::warn("Could not find building with key 0x{:016X} in buildings map", buildingKey);
        return texture;
    }
    const auto& building = buildingsById.at(buildingKey);
    if (!building.thumbnail.has_value()) {
        spdlog::warn("Building with key 0x{:016X} has no thumbnail", buildingKey);
    }

    const auto& thumbnail = building.thumbnail.value();

    rfl::visit([&](const auto& variant) {
        const auto& data = variant.data;
        const uint32_t width = variant.width;
        const uint32_t height = variant.height;

        if (data.empty() || width == 0 || height == 0) {
            return;
        }

        const size_t expectedSize = static_cast<size_t>(width) * height * 4;
        if (data.size() != expectedSize) {
            spdlog::warn("Building icon data size mismatch for key 0x{:016X}: expected {}, got {}",
                         buildingKey, expectedSize, data.size());
            return;
        }

        texture.Create(imguiService_, width, height, data.data());
    }, thumbnail);

    return texture;
}

void BuildingsPanelTab::RenderFilterUI_() {
        static char searchBuf[256] = {};
    if (searchBuffer_ != searchBuf) {
        std::strncpy(searchBuf, searchBuffer_.c_str(), sizeof(searchBuf) - 1);
        searchBuf[sizeof(searchBuf) - 1] = '\0';
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##SearchBuildings", "Search buildings...", searchBuf, sizeof(searchBuf))) {
        searchBuffer_ = searchBuf;
    }

    // Zone type filter
    const char* zoneTypes[] = {
        "Any zone", "Residential (R)", "Commercial (C)", "Industrial (I)", "Plopped", "None", "Other"
    };
    int currentZone = selectedZoneType_.has_value() ? (selectedZoneType_.value() + 1) : 0;
    ImGui::SetNextItemWidth(UI::kDropdownWidth);
    if (ImGui::Combo("##ZoneType", &currentZone, zoneTypes, 7)) {
        if (currentZone == 0) {
            selectedZoneType_.reset();
        } else {
            selectedZoneType_ = static_cast<uint8_t>(currentZone - 1);
        }
    }

    ImGui::SameLine();

    // Wealth filter
    const char* wealthOptions[] = {"Any wealth", "$", "$$", "$$$"};
    int currentWealth = selectedWealthType_.has_value() ? selectedWealthType_.value() : 0;
    ImGui::SetNextItemWidth(UI::kDropdownWidth);
    if (ImGui::Combo("##Wealth", &currentWealth, wealthOptions, 4)) {
        if (currentWealth == 0) {
            selectedWealthType_.reset();
        } else {
            selectedWealthType_ = static_cast<uint8_t>(currentWealth);
        }
    }

    ImGui::SameLine();

    // Growth stage filter
    const char* growthStages[] = {
        "Any stage", "Plopped (255)", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"
    };
    auto currentGrowthStageIndex = 0;
    if (selectedGrowthStage_.has_value()) {
        uint8_t val = selectedGrowthStage_.value();
        if (val == 255) {
            currentGrowthStageIndex = 1;
        } else if (val <= 15) {
            currentGrowthStageIndex = val + 2;
        }
    }
    ImGui::SetNextItemWidth(UI::kDropdownWidth);
    if (ImGui::Combo("##GrowthStage", &currentGrowthStageIndex, growthStages, 18)) {
        if (currentGrowthStageIndex == 0) {
            selectedGrowthStage_.reset();
        } else if (currentGrowthStageIndex == 1) {
            selectedGrowthStage_ = 255;
        } else {
            selectedGrowthStage_ = static_cast<uint8_t>(currentGrowthStageIndex - 2);
        }
    }

    ImGui::SameLine();
    ImGui::Checkbox("Favorites only", &favoritesOnly_);

    // Size filters
    ImGui::Text("Width:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::InputInt("##MinSizeX", &minSizeX_, 1, 1)) {
        minSizeX_ = std::clamp(minSizeX_, LotSize::kMinSize, LotSize::kMaxSize);
    }
    ImGui::SameLine();
    ImGui::Text("to");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::InputInt("##MaxSizeX", &maxSizeX_, 1, 1)) {
        maxSizeX_ = std::clamp(maxSizeX_, LotSize::kMinSize, LotSize::kMaxSize);
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    ImGui::Text("Depth:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::InputInt("##MinSizeZ", &minSizeZ_, 1, 1)) {
        minSizeZ_ = std::clamp(minSizeZ_, LotSize::kMinSize, LotSize::kMaxSize);
    }
    ImGui::SameLine();
    ImGui::Text("to");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::InputInt("##MaxSizeZ", &maxSizeZ_, 1, 1)) {
        maxSizeZ_ = std::clamp(maxSizeZ_, LotSize::kMinSize, LotSize::kMaxSize);
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    if (ImGui::Button("Clear filters")) {
        searchBuffer_.clear();
        searchBuf[0] = '\0';
        selectedOccupantGroups_.clear();
        selectedZoneType_.reset();
        selectedWealthType_.reset();
        selectedGrowthStage_.reset();
        favoritesOnly_ = false;
        minSizeX_ = LotSize::kMinSize;
        maxSizeX_ = LotSize::kMaxSize;
        minSizeZ_ = LotSize::kMinSize;
        maxSizeZ_ = LotSize::kMaxSize;
    }

    ImGui::Separator();
    RenderOccupantGroupFilter_();
}

void BuildingsPanelTab::RenderBuildingsTable_(const float tableHeight) {
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("BuildingsTable", 4, tableFlags, ImVec2(0, tableHeight))) {
        ImGui::TableSetupColumn("Thumbnail",
                                ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                UI::kIconColumnWidth);
        ImGui::TableSetupColumn("Name",
                                ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort |
                                ImGuiTableColumnFlags_PreferSortAscending);
        ImGui::TableSetupColumn("Description",
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Lots",
                                ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                40.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Handle sorting
        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
            if (specs->SpecsCount > 0) {
                sortDescending_ = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
            }
            specs->SpecsDirty = false;
        }

        // Sort buildings
        if (sortDescending_) {
            std::ranges::sort(filteredBuildings_, [](const Building* a, const Building* b) {
                return a->name > b->name;
            });
        } else {
            std::ranges::sort(filteredBuildings_, [](const Building* a, const Building* b) {
                return a->name < b->name;
            });
        }

        // Use clipper for virtualized scrolling
        constexpr float rowHeight = UI::kIconSize + 8.0f;
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filteredBuildings_.size()), rowHeight);

        while (clipper.Step()) {
            // Request texture loads for visible + margin items
            const int prefetchStart = std::max(0, clipper.DisplayStart - Cache::kPrefetchMargin);
            const int prefetchEnd = std::min(static_cast<int>(filteredBuildings_.size()),
                                              clipper.DisplayEnd + Cache::kPrefetchMargin);

            for (int i = prefetchStart; i < prefetchEnd; ++i) {
                const auto& building = filteredBuildings_[i];
                if (building->thumbnail.has_value()) {
                    thumbnailCache_.Request(MakeGIKey(building->groupId.value(), building->instanceId.value()));
                }
            }

            // Render visible rows
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto* building = filteredBuildings_[i];
                const bool isSelected = (building == selectedBuilding_);
                RenderBuildingRow_(*building, isSelected);
            }
        }

        // Process load queue each frame
        thumbnailCache_.ProcessLoadQueue([this](const uint64_t key) {
            return LoadBuildingTexture_(key);
        });

        ImGui::EndTable();
    }
}

void BuildingsPanelTab::RenderBuildingRow_(const Building& building, const bool isSelected) {
    const uint64_t key = MakeGIKey(building.groupId.value(), building.instanceId.value());

    ImGui::PushID(static_cast<int>(key));
    ImGui::TableNextRow();

    // Thumbnail column
    ImGui::TableNextColumn();
    auto thumbTextureId = thumbnailCache_.Get(key);
    if (thumbTextureId.has_value() && *thumbTextureId != nullptr) {
        ImGui::Image(*thumbTextureId, ImVec2(UI::kIconSize, UI::kIconSize));
    } else {
        ImGui::Dummy(ImVec2(UI::kIconSize, UI::kIconSize));
    }

    // Name column - make entire row selectable
    ImGui::TableNextColumn();
    if (ImGui::Selectable(building.name.c_str(), isSelected,
                          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
        selectedBuilding_ = &building;
    }

    // Description column
    ImGui::TableNextColumn();
    if (!building.description.empty()) {
        // std::string desc = building.description;
        // std::ranges::replace(desc, '\n', ' ');
        // std::ranges::replace(desc, '\r', ' ');
        ImGui::TextWrapped(building.description.c_str());
    }

    // Lots count column
    ImGui::TableNextColumn();
    ImGui::Text("%zu", building.lots.size());

    ImGui::PopID();
}

void BuildingsPanelTab::RenderLotsDetailTable_(const float tableHeight) {
    if (!selectedBuilding_) {
        ImGui::TextDisabled("Select a building above to see its lots");
        return;
    }

    ImGui::Text("Lots for: %s (%zu lots)", selectedBuilding_->name.c_str(), selectedBuilding_->lots.size());

    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("LotsDetailTable", 4, tableFlags, ImVec2(0, tableHeight))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, UI::kSizeColumnWidth);
        ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                UI::kActionColumnWidth);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Typically < 20 lots per building, no clipper needed
        for (const auto& lot : selectedBuilding_->lots) {
            RenderLotRow_(lot);
        }

        ImGui::EndTable();
    }
}

void BuildingsPanelTab::RenderLotRow_(const Lot& lot) {
    ImGui::PushID(static_cast<int>(lot.instanceId.value()));
    ImGui::TableNextRow();

    // Name
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(lot.name.c_str());

    // Size
    ImGui::TableNextColumn();
    ImGui::Text("%d x %d", lot.sizeX, lot.sizeZ);

    // Growth stage
    ImGui::TableNextColumn();
    if (lot.growthStage == 255) {
        ImGui::Text("Plop");
    } else {
        ImGui::Text("%d", lot.growthStage);
    }

    // Actions
    ImGui::TableNextColumn();
    if (ImGui::SmallButton("Plop")) {
        director_->TriggerLotPlop(lot.instanceId.value());
    }
    ImGui::SameLine();
    RenderFavButton_(lot.instanceId.value());

    ImGui::PopID();
}

void BuildingsPanelTab::RenderFavButton_(const uint32_t lotInstanceId) const {
    const bool isFavorite = director_->IsFavorite(lotInstanceId);
    const char* label = isFavorite ? "Unstar" : "Star";

    if (ImGui::SmallButton(label)) {
        director_->ToggleFavorite(lotInstanceId);
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isFavorite ? "Remove from favorites" : "Add to favorites");
    }
}

void BuildingsPanelTab::RenderOccupantGroupFilter_() {
    std::string preview;
    if (selectedOccupantGroups_.empty()) {
        preview = "All Occupant Groups";
    } else {
        preview = std::to_string(selectedOccupantGroups_.size()) + " selected";
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
            bool isSelected = selectedOccupantGroups_.contains(og.id);
            if (ImGui::Checkbox(og.name.data(), &isSelected)) {
                if (isSelected) {
                    selectedOccupantGroups_.insert(og.id);
                } else {
                    selectedOccupantGroups_.erase(og.id);
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
            selectedOccupantGroups_.clear();
        }
        ImGui::PopStyleVar();
    }
}

void BuildingsPanelTab::ApplyFilters_() {
    const auto& buildings = director_->GetBuildings();
    const auto& favoriteLots = director_->GetFavoriteLotIds();

    filteredBuildings_.clear();
    filteredBuildings_.reserve(buildings.size());

    // Convert search to lowercase
    std::string searchLower;
    searchLower.reserve(searchBuffer_.size());
    std::ranges::transform(searchBuffer_, std::back_inserter(searchLower),
                           [](unsigned char c) { return std::tolower(c); });

    for (const auto& building : buildings) {
        // Text filter on building name
        if (!searchLower.empty()) {
            std::string nameLower;
            nameLower.reserve(building.name.size());
            std::ranges::transform(building.name, std::back_inserter(nameLower),
                                   [](unsigned char c) { return std::tolower(c); });
            if (nameLower.find(searchLower) == std::string::npos) {
                continue;
            }
        }

        // Occupant group filter
        if (!selectedOccupantGroups_.empty()) {
            bool hasMatchingOG = std::ranges::any_of(building.occupantGroups,
                [this](uint32_t og) { return selectedOccupantGroups_.contains(og); });
            if (!hasMatchingOG) {
                continue;
            }
        }

        // Check if any lot passes the lot-level filters
        bool hasMatchingLot = std::ranges::any_of(building.lots, [&](const Lot& lot) {
            // Zone type filter
            if (selectedZoneType_.has_value()) {
                const uint8_t category = selectedZoneType_.value();
                if (!lot.zoneType.has_value()) {
                    if (category != 4) return false; // "None" category
                } else {
                    const uint8_t zoneValue = lot.zoneType.value();
                    bool matches = false;
                    if (category == 0) matches = (zoneValue >= 0x01 && zoneValue <= 0x03); // Residential
                    else if (category == 1) matches = (zoneValue >= 0x04 && zoneValue <= 0x06); // Commercial
                    else if (category == 2) matches = (zoneValue >= 0x07 && zoneValue <= 0x09); // Industrial
                    else if (category == 3) matches = (zoneValue == 0x0F); // Plopped
                    else if (category == 4) matches = (zoneValue == 0x00); // None
                    else if (category == 5) matches = (zoneValue >= 0x0A && zoneValue <= 0x0E); // Other
                    if (!matches) return false;
                }
            }

            // Wealth filter
            if (selectedWealthType_.has_value()) {
                if (!lot.wealthType.has_value() || lot.wealthType.value() != selectedWealthType_.value()) {
                    return false;
                }
            }

            // Growth stage filter
            if (selectedGrowthStage_.has_value()) {
                if (lot.growthStage != selectedGrowthStage_.value()) {
                    return false;
                }
            }

            // Size filter
            const int effectiveMinX = std::min(minSizeX_, maxSizeX_);
            const int effectiveMaxX = std::max(minSizeX_, maxSizeX_);
            const int effectiveMinZ = std::min(minSizeZ_, maxSizeZ_);
            const int effectiveMaxZ = std::max(minSizeZ_, maxSizeZ_);

            if (lot.sizeX < effectiveMinX || lot.sizeX > effectiveMaxX ||
                lot.sizeZ < effectiveMinZ || lot.sizeZ > effectiveMaxZ) {
                return false;
            }

            // Favorites filter
            if (favoritesOnly_) {
                if (!favoriteLots.contains(lot.instanceId.value())) {
                    return false;
                }
            }

            return true;
        });

        if (hasMatchingLot) {
            filteredBuildings_.push_back(&building);
        }
    }

    // Clear selection if it was filtered out
    if (selectedBuilding_) {
        bool found = std::ranges::any_of(filteredBuildings_, [this](const Building* b) {
            return b == selectedBuilding_;
        });
        if (!found) {
            selectedBuilding_ = nullptr;
        }
    }
}