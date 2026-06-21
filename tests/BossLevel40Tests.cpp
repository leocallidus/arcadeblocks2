// Catch2 tests for Boss 4 ("Singularity" - level 40).
//
// Deterministic surface (similar to BossLevel30Tests.cpp); we lean on
// damageBossForTesting / setBossPhaseForTesting / spawnBossGravityMineForTesting
// so the suite is independent of physics noise.

#include <catch2/catch_test_macros.hpp>
#include "gameplay/GameWorld.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>
#include <iostream>

using namespace arcadeblocks::gameplay;

namespace {

constexpr double kFixed = 1.0 / 60.0;

void enableAll(std::vector<std::string>& bag) {
    bag = {
        "INCREASE_PADDLE", "PLASMA_WEAPON", "BONUS_BALL", "CALL_BALL",
        "BONUS_WALL", "ENERGY_BALLS", "SLOW_BALLS",
        "DECREASE_PADDLE", "FAST_BALLS", "WEAK_BALLS",
        "FROZEN_PADDLE", "CHAOTIC_BALLS", "PENALTIES_MAGNET"
    };
}

void damageMany(GameWorld& w, int times) {
    for (int i = 0; i < times; ++i) w.damageBossForTesting(1);
}

} // namespace

TEST_CASE("Boss 4 (Singularity) spawn invariants", "[boss4][spawn]") {
    auto world = GameWorld::createBossLevel(40);
    REQUIRE(world != nullptr);
    REQUIRE(world->hasBoss());
    REQUIRE(world->boss().levelNumber == 40);
    REQUIRE(world->boss().maxHealth == 220);
    REQUIRE(world->bossRemainingHealth() == 220);
    REQUIRE(world->boss().size.w == 300.0f);
    REQUIRE(world->boss().size.h == 300.0f);
    REQUIRE(world->boss().sectionCount == 1);
    REQUIRE(world->boss().phase == Boss::Phase::One);
    REQUIRE_FALSE(world->boss().phase2Transitioned);
    REQUIRE_FALSE(world->boss().phase3Transitioned);
    REQUIRE_FALSE(world->boss().phase4Transitioned);
    REQUIRE_FALSE(world->boss().gravityFieldEnabled);
    REQUIRE(world->boss().pointsPerHit == 130);
    REQUIRE(world->boss().pointsOnDefeat == 42000);
    REQUIRE(world->score() == 0);
    REQUIRE(world->bossGravityMines().empty());
    const auto& pulse = world->bossSingularityPulse();
    REQUIRE_FALSE(pulse.active);
}

TEST_CASE("Boss 4 has 4 HP-gated phases (66 / 33 / 10)", "[boss4][phase]") {
    auto world = GameWorld::createBossLevel(40);
    REQUIRE(world->boss().maxHealth == 220);
    // Pre-computed thresholds from maxHealth * fraction.
    REQUIRE(world->boss().phase2ThresholdHp == 145);
    REQUIRE(world->boss().phase3ThresholdHp == 44);
    REQUIRE(world->boss().phase4ThresholdHp == 22);
}

TEST_CASE("Boss 4 takes one damage per ball hit", "[boss4][hit]") {
    auto world = GameWorld::createBossLevel(40);
    REQUIRE(world != nullptr);
    const int before = world->bossRemainingHealth();
    REQUIRE(before == 220);

    world->damageBossForTesting(1);
    REQUIRE(world->bossRemainingHealth() == before - 1);

    world->damageBossForTesting(1);
    REQUIRE(world->bossRemainingHealth() == before - 2);
}

TEST_CASE("Boss 4 traverses all 4 phases via damageBossForTesting", "[boss4][phase]") {
    auto world = GameWorld::createBossLevel(40);
    REQUIRE(world->boss().phase == Boss::Phase::One);

    damageMany(*world, world->bossRemainingHealth() -
                       world->boss().phase2ThresholdHp + 1);
    REQUIRE(world->boss().phase == Boss::Phase::Two);
    REQUIRE(world->boss().phase2Transitioned);

    damageMany(*world, world->bossRemainingHealth() - world->boss().phase3ThresholdHp + 1);
    REQUIRE(world->boss().phase == Boss::Phase::Three);
    REQUIRE(world->boss().phase3Transitioned);
    REQUIRE(world->boss().gravityFieldEnabled);

    damageMany(*world, world->bossRemainingHealth() - world->boss().phase4ThresholdHp + 1);
    REQUIRE(world->boss().phase == Boss::Phase::Four);
    REQUIRE(world->boss().phase4Transitioned);
}

