#pragma once

#include <array>
#include <filesystem>
#include <string>

#include "../AppState.hpp"

namespace fs = std::filesystem;

class ConfigPanel {
public:
    ConfigPanel();

    void Render(AppState& state);
    bool IsStartButtonClicked() const { return startButtonClicked_; }
    void ResetStartButton() { startButtonClicked_ = false; }

private:
    std::array<char, 512> gameRootBuf_{};
    std::array<char, 512> userPluginsBuf_{};
    std::array<char, 256> localeBuf_{};

    bool renderThumbnails_ = false;
    bool startButtonClicked_ = false;

    void OpenGameRootDialog_();
    void OpenUserPluginsDialog_();
};
