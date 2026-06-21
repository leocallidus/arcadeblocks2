#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "assets/AssetRegistry.hpp"
#include "core/CommandLine.hpp"
#include "core/Version.hpp"
#include "gameplay/GameWorld.hpp"
#include "levels/LevelLoader.hpp"
#include "levels/LevelRepository.hpp"
#include "platform/Paths.hpp"
#include "render/SpriteAtlas.hpp"
#include "settings/KeyBinding.hpp"
#include "settings/Settings.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

bool hasSeverity(
    const arcadeblocks::levels::LevelLoadResult& result,
    arcadeblocks::levels::DiagnosticSeverity severity) {
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.severity == severity) {
            return true;
        }
    }
    return false;
}

void require(bool condition, std::string_view message) {
    INFO(message);
    REQUIRE(condition);
}

float vectorLength(arcadeblocks::gameplay::Vec2 vector) {
    return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

bool hasAudioEvent(
    const std::vector<arcadeblocks::gameplay::AudioEvent>& events,
    arcadeblocks::gameplay::AudioEventType type) {
    for (const auto& event : events) {
        if (event.type == type) {
            return true;
        }
    }
    return false;
}

arcadeblocks::levels::LevelDefinition makeGameplayFixture(
    int brickHealth = 1,
    int brickPoints = 25,
    int columns = 1,
    int startY = 700,
    int brickCol = 0) {
    arcadeblocks::levels::LevelDefinition level;
    level.metadata.name = "Gameplay Fixture";
    level.metadata.description = "Minimal gameplay test level";
    level.layout = arcadeblocks::levels::LevelLayout{
        .brickColumns = columns,
        .brickRows = 1,
        .brickWidth = 60,
        .brickHeight = 80,
        .brickSpacing = 4,
        .startY = startY,
    };
    level.bricks.push_back(arcadeblocks::levels::BrickDefinition{
        .row = 0,
        .col = brickCol,
        .color = arcadeblocks::levels::BrickColor::Purple,
        .sourceColor = "purple",
        .health = brickHealth,
        .points = brickPoints,
    });
    return level;
}

arcadeblocks::levels::LevelDefinition makeExplosiveFixture() {
    arcadeblocks::levels::LevelDefinition level;
    level.metadata.name = "Explosive Fixture";
    level.metadata.description = "Explosive brick test level";
    level.layout = arcadeblocks::levels::LevelLayout{
        .brickColumns = 15,
        .brickRows = 1,
        .brickWidth = 80,
        .brickHeight = 40,
        .brickSpacing = 0,
        .startY = 700,
    };
    for (int col = 4; col <= 8; ++col) {
        level.bricks.push_back(arcadeblocks::levels::BrickDefinition{
            .row = 0,
            .col = col,
            .color = col == 6 ? arcadeblocks::levels::BrickColor::Explosive : arcadeblocks::levels::BrickColor::Blue,
            .sourceColor = col == 6 ? "explosive" : "blue",
            .health = col == 6 ? 1 : 2,
            .points = 25,
        });
    }
    return level;
}

} // namespace

