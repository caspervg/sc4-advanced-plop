#include "PropPainterInputControl.hpp"

#include <cmath>
#include <windows.h>

#include "cISC4Occupant.h"
#include "spdlog/spdlog.h"

namespace {
    constexpr auto kPropPainterControlID = 0x8A3F9D2B;
}

PropPainterInputControl::PropPainterInputControl()
    : cSC4BaseViewInputControl(kPropPainterControlID)
      , propIDToPaint_(0)
      , settings_({})
      , onCancel_() {}

PropPainterInputControl::~PropPainterInputControl() = default;

bool PropPainterInputControl::Init() {
    cSC4BaseViewInputControl::Init();
    if (state_ != ControlState::Uninitialized) {
        return true;
    }

    if (!cSC4BaseViewInputControl::Init()) {
        return false;
    }

    TransitionTo_(propIDToPaint_ != 0 ? ControlState::ReadyWithTarget : ControlState::ReadyNoTarget, "Init");
    spdlog::info("PropPainterInputControl initialized");
    return true;
}

bool PropPainterInputControl::Shutdown() {
    cSC4BaseViewInputControl::Shutdown();
    if (state_ == ControlState::Uninitialized) {
        return true;
    }

    spdlog::info("PropPainterInputControl shutting down");
    CancelAllPlacements();
    TransitionTo_(ControlState::Uninitialized, "Shutdown");
    return true;
}

bool PropPainterInputControl::OnMouseDownL(int32_t x, int32_t z, uint32_t modifiers) {
    if (!IsActiveState_(state_) || !IsOnTop()) {
        return false;
    }

    return HandleActiveMouseDownL_(x, z, modifiers);
}

bool PropPainterInputControl::OnMouseMove(const int32_t x, const int32_t z, uint32_t modifiers) {
    if (!IsActiveState_(state_) || !IsOnTop()) {
        return false;
    }

    return HandleActiveMouseMove_(x, z, modifiers);
}

bool PropPainterInputControl::OnKeyDown(const int32_t vkCode, uint32_t modifiers) {
    if (!IsActiveState_(state_) || !IsOnTop()) {
        return false;
    }

    return HandleActiveKeyDown_(vkCode, modifiers);
}

void PropPainterInputControl::Activate() {
    cSC4BaseViewInputControl::Activate();
    if (!Init()) {
        spdlog::warn("PropPainterInputControl: Init failed during Activate");
        return;
    }

    TransitionTo_(propIDToPaint_ != 0 ? ActiveStateForMode_(settings_.mode) : ControlState::ActiveNoTarget,
                  "Activate");
    spdlog::info("PropPainterInputControl activated");
}

void PropPainterInputControl::Deactivate() {
    DestroyPreviewProp_();

    if (state_ != ControlState::Uninitialized) {
        TransitionTo_(propIDToPaint_ != 0 ? ControlState::ReadyWithTarget : ControlState::ReadyNoTarget,
                      "Deactivate");
    }

    cSC4BaseViewInputControl::Deactivate();
    spdlog::info("PropPainterInputControl deactivated");
}

void PropPainterInputControl::SetPropToPaint(uint32_t propID, const PropPaintSettings& settings,
                                             const std::string& name) {
    const bool targetChanged = propIDToPaint_ != propID;

    propIDToPaint_ = propID;
    settings_ = settings;
    spdlog::info("Setting prop to paint: {} (0x{:08X}), rotation: {}", name, propID, settings.rotation);

    if (targetChanged) {
        DestroyPreviewProp_();
    }

    if (state_ == ControlState::Uninitialized) {
        return;
    }

    if (propIDToPaint_ == 0) {
        TransitionTo_(IsActiveState_(state_) ? ControlState::ActiveNoTarget : ControlState::ReadyNoTarget,
                      "SetPropToPaint clear target");
        return;
    }

    TransitionTo_(IsActiveState_(state_) ? ActiveStateForMode_(settings_.mode) : ControlState::ReadyWithTarget,
                  "SetPropToPaint");
}