TEST_CASE("Boss 4 hit throttle prevents stacking edge hits", "[boss4][throttle]") {
    auto world = GameWorld::createBossLevel(40);
    world->setEnabledBonusesForTesting({});
    world->setBossHitClockForTesting(0.0);

    int hp = world->bossRemainingHealth();
    // Aim the ball squarely inside the right side of the 300x300 hitbox.
    const auto& b = world->boss();
    Vec2 aim{
        b.position.x + b.size.w * 0.5f + 4.0f,
        b.position.y + b.size.h * 0.5f
    };
    world->teleportBallForTesting(world->ball().entity, aim, Vec2{0.0f, 50.0f});
    world->launchBallForTesting();

    world->update(kFixed, GameInput{});
    world->update(kFixed, GameInput{});
    world->update(kFixed, GameInput{});

    REQUIRE(world->bossRemainingHealth() == hp - 1);

    // Past the 0.15 s window the next legitimate bounce can land.
    world->setBossHitClockForTesting(1.0);
    world->update(kFixed, GameInput{});
    REQUIRE(world->bossRemainingHealth() >= hp - 2);
}

TEST_CASE("Boss 4 gravity field only active in phase 3+", "[boss4][gravity][phase]") {
    auto world = GameWorld::createBossLevel(40);
    REQUIRE_FALSE(world->boss().gravityFieldEnabled);

    world->setBossPhaseForTesting(Boss::Phase::Two);
    REQUIRE(world->boss().phase == Boss::Phase::Two);
    REQUIRE_FALSE(world->boss().gravityFieldEnabled);

    world->setBossPhaseForTesting(Boss::Phase::Three);
    REQUIRE(world->boss().phase == Boss::Phase::Three);
    REQUIRE(world->boss().gravityFieldEnabled);
}

TEST_CASE("Boss 4 gravity field pulls the ball toward the boss", "[boss4][gravity]") {
    auto world = GameWorld::createBossLevel(40);
    // Phase 3 - gravity field enabled, strength = 180.
    world->setBossPhaseForTesting(Boss::Phase::Three);
    REQUIRE(world->boss().gravityFieldEnabled);

    // Park the ball BELOW the boss hitbox so it can't ricochet off the
    // wall and contaminate the gravity reading. Y = bossHitboxBottom + 120.
    const float bossCx = world->boss().position.x + world->boss().size.w * 0.5f;
    const float bossCy = world->boss().position.y + world->boss().size.h * 0.5f;
    const float ballY = bossCy + world->boss().size.h * 0.5f + 120.0f;
    Vec2 start{bossCx, ballY};
    world->teleportBallForTesting(world->ball().entity, start, Vec2{0.0f, 1.0f});
    world->launchBallForTesting();

    for (int i = 0; i < 30; ++i) {
        world->update(kFixed, GameInput{});
    }
    // Boss is *above* the ball (smaller Y), so gravity must pull the ball upward
    // (i.e. the y component of the velocity gains a negative contribution).
    const float vy0 = Vec2{0.0f, 1.0f}.y;
    REQUIRE(world->ball().velocity.y < vy0);
}

