#pragma once

#include <cRZCOMDllDirector.h>
#include <cstdint>
#include <filesystem>
#include <vector>
#include "cIGZCommandServer.h"
#include "cIGZMessage2Standard.h"
#include "cIGZMessageServer2.h"
#include "cIGZWin.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4View3DWin.h"
#include "cRZAutoRefCount.h"
#include "cRZMessage2COMDirector.h"
#include "cRZMessage2COMDirector.h"
#include "GZServPtrs.h"
#include "imgui.h"
#include "../shared/entities.hpp"
#include "public/cIGZImGuiService.h"
#include "public/ImGuiPanel.h"
#include "public/ImGuiPanelAdapter.h"
#include "public/ImGuiServiceIds.h"

static constexpr uint32_t kSC4MessagePostCityInit = 0x26D31EC1;
static constexpr uint32_t kSC4MessagePreCityShutdown = 0x26D31EC2;

class SC4AdvancedLotPlopDirector final : public cRZMessage2COMDirector
{
public:
    SC4AdvancedLotPlopDirector();
    ~SC4AdvancedLotPlopDirector() override = default;

    uint32_t GetDirectorID() const override;
    bool OnStart(cIGZCOM* pCOM) override;

    bool PreFrameWorkInit() override;
    bool PreAppInit() override;
    bool PostAppInit() override;
    bool PreAppShutdown() override;
    bool PostAppShutdown() override;
    bool PostSystemServiceShutdown() override;
    bool AbortiveQuit() override;
    bool OnInstall() override;
    bool DoMessage(cIGZMessage2* pMsg) override;

    const std::vector<Lot>& GetLots() const;
    void TriggerLotPlop(uint32_t lotInstanceId) const;

private:
    void PostCityInit_(const cIGZMessage2Standard* pStandardMsg);
    void PreCityShutdown_(cIGZMessage2Standard* pStandardMsg);
    void LoadLots_();
    static std::filesystem::path GetUserPluginsPath_();

private:
    cIGZImGuiService* imguiService_ = nullptr;
    cRZAutoRefCount<cISC4City> pCity_;
    cISC4View3DWin* pView3D_ = nullptr;
    cRZAutoRefCount<cIGZMessageServer2> pMS2_;

    std::vector<Lot> lots_{};
    bool panelRegistered_{false};
};
