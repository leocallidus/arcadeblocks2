#include "platform/Paths.hpp"

#include <cstdlib>
#include <system_error>
#include <vector>

namespace arcadeblocks::platform {
namespace {

ResolvePathsResult fail(std::string error) {
    ResolvePathsResult result;
    result.ok = false;
    result.error = std::move(error);
    return result;
}

std::filesystem::path absoluteWeakly(const std::filesystem::path& path) {
    std::error_code error;
    auto absolute = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return absolute;
    }

    absolute = std::filesystem::absolute(path, error);
    if (!error) {
        return absolute;
    }

    return path;
}

bool looksLikeAssetsDirectory(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_directory(path, error)) {
        return false;
    }

    return std::filesystem::is_directory(path / "sprites", error)
        && std::filesystem::is_directory(path / "sounds", error)
        && std::filesystem::is_directory(path / "music", error)
        && std::filesystem::is_directory(path / "levels", error);
}

std::optional<std::filesystem::path> findAssetsDirectory(const std::filesystem::path& executablePath) {
    std::vector<std::filesystem::path> candidates;

    const auto current = absoluteWeakly(std::filesystem::current_path());
    candidates.push_back(current / "assets");
    candidates.push_back(current / "arcadeblocks2" / "assets");
    candidates.push_back(current / "share" / "arcadeblocks2" / "assets");
    candidates.push_back(current / "share" / "arcadeblocks2");

    auto cursor = absoluteWeakly(executablePath).parent_path();
    for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
        candidates.push_back(cursor / "assets");
        candidates.push_back(cursor / "arcadeblocks2" / "assets");
        candidates.push_back(cursor / "share" / "arcadeblocks2" / "assets");
        candidates.push_back(cursor / "share" / "arcadeblocks2");
        candidates.push_back(cursor / "share" / "ArcadeBlocksII" / "assets");
        candidates.push_back(cursor / "share" / "ArcadeBlocksII");
        candidates.push_back(cursor / "Resources" / "assets");
        candidates.push_back(cursor / "Resources");
        cursor = cursor.parent_path();
    }

    for (const auto& candidate : candidates) {
        const auto normalized = absoluteWeakly(candidate);
        if (looksLikeAssetsDirectory(normalized)) {
            return normalized;
        }
    }

    return std::nullopt;
}

const char* environment(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : nullptr;
}

std::filesystem::path writableDataDirectory() {
#if defined(_WIN32)
    if (const char* appData = environment("APPDATA")) {
        return std::filesystem::path{appData} / "ArcadeBlocksII";
    }
    if (const char* userProfile = environment("USERPROFILE")) {
        return std::filesystem::path{userProfile} / "AppData" / "Roaming" / "ArcadeBlocksII";
    }
    return std::filesystem::current_path() / "ArcadeBlocksIIData";
#elif defined(__APPLE__)
    if (const char* home = environment("HOME")) {
        return std::filesystem::path{home} / "Library" / "Application Support" / "ArcadeBlocksII";
    }
    return std::filesystem::current_path() / "ArcadeBlocksIIData";
#else
    if (const char* xdgDataHome = environment("XDG_DATA_HOME")) {
        return std::filesystem::path{xdgDataHome} / "arcadeblocks2";
    }
    if (const char* home = environment("HOME")) {
        return std::filesystem::path{home} / ".local" / "share" / "arcadeblocks2";
    }
    return std::filesystem::current_path() / "arcadeblocks2-data";
#endif
}

} // namespace

ResolvePathsResult resolvePaths(
    const std::filesystem::path& executablePath,
    const std::optional<std::filesystem::path>& assetsDirectoryOverride) {
    ResolvedPaths paths;
    paths.executablePath = absoluteWeakly(executablePath);

    if (assetsDirectoryOverride) {
        paths.assetsDirectory = absoluteWeakly(*assetsDirectoryOverride);
        if (!looksLikeAssetsDirectory(paths.assetsDirectory)) {
            return fail("Invalid --assets-dir '" + paths.assetsDirectory.string()
                + "': expected a directory containing sprites, sounds, music, and levels");
        }
    } else {
        auto detectedAssets = findAssetsDirectory(paths.executablePath);
        if (!detectedAssets) {
            return fail("Could not locate assets directory. Use --assets-dir <path>.");
        }
        paths.assetsDirectory = *detectedAssets;
    }

    paths.writableDataDirectory = absoluteWeakly(writableDataDirectory());
    paths.logDirectory = paths.writableDataDirectory / "logs";

    return ResolvePathsResult{std::move(paths), true, {}};
}

} // namespace arcadeblocks::platform