void PropPainterInputControl::SetCity(cISC4City* pCity) {
    city_ = pCity;
    if (pCity) {
        propManager_ = pCity->GetPropManager();
    }
    else {
        DestroyPreviewProp_();
        propManager_.Reset();
    }
}

void PropPainterInputControl::SetCameraService(cIGZS3DCameraService* cameraService) {
    cameraService_ = cameraService;
}

void PropPainterInputControl::SetOnCancel(std::function<void()> onCancel) {
    onCancel_ = std::move(onCancel);
}

bool PropPainterInputControl::IsActiveState_(ControlState state) {
    return state == ControlState::ActiveNoTarget ||
        state == ControlState::ActiveDirect ||
        state == ControlState::ActiveLine ||
        state == ControlState::ActivePolygon;
}

bool PropPainterInputControl::IsTargetActiveState_(ControlState state) {
    return state == ControlState::ActiveDirect ||
        state == ControlState::ActiveLine ||
        state == ControlState::ActivePolygon;
}

PropPainterInputControl::ControlState PropPainterInputControl::ActiveStateForMode_(PropPaintMode mode) {
    switch (mode) {
    case PropPaintMode::Direct:
        return ControlState::ActiveDirect;
    case PropPaintMode::Line:
        return ControlState::ActiveLine;
    case PropPaintMode::Polygon:
        return ControlState::ActivePolygon;
    default:
        return ControlState::ActiveDirect;
    }
}

const char* PropPainterInputControl::StateToString_(ControlState state) {
    switch (state) {
    case ControlState::Uninitialized:
        return "Uninitialized";
    case ControlState::ReadyNoTarget:
        return "ReadyNoTarget";
    case ControlState::ReadyWithTarget:
        return "ReadyWithTarget";
    case ControlState::ActiveNoTarget:
        return "ActiveNoTarget";
    case ControlState::ActiveDirect:
        return "ActiveDirect";
    case ControlState::ActiveLine:
        return "ActiveLine";
    case ControlState::ActivePolygon:
        return "ActivePolygon";
    default:
        return "Unknown";
    }
}

void PropPainterInputControl::TransitionTo_(ControlState newState, const char* reason) {
    if (state_ == newState) {
        SyncPreviewForState_();
        return;
    }

    const auto oldState = state_;
    state_ = newState;
    spdlog::debug("PropPainterInputControl state transition: {} -> {} ({})",
                  StateToString_(oldState), StateToString_(newState), reason);
    SyncPreviewForState_();
}

void PropPainterInputControl::SyncPreviewForState_() {
    if (!IsTargetActiveState_(state_) || !previewSettings_.showPreview) {
        if (previewOccupant_) {
            previewOccupant_->SetVisibility(false, true);
        }
        if (!IsTargetActiveState_(state_)) {
            DestroyPreviewProp_();
        }
        return;
    }

    if (!previewProp_) {
        CreatePreviewProp_();
    }
    else if (previewOccupant_) {
        previewOccupant_->SetVisibility(true, true);
        UpdatePreviewPropRotation_();
    }
}

bool PropPainterInputControl::HandleActiveMouseDownL_(int32_t x, int32_t z, uint32_t /*modifiers*/) {
    switch (state_) {
    case ControlState::ActiveDirect:
        return PlacePropAt_(x, z);
    case ControlState::ActiveLine:
        spdlog::debug("Line mode input is not implemented yet");
        return true;
    case ControlState::ActivePolygon:
        spdlog::debug("Polygon mode input is not implemented yet");
        return true;
    case ControlState::ActiveNoTarget:
    default:
        return false;
    }
}

bool PropPainterInputControl::HandleActiveMouseMove_(int32_t x, int32_t z, uint32_t /*modifiers*/) {
    if (!IsTargetActiveState_(state_)) {
        return false;
    }

    UpdatePreviewProp_(x, z);
    return true;
}