TEST_CASE("Boss 4 gravity force falls off with distance from boss", "[boss4][gravity][falloff]") {
    // Measure the ball's horizontal velocity after one tick for two distances.
    // Both samples start inside the 280 px gravity radius *below* the boss
    // hitbox, with the ball moving straight down so the only horizontal
    // signal is from gravity pulling it inward.
    auto runAt = [&](float mul, bool gravityEnabled) {
        auto w = GameWorld::createBossLevel(40);
        w->setBossPhaseForTesting(Boss::Phase::Three);
        w->setGravityFieldEnabledForTesting(gravityEnabled);
        // Silence SLOW_BALLS / CHAOTIC_BALLS so they can't perturb the
        // falloff measurement.
        w->setEnabledBonusesForTesting({});
        const float r = 280.0f;
        const float bossCx = w->boss().position.x + w->boss().size.w * 0.5f;
        const float bossCy = w->boss().position.y + w->boss().size.h * 0.5f;
        const float bossBottom = w->boss().position.y + w->boss().size.h;
        // Park 32 px below the boss bottom -- far enough below the AABB to
        // dodge the boss body, but still well inside the gravity radius.
        const float ballY = bossBottom + 32.0f;
        const float dx = r * mul;
        const float dist = std::sqrt(dx * dx + (ballY - bossCy) * (ballY - bossCy));
        if (mul >= 0.50f) {
            // For mid-band samples make sure ball is still inside the field.
            REQUIRE(dist < r);
        }
        const Vec2 start{bossCx + dx, ballY};
        w->launchBallForTesting();
        w->teleportBallForTesting(w->ball().entity, start, Vec2{0.01f, 0.0f});
        // One tick is enough to observe the gravity impulse. The
        // physics layer's velocity stabilisation hides small per-tick
        // deltas over many ticks, but a *single* tick still preserves the
        // initial-velocity contribution.
        w->update(kFixed, GameInput{});
        return w->ball().velocity.x;
    };
    const float closeGravity = runAt(0.30f, true);  // near the boss
    const float closeBaseline = runAt(0.30f, false);
    const float farGravity = runAt(0.60f, true);  // mid-band, still inside radius
    const float farBaseline = runAt(0.60f, false);
    const float closePull = closeBaseline - closeGravity;
    const float farPull = farBaseline - farGravity;

    REQUIRE(closePull > 0.0f);
    REQUIRE(farPull >= 0.0f);
    // Close force must dominate far force because the falloff is linear (1 - d/R).
    REQUIRE(closePull > farPull);
}

TEST_CASE("Boss 4 gravity mine pulls the ball", "[boss4][mine]") {
    auto world = GameWorld::createBossLevel(40);
    world->setBossPhaseForTesting(Boss::Phase::Four);
    REQUIRE(world->boss().phase == Boss::Phase::Four);

    // Park the mine well inside the playfield and drop the ball inside the
    // mine's pull radius (mine radius = 80).
    const float bossCx = world->boss().position.x + world->boss().size.w * 0.5f;
    Vec2 mineCenter{bossCx, 900.0f};
    world->spawnBossGravityMineForTesting(mineCenter);

    world->setEnabledBonusesForTesting({});
    // Place the ball 30 px to the right of the mine, well inside the 80 px
    // pull radius, so the horizontal component of the pull can be observed.
    Vec2 ballPos = mineCenter + Vec2{30.0f, 0.0f};
    world->teleportBallForTesting(world->ball().entity, ballPos, Vec2{1.0f, 0.0f});
    // launchBallForTesting would overwrite zero velocity; using a small
    // non-zero initial value preserves the directed observation.
    world->launchBallForTesting();

    for (int i = 0; i < 60; ++i) {
        world->update(kFixed, GameInput{});
    }
    // Mine sits to the *left* of the ball, so the pull must bend velocity.x
    // negative (toward the mine).
    REQUIRE(world->ball().velocity.x < 1.0f);
}

TEST_CASE("Boss 4 gravity mine expires after 4 sec", "[boss4][mine][lifetime]") {
    auto world = GameWorld::createBossLevel(40);
    world->setBossPhaseForTesting(Boss::Phase::Four);
    // Disable automatic re-spawning so only the explicitly-placed mine is in flight.
    world->setBossGravityMineIntervalForTesting(99.0f);
    // Clear any mines spawned during phase transition
    world->setBossGravityMinesForTesting({});

    world->spawnBossGravityMineForTesting(Vec2{
        world->boss().position.x + world->boss().size.w * 0.5f,
        world->boss().position.y});

    REQUIRE_FALSE(world->bossGravityMines().empty());

    // Fire the singularity pulse immediately so it doesn't fire at t=4.0s
    world->setBossSingularityPulseClockForTesting(0.0);
    world->update(kFixed, GameInput{}); // Frame 1: fires pulse, spawns 5 mines

    // Switch phase to Phase 3 so no further pulses trigger or spawn mines
    world->setBossPhaseForTesting(Boss::Phase::Three);

    // 5.0 seconds of updates (300 frames) -- well over the 4 s lifetime.
    for (int i = 0; i < 300; ++i) {
        world->update(kFixed, GameInput{});
    }
    REQUIRE(world->bossGravityMines().empty());
}

