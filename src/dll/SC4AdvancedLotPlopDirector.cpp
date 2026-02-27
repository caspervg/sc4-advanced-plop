// ReSharper disable CppDFAConstantConditions
// ReSharper disable CppDFAUnreachableCode
#include "SC4AdvancedLotPlopDirector.hpp"

#include <cIGZFrameWork.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>

#include <chrono>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include "cGZPersistResourceKey.h"
#include "cIGZCommandParameterSet.h"
#include "cIGZPersistResourceManager.h"
#include "cIGZWinKeyAccelerator.h"
#include "cIGZWinKeyAcceleratorRes.h"
#include "cRZBaseVariant.h"
#include "LotPlopPanel.hpp"
#include "PropPainterInputControl.hpp"
#include "Utils.hpp"
#include "public/cIGZS3DCameraService.h"
#include "public/S3DCameraServiceIds.h"
#include "rfl/cbor/load.hpp"
#include "rfl/cbor/save.hpp"
#include "spdlog/spdlog.h"

namespace {
    constexpr auto kSC4AdvancedLotPlopDirectorID = 0xE5C2B9A7u;

    constexpr auto kGZWin_WinSC4App = 0x6104489Au;
    constexpr auto kGZWin_SC4View3DWin = 0x9a47b417u;

    constexpr auto kLotPlopPanelId = 0xCA500001u;
    constexpr auto kToggleLotPlopWindowShortcutID = 0x9F21C3A1u;
    constexpr auto kKeyConfigType = 0xA2E3D533u;
    constexpr auto kKeyConfigGroup = 0x8F1E6D69u;
    constexpr auto kKeyConfigInstance = 0x5CBCFBF8u;
}

SC4AdvancedLotPlopDirector::SC4AdvancedLotPlopDirector()
    : imguiService_(nullptr)
      , drawService_(nullptr)
      , pView3D_(nullptr)
      , panelRegistered_(false) {
    spdlog::info("SC4AdvancedLotPlopDirector initialized");
}

SC4AdvancedLotPlopDirector::~SC4AdvancedLotPlopDirector() = default;

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

        if (mpFrameWork->GetSystemService(kS3DCameraServiceID, GZIID_cIGZS3DCameraService,
                                          reinterpret_cast<void**>(&cameraService_))) {
            spdlog::info("Acquired S3D camera service");
        }
        else {
            spdlog::warn("S3D camera service not available");
        }

        if (mpFrameWork->GetSystemService(kDrawServiceID, GZIID_cIGZDrawService,
                                          reinterpret_cast<void**>(&drawService_))) {
            spdlog::info("Acquired draw service");
            if (!drawService_->RegisterDrawPassCallback(
                DrawServicePass::PreDynamic,
                &DrawOverlayCallback_,
                this,
                &drawCallbackToken_)) {
                spdlog::warn("Failed to register draw pass callback");
            }
        }
        else {
            spdlog::warn("Draw service not available");
        }

        LoadLots_();
        LoadProps_();
        LoadFavorites_();

        panel_ = std::make_unique<LotPlopPanel>(this, imguiService_);
        const ImGuiPanelDesc desc = ImGuiPanelAdapter<LotPlopPanel>::MakeDesc(
            panel_.get(), kLotPlopPanelId, 100, true
        );

        if (imguiService_->RegisterPanel(desc)) {
            panelRegistered_ = true;
            panelVisible_ = false;
            panel_->SetOpen(false);
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
    // Destroy objects that may call ImGuiService first.
    if (propPainterControl_) {
        StopPropPainting();
        propPainterControl_->SetCity(nullptr);
        propPainterControl_->Shutdown();
        propPainterControl_.Reset();
    }

    if (imguiService_ && panelRegistered_) {
        SetLotPlopPanelVisible(false);
        imguiService_->UnregisterPanel(kLotPlopPanelId);
    }
    panelRegistered_ = false;
    if (panel_) {
        panel_->Shutdown(); // Release textures while the ImGui service is still alive
    }
    panel_.reset();

    if (drawService_ && drawCallbackToken_ != 0) {
        drawService_->UnregisterDrawPassCallback(drawCallbackToken_);
        drawCallbackToken_ = 0;
    }

    if (imguiService_) {
        imguiService_->Release();
        imguiService_ = nullptr;
    }

    if (drawService_) {
        drawService_->Release();
        drawService_ = nullptr;
    }

    if (cameraService_) {
        cameraService_->Release();
        cameraService_ = nullptr;
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
    case kToggleLotPlopWindowShortcutID: ToggleLotPlopPanel_();
        break;
    default: break;
    }
    return true;
}

