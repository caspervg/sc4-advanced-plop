#pragma once

#include "../AppState.hpp"

class ProgressPanel {
public:
    ProgressPanel() = default;

    void Render(AppState& state);
    bool IsCancelRequested() const { return cancelRequested_; }
    void ResetCancelRequest() { cancelRequested_ = false; }

private:
    bool cancelRequested_ = false;
    int spinnerFrame_ = 0;

    const char* GetSpinnerChar_() const;
};
