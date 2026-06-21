#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace arcadeblocks::platform {

struct ResolvedPaths {
    std::filesystem::path executablePath;
    std::filesystem::path assetsDirectory;
    std::filesystem::path writableDataDirectory;
    std::filesystem::path logDirectory;
};

struct ResolvePathsResult {
    ResolvedPaths paths;
    bool ok = true;
    std::string error;

    explicit operator bool() const noexcept {
        return ok;
    }
};

ResolvePathsResult resolvePaths(
    const std::filesystem::path& executablePath,
    const std::optional<std::filesystem::path>& assetsDirectoryOverride);

} // namespace arcadeblocks::platform