const std::vector<Building>& SC4AdvancedLotPlopDirector::GetBuildings() const {
    return buildings_;
}

const std::unordered_map<uint64_t, Building>& SC4AdvancedLotPlopDirector::GetBuildingsById() const {
    return buildingsById_;
}

const std::unordered_map<uint64_t, Lot>& SC4AdvancedLotPlopDirector::GetLotsById() const {
    return lotsById_;
}

const std::vector<Prop>& SC4AdvancedLotPlopDirector::GetProps() const {
    return props_;
}

const std::unordered_map<uint64_t, Prop>& SC4AdvancedLotPlopDirector::GetPropsById() const {
    return propsById_;
}

const std::unordered_map<uint32_t, std::string>& SC4AdvancedLotPlopDirector::GetPropFamilyNames() const {
    return propFamilyNames_;
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

bool SC4AdvancedLotPlopDirector::IsFavorite(uint32_t lotInstanceId) const {
    return favoriteLotIds_.contains(lotInstanceId);
}

const std::unordered_set<uint32_t>& SC4AdvancedLotPlopDirector::GetFavoriteLotIds() const {
    return favoriteLotIds_;
}

void SC4AdvancedLotPlopDirector::ToggleFavorite(uint32_t lotInstanceId) {
    if (favoriteLotIds_.contains(lotInstanceId)) {
        favoriteLotIds_.erase(lotInstanceId);
        spdlog::info("Removed favorite: 0x{:08X}", lotInstanceId);
    }
    else {
        favoriteLotIds_.insert(lotInstanceId);
        spdlog::info("Added favorite: 0x{:08X}", lotInstanceId);
    }
    SaveFavorites_();
}

bool SC4AdvancedLotPlopDirector::IsPropFavorite(const uint32_t groupId, const uint32_t instanceId) const {
    return favoritePropIds_.contains(MakeGIKey(groupId, instanceId));
}

const std::unordered_set<uint64_t>& SC4AdvancedLotPlopDirector::GetFavoritePropIds() const {
    return favoritePropIds_;
}

void SC4AdvancedLotPlopDirector::TogglePropFavorite(const uint32_t groupId, const uint32_t instanceId) {
    const uint64_t key = MakeGIKey(groupId, instanceId);
    if (favoritePropIds_.contains(key)) {
        favoritePropIds_.erase(key);
        spdlog::info("Removed prop favorite: 0x{:08X}/0x{:08X}", groupId, instanceId);
    }
    else {
        favoritePropIds_.insert(key);
        spdlog::info("Added prop favorite: 0x{:08X}/0x{:08X}", groupId, instanceId);
    }
    SaveFavorites_();
}

const std::vector<FamilyDisplayEntry>& SC4AdvancedLotPlopDirector::GetFamilyDisplayList() const {
    return familyDisplayList_;
}

size_t SC4AdvancedLotPlopDirector::GetActiveFamilyIndex() const {
    return activeFamilyDisplayIndex_;
}

void SC4AdvancedLotPlopDirector::SetActiveFamilyIndex(const size_t index) {
    if (familyDisplayList_.empty()) {
        activeFamilyDisplayIndex_ = 0;
        return;
    }
    activeFamilyDisplayIndex_ = std::min(index, familyDisplayList_.size() - 1);
}

const FamilyEntry* SC4AdvancedLotPlopDirector::GetActiveFamilyEntry() const {
    return GetStoredFamilyEntry(activeFamilyDisplayIndex_);
}

const FamilyEntry* SC4AdvancedLotPlopDirector::GetStoredFamilyEntry(const size_t displayIndex) const {
    if (displayIndex >= familyDisplayList_.size()) return nullptr;
    const int idx = familyDisplayList_[displayIndex].storedIndex;
    if (idx < 0 || static_cast<size_t>(idx) >= familyEntries_.size()) return nullptr;
    return &familyEntries_[idx];
}

std::vector<PaletteEntry> SC4AdvancedLotPlopDirector::ResolveFamilyProps(const size_t displayIndex) const {
    if (displayIndex >= familyDisplayList_.size()) return {};
    const auto& de = familyDisplayList_[displayIndex];

    if (de.familyId.has_value()) {
        const uint32_t famId = *de.familyId;
        std::unordered_map<uint32_t, float> weightOverrides;
        std::unordered_set<uint32_t> excludedIds;
        std::unordered_set<uint32_t> pinnedIds;

        if (de.storedIndex >= 0 && static_cast<size_t>(de.storedIndex) < familyEntries_.size()) {
            for (const auto& cfg : familyEntries_[de.storedIndex].propConfigs) {
                const uint32_t propId = cfg.propID.value();
                if (cfg.excluded) excludedIds.insert(propId);
                else weightOverrides[propId] = cfg.weight;
                if (cfg.pinned) pinnedIds.insert(propId);
            }
        }

        std::vector<PaletteEntry> result;
        std::unordered_set<uint32_t> seenIds;
        for (const auto& prop : props_) {
            if (!std::any_of(prop.familyIds.begin(), prop.familyIds.end(),
                [famId](const rfl::Hex<uint32_t>& id) { return id.value() == famId; })) {
                continue;
            }
            const uint32_t propId = prop.instanceId.value();
            if (excludedIds.count(propId)) continue;
            seenIds.insert(propId);
            float weight = 1.0f;
            if (const auto it = weightOverrides.find(propId); it != weightOverrides.end()) weight = it->second;
            result.push_back({rfl::Hex<uint32_t>(propId), weight});
        }
        for (const uint32_t pinnedId : pinnedIds) {
            if (!seenIds.count(pinnedId)) {
                float weight = 1.0f;
                if (const auto it = weightOverrides.find(pinnedId); it != weightOverrides.end()) weight = it->second;
                result.push_back({rfl::Hex<uint32_t>(pinnedId), weight});
            }
        }
        return result;
    }
    else {
        if (de.storedIndex < 0 || static_cast<size_t>(de.storedIndex) >= familyEntries_.size()) return {};
        std::vector<PaletteEntry> result;
        for (const auto& cfg : familyEntries_[de.storedIndex].propConfigs) {
            if (!cfg.excluded) result.push_back({cfg.propID, cfg.weight});
        }
        return result;
    }
}

FamilyEntry& SC4AdvancedLotPlopDirector::GetOrCreateStoredEntry_(const size_t displayIndex) {
    auto& de = familyDisplayList_[displayIndex];
    if (de.storedIndex >= 0 && static_cast<size_t>(de.storedIndex) < familyEntries_.size()) {
        return familyEntries_[de.storedIndex];
    }
    FamilyEntry entry;
    entry.name = de.name;
    entry.starred = de.starred;
    if (de.familyId.has_value()) entry.familyId = rfl::Hex<uint32_t>(*de.familyId);
    familyEntries_.push_back(std::move(entry));
    de.storedIndex = static_cast<int>(familyEntries_.size() - 1);
    return familyEntries_.back();
}

void SC4AdvancedLotPlopDirector::BuildFamilyDisplayList_() {
    std::optional<uint32_t> activeFamId;
    int activeStoredIdx = -1;
    if (activeFamilyDisplayIndex_ < familyDisplayList_.size()) {
        activeFamId = familyDisplayList_[activeFamilyDisplayIndex_].familyId;
        activeStoredIdx = familyDisplayList_[activeFamilyDisplayIndex_].storedIndex;
    }

    familyDisplayList_.clear();

    std::unordered_map<uint32_t, int> storedByFamilyId;
    for (int i = 0; i < static_cast<int>(familyEntries_.size()); ++i) {
        if (familyEntries_[i].familyId.has_value()) {
            storedByFamilyId.emplace(familyEntries_[i].familyId->value(), i);
        }
    }

    for (const auto& family : propFamilies_) {
        const uint32_t famId = family.familyId.value();
        FamilyDisplayEntry de;
        de.familyId = famId;
        auto it = storedByFamilyId.find(famId);
        if (it != storedByFamilyId.end()) {
            const auto& stored = familyEntries_[it->second];
            de.name = stored.name.empty() ? family.displayName : stored.name;
            de.starred = stored.starred;
            de.storedIndex = it->second;
        }
        else {
            if (!family.displayName.empty()) {
                de.name = family.displayName;
            }
            else {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "0x%08X", famId);
                de.name = buf;
            }
            de.starred = false;
            de.storedIndex = -1;
        }
        familyDisplayList_.push_back(std::move(de));
    }

    for (int i = 0; i < static_cast<int>(familyEntries_.size()); ++i) {
        if (!familyEntries_[i].familyId.has_value()) {
            FamilyDisplayEntry de;
            de.name = familyEntries_[i].name;
            de.starred = familyEntries_[i].starred;
            de.storedIndex = i;
            familyDisplayList_.push_back(std::move(de));
        }
    }

    std::stable_sort(familyDisplayList_.begin(), familyDisplayList_.end(),
        [](const FamilyDisplayEntry& a, const FamilyDisplayEntry& b) {
            if (a.starred != b.starred) return a.starred > b.starred;
            return a.name < b.name;
        });

    activeFamilyDisplayIndex_ = 0;
    for (size_t i = 0; i < familyDisplayList_.size(); ++i) {
        const auto& de = familyDisplayList_[i];
        if (activeFamId.has_value() && de.familyId == activeFamId) {
            activeFamilyDisplayIndex_ = i;
            break;
        }
        if (!activeFamId.has_value() && de.storedIndex == activeStoredIdx && activeStoredIdx >= 0) {
            activeFamilyDisplayIndex_ = i;
            break;
        }
    }
}

