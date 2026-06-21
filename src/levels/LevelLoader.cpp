#include "levels/LevelLoader.hpp"

#include "core/Log.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace arcadeblocks::levels {
namespace {

void addError(LevelLoadResult& result, std::string message) {
    result.diagnostics.push_back(LevelDiagnostic{DiagnosticSeverity::Error, std::move(message)});
}

void addWarning(LevelLoadResult& result, std::string message) {
    result.diagnostics.push_back(LevelDiagnostic{DiagnosticSeverity::Warning, std::move(message)});
    core::Log::warn(result.diagnostics.back().message);
}

bool hasErrors(const LevelLoadResult& result) {
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.severity == DiagnosticSeverity::Error) {
            return true;
        }
    }
    return false;
}

std::string sourceLabel(const std::filesystem::path& sourcePath) {
    return sourcePath.empty() ? std::string{"<memory>"} : sourcePath.string();
}

void validateLayout(const LevelLayout& layout, LevelLoadResult& result) {
    if (layout.brickColumns <= 0) {
        addError(result, "layout.brickColumns must be > 0");
    }
    if (layout.brickRows <= 0) {
        addError(result, "layout.brickRows must be > 0");
    }
    if (layout.brickWidth <= 0) {
        addError(result, "layout.brickWidth must be > 0");
    }
    if (layout.brickHeight <= 0) {
        addError(result, "layout.brickHeight must be > 0");
    }
    if (layout.brickSpacing < 0) {
        addError(result, "layout.brickSpacing must be >= 0");
    }
    if (layout.startY < 0) {
        addError(result, "layout.startY must be >= 0");
    }
}

} // namespace

LevelLoadResult parseLevelDefinition(std::string_view jsonText, std::filesystem::path sourcePath) {
    LevelLoadResult result;
    nlohmann::json document;

    try {
        document = nlohmann::json::parse(jsonText);
    } catch (const nlohmann::json::parse_error& error) {
        addError(result, "Failed to parse level JSON " + sourceLabel(sourcePath) + ": " + error.what());
        return result;
    }

    try {
        if (!document.is_object()) {
            addError(result, "Level root must be a JSON object");
            return result;
        }

        LevelDefinition level;
        level.metadata.sourcePath = std::move(sourcePath);
        if (document.contains("name_ru")) {
            level.metadata.name_ru = document.at("name_ru").get<std::string>();
        }
        if (document.contains("name_us")) {
            level.metadata.name_us = document.at("name_us").get<std::string>();
        }
        if (document.contains("name")) {
            level.metadata.name = document.at("name").get<std::string>();
        } else {
            level.metadata.name = !level.metadata.name_us.empty() ? level.metadata.name_us : level.metadata.name_ru;
        }

        if (document.contains("description_ru")) {
            level.metadata.description_ru = document.at("description_ru").get<std::string>();
        }
        if (document.contains("description")) {
            level.metadata.description = document.at("description").get<std::string>();
        } else {
            level.metadata.description = level.metadata.description_ru;
        }

        const auto& layout = document.at("layout");
        level.layout = LevelLayout{
            .brickColumns = layout.at("brickColumns").get<int>(),
            .brickRows = layout.at("brickRows").get<int>(),
            .brickWidth = layout.at("brickWidth").get<int>(),
            .brickHeight = layout.at("brickHeight").get<int>(),
            .brickSpacing = layout.at("brickSpacing").get<int>(),
            .startY = layout.at("startY").get<int>(),
        };
        validateLayout(level.layout, result);

        const auto& bricks = document.at("bricks");
        if (!bricks.is_array()) {
            addError(result, "bricks must be an array");
            return result;
        }

        level.bricks.reserve(bricks.size());
        for (std::size_t index = 0; index < bricks.size(); ++index) {
            const auto& source = bricks.at(index);
            BrickDefinition brick{
                .row = source.at("row").get<int>(),
                .col = source.at("col").get<int>(),
                .sourceColor = source.at("color").get<std::string>(),
                .health = source.at("health").get<int>(),
                .points = source.at("points").get<int>(),
            };

            const auto prefix = "bricks[" + std::to_string(index) + "]";
            if (brick.row < 0) {
                addError(result, prefix + ".row must be >= 0");
            }
            if (brick.col < 0) {
                addError(result, prefix + ".col must be >= 0");
            }
            if (brick.row >= level.layout.brickRows) {
                addError(result, prefix + ".row is outside layout.brickRows");
            }
            if (brick.col >= level.layout.brickColumns) {
                addError(result, prefix + ".col is outside layout.brickColumns");
            }
            if (brick.health <= 0) {
                addError(result, prefix + ".health must be > 0");
            }
            if (brick.points < 0) {
                addError(result, prefix + ".points must be >= 0");
            }

            if (const auto color = parseBrickColor(brick.sourceColor)) {
                brick.color = *color;
            } else {
                brick.color = fallbackBrickColor();
                brick.usedFallbackColor = true;
                addWarning(
                    result,
                    prefix + ".color '" + brick.sourceColor + "' is unknown; using fallback '"
                        + std::string{toString(fallbackBrickColor())} + "'");
            }

            level.bricks.push_back(std::move(brick));
        }

        if (!hasErrors(result)) {
            result.level = std::move(level);
        }
    } catch (const nlohmann::json::exception& error) {
        addError(result, "Invalid level format " + sourceLabel(sourcePath) + ": " + error.what());
    }

    return result;
}

LevelLoadResult loadLevelDefinition(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        LevelLoadResult result;
        addError(result, "Failed to open level JSON: " + path.string());
        return result;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parseLevelDefinition(buffer.str(), path);
}

} // namespace arcadeblocks::levels
