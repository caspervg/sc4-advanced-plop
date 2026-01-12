#pragma once
#include <__filesystem/filesystem_error.h>


class PluginScanner {
public:
    std::vector<std::filesystem::path> ScanDirectory(const std::filesystem::path& root);
private:
    bool IsPluginFile_(const std::filesystem::path& filePath);
};