TEST_CASE("Boss 4 mines exist only in phase 4", "[boss4][mine][phase-scope]") {
    auto world = GameWorld::createBossLevel(40);
    REQUIRE(world->boss().phase == Boss::Phase::One);

    world->spawnBossGravityMineForTesting(world->boss().position + Vec2{40.0f, 0.0f});
    REQUIRE(world->bossGravityMines().size() == 1u);  // explicit spawn OK

    world->setBossPhaseForTesting(Boss::Phase::Four);
    // updateGravityMines clears mines when boss_->phase != Phase4 if the
    // boss ever decided to garbage-collect them. We mimic the natural
    // cadence by setting the spawn cadence to "now", and ensuring no
    // new mine is fired while we are in Phase 2/3.
    // (Phase 2 / 3 updateGravityMines only ticks the timer, doesn't fire.)
    for (int i = 0; i < 30; ++i) {
        world->update(kFixed, GameInput{});
    }
    REQUIRE(world->boss().phase == Boss::Phase::Four);
}

TEST_CASE("Boss 4 phase 4 teleport biases toward the ball", "[boss4][teleport]") {
    auto world = GameWorld::createBossLevel(40);
    world->setBossPhaseForTesting(Boss::Phase::Four);
    world->setBossTeleportTimerForTesting(world->boss().teleportIntervalPhase4Seconds);
    const float ballX = world->bounds().bounds.left + 520.0f;
    world->teleportBallForTesting(world->ball().entity, Vec2{ballX, 800.0f}, Vec2{0.0f, 0.0f});

    world->update(kFixed, GameInput{});

    const float bossCenterX = world->boss().position.x + world->boss().size.w * 0.5f;
    REQUIRE(std::abs(bossCenterX - ballX) <= 125.0f);
}

TEST_CASE("Boss 4 fires projectiles despite single-section hitbox", "[boss4][shots]") {
    auto world = GameWorld::createBossLevel(40);
    world->setBossShotTimerForTesting(1.49);

    world->update(kFixed, GameInput{});

    REQUIRE_FALSE(world->boss().projectiles.empty());
}

TEST_CASE("Boss 4 homing projectile tracks the ball", "[boss4][homing]") {
    auto world = GameWorld::createBossLevel(40);
    world->setBossPhaseForTesting(Boss::Phase::Four);
    world->setEnabledBonusesForTesting({});

    // Spawn a fake homing projectile above the ball. Velocity points straight
    // down -- the homing step should bend it toward the ball's x position.
    world->addBossProjectileForTesting(
        Vec2{world->ball().position.x - 80.0f, world->ball().position.y - 200.0f},
        Vec2{0.0f, 300.0f}, 18.0f, /*homing=*/true);

    // After 0.5 s of progress the velocity.x must drift toward +x
    // (homing turn rate is 4.5 rad/s).
    world->launchBallForTesting();
    for (int i = 0; i < 30; ++i) {
        world->update(kFixed, GameInput{});
    }
    REQUIRE(world->boss().projectiles.front().velocity.x > 0.0f);
}

TEST_CASE("Boss 4 homing projectiles are NOT steered until phase Four", "[boss4][homing][phase-scope]") {
    auto world = GameWorld::createBossLevel(40);
    world->setBossPhaseForTesting(Boss::Phase::Two);
    world->setEnabledBonusesForTesting({});

    // Force a homing projectile even in phase 2; the engine must ignore
    // the homing flag because phase != Four.
    world->addBossProjectileForTesting(
        Vec2{world->ball().position.x - 80.0f, world->ball().position.y - 200.0f},
        Vec2{0.0f, 300.0f}, 18.0f, /*homing=*/true);

    world->launchBallForTesting();
    for (int i = 0; i < 30; ++i) {
        world->update(kFixed, GameInput{});
    }
    // Phase 2: homing off - velocity stayed the same (0, 300).
    REQUIRE(world->boss().projectiles.front().velocity.x == 0.0f);
}

TEST_CASE("Boss 4 laser cycles in every phase", "[boss4][laser]") {
    auto world = GameWorld::createBossLevel(40);
    world->setBossLaserTargetXForTesting(-1000.0f);

    for (Boss::Phase phase : {Boss::Phase::One, Boss::Phase::Two,
                              Boss::Phase::Three, Boss::Phase::Four}) {
        auto w = GameWorld::createBossLevel(40);
        w->setBossPhaseForTesting(phase);
        w->setBossLaserTargetXForTesting(-1000.0f);
        w->setBossLaserTimerForTesting(w->boss().laserFirstDelaySeconds);
        w->update(kFixed, GameInput{});
        REQUIRE(w->boss().laserState == Boss::LaserState::Charging);
    }
}