TEST_CASE("foundation systems satisfy Phase 1 invariants", "[foundation]") {
    require(arcadeblocks::core::productName() == std::string_view{"Arcade Blocks II"}, "product name");
    require(!arcadeblocks::core::version().empty(), "version is not empty");
    require(arcadeblocks::core::sdlTargetVersion() == std::string_view{"3.4.10"}, "SDL target version");

    {
        const char* argv[] = {
            "ArcadeBlocksII",
            "--level",
            "7",
            "--fullscreen",
            "--no-audio",
            "--debug",
            "--perf-summary",
            "--ui-scale=1.25",
            "--smoke-frames=5"};
        auto result = arcadeblocks::core::parseCommandLine(9, const_cast<char**>(argv));
        require(result.ok, "CLI accepts valid options");
        require(result.options.level == 7, "CLI level");
        require(result.options.levelSpecified, "CLI level specified");
        require(result.options.windowMode == arcadeblocks::core::WindowMode::Fullscreen, "CLI fullscreen");
        require(result.options.noAudio, "CLI no-audio");
        require(result.options.debug, "CLI debug");
        require(result.options.perfSummary, "CLI perf summary");
        require(std::abs(result.options.uiScale - 1.25f) < 0.001f, "CLI ui scale");
        require(result.options.smokeFrames == 5, "CLI smoke frames");
    }

    {
        const char* argv[] = {"ArcadeBlocksII", "--reset-settings", "--fullscreen"};
        auto result = arcadeblocks::core::parseCommandLine(3, const_cast<char**>(argv));
        require(result.ok, "CLI accepts reset-settings");
        require(result.options.resetSettings, "CLI reset-settings flag");
        require(result.options.windowModeSpecified, "CLI window mode specified");
    }

    {
        const char* argv[] = {
            "ArcadeBlocksII",
            "--settings-file",
            "/tmp/arcadeblocks2-test-settings.json",
            "--smoke-scenario=pause-help"};
        auto result = arcadeblocks::core::parseCommandLine(4, const_cast<char**>(argv));
        require(result.ok, "CLI accepts isolated settings path and smoke scenario");
        require(
            result.options.settingsFileOverride == std::filesystem::path{"/tmp/arcadeblocks2-test-settings.json"},
            "CLI settings path");
        require(
            result.options.smokeScenario == arcadeblocks::core::SmokeScenario::PauseHelp,
            "CLI pause-help scenario");
        require(result.options.smokeFrames == 3, "CLI smoke scenario gets bounded default frame count");
    }

    {
        const char* argv[] = {"ArcadeBlocksII", "--open-settings-smoke"};
        auto result = arcadeblocks::core::parseCommandLine(2, const_cast<char**>(argv));
        require(result.ok, "CLI accepts settings smoke alias");
        require(
            result.options.smokeScenario == arcadeblocks::core::SmokeScenario::Settings,
            "CLI settings smoke alias");
    }

    {
        const char* argv[] = {"ArcadeBlocksII", "--smoke-scenario", "help-cycle", "--smoke-frames", "60"};
        auto result = arcadeblocks::core::parseCommandLine(5, const_cast<char**>(argv));
        require(result.ok, "CLI accepts help cycle scenario");
        require(
            result.options.smokeScenario == arcadeblocks::core::SmokeScenario::HelpCycle,
            "CLI help cycle scenario");
        require(result.options.smokeFrames == 60, "CLI explicit cycle frame count");
    }

    {
        const char* argv[] = {"ArcadeBlocksII", "--smoke-scenario", "unknown"};
        auto result = arcadeblocks::core::parseCommandLine(3, const_cast<char**>(argv));
        require(!result.ok, "CLI rejects unknown smoke scenario");
    }

    {
        const char* argv[] = {"ArcadeBlocksII", "--level", "0"};
        auto result = arcadeblocks::core::parseCommandLine(3, const_cast<char**>(argv));
        require(!result.ok, "CLI rejects invalid level");
    }

    {
        const char* argv[] = {"ArcadeBlocksII", "--ui-scale", "3.0"};
        auto result = arcadeblocks::core::parseCommandLine(3, const_cast<char**>(argv));
        require(!result.ok, "CLI rejects invalid ui scale");
    }

    {
        const char* argv[] = {"ArcadeBlocksII"};
        auto result = arcadeblocks::core::parseCommandLine(1, const_cast<char**>(argv));
        require(result.ok, "CLI accepts default launch");
        require(!result.options.levelSpecified, "CLI default launch starts at menu");
    }

    {
        auto result = arcadeblocks::platform::resolvePaths(
            std::filesystem::current_path() / "build" / "linux-debug" / "ArcadeBlocksII",
            std::filesystem::path{ARCADEBLOCKS_SOURCE_DIR} / "assets");
        require(static_cast<bool>(result), "paths resolve with explicit assets directory");
        require(result.paths.assetsDirectory.filename() == "assets", "assets directory filename");
    }

    {
        const auto parsed = arcadeblocks::settings::parseResolutionString("1600x900");
        require(parsed.has_value(), "settings resolution parses");
        require(parsed->first == 1600, "settings resolution width");
        require(parsed->second == 900, "settings resolution height");
        require(!arcadeblocks::settings::parseResolutionString("bad").has_value(), "settings rejects invalid resolution");
    }

    {
        const auto defaults = arcadeblocks::settings::defaultSettings();
        require(defaults.version == 1, "settings default schema version");
        require(defaults.language == arcadeblocks::settings::Language::Russian, "settings default language");
        require(defaults.audio.masterVolume == Catch::Approx(0.85), "settings default master volume");
        require(defaults.controls.pause.keyName == "Escape", "settings default pause binding");
        require(!arcadeblocks::settings::findDuplicateBinding(defaults.controls), "settings defaults use unique keys");
    }

    {
        const auto atlas = arcadeblocks::render::SpriteAtlas::loadFromFile(
            std::filesystem::path{ARCADEBLOCKS_SOURCE_DIR} / "assets" / "sprites" / "sprite_atlas.json");
        require(atlas.frameCount() > 0, "sprite atlas has frames");
        require(atlas.contains("paddle.png"), "sprite atlas contains paddle");
        require(atlas.contains("ball.png"), "sprite atlas contains ball");
        require(atlas.find("missing_stage4_probe.png") == std::nullopt, "sprite atlas missing probe");
    }

    {
        const arcadeblocks::assets::AssetRegistry registry{
            std::filesystem::path{ARCADEBLOCKS_SOURCE_DIR} / "assets"};
        const auto& menu = registry.menu();
        const std::unordered_map<std::string, std::string> menuPairs{
            {"music/menu/main_menu.ogg", "sprites/main_menu/background.png"},
            {"music/menu/main_menu2.ogg", "sprites/main_menu/background2.png"},
            {"music/menu/main_menu3.ogg", "sprites/main_menu/background3.png"},
            {"music/menu/main_menu_4.ogg", "sprites/main_menu/background4.png"},
        };
        const auto selectedMenuPair = menuPairs.find(menu.music.generic_string());
        require(selectedMenuPair != menuPairs.end(), "registry menu music is known");
        require(menu.background.generic_string() == selectedMenuPair->second, "registry menu background matches music");
        require(registry.exists(menu.background), "registry menu background exists");
        require(registry.exists(menu.music), "registry menu music exists");

        const auto mapping = registry.level(1);
        require(mapping.levelJson.generic_string() == "levels/arcadeblocks_1/level1.json", "registry level1 json");
        require(mapping.background.generic_string() == "sprites/level_backgrounds/level1.jpg", "registry level1 compact jpg");
        require(mapping.background.generic_string() != "sprites/level_backgrounds/level1_png.jpg", "registry keeps level1_png explicit-only");
        require(mapping.music.generic_string() == "music/level/level1.ogg", "registry level1 music");
        require(registry.exists(mapping.levelJson), "registry level json exists");
        require(registry.exists(mapping.background), "registry background exists");
        require(registry.exists(mapping.music), "registry music exists");
        require(!registry.firstMissingRequiredAsset(mapping), "registry level1 MVP assets exist");

        const auto atlas = arcadeblocks::render::SpriteAtlas::loadFromFile(
            std::filesystem::path{ARCADEBLOCKS_SOURCE_DIR} / "assets" / "sprites" / "sprite_atlas.json");
        for (const auto& [_, sprite] : mapping.brickSprites) {
            require(atlas.contains(sprite), "registry brick sprite exists in atlas");
        }
        require(mapping.sfxByEvent.contains(arcadeblocks::gameplay::AudioEventType::BrickBreak), "registry brick break sfx");
        require(mapping.sfxByEvent.contains(arcadeblocks::gameplay::AudioEventType::PaddleHit), "registry paddle hit sfx");
        require(mapping.preloadSfx.size() >= 3, "registry preloads basic sfx");
    }

    {
        const arcadeblocks::levels::LevelRepository repository{
            std::filesystem::path{ARCADEBLOCKS_SOURCE_DIR} / "assets"};
        auto result = repository.loadClassicLevel(1);
        require(result.ok(), "level1 loads");
        require(!hasSeverity(result, arcadeblocks::levels::DiagnosticSeverity::Error), "level1 has no errors");
        require(result.level->metadata.name == "Neon Awakening" || result.level->metadata.name == "Неоновое Пробуждение", "level1 name");
        require(result.level->layout.brickColumns == 16, "level1 columns");
        require(result.level->layout.brickRows == 8, "level1 rows");
        require(result.level->layout.brickWidth == 60, "level1 brick width");
        require(result.level->layout.brickHeight == 30, "level1 brick height");
        require(result.level->layout.brickSpacing == 4, "level1 brick spacing");
        require(result.level->layout.startY == 90, "level1 startY");
        require(result.level->bricks.size() == 60, "level1 brick count");
        require(result.level->bricks.front().row == 0, "level1 first brick row");
        require(result.level->bricks.front().col == 4, "level1 first brick col");
        require(result.level->bricks.front().color == arcadeblocks::levels::BrickColor::Blue, "level1 first brick color");
        require(result.level->bricks.front().health == 1, "level1 first brick health");
        require(result.level->bricks.front().points == 124, "level1 first brick points");
        require(
            arcadeblocks::levels::parseBrickColor("*") == arcadeblocks::levels::BrickColor::Explosive,
            "legacy explosive marker parses");
    }

    {
        auto result = arcadeblocks::levels::parseLevelDefinition("{ broken json");
        require(!result.ok(), "broken JSON fails");
        require(hasSeverity(result, arcadeblocks::levels::DiagnosticSeverity::Error), "broken JSON has error");
    }

    {
        constexpr std::string_view json = R"json(
            {
              "name": "Unknown Color",
              "description": "Validation fixture",
              "layout": {
                "brickColumns": 2,
                "brickRows": 1,
                "brickWidth": 60,
                "brickHeight": 30,
                "brickSpacing": 4,
                "startY": 90
              },
              "bricks": [
                {"row": 0, "col": 0, "color": "mystery", "health": 1, "points": 10}
              ]
            }
        )json";
        auto result = arcadeblocks::levels::parseLevelDefinition(json);
        require(result.ok(), "unknown color level still loads");
        require(hasSeverity(result, arcadeblocks::levels::DiagnosticSeverity::Warning), "unknown color has warning");
        require(result.level->bricks.front().usedFallbackColor, "unknown color uses fallback flag");
        require(result.level->bricks.front().color == arcadeblocks::levels::fallbackBrickColor(), "unknown color fallback");
    }

    {
        constexpr std::string_view json = R"json(
            {
              "name": "Invalid Brick",
              "description": "Validation fixture",
              "layout": {
                "brickColumns": 2,
                "brickRows": 1,
                "brickWidth": 60,
                "brickHeight": 30,
                "brickSpacing": 4,
                "startY": 90
              },
              "bricks": [
                {"row": -1, "col": 3, "color": "blue", "health": 0, "points": -1}
              ]
            }
        )json";
        auto result = arcadeblocks::levels::parseLevelDefinition(json);
        require(!result.ok(), "invalid brick fails");
        require(hasSeverity(result, arcadeblocks::levels::DiagnosticSeverity::Error), "invalid brick has error");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture());
        require(world.entities().size() == 5, "gameplay spawns bounds wall paddle ball brick");
        require(world.activeBrickCount() == 1, "gameplay active brick count");
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Ready, "gameplay starts ready");
        require(world.ball().state == arcadeblocks::gameplay::BallState::AttachedToPaddle, "ball starts attached");

        const auto initialPaddleX = world.paddle().position.x;
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.moveLeft = true});
        require(world.paddle().position.x < initialPaddleX, "paddle moves left");
        require(world.ball().position.x < initialPaddleX + world.paddle().size.w * 0.5f, "attached ball follows paddle");

        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Playing, "space launches game");
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "ball launched");
        require(vectorLength(world.ball().velocity) >= 560.0f, "physics clamps minimum ball speed");
        require(vectorLength(world.ball().velocity) <= 780.0f, "physics clamps maximum ball speed");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture());
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        world.update(
            1.0 / 60.0,
            arcadeblocks::gameplay::GameInput{
                .mousePaddleActive = true,
                .mousePaddleCenterX = 1350.0f,
                .launchPressed = true,
            });
        const float expectedX = world.paddle().position.x + world.paddle().size.w * 0.5f;
        require(std::abs(world.ball().position.x - expectedX) < 0.001f, "ball launches from current paddle x after abrupt move");
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "abrupt move launch succeeds");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(100, 25, 16, 100, 15));
        world.setTurboBallSpeed(1180.0f);
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.turboBallActive = true});
        require(vectorLength(world.ball().velocity) > 1000.0f, "turbo ball accelerates while active");
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(vectorLength(world.ball().velocity) < 800.0f, "turbo ball restores normal speed when released");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(1, 75));
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        for (int i = 0; i < 80 && world.activeBrickCount() > 0; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(world.activeBrickCount() == 0, "brick is destroyed after damage");
        require(world.score() == 75, "score uses brick JSON points");
        require(world.phase() == arcadeblocks::gameplay::GamePhase::LevelComplete, "level completes when bricks are gone");
        const auto events = world.consumeAudioEvents();
        require(
            hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::BrickBreak),
            "brick break audio event emitted");
        require(
            hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::LevelComplete),
            "level complete audio event emitted");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(2, 75));
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        for (int i = 0; i < 80 && world.bricks().front().health == 2; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(world.bricks().front().maxHealth == 2, "brick keeps original max health for damage visuals");
        require(world.bricks().front().health == 1, "brick takes one damage on first contact");
        require(world.activeBrickCount() == 1, "brick cooldown prevents instant double damage");
        require(world.score() == 0, "score waits for brick destruction");
        const auto events = world.consumeAudioEvents();
        require(
            hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::BrickHit),
            "brick hit audio event emitted");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeExplosiveFixture());
        const auto explosiveEntity = world.bricks().at(2).entity;
        world.damageBrickForTesting(explosiveEntity, 1);

        require(!world.bricks().at(2).alive, "explosive brick is destroyed");
        int damagedNeighbors = 0;
        for (std::size_t i = 0; i < world.bricks().size(); ++i) {
            if (i != 2 && world.bricks().at(i).health == 1) {
                ++damagedNeighbors;
            }
        }
        require(damagedNeighbors == 3, "explosion damages three nearest neighboring bricks");
        const auto events = world.consumeAudioEvents();
        require(hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::Explosion), "explosion audio event emitted");
        require(!hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::BrickHit), "explosion splash damage is silent");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(1, 25, 16, 100, 15));
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        for (int i = 0; i < 260 && world.lives() == 3; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.moveLeft = true});
        }
        require(world.lives() == 2, "life is lost when ball falls");
        require(world.phase() == arcadeblocks::gameplay::GamePhase::LifeLost, "life lost phase");
        require(world.ball().state == arcadeblocks::gameplay::BallState::AttachedToPaddle, "ball reattaches after life lost");
        require(world.respawnLaunchDelayRemaining() > 0.0, "respawn launch delay starts after life lost");
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.ball().state == arcadeblocks::gameplay::BallState::AttachedToPaddle, "respawn delay blocks instant relaunch");
        for (int i = 0; i < 70 && world.respawnLaunchDelayRemaining() > 0.0; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "ball launches after respawn delay");
        const auto events = world.consumeAudioEvents();
        require(
            hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::LifeLost),
            "life lost audio event emitted");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture());
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.pausePressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Paused, "escape pauses ready state");
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.pausePressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Ready, "escape resumes previous state");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture());
        require(!world.physicsDebugDrawEnabled(), "physics debug starts disabled");
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.toggleDebugDrawPressed = true});
        require(world.physicsDebugDrawEnabled(), "physics debug toggles on");
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.toggleDebugDrawPressed = true});
        require(!world.physicsDebugDrawEnabled(), "physics debug toggles off");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture());
        world.spawnBonus("BONUS_SCORE", arcadeblocks::gameplay::Vec2{760.0f, 800.0f});
        require(world.fallingBonuses().size() == 1, "bonus spawned");
        require(world.fallingBonuses().front().type == "BONUS_SCORE", "bonus type score");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().front().position.y == 800.0f - 15.0f + 2.5f, "bonus moves down");

        world.spawnBonus("BONUS_SCORE_200", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 2, "second bonus spawned");

        const int initialScore = world.score();
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().size() == 1, "collided bonus is collected and erased");
        require(world.score() == initialScore + 1200, "bonus score added correctly");
        
        const auto events = world.consumeAudioEvents();
        require(hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::BonusPickup), "bonus pickup sfx queued");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture());
        world.spawnBonus("BONUS_SCORE_10000", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "bonus spawned");
        require(world.fallingBonuses().front().type == "BONUS_SCORE_10000", "bonus type score 10k");

        const int initialScore = world.score();
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().size() == 0, "collided bonus is collected");
        require(world.score() == initialScore + 11000, "bonus score added correctly");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture());
        world.spawnBonus("EXTRA_LIFE", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "extra life bonus spawned");

        const int initialLives = world.lives();
        const int initialScore = world.score();
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        require(world.fallingBonuses().empty(), "extra life collected");
        require(world.lives() == initialLives + 1, "extra life added");
        require(world.score() == initialScore + 1000, "positive bonus score awarded");

        const auto events = world.consumeAudioEvents();
        require(hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::BonusPickup), "pickup sfx queued");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(9999, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        world.spawnBonus("BONUS_BALL", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "bonus ball spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "primary ball launched");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "bonus ball collected");
        require(world.extraBalls().size() == 1, "extra ball spawned");
        const auto extraBallEntity = world.extraBalls().front().entity;

        world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{760.0f, 1200.0f});

        const int initialLives = world.lives();
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        require(world.lives() == initialLives, "lives not lost because extra ball was active");
        require(world.extraBalls().empty(), "extra ball promoted to primary");
        require(world.ball().entity == extraBallEntity, "extra ball entity promoted to primary ball");
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "promoted ball is launched");
    }

    {
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99));
        world.update(1.1, arcadeblocks::gameplay::GameInput{});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        static_cast<void>(world.consumeAudioEvents());

        world.spawnBonus("CALL_BALL", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "CALL_BALL collected");
        require(world.activeBonusTimers().size() == 1, "CALL_BALL timer starts");
        require(world.activeBonusTimers().front().type == "CALL_BALL", "CALL_BALL timer type");
        require(world.activeBonusTimers().front().remainingSeconds > 49.9, "CALL_BALL timer starts near 50 seconds");

        for (int i = 0; i < 600; ++i) {
            world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{300.0f, 300.0f});
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        const double beforeRefreshRemaining = world.activeBonusTimers().front().remainingSeconds;
        const double beforeRefreshDuration = world.activeBonusTimers().front().durationSeconds;
        world.spawnBonus("CALL_BALL", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.activeBonusTimers().size() == 1, "second CALL_BALL extends existing timer");
        require(world.activeBonusTimers().front().remainingSeconds > beforeRefreshRemaining + 49.0, "second CALL_BALL adds duration");
        require(world.activeBonusTimers().front().durationSeconds > beforeRefreshDuration + 49.0, "second CALL_BALL extends total duration");

        world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{300.0f, 300.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.callBallPressed = true});
        require(world.ball().velocity.x > 0.0f, "CALL_BALL redirects ball toward paddle x");
        require(world.ball().velocity.y > 0.0f, "CALL_BALL redirects ball toward paddle y");
        const auto events = world.consumeAudioEvents();
        require(hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::CallBallPaddle), "CALL_BALL effect audio event queued");

        for (int i = 0; i < 7000 && !world.activeBonusTimers().empty()
             && world.activeBonusTimers().front().remainingSeconds > 1.0 / 60.0; ++i) {
            world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{300.0f, 300.0f});
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{300.0f, 300.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(!world.activeBonusTimers().empty(), "expired CALL_BALL timer remains for fade-out");
        require(world.activeBonusTimers().front().remainingSeconds <= 1.0 / 60.0, "CALL_BALL timer reaches zero");
        require(world.activeBonusTimers().front().fadeOutRemainingSeconds > 0.0, "CALL_BALL timer fade-out starts");
    }

    {
        // Test SLOW_BALLS and FAST_BALLS speed multiplier logic
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99));
        world.update(1.1, arcadeblocks::gameplay::GameInput{});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});

        // Spawn and collect SLOW_BALLS
        world.spawnBonus("SLOW_BALLS", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("SLOW_BALLS"), "SLOW_BALLS active on pickup");

        // Spawn and collect FAST_BALLS
        world.spawnBonus("FAST_BALLS", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("FAST_BALLS"), "FAST_BALLS active on pickup");

        // Let's verify that timers are correctly updated and have expected values
        bool slowFound = false;
        bool fastFound = false;
        for (const auto& timer : world.activeBonusTimers()) {
            if (timer.type == "SLOW_BALLS") {
                slowFound = true;
                require(timer.remainingSeconds > 19.9, "SLOW_BALLS timer near 20 seconds");
            } else if (timer.type == "FAST_BALLS") {
                fastFound = true;
                require(timer.remainingSeconds > 14.9, "FAST_BALLS timer near 15 seconds");
            }
        }
        require(slowFound && fastFound, "both SLOW_BALLS and FAST_BALLS timers exist");
    }

    {
        // Test falling bonuses fade out on player death
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(9999, 25, 16, 100, 15));
        world.update(1.1, arcadeblocks::gameplay::GameInput{});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Playing, "phase is Playing");

        // Spawn a falling bonus
        world.spawnBonus("BONUS_SCORE", arcadeblocks::gameplay::Vec2{760.0f, 500.0f});
        require(world.fallingBonuses().size() == 1, "falling bonus spawned");
        require(world.fallingBonuses().front().fadeOutRemainingSeconds == 0.0, "no fade out initially");

        // Trigger life lost (player death) by dropping the ball
        world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{760.0f, 1200.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        require(world.phase() == arcadeblocks::gameplay::GamePhase::LifeLost, "life lost phase");
        require(world.fallingBonuses().size() == 1, "falling bonus still exists but is now fading out");
        require(world.fallingBonuses().front().fadeOutRemainingSeconds > 0.0, "falling bonus fade out has started");

        // Update world multiple times to let it fade out completely
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }

        require(world.fallingBonuses().empty(), "falling bonus is removed after completing fade out");
    }

    {
        // Test SCORE_RAIN bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(2, 50, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});

        // Spawn and collect SCORE_RAIN
        world.spawnBonus("SCORE_RAIN", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        const int initialScore = world.score();
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        require(world.isBonusActive("SCORE_RAIN"), "SCORE_RAIN active on pickup");
        require(world.score() == initialScore + 1000, "awarded 1000 points for positive capsule pickup");

        // Destroy a brick
        const auto targetBrickEntity = world.bricks().front().entity;
        const int scoreBeforeDestroy = world.score();
        world.damageBrickForTesting(targetBrickEntity, 2);

        // Brick break score: 50 base points + 1000 score rain bonus points = 1050 points
        require(world.score() == scoreBeforeDestroy + 1050, "awarded base points + 1000 SCORE_RAIN bonus points");
    }

    {
        // Test WEAK_BALLS bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(1, 25, 16, 100, 15));
        world.update(1.1, arcadeblocks::gameplay::GameInput{});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});

        // Spawn and collect WEAK_BALLS
        world.spawnBonus("WEAK_BALLS", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        const int initialScore = world.score();
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        require(world.isBonusActive("WEAK_BALLS"), "WEAK_BALLS active on pickup");
        require(world.score() == initialScore + 1000, "awarded 1000 points for capsule pickup");

        // The ball radius should start to shrink smoothly (initially 12.0)
        require(world.ball().radius < 12.0f, "ball radius starts to shrink");

        // Let's step enough times for it to shrink completely to 6.0
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(std::abs(world.ball().radius - 6.0f) < 0.001f, "ball radius shrinks to 6.0");

        // Direct hit on a brick should do 0 damage
        const auto targetBrickEntity = world.bricks().front().entity;
        const int initialHealth = world.bricks().front().health;
        world.applyBrickHit(targetBrickEntity);
        require(world.bricks().front().health == initialHealth, "brick health remains unchanged due to 0 damage");

        // Wait out the WEAK_BALLS bonus duration (15 seconds, i.e. 900 frames at 60fps)
        for (int i = 0; i < 900; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("WEAK_BALLS"), "WEAK_BALLS duration expired");

        // Radius should grow back to normal (12.0) smoothly
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(std::abs(world.ball().radius - 12.0f) < 0.001f, "ball radius grows back to 12.0");

        // Direct hit should now do damage
        world.applyBrickHit(targetBrickEntity);
        require(world.bricks().front().health == initialHealth - 1, "brick health decremented after bonus expired");
    }

    {
        // Test LEVEL_PASS bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Playing, "phase is Playing");

        world.spawnBonus("LEVEL_PASS", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "LEVEL_PASS spawned");
        require(world.fallingBonuses().front().type == "LEVEL_PASS", "bonus type is LEVEL_PASS");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "LEVEL_PASS collected");
        require(world.activeBrickCount() == 0, "all bricks destroyed upon collecting LEVEL_PASS");
        require(world.phase() == arcadeblocks::gameplay::GamePhase::LevelComplete, "LEVEL_PASS completes level");

        const auto events = world.consumeAudioEvents();
        bool hasSilentBreak = false;
        for (const auto& event : events) {
            if (event.type == arcadeblocks::gameplay::AudioEventType::BrickBreak && event.detail.rfind("silent", 0) == 0) {
                hasSilentBreak = true;
                break;
            }
        }
        require(hasSilentBreak, "silent break audio event was queued");
    }

    {
        // Test ENERGY_BALLS bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Playing, "phase is Playing");

        world.spawnBonus("ENERGY_BALLS", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "ENERGY_BALLS spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "ENERGY_BALLS collected");
        require(world.isBonusActive("ENERGY_BALLS"), "ENERGY_BALLS active after pickup");

        // Verify it expires
        world.update(5.1, arcadeblocks::gameplay::GameInput{});
        require(!world.isBonusActive("ENERGY_BALLS"), "ENERGY_BALLS expires after 5 seconds");
    }

    {
        // Test EXPLOSION_BALLS bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeExplosiveFixture());
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Playing, "phase is Playing");

        world.spawnBonus("EXPLOSION_BALLS", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "EXPLOSION_BALLS spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "EXPLOSION_BALLS collected");
        require(world.isBonusActive("EXPLOSION_BALLS"), "EXPLOSION_BALLS active after pickup");

        // The hit on a regular brick should now trigger an explosion
        const auto regularBrickEntity = world.bricks().at(0).entity;
        world.applyBrickHit(regularBrickEntity);

        // Under EXPLOSION_BALLS, hitting a brick triggers explodeNearbyBricks.
        // Let's verify that an explosion audio event is emitted.
        const auto events = world.consumeAudioEvents();
        require(hasAudioEvent(events, arcadeblocks::gameplay::AudioEventType::Explosion), "explosion audio event emitted due to EXPLOSION_BALLS");

        // Verify it expires after 5 seconds
        world.update(5.1, arcadeblocks::gameplay::GameInput{});
        require(!world.isBonusActive("EXPLOSION_BALLS"), "EXPLOSION_BALLS expires after 5 seconds");
    }

    {
        // Test INCREASE_PADDLE bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Playing, "phase is Playing");
        
        // Initial paddle size
        require(std::abs(world.paddle().size.w - 240.0f) < 0.001f, "initial width is 240");

        // Spawn and collect first INCREASE_PADDLE
        world.spawnBonus("INCREASE_PADDLE", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "INCREASE_PADDLE spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "INCREASE_PADDLE collected");
        require(world.isBonusActive("INCREASE_PADDLE"), "INCREASE_PADDLE active after pickup");
        
        // Verify stack count is 1
        require(world.activeBonusTimers().front().stacks == 1, "stack count is 1");
        
        // Step one frame to apply the target width to updatePaddle
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        // Paddle width should start to grow smoothly (transition speed 240.0f/s)
        require(world.paddle().size.w > 240.0f, "paddle width starts to grow");

        // Step enough times for it to expand completely to 360.0f (240 * 1.5)
        // Growth is 120px. At 240px/s, it should take 0.5s (30 frames at 60fps)
        // Since we already did 1 step, do 39 more steps.
        for (int i = 0; i < 39; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(std::abs(world.paddle().size.w - 360.0f) < 0.001f, "paddle width grows to 360.0 (1.5x)");

        // Spawn and collect second INCREASE_PADDLE
        world.spawnBonus("INCREASE_PADDLE", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.activeBonusTimers().front().stacks == 2, "stack count is 2");

        // Step one frame to apply the second stack's target width to updatePaddle
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        // Now target is 240 * 1.6 = 384.0f. It should start growing again.
        // Since we already did 1 step, do 29 more steps.
        for (int i = 0; i < 29; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(std::abs(world.paddle().size.w - 384.0f) < 0.001f, "paddle width grows to 384.0 (1.6x)");

        // Wait out the INCREASE_PADDLE bonus duration (30 seconds, i.e. 1800 frames at 60fps)
        for (int i = 0; i < 1850; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("INCREASE_PADDLE"), "INCREASE_PADDLE expired");

        // Paddle width should shrink back to 240.0f smoothly
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(std::abs(world.paddle().size.w - 240.0f) < 0.001f, "paddle width returns to 240.0");
    }

    {
        // Test DECREASE_PADDLE bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Playing, "phase is Playing");
        
        // Initial paddle size
        require(std::abs(world.paddle().size.w - 240.0f) < 0.001f, "initial width is 240");

        // Spawn and collect DECREASE_PADDLE
        world.spawnBonus("DECREASE_PADDLE", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "DECREASE_PADDLE spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "DECREASE_PADDLE collected");
        require(world.isBonusActive("DECREASE_PADDLE"), "DECREASE_PADDLE active after pickup");

        // Step one frame to allow the target width to apply in updatePaddle
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        // Paddle width should start to shrink smoothly towards 240.0f * 0.6 = 144.0f
        require(world.paddle().size.w < 240.0f, "paddle width starts to shrink");

        // Shrink distance is 96px. At 240px/s, it takes 0.4s (24 frames at 60fps)
        // Since we did 1 frame step, step 29 more frames to be fully done.
        for (int i = 0; i < 29; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(std::abs(world.paddle().size.w - 144.0f) < 0.001f, "paddle width shrinks to 144.0 (0.6x)");

        // Wait out the DECREASE_PADDLE bonus duration (20 seconds, i.e., 1200 frames at 60fps)
        for (int i = 0; i < 1250; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("DECREASE_PADDLE"), "DECREASE_PADDLE expired");

        // Paddle width should grow back to 240.0f smoothly
        for (int i = 0; i < 60; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(std::abs(world.paddle().size.w - 240.0f) < 0.001f, "paddle width returns to 240.0");
    }

    {
        // Test STICKY_PADDLE bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Playing, "phase is Playing");

        // Spawn and collect STICKY_PADDLE
        world.spawnBonus("STICKY_PADDLE", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "STICKY_PADDLE spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "STICKY_PADDLE collected");
        require(world.isBonusActive("STICKY_PADDLE"), "STICKY_PADDLE active after pickup");

        // Teleport the ball to collide with the paddle
        world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{800.0f, 910.0f});

        // Step a few frames to let the collision register in physics step
        for (int i = 0; i < 5; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        
        // Since STICKY_PADDLE is active, the ball should attach to the paddle
        require(world.ball().state == arcadeblocks::gameplay::BallState::AttachedToPaddle, "ball attached to paddle");

        // Move paddle right and see if the attached ball moves with it
        const float oldBallX = world.ball().position.x;
        // Move paddle right for 10 frames
        for (int i = 0; i < 10; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.moveRight = true});
        }
        require(world.ball().position.x > oldBallX, "attached ball moved right with the paddle");

        // Release the ball by pressing launch
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "ball launched");
        require(world.ball().velocity.y < -500.0f, "ball moving upwards");

        // Let it fly one more frame to clear paddle contact
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        // Let the ball stick again
        world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{world.paddle().position.x + 40.0f, 910.0f});

        for (int i = 0; i < 5; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(world.ball().state == arcadeblocks::gameplay::BallState::AttachedToPaddle, "ball attached again");

        // Wait out the STICKY_PADDLE bonus duration (20 seconds, i.e., 1200 frames at 60fps)
        for (int i = 0; i < 1250; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("STICKY_PADDLE"), "STICKY_PADDLE expired");

        // Ball should launch/release automatically after expiration
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "ball released automatically on expiry");
    }

    {
        // Test PLASMA_WEAPON and FROZEN_PADDLE bonuses
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Activate FROZEN_PADDLE before launching
        world.spawnBonus("FROZEN_PADDLE", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("FROZEN_PADDLE"), "Frozen paddle active");
        require(world.ball().state == arcadeblocks::gameplay::BallState::AttachedToPaddle, "Ball starts attached");

        // Try to launch the ball manually while frozen
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.ball().state == arcadeblocks::gameplay::BallState::AttachedToPaddle, "Ball remains attached when launching while frozen");

        // Wait a long time (e.g. 5 seconds) to make sure auto-launch countdown is paused while frozen
        for (int i = 0; i < 300; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        // Frozen paddle should be expired by now (3 seconds duration)
        require(!world.isBonusActive("FROZEN_PADDLE"), "Frozen paddle expired");
        require(world.ball().state == arcadeblocks::gameplay::BallState::AttachedToPaddle, "Ball did not auto-launch during frozen state");

        // Now launching manually should work!
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "Ball successfully launched after frozen paddle expired");
        require(world.phase() == arcadeblocks::gameplay::GamePhase::Playing, "phase is Playing");

        // 1. Spawn and collect PLASMA_WEAPON
        world.spawnBonus("PLASMA_WEAPON", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "PLASMA_WEAPON spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "PLASMA_WEAPON collected");
        require(world.isBonusActive("PLASMA_WEAPON"), "PLASMA_WEAPON active after pickup");
        require(world.plasmaAmmo() == 10, "Initial plasma ammo is 10");

        // 2. Reactivate/recharge PLASMA_WEAPON
        world.spawnBonus("PLASMA_WEAPON", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.plasmaAmmo() == 20, "Recharged plasma ammo is 20");

        // 3. Shoot bullets
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.shootPlasmaPressed = true});
        require(world.plasmaAmmo() == 19, "Ammo decreased after shooting");
        require(world.plasmaBullets().size() == 2, "Two plasma bullets spawned");

        // 4. Update bullets movement and let them hit a brick
        bool brickHitFound = false;
        for (int frame = 0; frame < 100; ++frame) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
            if (world.plasmaBullets().empty()) {
                brickHitFound = true;
                break;
            }
        }
        require(brickHitFound, "Bullets hit a brick and disappeared");

        // 5. Spawn and collect FROZEN_PADDLE
        world.spawnBonus("FROZEN_PADDLE", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "FROZEN_PADDLE spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "FROZEN_PADDLE collected");
        require(world.isBonusActive("FROZEN_PADDLE"), "FROZEN_PADDLE active after pickup");

        // Test that we cannot shoot plasma bullets while frozen
        // First recharge plasma ammo so we have ammo to shoot
        world.spawnBonus("PLASMA_WEAPON", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.plasmaAmmo() > 0, "We have plasma ammo");
        const std::size_t oldPlasmaBulletsCount = world.plasmaBullets().size();
        const int oldPlasmaAmmo = world.plasmaAmmo();
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.shootPlasmaPressed = true});
        require(world.plasmaAmmo() == oldPlasmaAmmo, "Plasma ammo did not decrease when frozen");
        require(world.plasmaBullets().size() == oldPlasmaBulletsCount, "No new plasma bullets spawned when frozen");

        // 6. Test frozen paddle blocks movement
        const float oldPaddleX = world.paddle().position.x;
        // Try to move left
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.moveLeft = true});
        require(world.paddle().position.x == oldPaddleX, "Paddle did not move left when frozen");
        // Try to move right
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.moveRight = true});
        require(world.paddle().position.x == oldPaddleX, "Paddle did not move right when frozen");

        // 7. Wait out the FROZEN_PADDLE bonus duration (3 seconds, i.e., 180 frames at 60fps)
        for (int i = 0; i < 200; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("FROZEN_PADDLE"), "FROZEN_PADDLE expired after 3 seconds");

        // Now movement should work!
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.moveRight = true});
        require(world.paddle().position.x > oldPaddleX, "Paddle moves right after FROZEN_PADDLE expired");
    }

    {
        // Test INVISIBLE_PADDLE bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Spawn and collect INVISIBLE_PADDLE
        world.spawnBonus("INVISIBLE_PADDLE", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "INVISIBLE_PADDLE spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "INVISIBLE_PADDLE collected");
        require(world.isBonusActive("INVISIBLE_PADDLE"), "INVISIBLE_PADDLE active after pickup");

        // 2. Recharge plasma weapon to test if shooting is blocked
        world.spawnBonus("PLASMA_WEAPON", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.plasmaAmmo() > 0, "We have plasma ammo");

        // Try to shoot plasma bullets while invisible
        const std::size_t oldPlasmaBulletsCount = world.plasmaBullets().size();
        const int oldPlasmaAmmo = world.plasmaAmmo();
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.shootPlasmaPressed = true});
        require(world.plasmaAmmo() == oldPlasmaAmmo, "Plasma ammo did not decrease when invisible");
        require(world.plasmaBullets().size() == oldPlasmaBulletsCount, "No new plasma bullets spawned when invisible");

        // 3. Test that paddle CAN still move when invisible
        const float oldPaddleX = world.paddle().position.x;
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.moveRight = true});
        require(world.paddle().position.x > oldPaddleX, "Paddle moves right when invisible");

        // 4. Wait out the INVISIBLE_PADDLE bonus duration (5 seconds, i.e., 300 frames at 60fps)
        // We do 320 frames just to be sure
        for (int i = 0; i < 320; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("INVISIBLE_PADDLE"), "INVISIBLE_PADDLE expired after 5 seconds");

        // 5. Try to shoot plasma now - it should work
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{.shootPlasmaPressed = true});
        require(world.plasmaAmmo() < oldPlasmaAmmo, "Plasma ammo decreased when no longer invisible");
        require(world.plasmaBullets().size() > oldPlasmaBulletsCount, "Plasma bullets spawned when no longer invisible");
    }

    {
        // Test BONUS_WALL bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s and launch the ball
        world.update(1.1, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "Ball is launched");

        // 1. Spawn and collect BONUS_WALL
        world.spawnBonus("BONUS_WALL", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "BONUS_WALL spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "BONUS_WALL collected");
        require(world.isBonusActive("BONUS_WALL"), "BONUS_WALL active after pickup");

        // Teleport ball close to the bottom (say y = 1050) with velocity moving downwards (vy = 400)
        world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{960.0f, 1050.0f}, arcadeblocks::gameplay::Vec2{0.0f, 400.0f});

        // Update physics and verify it bounces instead of falling below bottom
        for (int i = 0; i < 10; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }

        // Ball position should be pushed up, and its velocity Y should be negative (bounced)
        require(world.ball().velocity.y < 0.0f, "Ball bounced off safety wall (velocity Y is negative)");
        require(world.ball().position.y <= 1080.0f - world.ball().radius, "Ball position Y is bounded");
        require(world.lives() == 3, "No life was lost");

        // 2. Wait out the BONUS_WALL bonus duration (10.75 seconds total, i.e., 645 frames at 60fps)
        for (int i = 0; i < 670; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("BONUS_WALL"), "BONUS_WALL expired after 10.75 seconds");

        // Teleport ball close to the bottom again and let it fall out of bounds
        world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{960.0f, 1060.0f}, arcadeblocks::gameplay::Vec2{0.0f, 400.0f});

        // Update physics - ball should fall out and trigger life lost
        for (int i = 0; i < 10; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(world.lives() < 3, "Life was lost after BONUS_WALL expired");
    }

    {
        // Test DARKNESS bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Spawn and collect DARKNESS
        world.spawnBonus("DARKNESS", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "DARKNESS spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "DARKNESS collected");
        require(world.isBonusActive("DARKNESS"), "DARKNESS active after pickup");

        // 2. Wait out the DARKNESS bonus duration (15.75 seconds total, i.e., 945 frames at 60fps)
        for (int i = 0; i < 970; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("DARKNESS"), "DARKNESS expired after 15.75 seconds");
    }

    {
        // Test CHAOTIC_BALLS bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s and launch the ball
        world.update(1.1, arcadeblocks::gameplay::GameInput{.launchPressed = true});
        require(world.ball().state == arcadeblocks::gameplay::BallState::Launched, "Ball is launched");

        // 1. Spawn and collect CHAOTIC_BALLS
        world.spawnBonus("CHAOTIC_BALLS", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "CHAOTIC_BALLS spawned");

        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.fallingBonuses().empty(), "CHAOTIC_BALLS collected");
        require(world.isBonusActive("CHAOTIC_BALLS"), "CHAOTIC_BALLS active after pickup");

        // 2. Wait out the CHAOTIC_BALLS bonus duration (15.75 seconds total, i.e., 945 frames at 60fps)
        for (int i = 0; i < 970; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("CHAOTIC_BALLS"), "CHAOTIC_BALLS expired after 15.75 seconds");
    }

    {
        // Test BONUS_MAGNET and PENALTIES_MAGNET bonuses (with mutual exclusion)
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Spawn and collect BONUS_MAGNET
        world.spawnBonus("BONUS_MAGNET", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        require(world.fallingBonuses().size() == 1, "BONUS_MAGNET spawned");
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET active");
        require(!world.isBonusActive("PENALTIES_MAGNET"), "PENALTIES_MAGNET not active");

        // 2. Spawn and collect PENALTIES_MAGNET to trigger mutual exclusion
        world.spawnBonus("PENALTIES_MAGNET", arcadeblocks::gameplay::Vec2{760.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("PENALTIES_MAGNET"), "PENALTIES_MAGNET active");
        require(!world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET deactivated via mutual exclusion");

        // 3. Wait out the PENALTIES_MAGNET bonus duration (20.75 seconds total, i.e., 1245 frames at 60fps)
        for (int i = 0; i < 1270; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        require(!world.isBonusActive("PENALTIES_MAGNET"), "PENALTIES_MAGNET expired after 20.75 seconds");
    }

    {
        // Test BAD_LUCK bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Activate BONUS_MAGNET
        world.spawnBonus("BONUS_MAGNET", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET is active");

        // 2. Spawn a falling BONUS_WALL capsule on the screen
        world.spawnBonus("BONUS_WALL", arcadeblocks::gameplay::Vec2{500.0f, 500.0f});
        require(world.fallingBonuses().size() == 1, "BONUS_WALL is spawned and falling");

        // 3. Spawn and collect BAD_LUCK
        world.spawnBonus("BAD_LUCK", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        // 4. Verify positive timed bonuses are deactivated and falling positive capsules are destroyed
        require(!world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET was deactivated by BAD_LUCK");
        require(world.fallingBonuses().empty(), "Falling positive bonuses were cleared from screen");

        // 5. Verify that negative penalties are active (e.g. DARKNESS, CHAOTIC_BALLS)
        require(world.isBonusActive("DARKNESS"), "DARKNESS is active");
        require(world.isBonusActive("CHAOTIC_BALLS"), "CHAOTIC_BALLS is active");

        // 6. Verify that their durations are doubled (DARKNESS duration = 30.0s, i.e. 1800 frames at 60fps)
        // Wait out 20 seconds (1200 frames) - they should still be active
        for (int i = 0; i < 1200; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
            if (world.ball().position.y > 800.0f) {
                world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{960.0f, 300.0f}, arcadeblocks::gameplay::Vec2{0.0f, -400.0f});
            }
        }
        require(world.isBonusActive("DARKNESS"), "DARKNESS is still active after 20 seconds (since duration is 30s)");

        // Wait another 11 seconds (660 frames) - they should be expired by now
        for (int i = 0; i < 700; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
            if (world.ball().position.y > 800.0f) {
                world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{960.0f, 300.0f}, arcadeblocks::gameplay::Vec2{0.0f, -400.0f});
            }
        }
        require(!world.isBonusActive("DARKNESS"), "DARKNESS expired after doubled duration (30.75s total)");
    }

    {
        // Test TRICKSTER bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Activate DARKNESS (negative)
        world.spawnBonus("DARKNESS", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("DARKNESS"), "DARKNESS is active");

        // 2. Spawn a falling CHAOTIC_BALLS (negative) capsule on the screen
        world.spawnBonus("CHAOTIC_BALLS", arcadeblocks::gameplay::Vec2{500.0f, 500.0f});
        require(world.fallingBonuses().size() == 1, "CHAOTIC_BALLS is spawned and falling");

        // 3. Spawn and collect TRICKSTER
        world.spawnBonus("TRICKSTER", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        // 4. Verify negative timed penalties are deactivated and falling negative capsules are destroyed
        require(!world.isBonusActive("DARKNESS"), "DARKNESS was deactivated by TRICKSTER");
        require(world.fallingBonuses().empty(), "Falling negative bonuses were cleared from screen");

        // 5. Verify that positive timed bonuses are active with doubled durations (e.g., BONUS_WALL, BONUS_MAGNET, PLASMA_WEAPON with 20 ammo/seconds)
        require(world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET is active");
        require(world.isBonusActive("BONUS_WALL"), "BONUS_WALL is active");
        require(world.isBonusActive("PLASMA_WEAPON"), "PLASMA_WEAPON is active");
        require(world.plasmaAmmo() == 20, "PLASMA_WEAPON has 20 ammo");

        // 6. Verify that their durations are doubled (BONUS_MAGNET duration = 40.0s, i.e. 2400 frames at 60fps)
        // Wait out 30 seconds (1800 frames) - they should still be active
        for (int i = 0; i < 1800; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
            if (world.ball().position.y > 800.0f) {
                world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{960.0f, 300.0f}, arcadeblocks::gameplay::Vec2{0.0f, -400.0f});
            }
        }
        require(world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET is still active after 30 seconds (since duration is 40s)");

        // Wait another 11 seconds (660 frames) - they should be expired by now
        for (int i = 0; i < 700; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
            if (world.ball().position.y > 800.0f) {
                world.teleportBallForTesting(world.ball().entity, arcadeblocks::gameplay::Vec2{960.0f, 300.0f}, arcadeblocks::gameplay::Vec2{0.0f, -400.0f});
            }
        }
        require(!world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET expired after doubled duration");
    }

    {
        // Test ADD_FIVE_SECONDS bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Activate BONUS_MAGNET (positive, 20.0s)
        world.spawnBonus("BONUS_MAGNET", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET is active");

        // 2. Wait 5.0 seconds (300 frames)
        for (int i = 0; i < 300; ++i) {
            world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        }
        // Verify remaining seconds is around 15.0 seconds
        double initialRemaining = 0.0;
        for (const auto& timer : world.activeBonusTimers()) {
            if (timer.type == "BONUS_MAGNET") {
                initialRemaining = timer.remainingSeconds;
            }
        }
        require(initialRemaining > 14.5 && initialRemaining < 15.5, "BONUS_MAGNET remaining time decreased to ~15s");

        // 3. Collect ADD_FIVE_SECONDS
        world.spawnBonus("ADD_FIVE_SECONDS", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        // 4. Verify remaining seconds increased by 5.0
        double postRemaining = 0.0;
        for (const auto& timer : world.activeBonusTimers()) {
            if (timer.type == "BONUS_MAGNET") {
                postRemaining = timer.remainingSeconds;
            }
        }
        require(postRemaining > 19.5 && postRemaining < 20.5, "BONUS_MAGNET remaining time increased by 5s");

        // 5. Test plasma integration: spawn and collect PLASMA_WEAPON
        world.spawnBonus("PLASMA_WEAPON", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.plasmaAmmo() == 10, "PLASMA_WEAPON ammo initialized to 10");

        // 6. Collect ADD_FIVE_SECONDS
        world.spawnBonus("ADD_FIVE_SECONDS", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        // 7. Verify ammo is now 15 and timer is 15.0
        require(world.plasmaAmmo() == 15, "PLASMA_WEAPON ammo recharged to 15 by ADD_FIVE_SECONDS");
        double plasmaRemaining = 0.0;
        for (const auto& timer : world.activeBonusTimers()) {
            if (timer.type == "PLASMA_WEAPON") {
                plasmaRemaining = timer.remainingSeconds;
            }
        }
        require(plasmaRemaining > 14.5 && plasmaRemaining < 15.5, "PLASMA_WEAPON remaining time set to 15s");
    }

    {
        // Test RESET bonus
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Activate BONUS_MAGNET (positive)
        world.spawnBonus("BONUS_MAGNET", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET is active");

        // 2. Activate DARKNESS (negative)
        world.spawnBonus("DARKNESS", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.isBonusActive("DARKNESS"), "DARKNESS is active");

        // 3. Spawning plasma and verifying ammo is active
        world.spawnBonus("PLASMA_WEAPON", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(world.plasmaAmmo() == 10, "PLASMA_WEAPON ammo initialized to 10");

        // 4. Collect RESET
        world.spawnBonus("RESET", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        // 5. Verify all timers are cleared and plasma ammo is reset
        require(world.activeBonusTimers().empty(), "All active bonus timers cleared by RESET");
        require(world.plasmaAmmo() == 0, "Plasma weapon ammo reset to 0 by RESET");
        require(!world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET deactivated by RESET");
        require(!world.isBonusActive("DARKNESS"), "DARKNESS deactivated by RESET");
    }

    {
        // Test RANDOM_BONUS
        // We initialize the world with only "BONUS_MAGNET" and "RANDOM_BONUS" enabled.
        // That way, RANDOM_BONUS will deterministically choose BONUS_MAGNET.
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        std::vector<std::string> enabled = {"BONUS_MAGNET", "RANDOM_BONUS"};
        // Wait out the launch delay of 1.0s
        world.update(1.1, arcadeblocks::gameplay::GameInput{}, enabled);

        // 1. Verify BONUS_MAGNET is NOT active initially
        require(!world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET is not active initially");

        // 2. Spawn and collect RANDOM_BONUS
        world.spawnBonus("RANDOM_BONUS", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{}, enabled);

        // 3. Verify that BONUS_MAGNET was chosen and activated
        require(world.isBonusActive("BONUS_MAGNET"), "BONUS_MAGNET was chosen and activated by RANDOM_BONUS");
    }

    {
        // Test RAINBOW_BOUNTY activation
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        require(!world.isBonusActive("RAINBOW_BOUNTY"), "RAINBOW_BOUNTY is not active initially");

        // Spawn and collect RAINBOW_BOUNTY
        world.spawnBonus("RAINBOW_BOUNTY", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        require(world.isBonusActive("RAINBOW_BOUNTY"), "RAINBOW_BOUNTY is active after pickup");
        require(world.activeBonusTimers().front().remainingSeconds > 14.9, "RAINBOW_BOUNTY timer is near 15 seconds");

        // Test guaranteed drop
        // Verify a regular brick destruction spawns a positive drop
        const auto targetBrick = world.bricks().front().entity;
        world.damageBrickForTesting(targetBrick, 99);
        require(world.fallingBonuses().size() == 1, "Guaranteed drop spawns a falling bonus");
        require(arcadeblocks::gameplay::isPositiveBonus(world.fallingBonuses().front().type), "Guaranteed drop is a positive bonus");

        // Verify it was not RAINBOW_BOUNTY, TRICKSTER, LEVEL_PASS, RANDOM_BONUS, or BONUS_SCORE_10000
        std::string dropType = world.fallingBonuses().front().type;
        require(dropType != "RAINBOW_BOUNTY", "Drop is not RAINBOW_BOUNTY");
        require(dropType != "TRICKSTER", "Drop is not TRICKSTER");
        require(dropType != "LEVEL_PASS", "Drop is not LEVEL_PASS");
        require(dropType != "RANDOM_BONUS", "Drop is not RANDOM_BONUS");
        require(dropType != "BONUS_SCORE_10000", "Drop is not BONUS_SCORE_10000");

        // Test expiration
        world.update(15.1, arcadeblocks::gameplay::GameInput{});
        require(!world.isBonusActive("RAINBOW_BOUNTY"), "RAINBOW_BOUNTY expired after 15 seconds");
    }

    {
        // Test RAINBOW_BOUNTY helper candidate filtering
        std::vector<std::string> enabled1 = {"RAINBOW_BOUNTY", "TRICKSTER", "BONUS_SCORE"};
        auto opt1 = arcadeblocks::gameplay::pickRainbowBountyDrop(enabled1);
        require(opt1.has_value(), "Filter returns a value");
        require(*opt1 == "BONUS_SCORE", "TRICKSTER and RAINBOW_BOUNTY are filtered out");

        std::vector<std::string> enabled2 = {"RAINBOW_BOUNTY", "TRICKSTER"};
        auto opt2 = arcadeblocks::gameplay::pickRainbowBountyDrop(enabled2);
        require(!opt2.has_value(), "Filter returns nullopt when no positive candidates are left");

        std::vector<std::string> enabled3 = {"BONUS_MAGNET"};
        auto opt3 = arcadeblocks::gameplay::pickRainbowBountyDrop(enabled3);
        require(opt3.has_value(), "Filter returns magnet");
        require(*opt3 == "BONUS_MAGNET", "Magnet is selected");
    }

    {
        // Test RAINBOW_BOUNTY reset and bad luck clearing
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Reset check
        world.activateBonusForTesting("RAINBOW_BOUNTY", 15.0);
        require(world.isBonusActive("RAINBOW_BOUNTY"), "RAINBOW_BOUNTY active for reset test");
        world.spawnBonus("RESET", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(!world.isBonusActive("RAINBOW_BOUNTY"), "RAINBOW_BOUNTY cleared by RESET");

        // 2. Bad luck check
        world.activateBonusForTesting("RAINBOW_BOUNTY", 15.0);
        require(world.isBonusActive("RAINBOW_BOUNTY"), "RAINBOW_BOUNTY active for bad luck test");
        world.spawnBonus("BAD_LUCK", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(!world.isBonusActive("RAINBOW_BOUNTY"), "RAINBOW_BOUNTY cleared by BAD_LUCK");
    }

    {
        // Test BLOOD_TITHE activation
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        require(!world.isBonusActive("BLOOD_TITHE"), "BLOOD_TITHE is not active initially");

        // Spawn and collect BLOOD_TITHE
        world.spawnBonus("BLOOD_TITHE", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});

        require(world.isBonusActive("BLOOD_TITHE"), "BLOOD_TITHE is active after pickup");
        require(world.activeBonusTimers().front().remainingSeconds > 14.9, "BLOOD_TITHE timer is near 15 seconds");

        // Test guaranteed drop (only negatives allowed in the pool)
        // Verify a regular brick destruction spawns a negative drop
        const auto targetBrick = world.bricks().front().entity;
        world.damageBrickForTesting(targetBrick, 99);
        require(world.fallingBonuses().size() == 1, "Guaranteed drop spawns a falling bonus");
        require(!arcadeblocks::gameplay::isPositiveBonus(world.fallingBonuses().front().type), "Guaranteed drop is a negative bonus");

        // Verify it was not BLOOD_TITHE, BAD_LUCK, RESET, RANDOM_BONUS, or RAINBOW_BOUNTY
        std::string dropType = world.fallingBonuses().front().type;
        require(dropType != "BLOOD_TITHE", "Drop is not BLOOD_TITHE");
        require(dropType != "BAD_LUCK", "Drop is not BAD_LUCK");
        require(dropType != "RESET", "Drop is not RESET");
        require(dropType != "RANDOM_BONUS", "Drop is not RANDOM_BONUS");
        require(dropType != "RAINBOW_BOUNTY", "Drop is not RAINBOW_BOUNTY");

        // Test expiration
        world.update(15.1, arcadeblocks::gameplay::GameInput{});
        require(!world.isBonusActive("BLOOD_TITHE"), "BLOOD_TITHE expired after 15 seconds");
    }

    {
        // Test BLOOD_TITHE helper candidate filtering
        std::vector<std::string> enabled1 = {"BLOOD_TITHE", "BAD_LUCK", "DARKNESS"};
        auto opt1 = arcadeblocks::gameplay::pickBloodTitheDrop(enabled1);
        require(opt1.has_value(), "Filter returns a value");
        require(*opt1 == "DARKNESS", "BAD_LUCK and BLOOD_TITHE are filtered out");

        std::vector<std::string> enabled2 = {"BLOOD_TITHE", "BAD_LUCK"};
        auto opt2 = arcadeblocks::gameplay::pickBloodTitheDrop(enabled2);
        require(!opt2.has_value(), "Filter returns nullopt when no valid negative candidates are left");

        std::vector<std::string> enabled3 = {"DARKNESS"};
        auto opt3 = arcadeblocks::gameplay::pickBloodTitheDrop(enabled3);
        require(opt3.has_value(), "Filter returns darkness");
        require(*opt3 == "DARKNESS", "Darkness is selected");
    }

    {
        // Test BLOOD_TITHE reset and trickster clearing
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        // 1. Reset check
        world.activateBonusForTesting("BLOOD_TITHE", 15.0);
        require(world.isBonusActive("BLOOD_TITHE"), "BLOOD_TITHE active for reset test");
        world.spawnBonus("RESET", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(!world.isBonusActive("BLOOD_TITHE"), "BLOOD_TITHE cleared by RESET");

        // 2. Trickster check
        world.activateBonusForTesting("BLOOD_TITHE", 15.0);
        require(world.isBonusActive("BLOOD_TITHE"), "BLOOD_TITHE active for trickster test");
        world.spawnBonus("TRICKSTER", arcadeblocks::gameplay::Vec2{world.paddle().position.x + 20.0f, 930.0f});
        world.update(1.0 / 60.0, arcadeblocks::gameplay::GameInput{});
        require(!world.isBonusActive("BLOOD_TITHE"), "BLOOD_TITHE cleared by TRICKSTER");
    }

    {
        // Test conflict between BLOOD_TITHE and RAINBOW_BOUNTY (BLOOD_TITHE has priority)
        auto world = arcadeblocks::gameplay::GameWorld::fromLevel(makeGameplayFixture(99, 25, 16, 100, 15));
        world.update(1.1, arcadeblocks::gameplay::GameInput{});

        world.activateBonusForTesting("BLOOD_TITHE", 15.0);
        world.activateBonusForTesting("RAINBOW_BOUNTY", 15.0);
        require(world.isBonusActive("BLOOD_TITHE"), "BLOOD_TITHE is active");
        require(world.isBonusActive("RAINBOW_BOUNTY"), "RAINBOW_BOUNTY is active");

        const auto targetBrick = world.bricks().front().entity;
        world.damageBrickForTesting(targetBrick, 99);
        require(world.fallingBonuses().size() == 1, "Exactly one bonus drops");
        require(!arcadeblocks::gameplay::isPositiveBonus(world.fallingBonuses().front().type), "Negative drop (BLOOD_TITHE priority) occurs");
    }
}

