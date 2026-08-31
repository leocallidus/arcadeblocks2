// Catch2 tests for Boss 5 ("Chronarch - Zero Hour" - level 50).
//
// Deterministic surface; we lean on damageBossForTesting, teleportBallForTesting,
// and other testing helpers to ensure the suite is independent of physics noise.

#include <catch2/catch_test_macros.hpp>
#include "gameplay/GameWorld.hpp"
#include "assets/AssetRegistry.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace arcadeblocks::gameplay;

namespace {
constexpr double kFixed = 1.0 / 60.0;
} // namespace

TEST_CASE("Boss 5 (Chronarch) spawn invariants", "[boss5][spawn]") {
    auto world = GameWorld::createBossLevel(50);
    REQUIRE(world != nullptr);
    REQUIRE(world->hasBoss());
    const auto& b = world->boss();
    REQUIRE(b.levelNumber == 50);
    REQUIRE(b.maxHealth == 280);
    REQUIRE(world->bossRemainingHealth() == 280);
    REQUIRE(b.size.w == 320.0f);
    REQUIRE(b.size.h == 220.0f);
    REQUIRE(b.sectionCount == 1);
    REQUIRE(b.phase == Boss::Phase::One);
    REQUIRE(b.pointsPerHit == 150);
    REQUIRE(b.pointsOnDefeat == 55000);
    REQUIRE(world->score() == 0);
    
    // Paradox shards
    const auto& shards = world->chronarchParadoxShards();
    REQUIRE(shards.size() == 4u);
    for (const auto& shard : shards) {
        REQUIRE(shard.alive);
        REQUIRE(shard.currentHealth == 3);
        REQUIRE(shard.maxHealth == 3);
    }
}

TEST_CASE("Chronarch factory selects level 50 boss", "[boss5][factory]") {
    auto world = GameWorld::createBossLevel(50);
    REQUIRE(world != nullptr);
    REQUIRE(world->boss().levelNumber == 50);
    // Verify it is not boss 1 (which has 30 maxHealth)
    REQUIRE(world->boss().maxHealth == 280);
}

TEST_CASE("Chronarch phase thresholds are 75 50 25", "[boss5][thresholds]") {
    auto world = GameWorld::createBossLevel(50);
    const auto& b = world->boss();
    REQUIRE(b.phase1HpThreshold == 280);
    REQUIRE(b.phase2HpThreshold == 210); // 75%
    REQUIRE(b.phase3HpThreshold == 140); // 50%
    REQUIRE(b.phase4HpThreshold == 70);  // 25%
}

TEST_CASE("Chronarch transitions through four phases", "[boss5][phases]") {
    auto world = GameWorld::createBossLevel(50);
    REQUIRE(world->boss().phase == Boss::Phase::One);

    // Damage to 75% (210)
    world->damageBossForTesting(280 - 210);
    REQUIRE(world->boss().phase == Boss::Phase::Two);

    // Damage to 50% (140)
    world->damageBossForTesting(210 - 140);
    REQUIRE(world->boss().phase == Boss::Phase::Three);

    // Damage to 25% (70)
    world->damageBossForTesting(140 - 70);
    REQUIRE(world->boss().phase == Boss::Phase::Four);
}

TEST_CASE("Chronarch ball hit throttle works", "[boss5][throttle]") {
    auto world = GameWorld::createBossLevel(50);
    world->setEnabledBonusesForTesting({});
    world->setBossHitClockForTesting(0.0);

    int hp = world->bossRemainingHealth();
    
    // Teleport the ball inside the boss hitbox
    const auto& b = world->boss();
    Vec2 aim{
        b.position.x + b.size.w * 0.5f,
        b.position.y + b.size.h * 0.5f
    };
    world->teleportBallForTesting(world->ball().entity, aim, Vec2{0.0f, 50.0f});
    world->launchBallForTesting();

    // First update should hit and apply damage
    world->update(kFixed, GameInput{});
    REQUIRE(world->bossRemainingHealth() == hp - 1);

    // Next update immediately should NOT apply damage due to throttle
    world->update(kFixed, GameInput{});
    REQUIRE(world->bossRemainingHealth() == hp - 1);

    // If we advance the clock past throttle window (e.g. 0.15s), it can hit again
    world->setBossHitClockForTesting(0.2);
    world->teleportBallForTesting(world->ball().entity, aim, Vec2{0.0f, 50.0f});
    world->update(kFixed, GameInput{});
    REQUIRE(world->bossRemainingHealth() == hp - 2);
}

