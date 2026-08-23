#pragma once
#include <system_error>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include "index.hpp"

constexpr auto kDirectoryOptions = std::filesystem::directory_options::skip_permission_denied;
const auto kDbpfFileExtensions = std::unordered_set<std::string>{
    ".dat", ".sc4lot", ".sc4model", ".sc4desc"
};

template<typename Iter>
auto FindPlugins(Iter begin, Iter end, std::vector<std::filesystem::path>& out) -> void {
    for (auto it = begin; it != end;) {
        try {
            if (it->is_regular_file()) {
                auto ext = it->path().extension().string();
                std::ranges::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (kDbpfFileExtensions.contains(ext)) {
                    out.push_back(it->path());
                }
            }
            ++it;
        } catch (const std::system_error& error) {
            // Unreadable/special-char directory: skip its subtree, keep scanning siblings.
            spdlog::warn("Plugin scan skipping '{}': {}",
                         it->path().string(), error.code().message());
            try {
                if constexpr (requires { it.disable_recursion_pending(); }) {
                    it.disable_recursion_pending();
                }
                ++it;
            } catch (const std::system_error&) {
                // ponytail: two consecutive failures means we can't make progress —
                // abandon the rest of this tree rather than spin forever
                spdlog::warn("Plugin scan aborted, files below '{}' are skipped",
                             it->path().string());
                break;
            }
        }
    }
}

class PluginLocator {
public:
    explicit PluginLocator(PluginConfiguration config);

    [[nodiscard]] auto ListDbpfFiles() const -> std::vector<std::filesystem::path>;

private:
    static auto CollectTreeSorted_(const std::filesystem::path& root, bool recursive, std::vector<std::filesystem::path>& out) -> void;
    static auto CollectFiles_(const std::filesystem::path& root, bool recursive, std::vector<std::filesystem::path>& out) -> void;

private:
    PluginConfiguration config_;
};
