#pragma once

#include "levels/LevelLoader.hpp"

#include <filesystem>

namespace arcadeblocks::levels {

class LevelRepository {
public:
    explicit LevelRepository(std::filesystem::path assetsDirectory);

    [[nodiscard]] std::filesystem::path classicLevelPath(int levelNumber) const;
    [[nodiscard]] LevelLoadResult loadClassicLevel(int levelNumber) const;

private:
    std::filesystem::path assetsDirectory_;
};

} // namespace arcadeblocks::levels