void SC4AdvancedLotPlopDirector::SetFamilyStarred(const size_t displayIndex, const bool starred) {
    if (displayIndex >= familyDisplayList_.size()) return;
    auto& entry = GetOrCreateStoredEntry_(displayIndex);
    entry.starred = starred;
    familyDisplayList_[displayIndex].starred = starred;
    BuildFamilyDisplayList_();
    SaveFavorites_();
}

void SC4AdvancedLotPlopDirector::SetFamilyPropWeight(const size_t displayIndex, const uint32_t propID, const float weight) {
    if (displayIndex >= familyDisplayList_.size()) return;
    auto& entry = GetOrCreateStoredEntry_(displayIndex);
    for (auto& cfg : entry.propConfigs) {
        if (cfg.propID.value() == propID) {
            cfg.weight = std::max(0.1f, weight);
            SaveFavorites_();
            return;
        }
    }
    FamilyPropConfig cfg;
    cfg.propID = rfl::Hex<uint32_t>(propID);
    cfg.weight = std::max(0.1f, weight);
    entry.propConfigs.push_back(std::move(cfg));
    SaveFavorites_();
}

void SC4AdvancedLotPlopDirector::SetFamilyPropExcluded(const size_t displayIndex, const uint32_t propID, const bool excluded) {
    if (displayIndex >= familyDisplayList_.size()) return;
    auto& entry = GetOrCreateStoredEntry_(displayIndex);
    for (auto& cfg : entry.propConfigs) {
        if (cfg.propID.value() == propID) {
            cfg.excluded = excluded;
            SaveFavorites_();
            return;
        }
    }
    FamilyPropConfig cfg;
    cfg.propID = rfl::Hex<uint32_t>(propID);
    cfg.excluded = excluded;
    entry.propConfigs.push_back(std::move(cfg));
    SaveFavorites_();
}