bool PropPainterInputControl::HandleActiveKeyDown_(int32_t vkCode, uint32_t modifiers) {
    if (vkCode == VK_ESCAPE) {
        CancelAllPlacements();
        spdlog::info("PropPainterInputControl: ESC pressed, stopping paint mode");
        TransitionTo_(propIDToPaint_ != 0 ? ControlState::ReadyWithTarget : ControlState::ReadyNoTarget,
                      "ESC cancel");
        if (onCancel_) onCancel_();
        return true;
    }

    if (!IsTargetActiveState_(state_)) {
        return false;
    }

    if (vkCode == 'R') {
        settings_.rotation = (settings_.rotation + 1) % 4;
        UpdatePreviewPropRotation_();
        return true;
    }

    if (vkCode == 'Z' && modifiers & MOD_CONTROL) {
        UndoLastPlacement();
        return true;
    }

    if (vkCode == VK_RETURN) {
        CommitPlacements();
        return true;
    }

    if (vkCode == 'P') {
        previewSettings_.showPreview = !previewSettings_.showPreview;
        spdlog::info("Toggled preview visibility: {}", previewSettings_.showPreview);
        SyncPreviewForState_();
        return true;
    }

    return false;
}

void PropPainterInputControl::UndoLastPlacement() {
    if (placedProps_.empty()) {
        spdlog::debug("No props to undo");
        return;
    }

    if (!propManager_) {
        spdlog::warn("No prop manager available during undo; clearing local placed prop history");
        placedProps_.clear();
        return;
    }

    const auto& lastProp = placedProps_.back();

    if (propManager_->RemovePropA(lastProp)) {
        spdlog::info("Removed last placed prop ({} remaining)", placedProps_.size() - 1);
    }
    else {
        spdlog::warn("Failed to remove last placed prop");
    }

    placedProps_.pop_back();
}

void PropPainterInputControl::CancelAllPlacements() {
    if (placedProps_.empty()) {
        return;
    }

    if (!propManager_) {
        spdlog::warn("No prop manager available during cancel; clearing local placed prop history");
        placedProps_.clear();
        return;
    }

    spdlog::info("Canceling {} placed props", placedProps_.size());

    for (auto& prop : placedProps_) {
        if (propManager_->RemovePropA(prop)) {
            spdlog::debug("Removed placed prop");
        }
        else {
            spdlog::warn("Failed to remove placed prop");
        }
    }

    placedProps_.clear();
}

void PropPainterInputControl::CommitPlacements() {
    spdlog::info("Committing {} placed props", placedProps_.size());
    for (const auto& prop : placedProps_) {
        prop->SetHighlight(0x0, true);
    }
    placedProps_.clear();
}

bool PropPainterInputControl::PlacePropAt_(int32_t screenX, int32_t screenZ) {
    if (!propManager_ || !view3D) {
        spdlog::warn("PropPainterInputControl: PropManager or View3D not available");
        return false;
    }

    float worldCoords[3] = {0.0f, 0.0f, 0.0f};
    if (!view3D->PickTerrain(screenX, screenZ, worldCoords, false)) {
        spdlog::debug("Failed to pick terrain at screen ({}, {})", screenX, screenZ);
        return false;
    }

    cS3DVector3 position(worldCoords[0], worldCoords[1], worldCoords[2]);

    cISC4PropOccupant* prop = nullptr;
    if (!propManager_->CreateProp(propIDToPaint_, prop)) {
        spdlog::warn("Failed to create prop 0x{:08X}", propIDToPaint_);
        return false;
    }

    cRZAutoRefCount propRef(prop);
    cISC4Occupant* occupant = prop->AsOccupant();
    if (!occupant) {
        spdlog::warn("Failed to get occupant interface from created prop");
        return false;
    }

    if (!occupant->SetPosition(&position)) {
        spdlog::warn("Failed to set prop position");
        return false;
    }

    if (!prop->SetOrientation(settings_.rotation)) {
        spdlog::warn("Failed to set prop orientation");
        return false;
    }

    if (!propManager_->AddCityProp(occupant)) {
        spdlog::warn("Failed to add prop to city - validation failed (?)");
        return false;
    }

    if (!occupant->SetHighlight(0x9, true)) {
        spdlog::warn("Failed to set prop highlight");
        return false;
    }

    occupant->AddRef();
    placedProps_.emplace_back(occupant);

    spdlog::info("Placed prop 0x{:08X} at ({:.2f}, {:.2f}, {:.2f}), rotation: {}",
                 propIDToPaint_, position.fX, position.fY, position.fZ, settings_.rotation);

    return true;
}