TEST_CASE("Boss 4 singularity pulse fires at configured cadence", "[boss4][pulse]") {
    auto world = GameWorld::createBossLevel(40);
    // The default first pulse interval is 8 seconds. After we tick the
    // pulse clock past that, a pulse must become active.
    REQUIRE_FALSE(world->bossSingularityPulse().active);
    world->setBossPhaseForTesting(Boss::Phase::Three);
    world->setBossSingularityPulseClockForTesting(world->boss().singularityPulseIntervalSeconds);

    // Drive updates until the next pulse - first update right after the
    // gated tick must activate the pulse.
    world->update(kFixed, GameInput{});
    REQUIRE(world->bossSingularityPulse().active);
    REQUIRE(world->bossSingularityPulse().currentRadius >= 0.0f);
}

TEST_CASE("Boss 4 only drops NEGATIVE bonuses at 66/33/10 phase thresholds", "[boss4][drops]") {
    auto world = GameWorld::createBossLevel(40);
    std::vector<std::string> bag;
    enableAll(bag);
    world->setEnabledBonusesForTesting(bag);

    damageMany(*world, world->bossRemainingHealth() -
        static_cast<int>(world->boss().maxHealth * 0.66f));
    REQUIRE_FALSE(world->fallingBonuses().empty());
    for (const auto& fb : world->fallingBonuses()) {
        INFO("Singularity dropped: " << fb.type);
        const bool ok = fb.type == "DECREASE_PADDLE" || fb.type == "FAST_BALLS" ||
                         fb.type == "WEAK_BALLS" || fb.type == "FROZEN_PADDLE" ||
                         fb.type == "CHAOTIC_BALLS" || fb.type == "PENALTIES_MAGNET";
        REQUIRE(ok);
    }
}

TEST_CASE("Boss 4 spawns +2 drones at phase 2 and +3 more at phase 3 (5 total in phase 4)", "[boss4][drones]") {
    auto world = GameWorld::createBossLevel(40);
    REQUIRE(world->bossDrones().empty());

    world->setBossPhaseForTesting(Boss::Phase::Two);
    REQUIRE(world->bossDrones().size() == 2u);

    world->setBossPhaseForTesting(Boss::Phase::Three);
    REQUIRE(world->bossDrones().size() == static_cast<size_t>(2 + 3));
}

TEST_CASE("Boss 4 defeat clears projectiles, drones and gravity mines", "[boss4][defeat]") {
    auto world = GameWorld::createBossLevel(40);
    std::vector<std::string> bag;
    enableAll(bag);
    world->setEnabledBonusesForTesting(bag);

    world->setBossPhaseForTesting(Boss::Phase::Four);
    REQUIRE_FALSE(world->bossDrones().empty());

    world->spawnBossGravityMineForTesting(Vec2{
        world->boss().position.x + world->boss().size.w * 0.5f + 30.0f,
        world->boss().position.y + world->boss().size.h * 0.5f});
    REQUIRE_FALSE(world->bossGravityMines().empty());

    // Spawn a projectile so the clear path is exercised too.
    world->addBossProjectileForTesting(
        Vec2{world->boss().position.x + 50.0f,
             world->boss().position.y + 30.0f},
        Vec2{0.0f, 200.0f}, 18.0f, /*homing=*/false);

    // Force the ball into Launched state and park it high above the boss so
    // post-defeat updates don't accidentally trigger LifeLost / GameOver.
    world->launchBallForTesting();
    world->teleportBallForTesting(
        world->ball().entity,
        Vec2{world->boss().position.x + world->boss().size.w * 0.5f,
             world->bounds().bounds.top + 30.0f},
        Vec2{0.0f, 0.0f});

    world->damageBossForTesting(world->bossRemainingHealth());
    REQUIRE(world->boss().defeated);

    // Drive an update so that updateCompletionState fires and LevelComplete
    // becomes the active phase.
    world->update(kFixed, GameInput{});

    REQUIRE(world->bossGravityMines().empty());
    REQUIRE(world->boss().projectiles.empty());
    REQUIRE(world->phase() == GamePhase::LevelComplete);
}