bool SC4AdvancedLotPlopDirector::CreateManualPalette(const std::string& name) {
    if (name.empty()) return false;
    for (const auto& entry : familyEntries_) {
        if (!entry.familyId.has_value() && entry.name == name) return false;
    }
    FamilyEntry entry;
    entry.name = name;
    familyEntries_.push_back(std::move(entry));
    BuildFamilyDisplayList_();
    for (size_t i = 0; i < familyDisplayList_.size(); ++i) {
        if (!familyDisplayList_[i].familyId.has_value() &&
            familyDisplayList_[i].storedIndex == static_cast<int>(familyEntries_.size() - 1)) {
            activeFamilyDisplayIndex_ = i;
            break;
        }
    }
    SaveFavorites_();
    return true;
}

bool SC4AdvancedLotPlopDirector::DeleteFamilyEntry(const size_t displayIndex) {
    if (displayIndex >= familyDisplayList_.size()) return false;
    const int storedIdx = familyDisplayList_[displayIndex].storedIndex;
    if (storedIdx < 0 || static_cast<size_t>(storedIdx) >= familyEntries_.size()) return false;
    familyEntries_.erase(familyEntries_.begin() + storedIdx);
    BuildFamilyDisplayList_();
    if (!familyDisplayList_.empty()) {
        activeFamilyDisplayIndex_ = std::min(activeFamilyDisplayIndex_, familyDisplayList_.size() - 1);
    }
    SaveFavorites_();
    return true;
}

