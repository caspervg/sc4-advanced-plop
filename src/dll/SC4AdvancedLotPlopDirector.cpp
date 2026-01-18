// ReSharper disable CppDFAConstantConditions
// ReSharper disable CppDFAUnreachableCode
#include "SC4AdvancedLotPlopDirector.hpp"

#include <cIGZFrameWork.h>

#include "cIGZCommandParameterSet.h"
#include "cRZBaseVariant.h"
#include "LotPlopPanel.hpp"
#include "rfl/cbor/load.hpp"
#include "spdlog/spdlog.h"

namespace {
    constexpr auto kSC4AdvancedLotPlopDirectorID = 0xE5C2B9A7u;

    constexpr auto kGZWin_WinSC4App = 0x6104489Au;
    constexpr auto kGZWin_SC4View3DWin = 0x9a47b417u;

    constexpr auto kLotPlopPanelId = 0xCA500001u;
}

SC4AdvancedLotPlopDirector::SC4AdvancedLotPlopDirector()
    : imguiService_(nullptr)
      , pView3D_(nullptr)
      , panelRegistered_(false) {
    spdlog::info("SC4AdvancedLotPlopDirector initialized");
}

uint32_t SC4AdvancedLotPlopDirector::GetDirectorID() const {
    return kSC4AdvancedLotPlopDirectorID;
}

bool SC4AdvancedLotPlopDirector::OnStart(cIGZCOM* pCOM) {
    cRZMessage2COMDirector::OnStart(pCOM);

    if (auto* framework = RZGetFrameWork()) {
        framework->AddHook(this);
    }
    return true;
}

bool SC4AdvancedLotPlopDirector::PreFrameWorkInit() { return true; }
bool SC4AdvancedLotPlopDirector::PreAppInit() { return true; }

bool SC4AdvancedLotPlopDirector::PostAppInit() {
    cIGZMessageServer2Ptr pMS2;
    if (pMS2) {
        pMS2->AddNotification(this, kSC4MessagePostCityInit);
        pMS2->AddNotification(this, kSC4MessagePreCityShutdown);
        pMS2_ = pMS2;
        spdlog::info("Registered for city messages");
    }

    if (mpFrameWork && mpFrameWork->GetSystemService(kImGuiServiceID, GZIID_cIGZImGuiService,
                                                     reinterpret_cast<void**>(&imguiService_))) {
        spdlog::info("Acquired ImGui service");

        LoadLots_();

        auto* panel = new LotPlopPanel(this, imguiService_);
        const ImGuiPanelDesc desc = ImGuiPanelAdapter<LotPlopPanel>::MakeDesc(
            panel, kLotPlopPanelId, 100, true
        );

        if (imguiService_->RegisterPanel(desc)) {
            panelRegistered_ = true;
            spdlog::info("Registered ImGui panel");
        }
    }
    else {
        spdlog::warn("ImGui service not found or not available");
    }

    return true;
}

bool SC4AdvancedLotPlopDirector::PreAppShutdown() { return true; }

bool SC4AdvancedLotPlopDirector::PostAppShutdown() {
    if (imguiService_ && panelRegistered_) {
        imguiService_->UnregisterPanel(kLotPlopPanelId);
        panelRegistered_ = false;
    }

    if (imguiService_) {
        imguiService_->Release();
        imguiService_ = nullptr;
    }

    return true;
}

bool SC4AdvancedLotPlopDirector::PostSystemServiceShutdown() { return true; }

bool SC4AdvancedLotPlopDirector::AbortiveQuit() { return true; }

bool SC4AdvancedLotPlopDirector::OnInstall() { return true; }

bool SC4AdvancedLotPlopDirector::DoMessage(cIGZMessage2* pMsg) {
    const auto pStandardMsg = static_cast<cIGZMessage2Standard*>(pMsg);
    switch (pStandardMsg->GetType()) {
    case kSC4MessagePostCityInit: PostCityInit_(pStandardMsg);
        break;
    case kSC4MessagePreCityShutdown: PreCityShutdown_(pStandardMsg);
        break;
    default: break;
    }
    return true;
}

