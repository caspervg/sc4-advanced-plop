#pragma once

#include "../AppState.hpp"

class LogPanel {
public:
    LogPanel() = default;

    void Render(AppState& state);

private:
    bool autoScroll_ = true;
};