bool SC4AdvancedLotPlopDirector::RenameFamilyEntry(const size_t displayIndex, const std::string& newName) {
    if (displayIndex >= familyDisplayList_.size() || newName.empty()) return false;
    auto& entry = GetOrCreateStoredEntry_(displayIndex);
    entry.name = newName;
    familyDisplayList_[displayIndex].name = newName;
    SaveFavorites_();
    return true;
}

std::vector<std::pair<size_t, std::string>> SC4AdvancedLotPlopDirector::GetManualPaletteList() const {
    std::vector<std::pair<size_t, std::string>> result;
    for (size_t i = 0; i < familyEntries_.size(); ++i) {
        if (!familyEntries_[i].familyId.has_value()) {
            result.emplace_back(i, familyEntries_[i].name);
        }
    }
    return result;
}

bool SC4AdvancedLotPlopDirector::AddPropToManualPalette(const uint32_t propID, const size_t familyEntryIndex) {
    if (familyEntryIndex >= familyEntries_.size() || propID == 0) return false;
    auto& entry = familyEntries_[familyEntryIndex];
    if (entry.familyId.has_value()) return false;
    if (!FindPropByInstanceId_(propID)) return false;
    for (const auto& cfg : entry.propConfigs) {
        if (cfg.propID.value() == propID) return false;
    }
    FamilyPropConfig cfg;
    cfg.propID = rfl::Hex<uint32_t>(propID);
    entry.propConfigs.push_back(std::move(cfg));
    SaveFavorites_();
    return true;
}

void SC4AdvancedLotPlopDirector::AddPropToNewManualPalette(const uint32_t propID, const std::string& baseName) {
    const std::string defaultName = BuildDefaultPaletteName_(baseName);
    std::string candidateName = defaultName;
    int suffix = 2;
    auto nameExists = [this](const std::string& name) {
        return std::any_of(familyEntries_.begin(), familyEntries_.end(), [&](const FamilyEntry& e) {
            return !e.familyId.has_value() && e.name == name;
        });
    };
    while (nameExists(candidateName)) {
        candidateName = defaultName + " (" + std::to_string(suffix++) + ")";
    }
    if (!CreateManualPalette(candidateName)) return;
    const int newStoredIdx = familyDisplayList_[activeFamilyDisplayIndex_].storedIndex;
    if (newStoredIdx >= 0) {
        AddPropToManualPalette(propID, static_cast<size_t>(newStoredIdx));
    }
}

void SC4AdvancedLotPlopDirector::SaveFavoritesNow() const {
    SaveFavorites_();
}

bool SC4AdvancedLotPlopDirector::StartPropPainting(uint32_t propId, const PropPaintSettings& settings,
                                                   const std::string& name) {
    if (!pCity_ || !pView3D_) {
        spdlog::warn("Cannot start prop painting: city or view not available");
        return false;
    }

    if (!propPainterControl_) {
        auto* control = new PropPainterInputControl();
        propPainterControl_ = control;
        if (! propPainterControl_) {
            spdlog::error("Failed to allocate PropPainterInputControl");
            return false;
        }

        if (!propPainterControl_->Init()) {
            spdlog::error("Failed to initialize PropPainterInputControl");
            propPainterControl_.Reset();
            return false;
        }
    }

    propPainterControl_->SetCity(pCity_);
    propPainterControl_->SetWindow(pView3D_->AsIGZWin());
    propPainterControl_->SetCameraService(cameraService_);
    propPainterControl_->SetOnCancel([this]() {
        if (pView3D_ && propPainterControl_ &&
            pView3D_->GetCurrentViewInputControl() == propPainterControl_) {
            pView3D_->RemoveCurrentViewInputControl(false);
        }
        propPainting_ = false;
        spdlog::info("Stopped prop painting");
    });

    propPainterControl_->SetPropToPaint(propId, settings, name);
    if (!pView3D_->SetCurrentViewInputControl(
        propPainterControl_,
        cISC4View3DWin::ViewInputControlStackOperation_RemoveCurrentControl)) {
        spdlog::warn("Failed to set prop painter as current view input control");
        return false;
    }

    propPainting_ = true;
    spdlog::info("Started prop painting: 0x{:08X}, rotation {}", propId, settings.rotation);
    return true;
}

