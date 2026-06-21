#pragma once

#include "gameplay/GameWorld.hpp"
#include "levels/LevelTypes.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace arcadeblocks::assets {

struct MenuAssetMapping {
    std::filesystem::path background;
    std::filesystem::path logo;
    std::filesystem::path music;
    std::filesystem::path welcomeSound;
};

struct LevelAssetMapping {
    int levelNumber = 1;
    std::filesystem::path levelJson;
    std::filesystem::path background;
    std::filesystem::path music;
    std::unordered_map<levels::BrickColor, std::string> brickSprites;
    std::unordered_map<gameplay::AudioEventType, std::vector<std::filesystem::path>> sfxByEvent;
    std::vector<std::filesystem::path> preloadSfx;
};

class AssetRegistry {
public:
    explicit AssetRegistry(std::filesystem::path assetsDirectory);

    [[nodiscard]] const MenuAssetMapping& menu() const noexcept;
    [[nodiscard]] LevelAssetMapping level(int levelNumber) const;
    [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& relativePath) const;
    [[nodiscard]] bool exists(const std::filesystem::path& relativePath) const;
    [[nodiscard]] std::optional<std::filesystem::path> firstMissingRequiredAsset(const LevelAssetMapping& mapping) const;
    [[nodiscard]] std::optional<std::string> firstMissingBrickSprite(
        const LevelAssetMapping& mapping,
        const std::vector<std::string>& availableSprites) const;

private:
    [[nodiscard]] std::filesystem::path classicLevelJson(int levelNumber) const;
    [[nodiscard]] std::filesystem::path levelBackground(int levelNumber) const;
    [[nodiscard]] std::filesystem::path levelMusic(int levelNumber) const;
    [[nodiscard]] std::unordered_map<levels::BrickColor, std::string> brickSprites() const;
    [[nodiscard]] std::unordered_map<gameplay::AudioEventType, std::vector<std::filesystem::path>> sfxByEvent(int levelNumber) const;
    [[nodiscard]] std::vector<std::filesystem::path> preloadSfx() const;

    std::filesystem::path assetsDirectory_;
    MenuAssetMapping menu_;
};

} // namespace arcadeblocks::assets
