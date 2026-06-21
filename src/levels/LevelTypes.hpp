#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arcadeblocks::levels {

enum class DiagnosticSeverity {
    Warning,
    Error
};

struct LevelDiagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
};

enum class BrickColor {
    Blue,
    Cyan,
    DarkBlue,
    Explosive,
    Green,
    Indestructible,
    LightGray,
    Orange,
    Pink,
    Purple,
    Red,
    Shielded,
    Yellow
};

struct LevelMetadata {
    std::string name;
    std::string name_ru;
    std::string name_us;
    std::string description;
    std::string description_ru;
    std::filesystem::path sourcePath;
};

struct LevelLayout {
    int brickColumns = 0;
    int brickRows = 0;
    int brickWidth = 0;
    int brickHeight = 0;
    int brickSpacing = 0;
    int startY = 0;
};

struct BrickDefinition {
    int row = 0;
    int col = 0;
    BrickColor color = BrickColor::Blue;
    std::string sourceColor = "blue";
    bool usedFallbackColor = false;
    int health = 1;
    int points = 0;
};

struct LevelDefinition {
    LevelMetadata metadata;
    LevelLayout layout;
    std::vector<BrickDefinition> bricks;
};

struct LevelLoadResult {
    std::optional<LevelDefinition> level;
    std::vector<LevelDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return level.has_value();
    }
};

[[nodiscard]] std::string_view toString(BrickColor color) noexcept;
[[nodiscard]] BrickColor fallbackBrickColor() noexcept;
[[nodiscard]] std::optional<BrickColor> parseBrickColor(std::string_view color) noexcept;

} // namespace arcadeblocks::levels