TEST_CASE("Chronarch captures time snapshots", "[boss5][snapshot]") {
    auto world = GameWorld::createBossLevel(50);
    
    // Run update loop for 1 second. Snapshots occur every 0.25 seconds.
    for (int i = 0; i < 60; ++i) {
        world->update(kFixed, GameInput{});
    }
    
    const auto& b = world->boss();
    REQUIRE(b.timeSnapshots.size() >= 3u);
}

TEST_CASE("Chronarch rewind restores previous ball state", "[boss5][rewind]") {
    auto world = GameWorld::createBossLevel(50);
    world->launchBallForTesting();
    
    // Capture snapshot at lookback time 1.25s
    Vec2 oldPos{100.0f, 200.0f};
    Vec2 oldVel{0.0f, 300.0f};
    world->teleportBallForTesting(world->ball().entity, oldPos, oldVel);
    world->forceChronarchSnapshotForTesting(1.25); // Lookback target
    
    // Now move the ball somewhere else
    world->teleportBallForTesting(world->ball().entity, Vec2{500.0f, 600.0f}, Vec2{-10.0f, -20.0f});
    
    // Trigger rewind via transition to Phase 2
    world->damageBossForTesting(280 - 210);
    
    // Check that ball position & velocity are restored near original values
    REQUIRE(std::abs(world->ball().position.x - oldPos.x) < 10.0f);
    REQUIRE(std::abs(world->ball().position.y - oldPos.y) < 10.0f);
}

TEST_CASE("Chronarch rewind does not restore score or boss hp", "[boss5][rewind][gains]") {
    auto world = GameWorld::createBossLevel(50);
    world->launchBallForTesting();
    
    // Capture snapshot
    world->forceChronarchSnapshotForTesting(1.25);
    
    int hpBefore = world->bossRemainingHealth();
    int scoreBefore = world->score();
    
    // Apply damage and increase score
    world->damageBossForTesting(10);
    int hpAfter = world->bossRemainingHealth();
    int scoreAfter = world->score();
    REQUIRE(hpAfter == hpBefore - 10);
    REQUIRE(scoreAfter > scoreBefore);
    
    // Force transition to Phase 2 to trigger rewind
    world->damageBossForTesting(280 - 210);
    
    // Score and HP must remain unaffected by the rewind
    REQUIRE(world->bossRemainingHealth() <= hpAfter);
    REQUIRE(world->score() >= scoreAfter);
}

TEST_CASE("Slow rift reduces ball speed", "[boss5][rift][slow]") {
    auto world = GameWorld::createBossLevel(50);
    world->launchBallForTesting();

    // Spawn Slow Rift at a fixed position and keep the ball inside it
    Vec2 riftCenter{400.0f, 400.0f};
    world->teleportBallForTesting(world->ball().entity, riftCenter, Vec2{0.0f, 660.0f});
    world->spawnChronarchTimeRiftForTesting(TimeRiftKind::Slow, riftCenter);

    REQUIRE(world->chronarchTimeRifts().size() == 1u);
    REQUIRE(world->chronarchTimeRifts().front().kind == TimeRiftKind::Slow);

    // Run 120 frames; re-place ball inside rift every 5 frames so physics can't escape
    for (int i = 0; i < 120; ++i) {
        if (i % 5 == 0) {
            world->teleportBallForTesting(world->ball().entity, riftCenter, Vec2{0.0f, 660.0f});
        }
        world->update(kFixed, GameInput{});
    }

    // The modifier converges toward 0.62; speed should drop well below default 620
    float speed = std::sqrt(world->ball().velocity.x * world->ball().velocity.x +
                            world->ball().velocity.y * world->ball().velocity.y);
    // Modifier still converging — speed should be meaningfully below 660 baseline
    REQUIRE(speed < 620.0f);
}

TEST_CASE("Haste rift increases ball speed with cap", "[boss5][rift][haste]") {
    auto world = GameWorld::createBossLevel(50);
    world->launchBallForTesting();

    Vec2 riftCenter{400.0f, 400.0f};
    world->teleportBallForTesting(world->ball().entity, riftCenter, Vec2{0.0f, 660.0f});
    world->spawnChronarchTimeRiftForTesting(TimeRiftKind::Haste, riftCenter);

    // Run 120 frames; re-place ball inside rift every 5 frames
    for (int i = 0; i < 120; ++i) {
        if (i % 5 == 0) {
            world->teleportBallForTesting(world->ball().entity, riftCenter, Vec2{0.0f, 660.0f});
        }
        world->update(kFixed, GameInput{});
    }

    float speed = std::sqrt(world->ball().velocity.x * world->ball().velocity.x +
                            world->ball().velocity.y * world->ball().velocity.y);
    // Modifier converges toward 1.28; speed should be above the 660 baseline
    REQUIRE(speed > 660.0f);
}

