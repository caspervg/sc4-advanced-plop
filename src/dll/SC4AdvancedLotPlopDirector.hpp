#pragma once

#include <cRZCOMDllDirector.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
#include "PropPainterInputControl.hpp"
#include "../shared/entities.hpp"
#include "public/cIGZImGuiService.h"
#include "public/cIGZDrawService.h"
#include "public/ImGuiPanel.h"
#include "public/ImGuiPanelAdapter.h"
#include "public/ImGuiServiceIds.h"

class LotPlopPanel;
struct PropPaintSettings;
class cIGZS3DCameraService;

static constexpr uint32_t kSC4MessagePostCityInit = 0x26D31EC1;
static constexpr uint32_t kSC4MessagePreCityShutdown = 0x26D31EC2;

// A display-list entry for the Families tab. Computed at runtime from the
// props cache + stored family entries; not persisted.
struct FamilyDisplayEntry {
    std::string name;
    bool starred = false;
    std::optional<uint32_t> familyId;  // absent for manual palettes
    int storedIndex = -1;              // index into familyEntries_, or -1 if no stored data
};

class SC4AdvancedLotPlopDirector final : public cRZMessage2COMDirector
{
public:
    SC4AdvancedLotPlopDirector();
    ~SC4AdvancedLotPlopDirector() override;

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

    [[nodiscard]] const std::vector<Building>& GetBuildings() const;
    [[nodiscard]] const std::unordered_map<uint64_t, Building>& GetBuildingsById() const;
    [[nodiscard]] const std::unordered_map<uint64_t, Lot>& GetLotsById() const;
    [[nodiscard]] const std::vector<Prop>& GetProps() const;
    [[nodiscard]] const std::unordered_map<uint64_t, Prop>& GetPropsById() const;
    [[nodiscard]] const std::unordered_map<uint32_t, std::string>& GetPropFamilyNames() const;
    void TriggerLotPlop(uint32_t lotInstanceId) const;

    // Lot/prop favorites
    [[nodiscard]] bool IsFavorite(uint32_t lotInstanceId) const;
    [[nodiscard]] const std::unordered_set<uint32_t>& GetFavoriteLotIds() const;
    void ToggleFavorite(uint32_t lotInstanceId);
    [[nodiscard]] bool IsPropFavorite(uint32_t groupId, uint32_t instanceId) const;
    [[nodiscard]] const std::unordered_set<uint64_t>& GetFavoritePropIds() const;
    void TogglePropFavorite(uint32_t groupId, uint32_t instanceId);

    // Families tab: read
    [[nodiscard]] const std::vector<FamilyDisplayEntry>& GetFamilyDisplayList() const;
    [[nodiscard]] size_t GetActiveFamilyIndex() const;
    void SetActiveFamilyIndex(size_t index);
    [[nodiscard]] const FamilyEntry* GetActiveFamilyEntry() const;
    [[nodiscard]] const FamilyEntry* GetStoredFamilyEntry(size_t displayIndex) const;
    [[nodiscard]] std::vector<PaletteEntry> ResolveFamilyProps(size_t displayIndex) const;

    // Families tab: mutations
    void SetFamilyStarred(size_t displayIndex, bool starred);
    void SetFamilyPropWeight(size_t displayIndex, uint32_t propID, float weight);
    void SetFamilyPropExcluded(size_t displayIndex, uint32_t propID, bool excluded);
    bool CreateManualPalette(const std::string& name);
    bool DeleteFamilyEntry(size_t displayIndex);
    bool RenameFamilyEntry(size_t displayIndex, const std::string& newName);

    // Props tab: add props to manual palettes
    [[nodiscard]] std::vector<std::pair<size_t, std::string>> GetManualPaletteList() const;
    bool AddPropToManualPalette(uint32_t propID, size_t familyEntryIndex);
    void AddPropToNewManualPalette(uint32_t propID, const std::string& baseName);

    void SaveFavoritesNow() const;
    bool StartPropPainting(uint32_t propId, const PropPaintSettings& settings, const std::string& name);
    bool SwitchPropPaintingTarget(uint32_t propId, const std::string& name);
    void StopPropPainting();
    [[nodiscard]] bool IsPropPainting() const;
    void SetLotPlopPanelVisible(bool visible);

private:
    void PostCityInit_(const cIGZMessage2Standard* pStandardMsg);
    void PreCityShutdown_(cIGZMessage2Standard* pStandardMsg);
    void ToggleLotPlopPanel_();
    bool RegisterLotPlopShortcut_();
    void UnregisterLotPlopShortcut_();
    void LoadLots_();
    void LoadProps_();
    void LoadFavorites_();
    void SaveFavorites_() const;
    void BuildFamilyDisplayList_();
    FamilyEntry& GetOrCreateStoredEntry_(size_t displayIndex);
    static std::filesystem::path GetUserPluginsPath_();
    static void DrawOverlayCallback_(DrawServicePass pass, bool begin, void* pThis);
    [[nodiscard]] const Prop* FindPropByInstanceId_(uint32_t propID) const;
    static std::string BuildDefaultPaletteName_(const std::string& baseName);

private:
    cIGZImGuiService* imguiService_ = nullptr;
    cIGZDrawService* drawService_ = nullptr;
    cRZAutoRefCount<cISC4City> pCity_;
    cISC4View3DWin* pView3D_ = nullptr;
    cRZAutoRefCount<cIGZMessageServer2> pMS2_;
    cIGZS3DCameraService* cameraService_ = nullptr;

    std::vector<Building> buildings_{};
    std::unordered_map<uint64_t, Building> buildingsById_{};
    std::unordered_set<uint32_t> openBuildings_{};
    std::unordered_map<uint64_t, Lot> lotsById_{};
    std::vector<Prop> props_{};
    std::unordered_map<uint64_t, Prop> propsById_{};
    std::vector<PropFamilyInfo> propFamilies_{};           // all families from cache
    std::unordered_map<uint32_t, std::string> propFamilyNames_{};  // id → display name (named only)
    std::unordered_set<uint32_t> favoriteLotIds_{};
    std::unordered_set<uint64_t> favoritePropIds_{};
    std::vector<FamilyEntry> familyEntries_{};             // persisted: overrides + manual palettes
    std::vector<FamilyDisplayEntry> familyDisplayList_{};  // computed, not persisted
    size_t activeFamilyDisplayIndex_{0};
    bool panelRegistered_{false};
    bool panelVisible_{false};
    bool shortcutRegistered_{false};
    std::unique_ptr<LotPlopPanel> panel_;
    cRZAutoRefCount<PropPainterInputControl> propPainterControl_;
    bool propPainting_{false};
    uint32_t drawCallbackToken_{0};
};
