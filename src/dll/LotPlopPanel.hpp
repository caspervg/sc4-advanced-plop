#pragma once
#include "SC4AdvancedLotPlopDirector.hpp"
#include "public/ImGuiTexture.h"
#include <unordered_map>


class LotPlopPanel final : public ImGuiPanel {
public:
    explicit LotPlopPanel(SC4AdvancedLotPlopDirector* director, cIGZImGuiService* imguiService);
    void OnRender() override;
    void OnShutdown() override { delete this; }

private:
    void LoadIconTexture_(uint32_t buildingInstanceId, const Building& building);

    SC4AdvancedLotPlopDirector* director_;
    cIGZImGuiService* imguiService_;
    std::unordered_map<uint32_t, ImGuiTexture> iconCache_;
    uint32_t lastDeviceGeneration_ = 0;
    bool texturesLoaded_ = false;
};
