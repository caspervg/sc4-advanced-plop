#pragma once
#include <functional>
#include <string>
#include <vector>

#include "cISC4City.h"
#include "cISC4Occupant.h"
#include "cISC4PropManager.h"
#include "cRZAutoRefCount.h"
#include "cS3DVector3.h"
#include "cSC4BaseViewInputControl.h"
#include "public/cIGZS3DCameraService.h"

struct PropPainterPreviewState {
    bool cursorValid = false;
    cS3DVector3 cursorWorldPos;
    std::string propName;
    uint32_t propID = 0;
    int32_t rotation = 0;
    int32_t paintMode = 0;
};

enum class PropPaintMode {
    Direct = 0,
    Line = 1,
    Polygon = 2
};

struct PropPaintSettings {
    PropPaintMode mode = PropPaintMode::Direct;
    int32_t rotation = 0;
    float spacingMeters = 5.0f;
    float densityPer100Sqm = 1.0f;
    uint32_t randomSeed = 0;
};

class PropPainterInputControl : public cSC4BaseViewInputControl {
public:
    PropPainterInputControl();
    ~PropPainterInputControl() override;

    bool Init() override;
    bool Shutdown() override;

    bool OnMouseDownL(int32_t x, int32_t z, uint32_t modifiers) override;
    bool OnMouseMove(int32_t x, int32_t z, uint32_t modifiers) override;
    bool OnKeyDown(int32_t vkCode, uint32_t modifiers) override;

    void Activate() override;
    void Deactivate() override;

    void SetPropToPaint(uint32_t propID, const PropPaintSettings& settings, const std::string& name);
    void SetCity(cISC4City* pCity);
    void SetCameraService(cIGZS3DCameraService* cameraService);
    void SetOnCancel(std::function<void()> onCancel);

    [[nodiscard]] const PropPaintSettings& GetSettings() const { return settings_; }

    void UndoLastPlacement();
    void CancelAllPlacements();
    void CommitPlacements();
    // void ShowPropPreview(const cS3DVector3& position, int32_t rotation);
    // void ClearPreview();

private:
    enum class ControlState {
        Uninitialized,
        ReadyNoTarget,
        ReadyWithTarget,
        ActiveNoTarget,
        ActiveDirect,
        ActiveLine,
        ActivePolygon,
    };

    [[nodiscard]] static bool IsActiveState_(ControlState state);
    [[nodiscard]] static bool IsTargetActiveState_(ControlState state);
    [[nodiscard]] static ControlState ActiveStateForMode_(PropPaintMode mode);
    [[nodiscard]] static const char* StateToString_(ControlState state);
    void TransitionTo_(ControlState newState, const char* reason);
    void SyncPreviewForState_();
    bool HandleActiveMouseDownL_(int32_t x, int32_t z, uint32_t modifiers);
    bool HandleActiveMouseMove_(int32_t x, int32_t z, uint32_t modifiers);
    bool HandleActiveKeyDown_(int32_t vkCode, uint32_t modifiers);

    bool PlacePropAt_(int32_t screenX, int32_t screenZ);
    void CreatePreviewProp_();
    void DestroyPreviewProp_();
    void UpdatePreviewPropRotation_();
    void UpdatePreviewProp_(int32_t screenX, int32_t screenZ);

    cRZAutoRefCount<cISC4City> city_;
    cRZAutoRefCount<cISC4PropManager> propManager_;
    ControlState state_ = ControlState::Uninitialized;

    uint32_t propIDToPaint_;
    PropPaintSettings settings_{};
    cIGZS3DCameraService* cameraService_ = nullptr;
    std::function<void()> onCancel_{};

    std::vector<cRZAutoRefCount<cISC4Occupant>> placedProps_;

    cRZAutoRefCount<cISC4PropOccupant> previewProp_;
    cRZAutoRefCount<cISC4Occupant> previewOccupant_;
    bool previewActive_ = false;
    cS3DVector3 lastPreviewPosition_;
    int32_t lastPreviewRotation_{0};

    struct PreviewSettings {
        bool showPreview = true;
    } previewSettings_;
};