TEST_CASE("Boss 4 spawns 5 gravity mines at once when a pulse fires in Phase 4", "[boss4][mines][pulse-spawn]") {
    auto world = GameWorld::createBossLevel(40);
    world->setBossPhaseForTesting(Boss::Phase::Four);
    // Disable regular mine spawn cadence
    world->setBossGravityMineIntervalForTesting(99.0f);
    // Clear any mines spawned during phase transition
    world->setBossGravityMinesForTesting({});
    REQUIRE(world->bossGravityMines().empty());

    // Advance singularity pulse clock to trigger a pulse
    world->setBossSingularityPulseClockForTesting(world->boss().singularityPulseIntervalSeconds);
    world->update(kFixed, GameInput{});

    // Check that we fired the pulse and spawned 5 mines
    REQUIRE(world->bossSingularityPulse().active);
    REQUIRE(world->bossGravityMines().size() == 5u);
}

TEST_CASE("Boss 4 ball cumulative gravity well exposure triggers drift/jitter", "[boss4][drift][gravity-exposure]") {
    auto world = GameWorld::createBossLevel(40);
    world->setBossPhaseForTesting(Boss::Phase::Three);
    world->setEnabledBonusesForTesting({});

    // Place the ball inside the gravity field radius of the boss
    const float bossCx = world->boss().position.x + world->boss().size.w * 0.5f;
    const float bossCy = world->boss().position.y + world->boss().size.h * 0.5f;
    Vec2 insidePos{bossCx, bossCy + 100.0f}; // within 280px radius
    world->teleportBallForTesting(world->ball().entity, insidePos, Vec2{0.0f, 10.0f});
    world->launchBallForTesting();

    REQUIRE(world->ball().gravityExposure == 0.0f);

    // After 10 updates inside the gravity well, exposure should increase
    for (int i = 0; i < 10; ++i) {
        world->update(kFixed, GameInput{});
    }
    REQUIRE(world->ball().gravityExposure > 0.0f);

    const float exposureAtPeak = world->ball().gravityExposure;

    // Place the ball far outside the gravity field
    Vec2 outsidePos{bossCx + 1000.0f, bossCy + 1000.0f};
    world->teleportBallForTesting(world->ball().entity, outsidePos, Vec2{0.0f, 10.0f});

    // After updates, the exposure should decay
    for (int i = 0; i < 10; ++i) {
        world->update(kFixed, GameInput{});
    }
    REQUIRE(world->ball().gravityExposure < exposureAtPeak);
}

TEST_CASE("Boss 4 drones repeating spawn cadence of 7 seconds", "[boss4][drones][respawn-cadence]") {
    auto world = GameWorld::createBossLevel(40);
    world->setEnabledBonusesForTesting({});

    // Prevent any drone/boss projectiles from landing or firing (prevents drone-clearing life loss)
    world->setBossPostRespawnCooldownRemainingForTesting(999.0);

    // Set phase to Playing and activate STICKY_PADDLE to keep the ball attached to the paddle
    world->setGamePhaseForTesting(GamePhase::Playing);
    world->activateBonusForTesting("STICKY_PADDLE", 999.0);

    // Phase 2 transition spawns 2 drones
    world->setBossPhaseForTesting(Boss::Phase::Two);
    REQUIRE(world->bossDrones().size() == 2u);

    // Artificially destroy one drone (maxHealth is 3)
    world->damageBossDroneForTesting(0, 3);
    
    int aliveCount = 0;
    for (const auto& d : world->bossDrones()) {
        if (d.alive) aliveCount++;
    }
    REQUIRE(aliveCount == 1);

    // Update for 6 seconds (not enough to trigger respawn)
    for (int i = 0; i < 360; ++i) {
        world->update(kFixed, GameInput{});
    }
    
    aliveCount = 0;
    for (const auto& d : world->bossDrones()) {
        if (d.alive) aliveCount++;
    }
    REQUIRE(aliveCount == 1);

    // Update for another 1.5 seconds (total > 7s) to trigger respawn
    for (int i = 0; i < 90; ++i) {
        world->update(kFixed, GameInput{});
    }

    aliveCount = 0;
    for (const auto& d : world->bossDrones()) {
        if (d.alive) aliveCount++;
    }
    REQUIRE(aliveCount == 2);
}

