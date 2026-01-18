#include "LotPlopPanel.hpp"

#include "imgui_impl_win32.h"
#include "rfl/visit.hpp"
#include "spdlog/spdlog.h"

LotPlopPanel::LotPlopPanel(SC4AdvancedLotPlopDirector* director, cIGZImGuiService* imguiService)
    : director_(director), imguiService_(imguiService) {}

void LotPlopPanel::OnRender() {
    ImGui::Begin("Advanced Lot Plop", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    const auto& lots = director_->GetLots();

    if (lots.empty()) {
        ImGui::TextUnformatted("No lots loaded. Please ensure lot_configs.cbor exists in Documents/SimCity 4/Plugins");
        ImGui::End();
        return;
    }

    // Check for device generation change (textures invalidated)
    if (imguiService_) {
        const uint32_t currentGen = imguiService_->GetDeviceGeneration();
        if (currentGen != lastDeviceGeneration_) {
            iconCache_.clear();
            texturesLoaded_ = false;
            lastDeviceGeneration_ = currentGen;
        }
    }

    // Lazy-load textures on first render (limit to avoid stalling)
    if (!texturesLoaded_ && imguiService_) {
        size_t loaded = 0;
        for (const auto& lot : lots) {
            if (loaded >= kMaxIconsToLoadPerFrame) break;
            if (lot.building.thumbnail.has_value() &&
                !iconCache_.contains(lot.building.instanceId.value())) {
                LoadIconTexture_(lot.building.instanceId.value(), lot.building);
                loaded++;
            }
        }
        if (loaded == 0) {
            texturesLoaded_ = true;
        }
    }

    ImGui::Text("Loaded %zu lots", lots.size());
    ImGui::Separator();

    // Table with columns: Icon, Name, Size, Growth, Building, Plop
    if (ImGui::BeginTable("Lots", 6, ImGuiTableFlags_Borders |
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                          ImVec2(0, 400))) {
        ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Lot name");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Growth");
        ImGui::TableSetupColumn("Building name");
        ImGui::TableSetupColumn("Plop");
        ImGui::TableHeadersRow();

        for (const auto& lot : lots) {
            ImGui::TableNextRow();

            // Icon
            ImGui::TableNextColumn();
            auto it = iconCache_.find(lot.building.instanceId.value());
            if (it != iconCache_.end()) {
                void* texId = it->second.GetID();
                if (texId) {
                    ImGui::Image(texId, ImVec2(32, 32));
                } else {
                    ImGui::Dummy(ImVec2(32, 32));
                }
            } else {
                ImGui::Dummy(ImVec2(32, 32));
            }

            // Lot Name
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(lot.name.c_str());

            // Size
            ImGui::TableNextColumn();
            ImGui::Text("%dx%d", lot.sizeX, lot.sizeZ);

            // Growth Stage
            ImGui::TableNextColumn();
            ImGui::Text("%u", lot.growthStage);

            // Building Name
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(lot.building.name.c_str());

            // Plop Button
            ImGui::TableNextColumn();
            std::string buttonLabel = "Plop##" +
                std::to_string(lot.instanceId.value());
            if (ImGui::Button(buttonLabel.c_str())) {
                director_->TriggerLotPlop(lot.instanceId.value());
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

void LotPlopPanel::LoadIconTexture_(uint32_t buildingInstanceId, const Building& building) {
    if (!imguiService_ || !building.thumbnail.has_value()) {
        return;
    }

    // Already loaded?
    if (iconCache_.contains(buildingInstanceId)) {
        return;
    }

    // Extract RGBA32 data from the thumbnail variant
    const auto& thumbnail = building.thumbnail.value();

    // Use rfl::visit to handle the TaggedUnion
    rfl::visit([&](const auto& variant) {
        const auto& data = variant.data;
        const uint32_t width = variant.width;
        const uint32_t height = variant.height;

        if (data.empty() || width == 0 || height == 0) {
            return;
        }

        // Verify data size matches expected RGBA32 size
        const size_t expectedSize = static_cast<size_t>(width) * height * 4;
        if (data.size() != expectedSize) {
            spdlog::warn("Icon data size mismatch for building 0x{:08X}: expected {}, got {}",
                         buildingInstanceId, expectedSize, data.size());
            return;
        }

        // Create ImGui texture directly from pre-decoded RGBA32 data
        ImGuiTexture texture;
        if (texture.Create(imguiService_, width, height, data.data())) {
            iconCache_.emplace(buildingInstanceId, std::move(texture));
            spdlog::debug("Loaded icon for building 0x{:08X} ({}x{})",
                          buildingInstanceId, width, height);
        } else {
            spdlog::warn("Failed to create texture for building 0x{:08X}", buildingInstanceId);
        }
    }, thumbnail);
}
