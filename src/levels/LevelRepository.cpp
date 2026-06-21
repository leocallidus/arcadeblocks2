#include "levels/LevelRepository.hpp"

#include <utility>

namespace arcadeblocks::levels {

LevelRepository::LevelRepository(std::filesystem::path assetsDirectory)
    : assetsDirectory_(std::move(assetsDirectory)) {}

std::filesystem::path LevelRepository::classicLevelPath(int levelNumber) const {
    return assetsDirectory_ / "levels" / "arcadeblocks_1" / ("level" + std::to_string(levelNumber) + ".json");
}

LevelLoadResult LevelRepository::loadClassicLevel(int levelNumber) const {
    if (levelNumber <= 0) {
        LevelLoadResult result;
        result.diagnostics.push_back(LevelDiagnostic{DiagnosticSeverity::Error, "levelNumber must be > 0"});
        return result;
    }

    return loadLevelDefinition(classicLevelPath(levelNumber));
}

} // namespace arcadeblocks::levels
