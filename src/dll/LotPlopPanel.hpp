#pragma once
#include <memory>
#include <vector>
#include "PanelTab.hpp"
#include "SC4AdvancedLotPlopDirector.hpp"
#include "public/ImGuiTexture.h"

class LotPlopPanel final : public ImGuiPanel {
public:
    explicit LotPlopPanel(SC4AdvancedLotPlopDirector* director, cIGZImGuiService* imguiService);
    void OnInit() override {}
    void OnRender() override;
    void OnShutdown() override { delete this; }
    void SetOpen(bool open);
    [[nodiscard]] bool IsOpen() const;

private:
    SC4AdvancedLotPlopDirector* director_;
    cIGZImGuiService* imguiService_;
    bool isOpen_{false};
    uint32_t lastDeviceGeneration_{0};
    std::vector<std::unique_ptr<PanelTab>> tabs_;
};