const std::vector<Lot>& SC4AdvancedLotPlopDirector::GetLots() const {
    return lots_;
}

void SC4AdvancedLotPlopDirector::TriggerLotPlop(uint32_t lotInstanceId) const {
    if (!pView3D_) {
        spdlog::warn("Cannot plop: View3D not available (city not loaded?)");
        return;
    }

    cIGZCommandServerPtr pCmdServer;
    if (!pCmdServer) {
        spdlog::warn("Cannot plop: Command server not available");
        return;
    }

    cIGZCommandParameterSet* pCmd1 = nullptr;
    cIGZCommandParameterSet* pCmd2 = nullptr;

    if (!pCmdServer->CreateCommandParameterSet(&pCmd1) || !pCmd1 ||
        !pCmdServer->CreateCommandParameterSet(&pCmd2) || !pCmd2) {
        if (pCmd1) pCmd1->Release();
        if (pCmd2) pCmd2->Release();
        spdlog::error("Failed to create command parameter sets");
        return;
    }

    // Create a fake variant in pCmd1, this will get clobbered in AppendParameter anyway
    cRZBaseVariant dummyVariant;
    dummyVariant.SetValUint32(0);
    pCmd1->AppendParameter(dummyVariant);

    // Get the game's internal variant and patch it directly
    cIGZVariant* storedParam = pCmd1->GetParameter(0);
    if (storedParam) {
        storedParam->SetValUint32(lotInstanceId);
    }

    // Trigger lot plop command (0xec3e82f8 is the lot plop command ID)
    pView3D_->ProcessCommand(0xec3e82f8, *pCmd1, *pCmd2);

    spdlog::info("Triggered lot plop for instance ID: 0x{:08X}", lotInstanceId);

    pCmd1->Release();
    pCmd2->Release();
}

void SC4AdvancedLotPlopDirector::PostCityInit_(const cIGZMessage2Standard* pStandardMsg) {
    pCity_ = static_cast<cISC4City*>(pStandardMsg->GetVoid1());

    cISC4AppPtr pSC4App;
    if (pSC4App) {
        cIGZWin* pMainWindow = pSC4App->GetMainWindow();
        if (pMainWindow) {
            cIGZWin* pWinSC4App = pMainWindow->GetChildWindowFromID(kGZWin_WinSC4App);
            if (pWinSC4App) {
                if (pWinSC4App->GetChildAs(
                    kGZWin_SC4View3DWin, kGZIID_cISC4View3DWin, reinterpret_cast<void**>(&pView3D_))) {
                    spdlog::info("Acquired View3D interface");
                }
            }
        }
    }
}

void SC4AdvancedLotPlopDirector::PreCityShutdown_(cIGZMessage2Standard* pStandardMsg) {
    pCity_ = nullptr;
    pView3D_ = nullptr;
    spdlog::info("City shutdown - released resources");
}

void SC4AdvancedLotPlopDirector::LoadLots_() {
    try {
        const auto pluginsPath = GetUserPluginsPath_();
        const auto cborPath = pluginsPath / "lot_configs.cbor";

        if (!std::filesystem::exists(cborPath)) {
            spdlog::warn("Lot config CBOR file not found: {}", cborPath.string());
            return;
        }

        auto result = rfl::cbor::load<std::vector<Lot>>(cborPath.string());
        if (result) {
            lots_ = std::move(*result);
            spdlog::info("Loaded {} lots from {}", lots_.size(), cborPath.string());
        }
        else {
            spdlog::error("Failed to load lots from CBOR file: {}", result.error().what());
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error loading lots: {}", e.what());
    }
}

std::filesystem::path SC4AdvancedLotPlopDirector::GetUserPluginsPath_() {
    const std::string userProfile = std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : "";
    if (userProfile.empty()) {
        return {};
    }
    return std::filesystem::path(userProfile) / "Documents" / "SimCity 4" / "Plugins";
}