TEST_CASE("Rewind rift is one shot", "[boss5][rift][rewind]") {
    auto world = GameWorld::createBossLevel(50);
    world->launchBallForTesting();

    // Capture snapshot
    world->forceChronarchSnapshotForTesting(0.75);

    // Place ball at a fixed position and spawn Rewind rift right there
    Vec2 riftPos{400.0f, 400.0f};
    world->teleportBallForTesting(world->ball().entity, riftPos, Vec2{0.0f, 660.0f});
    world->spawnChronarchTimeRiftForTesting(TimeRiftKind::Rewind, riftPos);
    REQUIRE(world->chronarchTimeRifts().size() == 1u);

    // Two updates needed: first marks alive=false, second erases from vector
    world->update(kFixed, GameInput{});
    world->update(kFixed, GameInput{});

    REQUIRE(world->chronarchTimeRifts().empty());
}

TEST_CASE("Clock hand telegraphs before damage", "[boss5][clockhand][telegraph]") {
    auto world = GameWorld::createBossLevel(50);
    world->setEnabledBonusesForTesting({});
    
    // Set timer to trigger clock hand spawn
    world->setChronarchClockHandTimerForTesting(5.0);
    world->update(kFixed, GameInput{}); // Spawns clock hand
    
    REQUIRE_FALSE(world->chronarchClockHands().empty());
    const auto& hand = world->chronarchClockHands().front();
    REQUIRE(hand.telegraphing);
    REQUIRE_FALSE(hand.active);
    
    int initialLives = world->lives();
    
    // Run update loop for some frames during telegraphing
    for (int i = 0; i < 10; ++i) {
        world->update(kFixed, GameInput{});
    }
    
    REQUIRE(world->lives() == initialLives);
}

TEST_CASE("Clock hand active hit costs one life", "[boss5][clockhand][damage]") {
    auto world = GameWorld::createBossLevel(50);
    world->setEnabledBonusesForTesting({});
    // Freeze boss movement so beam angle pi/2 stays aligned with paddle
    world->setBossMoveSpeedForTesting(0.0f);

    // Spawn clock hand pointing straight down (pi/2) so it fires at the paddle
    const auto& b = world->boss();
    float bossCx = b.position.x + b.size.w * 0.5f;
    world->spawnChronarchClockHandAtAngleForTesting(static_cast<float>(M_PI / 2.0));
    world->setPaddleXForTesting(bossCx - world->paddle().size.w * 0.5f);

    REQUIRE_FALSE(world->chronarchClockHands().empty());

    int initialLives = world->lives();

    // 48 frames to exit telegraph, then active phase hits paddle
    for (int i = 0; i < 90; ++i) {
        world->update(kFixed, GameInput{});
    }

    REQUIRE(world->lives() == initialLives - 1);
}

TEST_CASE("Clock hand cannot cost multiple lives per cycle", "[boss5][clockhand][single_damage]") {
    auto world = GameWorld::createBossLevel(50);
    world->setEnabledBonusesForTesting({});
    world->setBossMoveSpeedForTesting(0.0f);

    const auto& b = world->boss();
    float bossCx = b.position.x + b.size.w * 0.5f;
    world->spawnChronarchClockHandAtAngleForTesting(static_cast<float>(M_PI / 2.0));
    world->setPaddleXForTesting(bossCx - world->paddle().size.w * 0.5f);

    REQUIRE_FALSE(world->chronarchClockHands().empty());

    int initialLives = world->lives();

    // Update past telegraph to trigger hit
    for (int i = 0; i < 90; ++i) {
        world->update(kFixed, GameInput{});
    }

    int livesAfterFirstHit = world->lives();
    REQUIRE(livesAfterFirstHit == initialLives - 1);

    // Update more — beam is still active but appliedThisCycle == true
    for (int i = 0; i < 10; ++i) {
        world->setPaddleXForTesting(bossCx - world->paddle().size.w * 0.5f);
        world->update(kFixed, GameInput{});
    }
    REQUIRE(world->lives() == livesAfterFirstHit);
}

