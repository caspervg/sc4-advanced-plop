#pragma once
#include <cstdint>

#include "SC4AdvancedLotPlopDirector.hpp"


class cIGZImGuiService;

class PanelTab {
public:
    explicit PanelTab(SC4AdvancedLotPlopDirector* director, cIGZImGuiService* imguiService)
        : director_(director)
        , imguiService_(imguiService) {}

    virtual ~PanelTab() = default;

    [[nodiscard]] virtual const char* GetTabName() const = 0;

    virtual void OnRender() = 0;

    virtual void OnDeviceReset(uint32_t deviceGeneration) = 0;

protected:
    SC4AdvancedLotPlopDirector* director_;
    cIGZImGuiService* imguiService_;
};
