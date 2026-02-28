#include "FamiliesPanelTab.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <unordered_map>

#include "SC4AdvancedLotPlopDirector.hpp"
#include "Utils.hpp"
#include "rfl/visit.hpp"
#include "spdlog/spdlog.h"

const char* FamiliesPanelTab::GetTabName() const {
    return "Families";
}

void FamiliesPanelTab::OnRender() {
    if (!imguiService_) {
        ImGui::TextDisabled("ImGui service unavailable.");
        return;
    }

    const auto& displayList = director_->GetFamilyDisplayList();

    // ── Search + list ────────────────────────────────────────────────────────
    ImGui::SetNextItemWidth(-80.0f);
    ImGui::InputText("##search", searchBuf_, sizeof(searchBuf_));
    ImGui::SameLine();
    if (ImGui::SmallButton("+##newpalette")) {
        newPalettePopupOpen_ = true;
        std::strncpy(newPaletteName_, "New palette", sizeof(newPaletteName_) - 1);
        newPaletteName_[sizeof(newPaletteName_) - 1] = '\0';
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create new manual palette");

    const bool hasSearch = searchBuf_[0] != '\0';
    const std::string searchStr(searchBuf_);

    size_t activeIndex = director_->GetActiveFamilyIndex();
    if (activeIndex >= displayList.size()) activeIndex = 0;

    if (ImGui::BeginListBox("##families", ImVec2(-1.0f, 140.0f))) {
        for (size_t i = 0; i < displayList.size(); ++i) {
            const auto& de = displayList[i];
            if (hasSearch) {
                std::string nameLower = de.name;
                std::string searchLower = searchStr;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
                if (nameLower.find(searchLower) == std::string::npos) continue;
            }

            std::string label;
            if (de.starred) label += "* ";
            if (!de.familyId.has_value()) label += "[P] ";
            label += de.name;
            label += "##";
            label += std::to_string(i);

            const bool selected = (i == activeIndex);
            if (ImGui::Selectable(label.c_str(), selected)) {
                director_->SetActiveFamilyIndex(i);
                activeIndex = i;
            }
        }
        ImGui::EndListBox();
    }

    if (displayList.empty()) {
        ImGui::TextDisabled("No families found. Run the cache builder to scan your plugins.");
        RenderNewPalettePopup_();
        return;
    }

    ImGui::Separator();

    if (activeIndex < displayList.size()) {
        RenderEntryDetail_(activeIndex);
    }

    RenderNewPalettePopup_();
    RenderDeleteEntryPopup_(activeIndex);
}

void FamiliesPanelTab::RenderEntryDetail_(const size_t displayIndex) {
    const auto& displayList = director_->GetFamilyDisplayList();
    if (displayIndex >= displayList.size()) return;
    const auto& de = displayList[displayIndex];

    char nameBuffer[128] = {};
    std::strncpy(nameBuffer, de.name.c_str(), sizeof(nameBuffer) - 1);
    if (ImGui::InputText("Name##famname", nameBuffer, sizeof(nameBuffer))) {
        director_->RenameFamilyEntry(displayIndex, nameBuffer);
    }

    ImGui::SameLine();
    const bool starred = de.starred;
    if (ImGui::SmallButton(starred ? "Unstar" : "Star")) {
        director_->SetFamilyStarred(displayIndex, !starred);
    }

    if (de.storedIndex >= 0) {
        ImGui::SameLine();
        if (ImGui::SmallButton("X##deleteentry")) {
            deleteEntryPopupOpen_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(de.familyId.has_value()
                ? "Remove stored overrides (resets to default)"
                : "Delete this manual palette");
        }
    }

    ImGui::Separator();

    // Resolve props once; pass to sub-renders so they don't repeat the call
    const auto resolved = director_->ResolveFamilyProps(displayIndex);

    if (de.familyId.has_value()) {
        RenderGameFamilyDetail_(displayIndex, resolved);
    }
    else {
        RenderManualPaletteDetail_(displayIndex, resolved);
    }
}

void FamiliesPanelTab::RenderGameFamilyDetail_(const size_t displayIndex, const std::vector<PaletteEntry>& resolved) {
    const auto* stored = director_->GetStoredFamilyEntry(displayIndex);

    // Build per-prop override lookups from stored entry
    std::unordered_map<uint32_t, float> storedWeights;
    std::unordered_set<uint32_t> storedExcluded;
    std::unordered_set<uint32_t> storedPinned;
    if (stored) {
        for (const auto& cfg : stored->propConfigs) {
            const uint32_t id = cfg.propID.value();
            if (cfg.excluded) storedExcluded.insert(id);
            else storedWeights[id] = cfg.weight;
            if (cfg.pinned) storedPinned.insert(id);
        }
    }

    const auto& de = director_->GetFamilyDisplayList()[displayIndex];
    const uint32_t famId = *de.familyId;
    const auto& props = director_->GetProps();

    struct FamilyPropRow {
        uint32_t propID;
        const Prop* prop;
        bool excluded;
        bool pinned;
        float weight;
    };
    std::vector<FamilyPropRow> rows;
    for (const auto& prop : props) {
        if (!std::any_of(prop.familyIds.begin(), prop.familyIds.end(),
            [famId](const rfl::Hex<uint32_t>& id) { return id.value() == famId; })) {
            continue;
        }
        const uint32_t propId = prop.instanceId.value();
        FamilyPropRow row;
        row.propID  = propId;
        row.prop    = &prop;
        row.excluded = storedExcluded.count(propId) > 0;
        row.pinned   = storedPinned.count(propId)   > 0;
        row.weight   = storedWeights.count(propId)  ? storedWeights.at(propId) : 1.0f;
        rows.push_back(row);
    }

    ImGui::Text("%zu props in family (%zu active)", rows.size(),
                static_cast<size_t>(std::count_if(rows.begin(), rows.end(),
                    [](const FamilyPropRow& r) { return !r.excluded; })));

    if (rows.empty()) {
        ImGui::TextDisabled("No props found for this family.");
    }
    else {
        if (ImGui::BeginTable("FamilyProps", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(0, 200))) {
            ImGui::TableSetupColumn("##icon", ImGuiTableColumnFlags_WidthFixed, 26.0f);
            ImGui::TableSetupColumn("Prop",   ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("Pin",    ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Excl",   ImGuiTableColumnFlags_WidthFixed, 36.0f);
            ImGui::TableHeadersRow();

            for (auto& row : rows) {
                const uint64_t key = row.prop
                    ? MakeGIKey(row.prop->groupId.value(), row.prop->instanceId.value())
                    : 0;

                ImGui::PushID(static_cast<int>(row.propID));
                ImGui::TableNextRow();

                // Thumbnail
                ImGui::TableNextColumn();
                if (row.prop && row.prop->thumbnail) {
                    thumbnailCache_.Request(key);
                    auto tex = thumbnailCache_.Get(key);
                    if (tex.has_value() && *tex != nullptr) ImGui::Image(*tex, ImVec2(20, 20));
                    else ImGui::Dummy(ImVec2(20, 20));
                }
                else {
                    ImGui::Dummy(ImVec2(20, 20));
                }

                // Name
                ImGui::TableNextColumn();
                if (row.excluded) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                if (row.prop) ImGui::TextUnformatted(PropDisplayName_(*row.prop).c_str());
                else ImGui::Text("0x%08X", row.propID);
                if (row.excluded) ImGui::PopStyleColor();

                // Weight slider
                ImGui::TableNextColumn();
                ImGui::BeginDisabled(row.excluded);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::SliderFloat("##w", &row.weight, 0.1f, 10.0f, "%.1f")) {
                    director_->SetFamilyPropWeight(displayIndex, row.propID, row.weight);
                }
                ImGui::EndDisabled();

                // Pin checkbox
                ImGui::TableNextColumn();
                bool pinned = row.pinned;
                if (ImGui::Checkbox("##p", &pinned)) {
                    director_->SetFamilyPropPinned(displayIndex, row.propID, pinned);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Force-include even if excluded");

                // Exclude checkbox
                ImGui::TableNextColumn();
                bool excl = row.excluded;
                if (ImGui::Checkbox("##x", &excl)) {
                    director_->SetFamilyPropExcluded(displayIndex, row.propID, excl);
                }

                ImGui::PopID();
            }
            ImGui::EndTable();

            thumbnailCache_.ProcessLoadQueue([this](const uint64_t key) {
                return LoadPropTexture_(key);
            });
        }
    }

    ImGui::Separator();
    RenderPaintControls_(displayIndex, resolved);
}

void FamiliesPanelTab::RenderManualPaletteDetail_(const size_t displayIndex, const std::vector<PaletteEntry>& resolved) {
    const auto* stored = director_->GetStoredFamilyEntry(displayIndex);
    if (!stored || stored->propConfigs.empty()) {
        ImGui::TextDisabled("Empty palette. Use '+' in the Props tab to add props.");
        ImGui::Separator();
        RenderPaintControls_(displayIndex, resolved);
        return;
    }

    const auto& entries = stored->propConfigs;
    ImGui::Text("%zu props in palette", entries.size());

    // Build instanceId → Prop* map once for the whole table (avoids O(n) per row)
    std::unordered_map<uint32_t, const Prop*> propsByInstId;
    for (const auto& prop : director_->GetProps()) {
        propsByInstId[prop.instanceId.value()] = &prop;
    }

    if (ImGui::BeginTable("PaletteEntries", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                          ImVec2(0, 200))) {
        ImGui::TableSetupColumn("##icon",   ImGuiTableColumnFlags_WidthFixed, 26.0f);
        ImGui::TableSetupColumn("Prop",     ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Weight",   ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableHeadersRow();

        bool removed = false;
        for (size_t i = 0; i < entries.size() && !removed; ++i) {
            const auto& cfg = entries[i];
            const uint32_t propId = cfg.propID.value();
            const auto it = propsByInstId.find(propId);
            const Prop* prop = (it != propsByInstId.end()) ? it->second : nullptr;
            const uint64_t key = prop ? MakeGIKey(prop->groupId.value(), prop->instanceId.value()) : 0;

            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (prop && prop->thumbnail) {
                thumbnailCache_.Request(key);
                auto tex = thumbnailCache_.Get(key);
                if (tex.has_value() && *tex != nullptr) ImGui::Image(*tex, ImVec2(20, 20));
                else ImGui::Dummy(ImVec2(20, 20));
            }
            else {
                ImGui::Dummy(ImVec2(20, 20));
            }

            ImGui::TableNextColumn();
            if (prop) ImGui::TextUnformatted(PropDisplayName_(*prop).c_str());
            else ImGui::Text("Missing 0x%08X", propId);

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            float weight = cfg.weight;
            if (ImGui::SliderFloat("##w", &weight, 0.1f, 10.0f, "%.1f")) {
                director_->SetFamilyPropWeight(displayIndex, propId, weight);
            }

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x")) {
                director_->RemovePropFromManualPalette(displayIndex, propId);
                removed = true;  // entries pointer now invalid; bail out of loop
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove from palette");

            ImGui::PopID();
        }

        ImGui::EndTable();

        thumbnailCache_.ProcessLoadQueue([this](const uint64_t key) {
            return LoadPropTexture_(key);
        });
    }

    ImGui::Separator();
    RenderPaintControls_(displayIndex, resolved);
}

void FamiliesPanelTab::RenderPaintControls_(const size_t displayIndex, const std::vector<PaletteEntry>& resolved) {
    ImGui::TextUnformatted("Paint Settings");

    // Density variation is stored per entry
    const auto* stored = director_->GetStoredFamilyEntry(displayIndex);
    float densVar = stored ? stored->densityVariation : 0.0f;
    if (ImGui::SliderFloat("Density variation", &densVar, 0.0f, 1.0f, "%.2f")) {
        director_->SetFamilyDensityVariation(displayIndex, densVar);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Spread in placement density across individual props (0 = uniform)");

    ImGui::SliderFloat("Line spacing (m)", &paintDefaults_.spacingMeters, 0.5f, 50.0f, "%.1f");
    ImGui::SliderFloat("Polygon density (/100 m^2)", &paintDefaults_.densityPer100Sqm, 0.1f, 20.0f, "%.1f");
    ImGui::Checkbox("Align to path", &paintDefaults_.alignToPath);
    ImGui::Checkbox("Random rotation", &paintDefaults_.randomRotation);
    ImGui::SliderFloat("Lateral jitter (m)", &paintDefaults_.randomOffset, 0.0f, 5.0f, "%.1f");
    int rotation = paintDefaults_.rotation;
    ImGui::RadioButton("0 deg", &rotation, 0); ImGui::SameLine();
    ImGui::RadioButton("90 deg", &rotation, 1); ImGui::SameLine();
    ImGui::RadioButton("180 deg", &rotation, 2); ImGui::SameLine();
    ImGui::RadioButton("270 deg", &rotation, 3);
    paintDefaults_.rotation = rotation;

    const bool canPaint = !resolved.empty();
    if (!canPaint) ImGui::BeginDisabled();
    if (ImGui::Button("Paint line"))    StartPainting_(PropPaintMode::Line,    displayIndex, resolved);
    ImGui::SameLine();
    if (ImGui::Button("Paint polygon")) StartPainting_(PropPaintMode::Polygon, displayIndex, resolved);
    if (!canPaint) ImGui::EndDisabled();
}

bool FamiliesPanelTab::StartPainting_(const PropPaintMode mode, const size_t displayIndex,
                                      const std::vector<PaletteEntry>& resolved) {
    if (resolved.empty()) return false;

    const auto& displayList = director_->GetFamilyDisplayList();
    const std::string name = displayIndex < displayList.size() ? displayList[displayIndex].name : "Family";

    const auto* stored = director_->GetStoredFamilyEntry(displayIndex);

    PropPaintSettings settings = paintDefaults_;
    settings.mode = mode;
    settings.activePalette = resolved;
    settings.densityVariation = stored ? stored->densityVariation : 0.0f;
    if (settings.randomSeed == 0) {
        settings.randomSeed = static_cast<uint32_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }

    return director_->StartPropPainting(resolved.front().propID.value(), settings, name);
}

void FamiliesPanelTab::OnDeviceReset(const uint32_t deviceGeneration) {
    if (deviceGeneration != lastDeviceGeneration_) {
        thumbnailCache_.OnDeviceReset();
        lastDeviceGeneration_ = deviceGeneration;
    }
}

ImGuiTexture FamiliesPanelTab::LoadPropTexture_(const uint64_t propKey) {
    ImGuiTexture texture;
    if (!imguiService_) return texture;

    const auto& propsById = director_->GetPropsById();
    if (!propsById.contains(propKey)) return texture;

    const auto& prop = propsById.at(propKey);
    if (!prop.thumbnail.has_value()) return texture;

    rfl::visit([&](const auto& variant) {
        const auto& data = variant.data;
        const uint32_t width = variant.width;
        const uint32_t height = variant.height;
        if (data.empty() || width == 0 || height == 0) return;
        if (data.size() != static_cast<size_t>(width) * height * 4) return;
        texture.Create(imguiService_, width, height, data.data());
    }, prop.thumbnail.value());

    return texture;
}

void FamiliesPanelTab::RenderNewPalettePopup_() {
    if (!newPalettePopupOpen_) return;
    ImGui::OpenPopup("Create Palette");
    if (ImGui::BeginPopupModal("Create Palette", &newPalettePopupOpen_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", newPaletteName_, sizeof(newPaletteName_));
        const bool canCreate = std::strlen(newPaletteName_) > 0;
        if (!canCreate) ImGui::BeginDisabled();
        if (ImGui::Button("Create")) {
            director_->CreateManualPalette(newPaletteName_);
            newPalettePopupOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (!canCreate) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            newPalettePopupOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FamiliesPanelTab::RenderDeleteEntryPopup_(const size_t displayIndex) {
    if (!deleteEntryPopupOpen_) return;
    ImGui::OpenPopup("Delete / Reset Entry");
    if (ImGui::BeginPopupModal("Delete / Reset Entry", &deleteEntryPopupOpen_, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto& displayList = director_->GetFamilyDisplayList();
        if (displayIndex < displayList.size() && displayList[displayIndex].familyId.has_value()) {
            ImGui::TextUnformatted("Remove stored overrides for this family (reset to defaults)?");
        }
        else {
            ImGui::TextUnformatted("Delete this manual palette?");
        }
        if (ImGui::Button("Confirm")) {
            director_->DeleteFamilyEntry(displayIndex);
            deleteEntryPopupOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            deleteEntryPopupOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

std::string FamiliesPanelTab::PropDisplayName_(const Prop& prop) {
    if (!prop.visibleName.empty()) return prop.visibleName;
    if (!prop.exemplarName.empty()) return prop.exemplarName;
    return "<unnamed>";
}