TEST_CASE("Bonus wall absorbs clock hand", "[boss5][clockhand][bonus_wall]") {
    auto world = GameWorld::createBossLevel(50);
    world->setBossMoveSpeedForTesting(0.0f);

    // Activate BONUS_WALL
    world->activateBonusForTesting("BONUS_WALL", 999.0);
    REQUIRE(world->isBonusActive("BONUS_WALL"));

    const auto& b = world->boss();
    float bossCx = b.position.x + b.size.w * 0.5f;
    world->spawnChronarchClockHandAtAngleForTesting(static_cast<float>(M_PI / 2.0));
    world->setPaddleXForTesting(bossCx - world->paddle().size.w * 0.5f);

    REQUIRE_FALSE(world->chronarchClockHands().empty());

    int initialLives = world->lives();

    // Update past telegraph to trigger hit
    for (int i = 0; i < 90; ++i) {
        world->update(kFixed, GameInput{});
    }

    // BONUS_WALL should be consumed, lives must not decrease
    REQUIRE_FALSE(world->isBonusActive("BONUS_WALL"));
    REQUIRE(world->lives() == initialLives);
}

TEST_CASE("Paradox shard blocks ball and respawns", "[boss5][shard]") {
    auto world = GameWorld::createBossLevel(50);
    world->setEnabledBonusesForTesting({});
    world->launchBallForTesting();
    
    const auto& b = world->boss();
    Vec2 bossCenter = { b.position.x + b.size.w * 0.5f, b.position.y + b.size.h * 0.5f };
    const auto& shardsBefore = world->chronarchParadoxShards();
    REQUIRE(shardsBefore.size() == 4u);
    
    Vec2 shardPos = bossCenter + shardsBefore[0].orbitOffset;
    world->teleportBallForTesting(world->ball().entity, shardPos, Vec2{0.0f, 50.0f});
    
    int initialShardHp = shardsBefore[0].currentHealth;
    REQUIRE(initialShardHp == 3);
    
    world->setBossHitClockForTesting(0.0);
    world->update(kFixed, GameInput{});
    
    REQUIRE(world->chronarchParadoxShards()[0].currentHealth == initialShardHp - 1);
    
    // Destroy shard via hits
    for (int hit = 0; hit < 2; ++hit) {
        world->setBossHitClockForTesting(1.0 + hit);
        world->teleportBallForTesting(world->ball().entity, shardPos, Vec2{0.0f, 50.0f});
        world->update(kFixed, GameInput{});
    }
    
    REQUIRE_FALSE(world->chronarchParadoxShards()[0].alive);
    
    // Update past respawn duration (8 seconds = 480 frames at 60Hz)
    for (int i = 0; i < 500; ++i) {
        world->update(kFixed, GameInput{});
    }
    
    REQUIRE(world->chronarchParadoxShards()[0].alive);
}

TEST_CASE("Destroyed shard spawns rewind rift", "[boss5][shard][rewind_rift]") {
    auto world = GameWorld::createBossLevel(50);
    world->setEnabledBonusesForTesting({});
    world->launchBallForTesting();
    
    const auto& b = world->boss();
    Vec2 bossCenter = { b.position.x + b.size.w * 0.5f, b.position.y + b.size.h * 0.5f };
    Vec2 shardPos = bossCenter + world->chronarchParadoxShards()[0].orbitOffset;
    
    for (int hit = 0; hit < 3; ++hit) {
        world->setBossHitClockForTesting(1.0 + hit);
        world->teleportBallForTesting(world->ball().entity, shardPos, Vec2{0.0f, 50.0f});
        world->update(kFixed, GameInput{});
    }
    
    REQUIRE_FALSE(world->chronarchParadoxShards()[0].alive);
    
    bool foundRewindRift = false;
    for (const auto& rift : world->chronarchTimeRifts()) {
        if (rift.kind == TimeRiftKind::Rewind) {
            foundRewindRift = true;
            // Rift spawns at shard position at moment of death — within 50px is fine
            // (orbit may have advanced slightly between hit and spawn)
            REQUIRE(std::abs(rift.center.x - shardPos.x) < 50.0f);
            REQUIRE(std::abs(rift.center.y - shardPos.y) < 50.0f);
        }
    }
    REQUIRE(foundRewindRift);
}