bool SC4AdvancedLotPlopDirector::SwitchPropPaintingTarget(uint32_t propId, const std::string& name) {
    if (!propPainterControl_ || !propPainting_ || !pView3D_) {
        return false;
    }
    if (pView3D_->GetCurrentViewInputControl() != propPainterControl_) {
        return false;
    }

    const auto& settings = propPainterControl_->GetSettings();
    return StartPropPainting(propId, settings, name);
}

void SC4AdvancedLotPlopDirector::StopPropPainting() {
    if (pView3D_ && propPainterControl_) {
        if (pView3D_->GetCurrentViewInputControl() == propPainterControl_) {
            pView3D_->RemoveCurrentViewInputControl(false);
        }
    }

    propPainting_ = false;
    spdlog::info("Stopped prop painting");
}

bool SC4AdvancedLotPlopDirector::IsPropPainting() const {
    return propPainting_;
}

void SC4AdvancedLotPlopDirector::DrawOverlayCallback_(const DrawServicePass pass, const bool begin, void* pThis) {
    if (pass != DrawServicePass::PreDynamic || begin) {
        return;
    }

    auto* director = static_cast<SC4AdvancedLotPlopDirector*>(pThis);
    if (!director || !director->imguiService_ || !director->propPainting_ || !director->propPainterControl_) {
        return;
    }

    IDirect3DDevice7* device = nullptr;
    IDirectDraw7* dd = nullptr;
    if (!director->imguiService_->AcquireD3DInterfaces(&device, &dd)) {
        return;
    }

    director->propPainterControl_->DrawOverlay(device);
    device->Release();
    dd->Release();
}

void SC4AdvancedLotPlopDirector::SetLotPlopPanelVisible(const bool visible) {
    if (!imguiService_ || !panelRegistered_ || !panel_) {
        return;
    }

    panelVisible_ = visible;
    panel_->SetOpen(visible);
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
                    RegisterLotPlopShortcut_();
                }
            }
        }
    }
}

void SC4AdvancedLotPlopDirector::PreCityShutdown_(cIGZMessage2Standard* pStandardMsg) {
    SetLotPlopPanelVisible(false);
    StopPropPainting();
    if (propPainterControl_) {
        propPainterControl_->SetCity(nullptr);
    }
    pCity_ = nullptr;
    if (pView3D_) {
        pView3D_->Release();
        pView3D_ = nullptr;
    }
    UnregisterLotPlopShortcut_();
    spdlog::info("City shutdown - released resources");
}

void SC4AdvancedLotPlopDirector::ToggleLotPlopPanel_() {
    SetLotPlopPanelVisible(!panelVisible_);
}

bool SC4AdvancedLotPlopDirector::RegisterLotPlopShortcut_() {
    if (shortcutRegistered_) {
        return true;
    }
    if (!pView3D_) {
        spdlog::warn("Cannot register lot plop shortcut: View3D not available");
        return false;
    }
    if (!pMS2_) {
        spdlog::warn("Cannot register lot plop shortcut: message server not available");
        return false;
    }

    cIGZPersistResourceManagerPtr pRM;
    if (!pRM) {
        spdlog::warn("Cannot register lot plop shortcut: resource manager unavailable");
        return false;
    }

    cRZAutoRefCount<cIGZWinKeyAcceleratorRes> acceleratorRes;
    const cGZPersistResourceKey key(kKeyConfigType, kKeyConfigGroup, kKeyConfigInstance);
    if (!pRM->GetPrivateResource(key, kGZIID_cIGZWinKeyAcceleratorRes,
                                 acceleratorRes.AsPPVoid(), 0, nullptr)) {
        spdlog::warn("Failed to load key config resource 0x{:08X}/0x{:08X}/0x{:08X}",
                     kKeyConfigType, kKeyConfigGroup, kKeyConfigInstance);
        return false;
    }

    auto* accelerator = pView3D_->GetKeyAccelerator();
    if (!accelerator) {
        spdlog::warn("Cannot register lot plop shortcut: key accelerator not available");
        return false;
    }

    if (!acceleratorRes->RegisterResources(accelerator)) {
        spdlog::warn("Failed to register key accelerator resources");
        return false;
    }

    if (!pMS2_->AddNotification(this, kToggleLotPlopWindowShortcutID)) {
        spdlog::warn("Failed to register shortcut notification 0x{:08X}",
                     kToggleLotPlopWindowShortcutID);
        return false;
    }

    shortcutRegistered_ = true;
    return true;
}