void PropPainterInputControl::CreatePreviewProp_() {
    if (!propManager_) {
        spdlog::warn("Cannot create preview prop: prop manager not available");
        return;
    }

    if (propIDToPaint_ == 0) {
        spdlog::warn("Cannot create preview prop: no target prop selected");
        return;
    }

    if (previewProp_) {
        spdlog::warn("Preview prop already created");
        return;
    }

    cISC4PropOccupant* prop = nullptr;
    if (!propManager_->CreateProp(propIDToPaint_, prop)) {
        spdlog::warn("Failed to create prop for preview");
        return;
    }

    cISC4Occupant* previewOccupant = prop->AsOccupant();
    if (!previewOccupant) {
        spdlog::warn("Failed to get occupant interface for preview prop");
        return;
    }

    previewProp_ = cRZAutoRefCount<cISC4PropOccupant>(prop);
    previewOccupant_ = cRZAutoRefCount<cISC4Occupant>(previewOccupant);

    cS3DVector3 initialPos(0, 1000, 0); // Way above ground
    lastPreviewPosition_ = initialPos;
    previewOccupant_->SetPosition(&initialPos);
    previewProp_->SetOrientation(settings_.rotation);
    lastPreviewRotation_ = settings_.rotation;

    if (!propManager_->AddCityProp(previewOccupant_)) {
        spdlog::warn("Failed to add prop to city - validation failed (?)");
        previewProp_.Reset();
        previewOccupant_.Reset();
        return;
    }

    previewOccupant_->SetVisibility(true, true);
    previewOccupant_->SetHighlight(0x3, true);
    previewActive_ = true;
    spdlog::info("Created preview prop in CreatePreviewProp");
}

void PropPainterInputControl::DestroyPreviewProp_() {
    if (!previewOccupant_) return;

    if (propManager_) {
        propManager_->RemovePropA(previewOccupant_);
    }

    previewProp_.Reset();
    previewActive_ = false;

    spdlog::info("Destroyed preview prop");
}

void PropPainterInputControl::UpdatePreviewPropRotation_() {
    if (!previewSettings_.showPreview || !previewActive_ || !previewOccupant_ || !view3D) {
        return;
    }

    if (settings_.rotation != lastPreviewRotation_) {
        previewProp_->SetOrientation(settings_.rotation);
        lastPreviewRotation_ = settings_.rotation;
    }
    previewOccupant_->SetHighlight(0x2, false);
    previewOccupant_->SetHighlight(0x3, true);
}

void PropPainterInputControl::UpdatePreviewProp_(int32_t screenX, int32_t screenZ) {
    if (!previewSettings_.showPreview || !previewActive_ || !previewOccupant_ || !view3D) {
        return;
    }

    float worldCoords[3] = {0.0f, 0.0f, 0.0f};
    if (!view3D->PickTerrain(screenX, screenZ, worldCoords, false)) {
        // Mouse not on valid terrain, hide!
        previewOccupant_->SetVisibility(false, true);
        return;
    }

    cS3DVector3 worldPos(worldCoords[0], worldCoords[1], worldCoords[2]);

    const auto posChanged =
        std::abs(worldPos.fX - lastPreviewPosition_.fX) > 0.05f ||
        std::abs(worldPos.fY - lastPreviewPosition_.fY) > 0.05f ||
        std::abs(worldPos.fZ - lastPreviewPosition_.fZ) > 0.05f;

    const auto rotChanged = settings_.rotation != lastPreviewRotation_;

    if (posChanged || rotChanged) {
        previewOccupant_->SetPosition(&worldPos);
        lastPreviewPosition_ = worldPos;

        if (rotChanged) {
            previewProp_->SetOrientation(settings_.rotation);
            lastPreviewRotation_ = settings_.rotation;
        }

        previewOccupant_->SetHighlight(0x2, false);
        previewOccupant_->SetHighlight(0x3, true);
    }
    previewOccupant_->SetVisibility(true, true);
}
