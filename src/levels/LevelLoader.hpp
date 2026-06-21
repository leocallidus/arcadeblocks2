#pragma once

#include "levels/LevelTypes.hpp"

#include <filesystem>
#include <string_view>

namespace arcadeblocks::levels {

[[nodiscard]] LevelLoadResult parseLevelDefinition(
    std::string_view jsonText,
    std::filesystem::path sourcePath = {});

[[nodiscard]] LevelLoadResult loadLevelDefinition(const std::filesystem::path& path);

} // namespace arcadeblocks::levels
