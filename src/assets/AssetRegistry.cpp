#include "assets/AssetRegistry.hpp"

#include <algorithm>
#include <array>
#include <random>
#include <system_error>
#include <utility>

namespace arcadeblocks::assets {
namespace {

constexpr int level1 = 1;

} // namespace

AssetRegistry::AssetRegistry(std::filesystem::path assetsDirectory)
    : assetsDirectory_(std::move(assetsDirectory)) {
    const std::array<MenuAssetMapping, 4> menuPool{{
        {
            .background = std::filesystem::path{"sprites/main_menu/background.png"},
            .logo = std::filesystem::path{"sprites/arcadeblocks2_logo.png"},
            .music = std::filesystem::path{"music/menu/main_menu.ogg"},
            .welcomeSound = std::filesystem::path{"sounds/menu/welcomesound.ogg"},
        },
        {
            .background = std::filesystem::path{"sprites/main_menu/background2.png"},
            .logo = std::filesystem::path{"sprites/arcadeblocks2_logo.png"},
            .music = std::filesystem::path{"music/menu/main_menu2.ogg"},
            .welcomeSound = std::filesystem::path{"sounds/menu/welcomesound2.ogg"},
        },
        {
            .background = std::filesystem::path{"sprites/main_menu/background3.png"},
            .logo = std::filesystem::path{"sprites/arcadeblocks2_logo.png"},
            .music = std::filesystem::path{"music/menu/main_menu3.ogg"},
            .welcomeSound = std::filesystem::path{"sounds/menu/welcomesound3.ogg"},
        },
        {
            .background = std::filesystem::path{"sprites/main_menu/background4.png"},
            .logo = std::filesystem::path{"sprites/arcadeblocks2_logo.png"},
            .music = std::filesystem::path{"music/menu/main_menu_4.ogg"},
            .welcomeSound = std::filesystem::path{"sounds/menu/welcomesound4.ogg"},
        },
    }};

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<std::size_t> menuDist(0, menuPool.size() - 1);

    menu_ = menuPool[menuDist(rng)];
}

const MenuAssetMapping& AssetRegistry::menu() const noexcept {
    return menu_;
}

LevelAssetMapping AssetRegistry::level(int levelNumber) const {
    const int normalizedLevel = levelNumber > 0 ? levelNumber : level1;
    auto mapping = LevelAssetMapping{
        .levelNumber = normalizedLevel,
        .levelJson = classicLevelJson(normalizedLevel),
        .background = levelBackground(normalizedLevel),
        .music = levelMusic(normalizedLevel),
        .brickSprites = brickSprites(),
        .sfxByEvent = sfxByEvent(normalizedLevel),
        .preloadSfx = preloadSfx(),
    };
    if (normalizedLevel == 31) {
        mapping.background = std::filesystem::path{"sprites/level_backgrounds/easter_egg.png"};
    }
    return mapping;
}

std::filesystem::path AssetRegistry::resolve(const std::filesystem::path& relativePath) const {
    if (relativePath.is_absolute()) {
        return relativePath;
    }
    return assetsDirectory_ / relativePath;
}

bool AssetRegistry::exists(const std::filesystem::path& relativePath) const {
    std::error_code error;
    return std::filesystem::exists(resolve(relativePath), error);
}

std::optional<std::filesystem::path> AssetRegistry::firstMissingRequiredAsset(const LevelAssetMapping& mapping) const {
    const std::vector<std::filesystem::path> required{
        mapping.levelJson,
        mapping.background,
        mapping.music,
    };

    for (const auto& asset : required) {
        if (!exists(asset)) {
            return asset;
        }
    }

    for (const auto& sound : mapping.preloadSfx) {
        if (!exists(sound)) {
            return sound;
        }
    }

    for (const auto& [_, sounds] : mapping.sfxByEvent) {
        for (const auto& sound : sounds) {
            if (!exists(sound)) {
                return sound;
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string> AssetRegistry::firstMissingBrickSprite(
    const LevelAssetMapping& mapping,
    const std::vector<std::string>& availableSprites) const {
    for (const auto& [_, sprite] : mapping.brickSprites) {
        if (std::find(availableSprites.begin(), availableSprites.end(), sprite) == availableSprites.end()) {
            return sprite;
        }
    }
    return std::nullopt;
}

std::filesystem::path AssetRegistry::classicLevelJson(int levelNumber) const {
    return std::filesystem::path{"levels/arcadeblocks_1"} / ("level" + std::to_string(levelNumber) + ".json");
}

std::filesystem::path AssetRegistry::levelBackground(int levelNumber) const {
    if (levelNumber == 10) {
        return "sprites/boss_backgrounds/boss1_background.jpg";
    } else if (levelNumber == 20) {
        return "sprites/boss_backgrounds/boss_background2.jpg";
    } else if (levelNumber == 30) {
        return "sprites/boss_backgrounds/boss_background3.jpg";
    } else if (levelNumber == 40) {
        return "sprites/boss_backgrounds/boss_background4.jpg";
    } else if (levelNumber == 50) {
        return "sprites/boss_backgrounds/boss_background5.jpg";
    }
    // Stage 10 rule: level1.jpg is the compact legacy background. level1_png.jpg
    // is a separate converted PNG background and is only used by explicit mapping.
    return std::filesystem::path{"sprites/level_backgrounds"} / ("level" + std::to_string(levelNumber) + ".jpg");
}

std::filesystem::path AssetRegistry::levelMusic(int levelNumber) const {
    if (levelNumber == 10) {
        return "music/boss/boss_music1.ogg";
    } else if (levelNumber == 20) {
        return "music/boss/boss_music2.ogg";
    } else if (levelNumber == 30) {
        return "music/boss/boss_music3.ogg";
    } else if (levelNumber == 40) {
        return "music/boss/boss_music4.ogg";
    } else if (levelNumber == 50) {
        return "music/boss/boss_music5.ogg";
    }
    const auto candidate = std::filesystem::path{"music/level"} / ("level" + std::to_string(levelNumber) + ".ogg");
    return exists(candidate) ? candidate : std::filesystem::path{"music/level/level1.ogg"};
}

std::unordered_map<levels::BrickColor, std::string> AssetRegistry::brickSprites() const {
    return {
        {levels::BrickColor::Blue, "blue_brick.png"},
        {levels::BrickColor::Cyan, "blue_brick.png"},
        {levels::BrickColor::DarkBlue, "dark_blue_brick.png"},
        {levels::BrickColor::Green, "green_brick.png"},
        {levels::BrickColor::Pink, "pink_brick.png"},
        {levels::BrickColor::Purple, "pink_brick.png"},
        {levels::BrickColor::Yellow, "yellow_brick.png"},
        {levels::BrickColor::Orange, "yellow_brick.png"},
        {levels::BrickColor::Red, "yellow_brick.png"},
        {levels::BrickColor::Explosive, "explosive_bricks.png"},
        {levels::BrickColor::Indestructible, "yellow_brick.png"},
        {levels::BrickColor::LightGray, "light_gray_brick.png"},
        {levels::BrickColor::Shielded, "yellow_brick.png"},
    };
}

std::unordered_map<gameplay::AudioEventType, std::vector<std::filesystem::path>> AssetRegistry::sfxByEvent(int levelNumber) const {
    auto m = std::unordered_map<gameplay::AudioEventType, std::vector<std::filesystem::path>>{
        {gameplay::AudioEventType::PaddleSpawn, {"sounds/gameplay/paddle_spawn.ogg"}},
        {gameplay::AudioEventType::BallLaunch, {"sounds/gameplay/ball_launch.ogg"}},
        {gameplay::AudioEventType::PaddleHit, {"sounds/gameplay/paddle_hit.ogg"}},
        {gameplay::AudioEventType::WallHit, {"sounds/gameplay/wall_bounce.ogg"}},
        {gameplay::AudioEventType::BrickHit, {"sounds/gameplay/no_dest_brick1.ogg"}},
        {gameplay::AudioEventType::BrickBreak, {"sounds/gameplay/brick_break.ogg"}},
        {gameplay::AudioEventType::Explosion, {"sounds/gameplay/explosion.ogg"}},
        {gameplay::AudioEventType::LifeLost, {"sounds/gameplay/life_lost.ogg"}},
        {gameplay::AudioEventType::GameOver, {"sounds/gameplay/game_over.ogg"}},
        {gameplay::AudioEventType::LevelComplete, {"sounds/gameplay/level_complete.ogg"}},
    };
    
    if (levelNumber == 20) {
        m[gameplay::AudioEventType::BossHit] = {
            "sounds/boss/boss2_hit1.ogg",
            "sounds/boss/boss2_hit2.ogg",
            "sounds/boss/boss2_hit3.ogg",
            "sounds/boss/boss2_hit4.ogg"
        };
        m[gameplay::AudioEventType::BossDefeated] = {"sounds/boss/boss_completed2.ogg"};
        if (exists("sounds/boss/boss2_shot.ogg")) {
            m[gameplay::AudioEventType::BossShot] = {"sounds/boss/boss2_shot.ogg"};
        }
        if (exists("sounds/boss/boss2_projectile_hit.ogg")) {
            m[gameplay::AudioEventType::BossProjectileHitPaddle] = {"sounds/boss/boss2_projectile_hit.ogg"};
        }
        if (exists("sounds/boss/boss2_shield_block.ogg")) {
            m[gameplay::AudioEventType::BossShieldBlock] = {"sounds/boss/boss2_shield_block.ogg"};
        }
        if (exists("sounds/boss/boss2_section1_destroyed.ogg")) {
            m[gameplay::AudioEventType::BossSectionDestroyed] = {
                "sounds/boss/boss2_section1_destroyed.ogg",
                "sounds/boss/boss2_section2_destroyed.ogg",
                "sounds/boss/boss2_section3_destroyed.ogg"
            };
        }
    } else if (levelNumber == 30) {
        m[gameplay::AudioEventType::BossHit] = {
            "sounds/boss/boss3_hit1.ogg",
            "sounds/boss/boss3_hit2.ogg",
            "sounds/boss/boss3_hit3.ogg",
            "sounds/boss/boss3_hit4.ogg"
        };
        m[gameplay::AudioEventType::BossDefeated] = {"sounds/boss/boss_completed3.ogg"};
        if (exists("sounds/boss/boss2_shot.ogg")) {
            m[gameplay::AudioEventType::BossShot] = {"sounds/boss/boss2_shot.ogg"};
        }
        if (exists("sounds/boss/boss3_teleport.ogg")) {
            m[gameplay::AudioEventType::BossTeleport] = {"sounds/boss/boss3_teleport.ogg"};
        }
        if (exists("sounds/boss/boss3_laser_charge.ogg")) {
            m[gameplay::AudioEventType::BossLaserCharge] = {"sounds/boss/boss3_laser_charge.ogg"};
        }
        if (exists("sounds/boss/boss3_laser_fire.ogg")) {
            m[gameplay::AudioEventType::BossLaserFire] = {"sounds/boss/boss3_laser_fire.ogg"};
        }
        if (exists("sounds/boss/boss3_drone_spawn.ogg")) {
            m[gameplay::AudioEventType::BossPhaseTransition] = {"sounds/boss/boss3_drone_spawn.ogg"};
        }
        if (exists("sounds/boss/boss3_phase2.ogg")) {
            m[gameplay::AudioEventType::BossPhaseTransition] = {"sounds/boss/boss3_phase2.ogg"};
        }
    } else if (levelNumber == 40) {
        m[gameplay::AudioEventType::BossHit] = {
            "sounds/boss/boss4_hit1.ogg",
            "sounds/boss/boss4_hit2.ogg",
            "sounds/boss/boss4_hit3.ogg",
            "sounds/boss/boss4_hit4.ogg"
        };
        m[gameplay::AudioEventType::BossDefeated] = {"sounds/boss/boss_completed4.ogg"};
        if (exists("sounds/boss/boss_loading4.ogg")) {
            m[gameplay::AudioEventType::BossLoading] = {"sounds/boss/boss_loading4.ogg"};
        }
        // Boss 4 (Singularity) shares the optional laser/shot/logic with boss 3
        // when those files are present. Each is wrapped in `exists()` so an
        // absent ElevenLabs render does not break anything.
        if (exists("sounds/boss/boss2_shot.ogg")) {
            m[gameplay::AudioEventType::BossShot] = {"sounds/boss/boss2_shot.ogg"};
        }
        if (exists("sounds/boss/boss3_teleport.ogg")) {
            m[gameplay::AudioEventType::BossTeleport] = {"sounds/boss/boss3_teleport.ogg"};
        }
        if (exists("sounds/boss/boss3_laser_charge.ogg")) {
            m[gameplay::AudioEventType::BossLaserCharge] = {"sounds/boss/boss3_laser_charge.ogg"};
        }
        if (exists("sounds/boss/boss3_laser_fire.ogg")) {
            m[gameplay::AudioEventType::BossLaserFire] = {"sounds/boss/boss3_laser_fire.ogg"};
        }
        if (exists("sounds/boss/boss3_drone_spawn.ogg")) {
            m[gameplay::AudioEventType::BossPhaseTransition] = {"sounds/boss/boss3_drone_spawn.ogg"};
        }
        if (exists("sounds/boss/boss3_phase2.ogg")) {
            // Override if both exist, prefer the boss 3 phase-2 cue for cinematic flair.
            m[gameplay::AudioEventType::BossPhaseTransition] = {"sounds/boss/boss3_phase2.ogg"};
        }
    } else if (levelNumber == 50) {
        m[gameplay::AudioEventType::BossHit] = {
            "sounds/boss/boss5_hit1.ogg",
            "sounds/boss/boss5_hit2.ogg",
            "sounds/boss/boss5_hit3.ogg",
            "sounds/boss/boss5_hit4.ogg"
        };
        if (exists("sounds/boss/boss_completed5.ogg")) {
            m[gameplay::AudioEventType::BossDefeated] = {"sounds/boss/boss_completed5.ogg"};
        } else {
            m[gameplay::AudioEventType::BossDefeated] = {"sounds/boss/boss_completed4.ogg"};
        }
        if (exists("sounds/boss/boss_loading5.ogg")) {
            m[gameplay::AudioEventType::BossLoading] = {"sounds/boss/boss_loading5.ogg"};
        }
        if (exists("sounds/boss/boss3_laser_charge.ogg")) {
            m[gameplay::AudioEventType::BossLaserCharge] = {"sounds/boss/boss3_laser_charge.ogg"};
        }
        if (exists("sounds/boss/boss3_laser_fire.ogg")) {
            m[gameplay::AudioEventType::BossLaserFire] = {"sounds/boss/boss3_laser_fire.ogg"};
        }
        if (exists("sounds/boss/boss3_phase2.ogg")) {
            m[gameplay::AudioEventType::BossPhaseTransition] = {"sounds/boss/boss3_phase2.ogg"};
        }
    } else {
        m[gameplay::AudioEventType::BossHit] = {
            "sounds/boss/boss1_hit1.ogg",
            "sounds/boss/boss1_hit2.ogg",
            "sounds/boss/boss1_hit3.ogg",
            "sounds/boss/boss1_hit4.ogg"
        };
        m[gameplay::AudioEventType::BossDefeated] = {"sounds/boss/boss_completed1.ogg"};
    }
    return m;
}

std::vector<std::filesystem::path> AssetRegistry::preloadSfx() const {
    std::vector<std::filesystem::path> result = {
        "sounds/gameplay/brick_break.ogg",
        "sounds/gameplay/brick_break2.ogg",
        "sounds/gameplay/brick_break3.ogg",
        "sounds/gameplay/brick_break4.ogg",
        "sounds/gameplay/explosion.ogg",
        "sounds/gameplay/no_dest_brick1.ogg",
        "sounds/gameplay/no_dest_brick2.ogg",
        "sounds/gameplay/ball_launch.ogg",
        "sounds/gameplay/paddle_spawn.ogg",
        "sounds/gameplay/paddle_hit.ogg",
        "sounds/gameplay/wall_bounce.ogg",
        "sounds/gameplay/life_lost.ogg",
        "sounds/gameplay/life_lost2.ogg",
        "sounds/gameplay/life_lost3.ogg",
        "sounds/gameplay/life_lost4.ogg",
        "sounds/gameplay/small_count_lives.ogg",
        "sounds/gameplay/game_over.ogg",
        "sounds/gameplay/level_complete.ogg",
        "sounds/bonus_activation/bonus_pickup.ogg",
        "sounds/bonus_activation/extra_score.ogg",
        "sounds/bonus_activation/extra_life.ogg",
        "sounds/bonus_activation/extra_ball.ogg",
        "sounds/bonus_activation/call_ball_activation.ogg",
        "sounds/bonus_activation/slow_balls.ogg",
        "sounds/bonus_activation/fast_balls.ogg",
        "sounds/bonus_activation/score_rain.ogg",
        "sounds/bonus_activation/weak_balls.ogg",
        "sounds/bonus_activation/energy_balls.ogg",
        "sounds/bonus_activation/explosion_ball.ogg",
        "sounds/bonus_activation/increase_paddle.ogg",
        "sounds/bonus_activation/decrease_paddle.ogg",
        "sounds/bonus_activation/sticky_paddle.ogg",
        "sounds/bonus_activation/frozen_paddle.ogg",
        "sounds/bonus_activation/ghost_paddle.ogg",
        "sounds/bonus_activation/plasma_weapon.ogg",
        "sounds/bonus_activation/recharge_plasma_weapon.ogg",
        "sounds/bonus_activation/add_five_seconds.ogg",
        "sounds/bonus_effects/plasma_shot.ogg",
        "sounds/gameplay/plasma_brick_hit1.ogg",
        "sounds/gameplay/plasma_brick_hit2.ogg",
        "sounds/gameplay/energy_ball_brick_hit.ogg",
        "sounds/bonus_effects/call_ball_paddle.ogg",
        "sounds/bonus_activation/bonus_wall.ogg",
        "sounds/bonus_activation/darkness.ogg",
        "sounds/bonus_activation/chaotic_balls.ogg",
        "sounds/bonus_activation/magnet_active.ogg",
        "sounds/bonus_activation/penalty_magnet.ogg",
        "sounds/bonus_activation/bad_luck.ogg",
        "sounds/bonus_activation/trickster.ogg",
        "sounds/bonus_activation/reset.ogg",
        "sounds/bonus_activation/random.ogg",
        "sounds/boss/boss_loading1.ogg",
        "sounds/boss/boss1_hit1.ogg",
        "sounds/boss/boss1_hit2.ogg",
        "sounds/boss/boss1_hit3.ogg",
        "sounds/boss/boss1_hit4.ogg",
        "sounds/boss/boss_completed1.ogg",
        "sounds/boss/boss_loading2.ogg",
        "sounds/boss/boss2_hit1.ogg",
        "sounds/boss/boss2_hit2.ogg",
        "sounds/boss/boss2_hit3.ogg",
        "sounds/boss/boss2_hit4.ogg",
        "sounds/boss/boss_completed2.ogg",
        "sounds/boss/boss_loading3.ogg",
        "sounds/boss/boss3_hit1.ogg",
        "sounds/boss/boss3_hit2.ogg",
        "sounds/boss/boss3_hit3.ogg",
        "sounds/boss/boss3_hit4.ogg",
        "sounds/boss/boss_completed3.ogg",
    };
    if (exists("sounds/boss/boss2_shot.ogg")) result.push_back("sounds/boss/boss2_shot.ogg");
    if (exists("sounds/boss/boss2_projectile_hit.ogg")) result.push_back("sounds/boss/boss2_projectile_hit.ogg");
    if (exists("sounds/boss/boss2_shield_block.ogg")) result.push_back("sounds/boss/boss2_shield_block.ogg");
    if (exists("sounds/boss/boss2_section1_destroyed.ogg")) {
        result.push_back("sounds/boss/boss2_section1_destroyed.ogg");
        result.push_back("sounds/boss/boss2_section2_destroyed.ogg");
        result.push_back("sounds/boss/boss2_section3_destroyed.ogg");
    }
    if (exists("sounds/boss/boss3_teleport.ogg")) result.push_back("sounds/boss/boss3_teleport.ogg");
    if (exists("sounds/boss/boss3_laser_charge.ogg")) result.push_back("sounds/boss/boss3_laser_charge.ogg");
    if (exists("sounds/boss/boss3_laser_fire.ogg")) result.push_back("sounds/boss/boss3_laser_fire.ogg");
    if (exists("sounds/boss/boss3_drone_spawn.ogg")) result.push_back("sounds/boss/boss3_drone_spawn.ogg");
    // Boss 4 (Singularity) preloads. The loading sting is only included if the
    // ElevenLabs render of boss_loading4.ogg has actually been generated; same
    // story for the boss_completed4.ogg defeat sting.
    if (exists("sounds/boss/boss_loading4.ogg")) result.push_back("sounds/boss/boss_loading4.ogg");
    if (exists("sounds/boss/boss_completed4.ogg")) result.push_back("sounds/boss/boss_completed4.ogg");
    result.push_back("sounds/boss/boss4_hit1.ogg");
    result.push_back("sounds/boss/boss4_hit2.ogg");
    result.push_back("sounds/boss/boss4_hit3.ogg");
    result.push_back("sounds/boss/boss4_hit4.ogg");
    if (exists("sounds/boss/boss3_phase2.ogg")) result.push_back("sounds/boss/boss3_phase2.ogg");
    if (exists("sounds/boss/boss_loading5.ogg")) result.push_back("sounds/boss/boss_loading5.ogg");
    if (exists("sounds/boss/boss_completed5.ogg")) result.push_back("sounds/boss/boss_completed5.ogg");
    result.push_back("sounds/boss/boss5_hit1.ogg");
    result.push_back("sounds/boss/boss5_hit2.ogg");
    result.push_back("sounds/boss/boss5_hit3.ogg");
    result.push_back("sounds/boss/boss5_hit4.ogg");
    return result;
}

} // namespace arcadeblocks::assets
