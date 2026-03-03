#pragma once
#include "PropPainterInputControl.hpp"
#include "public/ImGuiPanel.h"

class PropPaintStatusPanel final : public ImGuiPanel {
public:
    void OnRender() override;
    void OnShutdown() override { delete this; }

    void SetActiveControl(PropPainterInputControl* control);
    void SetVisible(bool visible);

private:
    PropPainterInputControl* activeControl_ = nullptr;
    bool visible_ = false;
};