void SC4AdvancedLotPlopDirector::UnregisterLotPlopShortcut_() {
    if (!shortcutRegistered_) {
        return;
    }

    if (pMS2_) {
        pMS2_->RemoveNotification(this, kToggleLotPlopWindowShortcutID);
    }
    shortcutRegistered_ = false;
}

void SC4AdvancedLotPlopDirector::LoadLots_() {
    try {
        const auto pluginsPath = GetUserPluginsPath_();
        const auto cborPath = pluginsPath / "lot_configs.cbor";

        if (!std::filesystem::exists(cborPath)) {
            spdlog::warn("Lot config CBOR file not found: {}", cborPath.string());
            return;
        }

        auto result = rfl::cbor::load<std::vector<Building>>(cborPath.string());
        if (result) {
            buildings_ = std::move(*result);
            buildingsById_ = std::unordered_map<uint64_t, Building>(buildings_.size());

            size_t lotCount = 0;
            std::unordered_set<uint64_t> lotKeys;
            size_t duplicateLots = 0;
            for (const auto& b : buildings_) {
                buildingsById_.emplace(MakeGIKey(b.groupId.value(), b.instanceId.value()), b);
                for (const auto& lot : b.lots) {
                    ++lotCount;
                    const uint64_t key = MakeGIKey(lot.groupId.value(), lot.instanceId.value());
                    if (!lotKeys.insert(key).second) {
                        ++duplicateLots;
                        spdlog::warn("Duplicate lot in CBOR: group=0x{:08X}, instance=0x{:08X}", lot.groupId.value(),
                                     lot.instanceId.value());
                    }
                    lotsById_.emplace(key, lot);
                }
            }

            spdlog::info("Loaded {} buildings / {} lots from {}", buildings_.size(), lotCount, cborPath.string());
            if (duplicateLots > 0) {
                spdlog::warn("Detected {} duplicate lot IDs in CBOR", duplicateLots);
            }
        }
        else {
            spdlog::error("Failed to load lots from CBOR file: {}", result.error().what());
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error loading lots: {}", e.what());
    }
}

void SC4AdvancedLotPlopDirector::LoadProps_() {
    try {
        const auto pluginsPath = GetUserPluginsPath_();
        const auto cborPath = pluginsPath / "props.cbor";

        if (!std::filesystem::exists(cborPath)) {
            spdlog::warn("Prop CBOR file not found: {}", cborPath.string());
            return;
        }

        props_.clear();
        propsById_.clear();
        propFamilies_.clear();
        propFamilyNames_.clear();

        auto rebuildPropIndexes = [this]() {
            propsById_ = std::unordered_map<uint64_t, Prop>(props_.size());
            for (const auto& p : props_) {
                propsById_.emplace((static_cast<uint64_t>(p.groupId.value()) << 32) | p.instanceId.value(), p);
            }
        };

        if (auto result = rfl::cbor::load<PropsCache>(cborPath.string())) {
            props_ = std::move(result->props);
            propFamilies_ = std::move(result->propFamilies);
            propFamilyNames_.clear();
            for (const auto& family : propFamilies_) {
                if (!family.displayName.empty()) {
                    propFamilyNames_.emplace(family.familyId.value(), family.displayName);
                }
            }
            rebuildPropIndexes();

            spdlog::info("Loaded {} props and {} prop families from {}",
                         props_.size(), propFamilyNames_.size(), cborPath.string());
            return;
        }

        if (auto legacyResult = rfl::cbor::load<std::vector<Prop>>(cborPath.string())) {
            props_ = std::move(*legacyResult);
            propFamilies_.clear();
            propFamilyNames_.clear();
            rebuildPropIndexes();

            spdlog::info("Loaded {} props from legacy cache format in {}",
                         props_.size(), cborPath.string());
        }
        else {
            spdlog::error("Failed to load props from CBOR file: {}", legacyResult.error().what());
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error loading props: {}", e.what());
    }
}

void SC4AdvancedLotPlopDirector::LoadFavorites_() {
    try {
        const auto pluginsPath = GetUserPluginsPath_();
        const auto cborPath = pluginsPath / "favorites.cbor";

        if (!std::filesystem::exists(cborPath)) {
            spdlog::info("Favorites file not found, starting with empty favorites");
            favoriteLotIds_.clear();
            favoritePropIds_.clear();
            familyEntries_.clear();
            activeFamilyDisplayIndex_ = 0;
            BuildFamilyDisplayList_();
            return;
        }

        if (auto result = rfl::cbor::load<AllFavorites>(cborPath.string())) {
            // Extract lot favorites from the loaded data
            favoriteLotIds_.clear();
            for (const auto& hexId : result->lots.items) {
                favoriteLotIds_.insert(static_cast<uint32_t>(hexId.value()));
            }
            favoritePropIds_.clear();
            if (result->props) {
                for (const auto& hexId : result->props->items) {
                    favoritePropIds_.insert(hexId.value());
                }
            }

            familyEntries_.clear();
            if (result->families) {
                familyEntries_ = *result->families;
                for (auto& entry : familyEntries_) {
                    entry.densityVariation = std::clamp(entry.densityVariation, 0.0f, 1.0f);
                    if (!entry.familyId.has_value()) {
                        // Manual palette: remove stale prop references
                        std::erase_if(entry.propConfigs, [this](const FamilyPropConfig& cfg) {
                            return FindPropByInstanceId_(cfg.propID.value()) == nullptr;
                        });
                    }
                    for (auto& cfg : entry.propConfigs) {
                        if (!cfg.excluded) cfg.weight = std::max(0.1f, cfg.weight);
                    }
                }
            }
            activeFamilyDisplayIndex_ = 0;
            BuildFamilyDisplayList_();
            spdlog::info("Loaded {} favorite lots, {} family entries from {}",
                         favoriteLotIds_.size(), familyEntries_.size(), cborPath.string());
        }
        else {
            spdlog::warn("Failed to load favorites from CBOR file: {}", result.error().what());
        }
    }
    catch (const std::exception& e) {
        spdlog::warn("Error loading favorites (will start empty): {}", e.what());
    }
}

void SC4AdvancedLotPlopDirector::SaveFavorites_() const {
    try {
        const auto pluginsPath = GetUserPluginsPath_();
        const auto cborPath = pluginsPath / "favorites.cbor";

        // Build the AllFavorites structure
        AllFavorites allFavorites;
        allFavorites.version = 3;

        // Convert favorites set to vector of Hex<uint32_t>
        for (uint32_t id : favoriteLotIds_) {
            allFavorites.lots.items.emplace_back(id);
        }

        // Set timestamp to current time using std::chrono
        const auto now = std::chrono::system_clock::now();
        const auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_now;
        localtime_s(&tm_now, &time_t_now);
        allFavorites.lastModified = rfl::Timestamp<"%Y-%m-%dT%H:%M:%S">(tm_now);

        if (!favoritePropIds_.empty()) {
            TabFavorites propFavorites;
            propFavorites.items.reserve(favoritePropIds_.size());
            for (uint64_t id : favoritePropIds_) {
                propFavorites.items.emplace_back(id);
            }
            allFavorites.props = std::move(propFavorites);
        }
        else {
            allFavorites.props = std::nullopt;
        }
        allFavorites.flora = std::nullopt;
        if (familyEntries_.empty()) {
            allFavorites.families = std::nullopt;
        }
        else {
            allFavorites.families = familyEntries_;
        }

        // Save to CBOR file
        if (const auto saveResult = rfl::cbor::save(cborPath.string(), allFavorites)) {
            spdlog::info("Saved {} favorites to {}", favoriteLotIds_.size(), cborPath.string());
        }
        else {
            spdlog::error("Failed to save favorites: {}", saveResult.error().what());
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error saving favorites: {}", e.what());
    }
}

const Prop* SC4AdvancedLotPlopDirector::FindPropByInstanceId_(const uint32_t propID) const {
    for (const auto& prop : props_) {
        if (prop.instanceId.value() == propID) {
            return &prop;
        }
    }
    return nullptr;
}

std::string SC4AdvancedLotPlopDirector::BuildDefaultPaletteName_(const std::string& baseName) {
    std::string name = baseName.empty() ? std::string("Palette") : baseName;
    name += " mix";
    return name;
}

std::filesystem::path SC4AdvancedLotPlopDirector::GetUserPluginsPath_() {
    // Get the directory where this DLL is loaded from
    try {
        // Get the module path using WIL's safe wrapper
        const auto modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());

        // Convert to filesystem::path and get the parent directory
        std::filesystem::path dllDir = std::filesystem::path(modulePath.get()).parent_path();
        spdlog::info("DLL directory: {}", dllDir.string());
        return dllDir;
    }
    catch (const wil::ResultException& e) {
        spdlog::error("Failed to get DLL directory: {}", e.what());
        return {};
    }
}