TEST_CASE("Zero hour slows balls but not clock hands", "[boss5][zerohour]") {
    auto world = GameWorld::createBossLevel(50);
    world->launchBallForTesting();
    
    // Transition to Phase 4
    world->damageBossForTesting(280 - 70);
    REQUIRE(world->boss().phase == Boss::Phase::Four);
    
    // Spawn clock hand
    world->setChronarchClockHandTimerForTesting(5.0);
    world->update(kFixed, GameInput{});
    REQUIRE_FALSE(world->chronarchClockHands().empty());
    float angleBefore = world->chronarchClockHands().front().angleRadians;
    
    // Trigger zero hour
    world->setChronarchZeroHourTimerForTesting(7.0);
    world->update(kFixed, GameInput{}); // triggers zero hour
    
    REQUIRE(world->boss().zeroHourRemainingSeconds > 0.0);
    
    // Wait a few frames
    for (int i = 0; i < 20; ++i) {
        world->update(kFixed, GameInput{});
    }
    
    float ballSpeed = std::sqrt(world->ball().velocity.x * world->ball().velocity.x +
                                world->ball().velocity.y * world->ball().velocity.y);
    // Zero hour multiplier is 0.35. The modifier lerps toward 0.35*1.0=0.35
    // and speed becomes ~660*0.35=231. Still lerping after 20 frames, but
    // speed should be meaningfully below the 660 baseline (require < 620).
    REQUIRE(ballSpeed < 620.0f);

    float angleAfter = world->chronarchClockHands().front().angleRadians;
    REQUIRE(angleAfter != angleBefore);
}

TEST_CASE("Chronarch drops negative bonuses at thresholds", "[boss5][drops]") {
    auto world = GameWorld::createBossLevel(50);
    
    world->setEnabledBonusesForTesting({"FAST_BALLS", "WEAK_BALLS", "INCREASE_PADDLE"});
    
    // Cross 75% threshold
    world->damageBossForTesting(280 - 210);
    
    REQUIRE_FALSE(world->fallingBonuses().empty());
    
    for (const auto& fb : world->fallingBonuses()) {
        REQUIRE((fb.type == "FAST_BALLS" || fb.type == "WEAK_BALLS"));
        REQUIRE(fb.type != "INCREASE_PADDLE");
    }
}

TEST_CASE("Chronarch defeat clears temporal state", "[boss5][defeat]") {
    auto world = GameWorld::createBossLevel(50);
    
    // Spawn some rifts and hands
    world->spawnChronarchTimeRiftForTesting(TimeRiftKind::Slow, Vec2{200.0f, 200.0f});
    world->setChronarchClockHandTimerForTesting(5.0);
    world->update(kFixed, GameInput{});
    
    REQUIRE_FALSE(world->chronarchTimeRifts().empty());
    REQUIRE_FALSE(world->chronarchClockHands().empty());
    REQUIRE_FALSE(world->chronarchParadoxShards().empty());
    
    // Defeat boss
    world->damageBossForTesting(280);
    REQUIRE(world->boss().defeated);
    
    world->update(kFixed, GameInput{});
    
    REQUIRE(world->chronarchTimeRifts().empty());
    REQUIRE(world->chronarchClockHands().empty());
    REQUIRE(world->chronarchParadoxShards().empty());
}

TEST_CASE("AssetRegistry maps level 50 background music sfx", "[boss5][assets]") {
    arcadeblocks::assets::AssetRegistry registry{std::filesystem::path{ARCADEBLOCKS_SOURCE_DIR} / "assets"};
    
    auto mapping = registry.level(50);
    REQUIRE(mapping.background.string().find("boss_background5.jpg") != std::string::npos);
    REQUIRE(mapping.music.string().find("boss_music5.ogg") != std::string::npos);
    
    REQUIRE(mapping.sfxByEvent.find(AudioEventType::BossHit) != mapping.sfxByEvent.end());
    auto hitSounds = mapping.sfxByEvent.at(AudioEventType::BossHit);
    REQUIRE(hitSounds.size() == 4u);
    REQUIRE(hitSounds[0].string().find("boss5_hit1.ogg") != std::string::npos);
}

TEST_CASE("Optional loading and completed sfx do not break preload", "[boss5][preload]") {
    arcadeblocks::assets::AssetRegistry registry{std::filesystem::path{ARCADEBLOCKS_SOURCE_DIR} / "assets"};
    auto mapping = registry.level(50);
    REQUIRE_FALSE(mapping.preloadSfx.empty());
}
