#include "gameplay/GameWorld.hpp"

#include "physics/PhysicsWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>
#include <iostream>

namespace arcadeblocks::gameplay {
namespace {
constexpr Bounds logicalBounds{0.0f, 0.0f, 1920.0f, 1080.0f};
constexpr float brickStageWidth = 1200.0f;
constexpr float brickStageLeft = 360.0f;
constexpr float paddleY = 930.0f;
constexpr float ballLaunchSpeed = 620.0f;
constexpr double respawnLaunchDelaySeconds = 0.9;
constexpr double callBallDurationSeconds = 50.0;
constexpr double slowBallsDurationSeconds = 20.0;
constexpr double fastBallsDurationSeconds = 15.0;
constexpr double scoreRainDurationSeconds = 20.0;
constexpr double weakBallsDurationSeconds = 15.0;
constexpr double energyBallsDurationSeconds = 5.0;
constexpr double explosionBallsDurationSeconds = 5.0;
constexpr double increasePaddleDurationSeconds = 30.0;
constexpr double decreasePaddleDurationSeconds = 20.0;
constexpr double stickyPaddleDurationSeconds = 20.0;
constexpr double frozenPaddleDurationSeconds = 3.0;
constexpr double invisiblePaddleDurationSeconds = 5.0;
constexpr double bonusWallDurationSeconds = 10.0;
constexpr double darknessDurationSeconds = 15.0;
constexpr double chaoticBallsDurationSeconds = 15.0;
constexpr double bonusMagnetDurationSeconds = 20.0;
constexpr double penaltiesMagnetDurationSeconds = 20.0;
constexpr double rainbowBountyDurationSeconds = 15.0;
constexpr double bloodTitheDurationSeconds = 15.0;
constexpr double bonusTimerFadeOutSeconds = 0.75;
constexpr float callBallPullSpeed = 840.0f;
constexpr float explosiveBrickRadius = 160.0f;
constexpr int maxExplosionTargets = 3;
// Per-ball throttle so a ball grazed against the boss side wall cannot chain
// hits back-to-back across consecutive physics ticks. We pick a generous
// 0.15 s window - the same as the visible hit-flash - so legitimate bounces
// inside the body still register while grazing misses are coalesced.
constexpr double bossHitCooldownSeconds = 0.15;

} // namespace

bool isPositiveBonus(const std::string& type) {
    if (type == "CHAOTIC_BALLS" || type == "FROZEN_PADDLE" || type == "DECREASE_PADDLE" ||
        type == "FAST_BALLS" || type == "PENALTIES_MAGNET" || type == "WEAK_BALLS" ||
        type == "INVISIBLE_PADDLE" || type == "DARKNESS" || type == "BAD_LUCK" ||
        type == "RESET" || type == "BLOOD_TITHE") {
        return false;
    }
    return true;
}

void ScoreSystem::add(int points) {
    score += std::max(0, points);
}

bool ScoreSystem::spend(int points) {
    const int cost = std::max(0, points);
    if (score < cost) {
        return false;
    }
    score -= cost;
    return true;
}

bool LivesSystem::loseLife() {
    if (lives > 0) {
        --lives;
    }
    return lives > 0;
}

void LivesSystem::restore(int value) {
    lives = std::max(0, value);
}

GameWorld GameWorld::fromLevel(const levels::LevelDefinition& level) {
    GameWorld world;
    world.spawnBounds();
    world.spawnPaddle();
    world.spawnBall();
    world.spawnBricks(level);
    world.attachBallToPaddle();
    world.physics_ = std::make_unique<physics::PhysicsWorld>(world);
    return world;
}

std::unique_ptr<GameWorld> GameWorld::createBossLevel(int levelNumber) {
    auto world = std::make_unique<GameWorld>();
    world->spawnBounds();
    world->spawnPaddle();
    world->spawnBall();
    world->attachBallToPaddle();
    
    switch (levelNumber) {
        case 10:
            world->initBossLevelOne();
            break;
        case 20:
            world->initBossLevelTwo_Kaira();
            break;
        case 30:
            world->initBossLevelThree_Helios();
            break;
        case 40:
            world->initBossLevelFour_Singularity();
            break;
        case 50:
            world->initBossLevelFive_Chronarch();
            break;
        default:
            world->initBossLevelOne();
            break;
    }

    world->physics_ = std::make_unique<physics::PhysicsWorld>(*world);
    return world;
}

void GameWorld::initBossLevelOne() {
    boss_ = std::make_unique<Boss>();
    boss_->levelNumber = 10;
    boss_->maxHealth = 30;
    boss_->currentHealth = 30;
    boss_->moveAmplitude = 360.0f;
    boss_->moveSpeed = 220.0f;
    boss_->pointsPerHit = 50;
    boss_->pointsOnDefeat = 5000;
    boss_->entity = spawn(EntityKind::Boss);
    boss_->size = Size{360.0f, 172.0f};
    boss_->position = Vec2{
        brickStageLeft + brickStageWidth * 0.5f - boss_->size.w * 0.5f,
        140.0f
    };
    boss_->sections.clear();
    boss_->sections.push_back(BossSection{.entity = spawn(EntityKind::Boss), .localBounds = {0, 0, 360, 172}});
    boss_->sectionHealth.push_back(30);
    boss_->sectionMaxHealth.push_back(30);
    boss_->sectionCount = 1;
    boss_->baseY = 140.0f;
}

void GameWorld::initBossLevelTwo_Kaira() {
    boss_ = std::make_unique<Boss>();
    boss_->levelNumber = 20;
    boss_->size = Size{420.0f, 110.0f};
    boss_->sectionMaxHealth = {25, 30, 25};
    boss_->sectionHealth = {25, 30, 25};
    boss_->sectionCount = 3;
    boss_->maxHealth = 80;
    boss_->currentHealth = 80;
    boss_->pointsPerHit = 75;
    boss_->pointsOnDefeat = 12000;
    boss_->moveSpeed = 280.0f;
    boss_->moveAmplitude = 380.0f;
    boss_->edgePauseSeconds = 0.5;
    boss_->shotIntervalSeconds = 3.5;
    boss_->projectileSpeed = 380.0f;
    boss_->shieldCycleSeconds = 12.0;
    boss_->shieldDurationSeconds = 2.5;
    boss_->shieldCooldownSeconds = 9.5;
    boss_->diveIntervalSeconds = 6.0;
    boss_->diveDurationSeconds = 1.0;
    boss_->diveDepthPx = 200.0f;
    boss_->baseY = 170.0f;
    
    boss_->position = Vec2{
        brickStageLeft + brickStageWidth * 0.5f - boss_->size.w * 0.5f,
        170.0f
    };
    boss_->entity = spawn(EntityKind::Boss);
    boss_->sections.clear();
    boss_->sections.push_back(BossSection{.entity = spawn(EntityKind::Boss), .localBounds = {0, 0, 140, 110}});
    boss_->sections.push_back(BossSection{.entity = spawn(EntityKind::Boss), .localBounds = {140, 0, 280, 110}});
    boss_->sections.push_back(BossSection{.entity = spawn(EntityKind::Boss), .localBounds = {280, 0, 420, 110}});
}

void GameWorld::initBossLevelThree_Helios() {
    boss_ = std::make_unique<Boss>();
    boss_->levelNumber = 30;
    // Portrait body that matches the 764x1024 aspect ratio of boss3_core.png
    // (ratio ~0.75). Drawing a tall sprite onto a short rect would squash
    // the AI into an unrecognisable pancake, so the hitbox is tall now.
    boss_->size = Size{220.0f, 300.0f};
    boss_->baseY = 110.0f;
    boss_->entity = spawn(EntityKind::Boss);

    boss_->maxHealth = 150;
    boss_->currentHealth = 150;
    boss_->pointsPerHit = 100;
    boss_->pointsOnDefeat = 25000;
    boss_->pointsPerDrone = 500;

    boss_->moveAmplitude = 380.0f;
    boss_->moveSpeed = 300.0f;
    boss_->edgePauseSeconds = 0.4;

    boss_->sectionCount = 1;
    boss_->sections.clear();
    boss_->sections.push_back(BossSection{.entity = spawn(EntityKind::Boss), .localBounds = {0, 0, 220, 300}});
    boss_->sectionHealth = {150};
    boss_->sectionMaxHealth = {150};

    boss_->phase2ThresholdFraction = 0.5f;
    boss_->phase2ThresholdHp = static_cast<int>(boss_->maxHealth * boss_->phase2ThresholdFraction);

    // Phase 1 telegraph tunables
    boss_->teleportIntervalSeconds = 5.5;
    boss_->teleportIntervalPhase2Seconds = 3.6;
    boss_->doubleTeleportFirstDelaySeconds = 2.0;
    boss_->invulnOnTeleportSeconds = 0.5;

    boss_->laserIntervalSeconds = 4.0;
    boss_->laserIntervalPhase2Seconds = 2.4;
    boss_->laserChargeSeconds = 1.2;
    boss_->laserFiringSeconds = 0.35;
    boss_->laserCooldownSeconds = 0.6;
    boss_->laserFirstDelaySeconds = 2.5;
    boss_->laserWidthPhaseOne = 28.0f;
    boss_->laserWidthPhaseTwo = 44.0f;
    boss_->laserWidth = boss_->laserWidthPhaseOne;

    boss_->droneShotIntervalSeconds = 2.8;

    boss_->position = Vec2{
        brickStageLeft + brickStageWidth * 0.5f - boss_->size.w * 0.5f,
        boss_->baseY
    };
    boss_->prevHealthAtHitForDropCheck = boss_->currentHealth;
}

GameWorld::~GameWorld() = default;

GameWorld::GameWorld(GameWorld&&) noexcept = default;

GameWorld& GameWorld::operator=(GameWorld&&) noexcept = default;

EntityId GameWorld::spawn(EntityKind kind) {
    const auto id = nextEntityId_++;
    entities_.push_back(Entity{.id = id, .kind = kind, .alive = true});
    return id;
}

void GameWorld::despawn(EntityId id) {
    for (auto& entity : entities_) {
        if (entity.id == id) {
            entity.alive = false;
            break;
        }
    }
}

void GameWorld::update(double fixedDeltaSeconds, const GameInput& input, const std::vector<std::string>& enabledBonuses) {
    enabledBonuses_ = enabledBonuses;

    // The throttle clock for boss hits advances on every tick that actually
    // advances the simulation. Tests can override the clock value via
    // setBossHitClockForTesting to drive the throttle deterministically.
    if (phase_ != GamePhase::Paused) {
        bossHitClockSeconds_ += fixedDeltaSeconds;
        if (boss_ && boss_->levelNumber == 40) {
            boss_->singularityPulseClockSeconds += fixedDeltaSeconds;
        }
    }

    if (input.toggleDebugDrawPressed) {
        setPhysicsDebugDrawEnabled(!physicsDebugDrawEnabled_);
    }

    if (input.pausePressed) {
        if (phase_ == GamePhase::Paused) {
            resumeFromPause();
        } else {
            requestPause();
        }
    }

    if (phase_ == GamePhase::LevelComplete || phase_ == GamePhase::GameOver) {
        updateBonusTimers(fixedDeltaSeconds);
        return;
    }

    if (phase_ == GamePhase::Paused) {
        return;
    }

    updatePaddle(fixedDeltaSeconds, input);
    updateBoss(fixedDeltaSeconds);
    physics_->syncPaddle(paddle_, fixedDeltaSeconds);
    physics_->setTurboBallActive(input.turboBallActive);
    
    if (paddleSpawnAge_ < 1.0) {
        paddleSpawnAge_ += fixedDeltaSeconds;
    }
    
    if (respawnLaunchDelayRemaining_ > 0.0) {
        respawnLaunchDelayRemaining_ = std::max(0.0, respawnLaunchDelayRemaining_ - fixedDeltaSeconds);
    }

    updateFallingBonuses(fixedDeltaSeconds);
    updateBonusTimers(fixedDeltaSeconds);
    if (phase_ == GamePhase::Playing && !isBonusActive("STICKY_PADDLE")) {
        releaseAttachedBalls();
    }

    if (input.callBallPressed && isBonusActive("CALL_BALL")) {
        callBallsToPaddle();
    }

    if (phase_ == GamePhase::Playing && input.shootPlasmaPressed && plasmaAmmo_ > 0 && !isBonusActive("INVISIBLE_PADDLE") && !isBonusActive("FROZEN_PADDLE")) {
        plasmaAmmo_--;
        const float bulletSpeed = -900.0f;
        PlasmaBullet b1{
            .entity = spawn(EntityKind::Bonus),
            .position = Vec2{paddle_.position.x + 10.0f, paddle_.position.y - 32.0f},
            .velocity = Vec2{0.0f, bulletSpeed},
            .size = Size{16.0f, 32.0f},
            .alive = true
        };
        PlasmaBullet b2{
            .entity = spawn(EntityKind::Bonus),
            .position = Vec2{paddle_.position.x + paddle_.size.w - 10.0f - 16.0f, paddle_.position.y - 32.0f},
            .velocity = Vec2{0.0f, bulletSpeed},
            .size = Size{16.0f, 32.0f},
            .alive = true
        };
        plasmaBullets_.push_back(b1);
        plasmaBullets_.push_back(b2);
        queueAudioEvent(AudioEventType::PlasmaShot, paddle_.position);
    }

    // Update plasma bullets
    for (auto& b : plasmaBullets_) {
        b.position.y += b.velocity.y * fixedDeltaSeconds;
        if (b.position.y + b.size.h < 0.0f) {
            b.alive = false;
        } else {
            for (auto& brick : bricks_) {
                if (brick.alive) {
                    bool overlap = b.position.x < brick.position.x + brick.size.w &&
                                   b.position.x + b.size.w > brick.position.x &&
                                   b.position.y < brick.position.y + brick.size.h &&
                                   b.position.y + b.size.h > brick.position.y;
                    if (overlap) {
                        b.alive = false;
                        applyBrickDamage(brick.entity, 9999, false);
                        queueAudioEvent(AudioEventType::PlasmaBrickHit, brick.position, brick.size, std::string{levels::toString(brick.color)});
                        break;
                    }
                }
            }
        }
    }

    plasmaBullets_.erase(
        std::remove_if(plasmaBullets_.begin(), plasmaBullets_.end(),
                       [](const PlasmaBullet& b) { return !b.alive; }),
        plasmaBullets_.end());

    if (!levelPassBonusSpawned_ && phase_ == GamePhase::Playing) {
        bool isLevelPassEnabled = false;
        for (const auto& id : enabledBonuses_) {
            if (id == "LEVEL_PASS") {
                isLevelPassEnabled = true;
                break;
            }
        }
        if (isLevelPassEnabled) {
            int active = activeBrickCount();
            if (active >= 3 && active <= 10) {
                levelPassBonusSpawned_ = true;
                float centerX = (bounds_.bounds.left + bounds_.bounds.right) * 0.5f;
                float spawnY = bounds_.bounds.top + 80.0f;
                spawnBonus("LEVEL_PASS", Vec2{centerX, spawnY});
            }
        }
    }

    if (ball_.state == BallState::AttachedToPaddle && phase_ != GamePhase::Playing) {
        attachBallToPaddle();
        physics_->syncAttachedBall(ball_);
        if (respawnLaunchDelayRemaining_ <= 0.0) {
            if (!isBonusActive("FROZEN_PADDLE")) {
                autoLaunchCountdown_ -= fixedDeltaSeconds;
                if (input.launchPressed || autoLaunchCountdown_ <= 0.0) {
                    launchBall();
                }
            }
        }
        return;
    }

    if (phase_ == GamePhase::Playing && input.launchPressed && !isBonusActive("FROZEN_PADDLE")) {
        releaseAttachedBalls();
    }

    // Position attached balls relative to the paddle
    if (ball_.state == BallState::AttachedToPaddle) {
        ball_.velocity = Vec2{};
        ball_.position = Vec2{
            paddle_.position.x + paddle_.size.w * 0.5f + ball_.attachOffsetX,
            paddle_.position.y - ball_.radius - 2.0f
        };
        if (physics_) {
            physics_->syncAttachedBall(ball_);
        }
    }
    for (auto& eb : extraBalls_) {
        if (eb.state == BallState::AttachedToPaddle) {
            eb.velocity = Vec2{};
            eb.position = Vec2{
                paddle_.position.x + paddle_.size.w * 0.5f + eb.attachOffsetX,
                paddle_.position.y - eb.radius - 2.0f
            };
            if (physics_) {
                physics_->syncAttachedBall(eb);
            }
        }
    }

    // Interpolate ball radius dynamically based on WEAK_BALLS bonus state
    const float normalRadius = 12.0f;
    const float weakRadius = 6.0f;
    const float targetRadius = isBonusActive("WEAK_BALLS") ? weakRadius : normalRadius;
    const float transitionSpeed = 12.0f; // change of 6 units takes 0.5s

    auto updateRadius = [&](float& currentRadius, double dt) {
        if (currentRadius < targetRadius) {
            currentRadius = std::min(targetRadius, currentRadius + static_cast<float>(transitionSpeed * dt));
        } else if (currentRadius > targetRadius) {
            currentRadius = std::max(targetRadius, currentRadius - static_cast<float>(transitionSpeed * dt));
        }
    };

    updateRadius(ball_.radius, fixedDeltaSeconds);
    for (auto& eb : extraBalls_) {
        updateRadius(eb.radius, fixedDeltaSeconds);
    }

    float speedMultiplier = 1.0f;
    if (isBonusActive("SLOW_BALLS")) {
        speedMultiplier *= 0.945f;
    }
    if (isBonusActive("FAST_BALLS")) {
        speedMultiplier *= 2.025f;
    }

    int attachedCount = 0;
    if (ball_.state == BallState::AttachedToPaddle) {
        attachedCount++;
    }
    for (const auto& eb : extraBalls_) {
        if (eb.state == BallState::AttachedToPaddle) {
            attachedCount++;
        }
    }

    const auto physicsResult = physics_->step(
        fixedDeltaSeconds,
        ball_,
        extraBalls_,
        speedMultiplier,
        isBonusActive("ENERGY_BALLS"),
        isBonusActive("STICKY_PADDLE"),
        attachedCount,
        isBonusActive("BONUS_WALL"),
        isBonusActive("CHAOTIC_BALLS"),
        boss_ ? boss_->ballSpeedMultipliers : std::unordered_map<EntityId, float>{}
    );

    // Process balls that hit the paddle and should attach
    for (const auto ballEntity : physicsResult.paddleHits) {
        float paddleCenter = paddle_.position.x + paddle_.size.w * 0.5f;
        if (ball_.entity == ballEntity && ball_.state == BallState::Launched) {
            ball_.state = BallState::AttachedToPaddle;
            ball_.velocity = Vec2{};
            ball_.attachOffsetX = std::clamp(ball_.position.x - paddleCenter, -paddle_.size.w * 0.5f, paddle_.size.w * 0.5f);
            ball_.position = Vec2{
                paddleCenter + ball_.attachOffsetX,
                paddle_.position.y - ball_.radius - 2.0f
            };
            if (physics_) {
                physics_->syncAttachedBall(ball_);
            }
        }
        for (auto& eb : extraBalls_) {
            if (eb.entity == ballEntity && eb.state == BallState::Launched) {
                eb.state = BallState::AttachedToPaddle;
                eb.velocity = Vec2{};
                eb.attachOffsetX = std::clamp(eb.position.x - paddleCenter, -paddle_.size.w * 0.5f, paddle_.size.w * 0.5f);
                eb.position = Vec2{
                    paddleCenter + eb.attachOffsetX,
                    paddle_.position.y - eb.radius - 2.0f
                };
                if (physics_) {
                    physics_->syncAttachedBall(eb);
                }
            }
        }
    }

    if (physicsResult.hitPaddle) {
        queueAudioEvent(AudioEventType::PaddleHit, ball_.position);
    }
    if (physicsResult.hitWall) {
        queueAudioEvent(AudioEventType::WallHit, ball_.position);
    }
    for (const auto brickEntity : physicsResult.brickHits) {
        applyBrickHit(brickEntity);
    }
    if (boss_ && boss_->levelNumber == 40) {
        updateGravityField(fixedDeltaSeconds);
        updateBallDriftFromGravity(fixedDeltaSeconds);
    }
    applyBallBossHitIfAny();

    for (auto it = extraBalls_.begin(); it != extraBalls_.end(); ) {
        if (it->position.y - it->radius > 1080.0f) {
            physics_->removeBall(it->entity);
            despawn(it->entity);
            it = extraBalls_.erase(it);
        } else {
            ++it;
        }
    }

    if (physicsResult.belowBottom) {
        if (!extraBalls_.empty()) {
            physics_->removeBall(ball_.entity);
            despawn(ball_.entity);

            ball_ = extraBalls_.front();
            ball_.state = BallState::Launched;
            extraBalls_.erase(extraBalls_.begin());
        } else {
            handleLifeLost();
        }
    }

    applyBallDroneHitIfAny();

    updateCompletionState();
}

void GameWorld::setPaddleSpeed(float speed) {
    paddle_.speed = std::clamp(speed, 200.0f, 2000.0f);
}

void GameWorld::setTurboBallSpeed(float speed) {
    if (physics_) {
        physics_->setTurboBallSpeed(std::clamp(speed, 620.0f, 12000.0f));
    }
}

bool GameWorld::continueAfterGameOver(int cost, int restoredLives) {
    if (phase_ != GamePhase::GameOver || restoredLives <= 0 || !score_.spend(cost)) {
        return false;
    }

    lives_.restore(restoredLives);
    phase_ = GamePhase::LifeLost;
    respawnLaunchDelayRemaining_ = respawnLaunchDelaySeconds;
    autoLaunchCountdown_ = 5.0;
    attachBallToPaddle();
    physics_->syncAttachedBall(ball_);
    return true;
}

const std::vector<Entity>& GameWorld::entities() const noexcept {
    return entities_;
}

const Paddle& GameWorld::paddle() const noexcept {
    return paddle_;
}

const Ball& GameWorld::ball() const noexcept {
    return ball_;
}

const std::vector<Brick>& GameWorld::bricks() const noexcept {
    return bricks_;
}

const Wall& GameWorld::wall() const noexcept {
    return wall_;
}

const LevelBounds& GameWorld::bounds() const noexcept {
    return bounds_;
}

const std::vector<FallingBonus>& GameWorld::fallingBonuses() const noexcept {
    return fallingBonuses_;
}

const std::vector<Ball>& GameWorld::extraBalls() const noexcept {
    return extraBalls_;
}

const std::vector<ActiveBonusTimer>& GameWorld::activeBonusTimers() const noexcept {
    return activeBonusTimers_;
}

const std::vector<PlasmaBullet>& GameWorld::plasmaBullets() const noexcept {
    return plasmaBullets_;
}

int GameWorld::plasmaAmmo() const noexcept {
    return plasmaAmmo_;
}

GamePhase GameWorld::phase() const noexcept {
    return phase_;
}

int GameWorld::score() const noexcept {
    return score_.score;
}

int GameWorld::lives() const noexcept {
    return lives_.lives;
}

int GameWorld::activeBrickCount() const noexcept {
    return static_cast<int>(std::count_if(bricks_.begin(), bricks_.end(), [](const Brick& brick) {
        return brick.alive;
    }));
}

bool GameWorld::physicsDebugDrawEnabled() const noexcept {
    return physicsDebugDrawEnabled_;
}

double GameWorld::respawnLaunchDelayRemaining() const noexcept {
    return respawnLaunchDelayRemaining_;
}

double GameWorld::paddleSpawnAge() const noexcept {
    return paddleSpawnAge_;
}

std::vector<AudioEvent> GameWorld::consumeAudioEvents() {
    auto result = std::move(audioEvents_);
    audioEvents_.clear();
    return result;
}

void GameWorld::requestPause() {
    if (phase_ == GamePhase::Ready || phase_ == GamePhase::Playing || phase_ == GamePhase::LifeLost) {
        phaseBeforePause_ = phase_;
        phase_ = GamePhase::Paused;
    }
}

void GameWorld::resumeFromPause() {
    if (phase_ == GamePhase::Paused) {
        phase_ = phaseBeforePause_;
    }
}

void GameWorld::spawnPaddle() {
    paddleSpawnAge_ = 0.0;
    respawnLaunchDelayRemaining_ = 1.0; // 1 second animation delay before launch
    paddle_ = Paddle{
        .entity = spawn(EntityKind::Paddle),
        .position = Vec2{760.0f, paddleY},
        .size = Size{240.0f, 22.0f},
        .speed = 760.0f,
    };
    queueAudioEvent(AudioEventType::PaddleSpawn, paddle_.position);
}

void GameWorld::spawnBall() {
    autoLaunchCountdown_ = 5.0;
    ball_ = Ball{
        .entity = spawn(EntityKind::Ball),
        .position = Vec2{},
        .velocity = Vec2{},
        .radius = 12.0f,
        .state = BallState::AttachedToPaddle,
    };
}

void GameWorld::spawnBounds() {
    wall_ = Wall{
        .entity = spawn(EntityKind::Wall),
        .bounds = logicalBounds,
    };
    bounds_ = LevelBounds{
        .entity = spawn(EntityKind::LevelBounds),
        .bounds = logicalBounds,
    };
}

void GameWorld::spawnBricks(const levels::LevelDefinition& level) {
    const auto& layout = level.layout;
    const float sourceGridWidth = static_cast<float>(
        layout.brickColumns * layout.brickWidth + std::max(0, layout.brickColumns - 1) * layout.brickSpacing);
    const float scale = sourceGridWidth > 0.0f ? brickStageWidth / sourceGridWidth : 1.0f;
    const float brickWidth = static_cast<float>(layout.brickWidth) * scale;
    const float brickHeight = static_cast<float>(layout.brickHeight) * scale;
    const float spacing = static_cast<float>(layout.brickSpacing) * scale;

    bricks_.reserve(level.bricks.size());
    for (const auto& source : level.bricks) {
        bricks_.push_back(Brick{
            .entity = spawn(EntityKind::Brick),
            .position = Vec2{
                brickStageLeft + static_cast<float>(source.col) * (brickWidth + spacing),
                static_cast<float>(layout.startY) + static_cast<float>(source.row) * (brickHeight + spacing),
            },
            .size = Size{brickWidth, brickHeight},
            .color = source.color,
            .health = source.health,
            .maxHealth = std::max(1, source.health),
            .points = source.points,
            .alive = true,
        });
    }
}

void GameWorld::attachBallToPaddle() {
    ball_.state = BallState::AttachedToPaddle;
    ball_.velocity = Vec2{};
    ball_.position = Vec2{
        paddle_.position.x + paddle_.size.w * 0.5f,
        paddle_.position.y - ball_.radius - 2.0f,
    };
}

void GameWorld::launchBall() {
    ball_.state = BallState::Launched;
    ball_.velocity = Vec2{220.0f, -ballLaunchSpeed};
    respawnLaunchDelayRemaining_ = 0.0;
    physics_->syncAttachedBall(ball_);
    physics_->launchBall(ball_.velocity);
    queueAudioEvent(AudioEventType::BallLaunch, ball_.position);
    phase_ = GamePhase::Playing;
}

void GameWorld::releaseAttachedBalls() {
    bool launchedAny = false;
    if (ball_.state == BallState::AttachedToPaddle) {
        ball_.state = BallState::Launched;
        ball_.velocity = Vec2{220.0f, -ballLaunchSpeed};
        if (physics_) {
            physics_->syncAttachedBall(ball_);
            physics_->setBallVelocity(ball_.entity, ball_.velocity);
        }
        launchedAny = true;
    }
    float spread = -150.0f;
    for (auto& eb : extraBalls_) {
        if (eb.state == BallState::AttachedToPaddle) {
            eb.state = BallState::Launched;
            eb.velocity = Vec2{spread, -ballLaunchSpeed};
            spread += 100.0f;
            if (physics_) {
                physics_->syncAttachedBall(eb);
                physics_->setBallVelocity(eb.entity, eb.velocity);
            }
            launchedAny = true;
        }
    }
    if (launchedAny) {
        queueAudioEvent(AudioEventType::BallLaunch, paddle_.position);
    }
}

void GameWorld::updatePaddle(double dt, const GameInput& input) {
    // Calculate target width based on active bonuses
    float increaseMultiplier = 1.0f;
    float decreaseMultiplier = 1.0f;
    for (const auto& timer : activeBonusTimers_) {
        if (timer.type == "INCREASE_PADDLE" && timer.remainingSeconds > 0.0) {
            increaseMultiplier = 1.5f + (timer.stacks - 1) * 0.1f;
            increaseMultiplier = std::min(2.0f, increaseMultiplier);
        } else if (timer.type == "DECREASE_PADDLE" && timer.remainingSeconds > 0.0) {
            decreaseMultiplier = 0.6f;
        }
    }
    float targetWidth = 240.0f * increaseMultiplier * decreaseMultiplier;
    float oldWidth = paddle_.size.w;
    
    if (paddle_.size.w != targetWidth) {
        const float paddleWidthTransitionSpeed = 240.0f; // px per second
        if (paddle_.size.w < targetWidth) {
            paddle_.size.w = std::min(targetWidth, paddle_.size.w + static_cast<float>(paddleWidthTransitionSpeed * dt));
        } else {
            paddle_.size.w = std::max(targetWidth, paddle_.size.w - static_cast<float>(paddleWidthTransitionSpeed * dt));
        }
        float diff = paddle_.size.w - oldWidth;
        paddle_.position.x -= diff * 0.5f;
    }

    if (!isBonusActive("FROZEN_PADDLE")) {
        float direction = 0.0f;
        if (input.moveLeft) {
            direction -= 1.0f;
        }
        if (input.moveRight) {
            direction += 1.0f;
        }

        if (direction == 0.0f && input.mousePaddleActive) {
            float targetX = input.mousePaddleCenterX - paddle_.size.w * 0.5f;
            if (boss_ && boss_->levelNumber == 50 && boss_->zeroHourRemainingSeconds > 0.0) {
                float maxDiff = paddle_.speed * 0.72f * static_cast<float>(dt);
                float diff = targetX - paddle_.position.x;
                if (std::abs(diff) > maxDiff) {
                    targetX = paddle_.position.x + (diff > 0.0f ? maxDiff : -maxDiff);
                }
            }
            paddle_.position.x = targetX;
        } else {
            float effectiveSpeed = paddle_.speed;
            if (boss_ && boss_->levelNumber == 50 && boss_->zeroHourRemainingSeconds > 0.0) {
                effectiveSpeed *= 0.72f;
            }
            paddle_.position.x += direction * effectiveSpeed * static_cast<float>(dt);
        }
    }
    paddle_.position.x = std::clamp(
        paddle_.position.x,
        bounds_.bounds.left,
        bounds_.bounds.right - paddle_.size.w);
}

void GameWorld::clearBossAttackState() {
    if (!boss_) return;
    // Drop any projectiles still falling toward the paddle so they cannot hurt
    // the freshly respawned player. The laser goes back to idle and drones are
    // dismissed; the boss will re-spawn drones when its cooldown elapses
    // (see enterBossPhase2 path; respawn doesn't change phase).
    boss_->projectiles.clear();
    boss_->laserState = Boss::LaserState::Idle;
    boss_->laserTimerSeconds = 0.0;
    boss_->laserStateTimer = 0.0;
    boss_->laserAlpha = 0.0f;
    boss_->invulnTimeRemaining = 0.0;
    // We deliberately do NOT reset phase2Transitioned / phase3Transitioned /
    // phase4Transitioned here: their lifetime is the boss. Once you have
    // entered a phase, you stay in it for the rest of the fight even if the
    // paddle dies (this is tested in Boss 3 / Singularity tests).
    boss_->drones.clear();
    // Boss 4 (Singularity): also dismiss mines and any active pulse.
    if (boss_->levelNumber == 40) {
        boss_->gravityMines.clear();
        boss_->singularityPulse = SingularityPulse{};
        boss_->singularityPulseNextFireSeconds = boss_->singularityPulseIntervalSeconds;
    }
    if (boss_->levelNumber == 50) {
        clearChronarchState();
    }
}

void GameWorld::handleLifeLost() {
    expireActiveBonusTimers();
    for (auto& bonus : fallingBonuses_) {
        if (bonus.alive && bonus.fadeOutRemainingSeconds <= 0.0) {
            bonus.fadeOutRemainingSeconds = bonusTimerFadeOutSeconds;
        }
    }
    // Give the boss a brief post-respawn pause: no new projectiles, no laser
    // and no in-flight shots can land on the just-respawned paddle.
    bossAttackCooldownSeconds_ = std::max(
        bossAttackCooldownSeconds_,
        respawnLaunchDelaySeconds + 1.5);
    clearBossAttackState();
    if (lives_.loseLife()) {
        queueAudioEvent(AudioEventType::LifeLost, ball_.position);
        phase_ = GamePhase::LifeLost;
        respawnLaunchDelayRemaining_ = respawnLaunchDelaySeconds;
        autoLaunchCountdown_ = 5.0;
        attachBallToPaddle();
        physics_->syncAttachedBall(ball_);
    } else {
        queueAudioEvent(AudioEventType::GameOver, ball_.position);
        phase_ = GamePhase::GameOver;
        respawnLaunchDelayRemaining_ = 0.0;
        ball_.state = BallState::AttachedToPaddle;
        ball_.velocity = Vec2{};
        physics_->syncAttachedBall(ball_);
    }
}

void GameWorld::updateCompletionState() {
    if (boss_) {
        if (boss_->defeated && phase_ != GamePhase::LevelComplete) {
            expireActiveBonusTimers();
            phase_ = GamePhase::LevelComplete;
            ball_.velocity = Vec2{};
            if (boss_) {
                boss_->projectiles.clear();
                boss_->drones.clear();
                boss_->laserState = Boss::LaserState::Idle;
                boss_->laserAlpha = 0.0f;
                boss_->invulnTimeRemaining = 0.0;
            }
            queueAudioEvent(AudioEventType::LevelComplete, ball_.position);
        }
        return;
    }
    if (activeBrickCount() == 0) {
        expireActiveBonusTimers();
        phase_ = GamePhase::LevelComplete;
        ball_.velocity = Vec2{};
        queueAudioEvent(AudioEventType::LevelComplete, ball_.position);
    }
}

void GameWorld::applyBrickHit(EntityId brickEntity) {
    int damage = 1;
    if (isBonusActive("WEAK_BALLS")) {
        damage = 0;
    }
    if (isBonusActive("ENERGY_BALLS")) {
        damage = 9999;
    }
    if (isBonusActive("EXPLOSION_BALLS")) {
        for (const auto& brick : bricks_) {
            if (brick.alive && brick.entity == brickEntity) {
                explodeNearbyBricks(brick);
                break;
            }
        }
    }
    applyBrickDamage(brickEntity, damage, true);
}

void GameWorld::applyBrickDamage(EntityId brickEntity, int damage, bool playImpactSound) {
    for (auto& brick : bricks_) {
        if (!brick.alive || brick.entity != brickEntity) {
            continue;
        }

        brick.health -= std::max(0, damage);
        if (brick.health <= 0) {
            brick.alive = false;
            despawn(brick.entity);
            int pointsEarned = brick.points;
            if (isBonusActive("SCORE_RAIN")) {
                pointsEarned += 1000;
            }
            score_.add(pointsEarned);
            physics_->removeBrick(brick.entity);
            queueAudioEvent(AudioEventType::BrickBreak, brick.position, brick.size, std::string{levels::toString(brick.color)});

            if (brick.color == levels::BrickColor::Explosive && !isBonusActive("ENERGY_BALLS")) {
                if (!isBonusActive("EXPLOSION_BALLS")) {
                    explodeNearbyBricks(brick);
                }
            }

            Vec2 spawnPos{
                brick.position.x + brick.size.w * 0.5f,
                brick.position.y + brick.size.h * 0.5f
            };

            thread_local std::mt19937 generator(std::random_device{}());
            std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
            if (isBonusActive("BLOOD_TITHE")) {
                auto guaranteed = pickBloodTitheDrop(enabledBonuses_);
                if (guaranteed) {
                    spawnBonus(*guaranteed, spawnPos);
                }
            } else if (isBonusActive("RAINBOW_BOUNTY")) {
                auto guaranteed = pickRainbowBountyDrop(enabledBonuses_);
                if (guaranteed) {
                    spawnBonus(*guaranteed, spawnPos);
                }
            } else {
                bool bloodTitheAllowed = !hasBoss() && isBonusEnabled(enabledBonuses_, "BLOOD_TITHE");
                bool rainbowBountyAllowed = !hasBoss() && isBonusEnabled(enabledBonuses_, "RAINBOW_BOUNTY");
                if (bloodTitheAllowed && chanceDist(generator) < 0.01f) {
                    spawnBonus("BLOOD_TITHE", spawnPos);
                } else if (rainbowBountyAllowed && chanceDist(generator) < 0.01f) {
                    spawnBonus("RAINBOW_BOUNTY", spawnPos);
                } else if (isBonusEnabled(enabledBonuses_, "BONUS_SCORE_10000") && chanceDist(generator) < 0.01f) {
                    spawnBonus("BONUS_SCORE_10000", spawnPos);
                } else {
                    std::vector<std::string> activeCandidates;
                    for (const auto& id : enabledBonuses_) {
                        if (id == "BONUS_SCORE" || id == "BONUS_SCORE_200" || id == "BONUS_SCORE_500" || id == "EXTRA_LIFE" || id == "BONUS_BALL" || id == "CALL_BALL" || id == "SLOW_BALLS" || id == "FAST_BALLS" || id == "SCORE_RAIN" || id == "WEAK_BALLS" || id == "ENERGY_BALLS" || id == "EXPLOSION_BALLS" || id == "INCREASE_PADDLE" || id == "DECREASE_PADDLE" || id == "STICKY_PADDLE" || id == "PLASMA_WEAPON" || id == "FROZEN_PADDLE" || id == "INVISIBLE_PADDLE" || id == "BONUS_WALL" || id == "DARKNESS" || id == "CHAOTIC_BALLS" || id == "BONUS_MAGNET" || id == "PENALTIES_MAGNET" || id == "BAD_LUCK" || id == "TRICKSTER" || id == "ADD_FIVE_SECONDS" || id == "RESET" || id == "RANDOM_BONUS" || id == "BLOOD_TITHE") {
                            activeCandidates.push_back(id);
                        }
                    }

                    if (!activeCandidates.empty()) {
                        if (chanceDist(generator) < 0.15f) {
                            std::uniform_int_distribution<std::size_t> selectDist(0, activeCandidates.size() - 1);
                            std::string chosenType = activeCandidates[selectDist(generator)];
                            spawnBonus(chosenType, spawnPos);
                        }
                    }
                }
            }
        } else if (playImpactSound) {
            queueAudioEvent(AudioEventType::BrickHit, brick.position, brick.size, std::string{levels::toString(brick.color)});
        }
        return;
    }
}

void GameWorld::explodeNearbyBricks(const Brick& sourceBrick) {
    const Vec2 sourceCenter{
        sourceBrick.position.x + sourceBrick.size.w * 0.5f,
        sourceBrick.position.y + sourceBrick.size.h * 0.5f,
    };
    queueAudioEvent(AudioEventType::Explosion, sourceBrick.position, sourceBrick.size, std::string{levels::toString(sourceBrick.color)});

    struct Target {
        EntityId entity = 0;
        float distance = 0.0f;
    };
    std::vector<Target> targets;
    targets.reserve(bricks_.size());

    for (const auto& brick : bricks_) {
        if (!brick.alive || brick.entity == sourceBrick.entity) {
            continue;
        }

        const Vec2 center{
            brick.position.x + brick.size.w * 0.5f,
            brick.position.y + brick.size.h * 0.5f,
        };
        const float dx = center.x - sourceCenter.x;
        const float dy = center.y - sourceCenter.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= explosiveBrickRadius) {
            targets.push_back(Target{.entity = brick.entity, .distance = distance});
        }
    }

    std::sort(targets.begin(), targets.end(), [](const Target& left, const Target& right) {
        return left.distance < right.distance;
    });

    const int count = std::min(maxExplosionTargets, static_cast<int>(targets.size()));
    for (int i = 0; i < count; ++i) {
        applyBrickDamage(targets[static_cast<std::size_t>(i)].entity, 1, false);
    }
}

void GameWorld::setPhysicsDebugDrawEnabled(bool enabled) {
    physicsDebugDrawEnabled_ = enabled;
    if (physics_) {
        physics_->setDebugDrawEnabled(enabled);
    }
}

void GameWorld::queueAudioEvent(AudioEventType type, Vec2 position, Size size, std::string detail) {
    audioEvents_.push_back(AudioEvent{.type = type, .position = position, .size = size, .detail = detail});
}

void GameWorld::demolishBricksExceptOne() {
    bool foundFirst = false;
    for (auto& brick : bricks_) {
        if (brick.alive) {
            if (!foundFirst) {
                foundFirst = true;
                continue;
            }
            brick.alive = false;
            despawn(brick.entity);
            if (physics_) {
                physics_->removeBrick(brick.entity);
            }
        }
    }
}

void GameWorld::damageBrickForTesting(EntityId entity, int damage) {
    applyBrickDamage(entity, damage, true);
    updateCompletionState();
}

void GameWorld::teleportBallForTesting(EntityId entity, Vec2 position, Vec2 velocity) {
    if (ball_.entity == entity) {
        ball_.position = position;
        ball_.velocity = velocity;
    } else {
        for (auto& eb : extraBalls_) {
            if (eb.entity == entity) {
                eb.position = position;
                eb.velocity = velocity;
                break;
            }
        }
    }
    if (physics_) {
        physics_->teleportBallForTesting(entity, position, velocity);
    }
}

void GameWorld::spawnBonus(const std::string& type, Vec2 centerPos) {
    FallingBonus bonus;
    bonus.entity = spawn(EntityKind::Bonus);
    bonus.size = Size{70.0f, 30.0f};
    bonus.position = Vec2{
        centerPos.x - bonus.size.w * 0.5f,
        centerPos.y - bonus.size.h * 0.5f
    };
    bonus.type = type;
    bonus.velocity = Vec2{0.0f, 150.0f};
    bonus.alive = true;

    fallingBonuses_.push_back(bonus);
    queueAudioEvent(AudioEventType::BonusSpawn, bonus.position, bonus.size, bonus.type);
}

void GameWorld::updateFallingBonuses(double dt) {
    for (auto& bonus : fallingBonuses_) {
        if (!bonus.alive) continue;

        if (bonus.fadeOutRemainingSeconds > 0.0) {
            bonus.fadeOutRemainingSeconds = std::max(0.0, bonus.fadeOutRemainingSeconds - dt);
            if (bonus.fadeOutRemainingSeconds <= 0.0) {
                bonus.alive = false;
                despawn(bonus.entity);
                continue;
            }
        }

        bool positive = isPositiveBonus(bonus.type);
        bool magnetActive = (positive && isBonusActive("BONUS_MAGNET")) || (!positive && isBonusActive("PENALTIES_MAGNET"));

        // Normal falling motion
        bonus.position.y += bonus.velocity.y * static_cast<float>(dt);
        bonus.age += dt;

        // Apply magnetic attraction / repulsion force
        if (bonus.fadeOutRemainingSeconds <= 0.0) {
            float radius = 0.0f;
            float force = 0.0f;
            if (positive) {
                if (magnetActive) {
                    radius = 1200.0f; // Screen-wide range
                    force = 1.8f;     // Tuned force (strong but avoidable)
                } else {
                    radius = 70.0f;
                    force = -1.5f;
                }
            } else {
                if (magnetActive) {
                    radius = 1200.0f; // Screen-wide range
                    force = 1.8f;     // Tuned force (strong but avoidable)
                } else {
                    radius = 80.0f;
                    force = 2.5f;
                }
            }

            if (radius > 0.0f) {
                Vec2 paddleCenter{
                    paddle_.position.x + paddle_.size.w * 0.5f,
                    paddle_.position.y + paddle_.size.h * 0.5f
                };
                Vec2 bonusCenter{
                    bonus.position.x + bonus.size.w * 0.5f,
                    bonus.position.y + bonus.size.h * 0.5f
                };
                Vec2 diff{ paddleCenter.x - bonusCenter.x, paddleCenter.y - bonusCenter.y };
                float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
                if (dist > 0.0f && dist < radius) {
                    float factor = 1.0f - dist / radius;
                    float multiplier = (force > 0.0f) ? 380.0f : 150.0f;
                    float dx = (diff.x / dist) * force * factor * multiplier * static_cast<float>(dt);
                    float dy = (diff.y / dist) * force * factor * multiplier * static_cast<float>(dt);
                    bonus.position.x += dx;
                    bonus.position.y += dy;
                }
            }
        }

        if (bonus.fadeOutRemainingSeconds > 0.0) {
            continue;
        }

        if (checkCollision(paddle_, bonus)) {
            bonus.alive = false;
            despawn(bonus.entity);

            score_.add(1000);

            std::string typeToApply = bonus.type;
            if (typeToApply == "RANDOM_BONUS") {
                std::vector<std::string> options;
                for (const auto& id : enabledBonuses_) {
                    if (id != "RANDOM_BONUS" && id != "LEVEL_PASS" && id != "TRICKSTER" && id != "BAD_LUCK") {
                        options.push_back(id);
                    }
                }
                if (!options.empty()) {
                    thread_local std::mt19937 generator(std::random_device{}());
                    std::uniform_int_distribution<std::size_t> dist(0, options.size() - 1);
                    typeToApply = options[dist(generator)];
                } else {
                    typeToApply = "BONUS_SCORE";
                }
                queueAudioEvent(AudioEventType::BonusPickup, bonus.position, bonus.size, "RANDOM_BONUS");
            }

            if (typeToApply == "BONUS_SCORE") {
                score_.add(1000);
            } else if (typeToApply == "BONUS_SCORE_200") {
                score_.add(200);
            } else if (typeToApply == "BONUS_SCORE_500") {
                score_.add(500);
            } else if (typeToApply == "BONUS_SCORE_10000") {
                score_.add(10000);
            } else if (typeToApply == "EXTRA_LIFE") {
                lives_.restore(lives_.lives + 1);
            } else if (typeToApply == "BONUS_BALL") {
                Ball newBall{
                    .entity = spawn(EntityKind::Ball),
                    .position = Vec2{
                        paddle_.position.x + paddle_.size.w * 0.5f,
                        paddle_.position.y - ball_.radius - 2.0f
                    },
                    .velocity = Vec2{100.0f, -550.0f},
                    .radius = ball_.radius,
                    .state = BallState::Launched
                };
                extraBalls_.push_back(newBall);
                if (physics_) {
                    physics_->addBall(newBall);
                }
            } else if (typeToApply == "CALL_BALL") {
                activateTimedBonus("CALL_BALL", callBallDurationSeconds);
            } else if (typeToApply == "SLOW_BALLS") {
                activateTimedBonus("SLOW_BALLS", slowBallsDurationSeconds);
            } else if (typeToApply == "FAST_BALLS") {
                activateTimedBonus("FAST_BALLS", fastBallsDurationSeconds);
            } else if (typeToApply == "SCORE_RAIN") {
                activateTimedBonus("SCORE_RAIN", scoreRainDurationSeconds);
            } else if (typeToApply == "WEAK_BALLS") {
                activateTimedBonus("WEAK_BALLS", weakBallsDurationSeconds);
            } else if (typeToApply == "ENERGY_BALLS") {
                activateTimedBonus("ENERGY_BALLS", energyBallsDurationSeconds);
            } else if (typeToApply == "EXPLOSION_BALLS") {
                activateTimedBonus("EXPLOSION_BALLS", explosionBallsDurationSeconds);
            } else if (typeToApply == "INCREASE_PADDLE") {
                activateTimedBonus("INCREASE_PADDLE", increasePaddleDurationSeconds);
            } else if (typeToApply == "DECREASE_PADDLE") {
                activateTimedBonus("DECREASE_PADDLE", decreasePaddleDurationSeconds);
            } else if (typeToApply == "STICKY_PADDLE") {
                activateTimedBonus("STICKY_PADDLE", stickyPaddleDurationSeconds);
            } else if (typeToApply == "FROZEN_PADDLE") {
                activateTimedBonus("FROZEN_PADDLE", frozenPaddleDurationSeconds);
            } else if (typeToApply == "INVISIBLE_PADDLE") {
                activateTimedBonus("INVISIBLE_PADDLE", invisiblePaddleDurationSeconds);
            } else if (typeToApply == "BONUS_WALL") {
                activateTimedBonus("BONUS_WALL", bonusWallDurationSeconds);
            } else if (typeToApply == "DARKNESS") {
                activateTimedBonus("DARKNESS", darknessDurationSeconds);
            } else if (typeToApply == "CHAOTIC_BALLS") {
                activateTimedBonus("CHAOTIC_BALLS", chaoticBallsDurationSeconds);
            } else if (typeToApply == "BONUS_MAGNET") {
                activateTimedBonus("BONUS_MAGNET", bonusMagnetDurationSeconds);
            } else if (typeToApply == "PENALTIES_MAGNET") {
                activateTimedBonus("PENALTIES_MAGNET", penaltiesMagnetDurationSeconds);
            } else if (typeToApply == "RAINBOW_BOUNTY") {
                activateTimedBonus("RAINBOW_BOUNTY", rainbowBountyDurationSeconds);
            } else if (typeToApply == "BLOOD_TITHE") {
                activateTimedBonus("BLOOD_TITHE", bloodTitheDurationSeconds);
            } else if (typeToApply == "BAD_LUCK") {
                std::erase_if(activeBonusTimers_, [](const ActiveBonusTimer& timer) {
                    return isPositiveBonus(timer.type);
                });
                plasmaAmmo_ = 0;

                for (auto& fb : fallingBonuses_) {
                    if (fb.alive && fb.entity != bonus.entity && isPositiveBonus(fb.type)) {
                        fb.alive = false;
                        despawn(fb.entity);
                    }
                }

                std::vector<std::pair<std::string, double>> penalties = {
                    {"CHAOTIC_BALLS", chaoticBallsDurationSeconds},
                    {"FROZEN_PADDLE", frozenPaddleDurationSeconds},
                    {"DECREASE_PADDLE", decreasePaddleDurationSeconds},
                    {"FAST_BALLS", fastBallsDurationSeconds},
                    {"PENALTIES_MAGNET", penaltiesMagnetDurationSeconds},
                    {"WEAK_BALLS", weakBallsDurationSeconds},
                    {"INVISIBLE_PADDLE", invisiblePaddleDurationSeconds},
                    {"DARKNESS", darknessDurationSeconds}
                };
                for (const auto& p : penalties) {
                    activateTimedBonus(p.first, p.second * 2.0);
                }
            } else if (typeToApply == "TRICKSTER") {
                std::erase_if(activeBonusTimers_, [](const ActiveBonusTimer& timer) {
                    return !isPositiveBonus(timer.type);
                });

                for (auto& fb : fallingBonuses_) {
                    if (fb.alive && fb.entity != bonus.entity && !isPositiveBonus(fb.type)) {
                        fb.alive = false;
                        despawn(fb.entity);
                    }
                }

                std::vector<std::pair<std::string, double>> positiveTimedBonuses = {
                    {"CALL_BALL", callBallDurationSeconds},
                    {"SLOW_BALLS", slowBallsDurationSeconds},
                    {"SCORE_RAIN", scoreRainDurationSeconds},
                    {"ENERGY_BALLS", energyBallsDurationSeconds},
                    {"EXPLOSION_BALLS", explosionBallsDurationSeconds},
                    {"INCREASE_PADDLE", increasePaddleDurationSeconds},
                    {"STICKY_PADDLE", stickyPaddleDurationSeconds},
                    {"BONUS_WALL", bonusWallDurationSeconds},
                    {"BONUS_MAGNET", bonusMagnetDurationSeconds}
                };
                for (const auto& b : positiveTimedBonuses) {
                    activateTimedBonus(b.first, b.second * 2.0);
                }

                plasmaAmmo_ = 20;
                bool foundPlasma = false;
                for (auto& timer : activeBonusTimers_) {
                    if (timer.type == "PLASMA_WEAPON") {
                        timer.durationSeconds = 20.0;
                        timer.remainingSeconds = 20.0;
                        timer.fadeOutRemainingSeconds = 0.0;
                        foundPlasma = true;
                        break;
                    }
                }
                if (!foundPlasma) {
                    activeBonusTimers_.push_back(ActiveBonusTimer{
                        .type = "PLASMA_WEAPON",
                        .durationSeconds = 20.0,
                        .remainingSeconds = 20.0,
                        .fadeOutRemainingSeconds = 0.0,
                        .stacks = 1
                    });
                }
            } else if (typeToApply == "ADD_FIVE_SECONDS") {
                for (auto& timer : activeBonusTimers_) {
                    if (timer.type == "PLASMA_WEAPON") {
                        plasmaAmmo_ += 5;
                        timer.durationSeconds = std::max(timer.durationSeconds, static_cast<double>(plasmaAmmo_));
                        timer.remainingSeconds = plasmaAmmo_;
                        timer.fadeOutRemainingSeconds = 0.0;
                        queueAudioEvent(AudioEventType::BonusPickup, bonus.position, bonus.size, "PLASMA_RECHARGE");
                    } else {
                        timer.durationSeconds += 5.0;
                        timer.remainingSeconds += 5.0;
                        timer.fadeOutRemainingSeconds = 0.0;
                    }
                }
            } else if (typeToApply == "RESET") {
                activeBonusTimers_.clear();
                plasmaAmmo_ = 0;
                plasmaBullets_.clear();
            } else if (typeToApply == "PLASMA_WEAPON") {
                if (plasmaAmmo_ > 0) {
                    plasmaAmmo_ += 10;
                    queueAudioEvent(AudioEventType::BonusPickup, bonus.position, bonus.size, "PLASMA_RECHARGE");
                } else {
                    plasmaAmmo_ = 10;
                    queueAudioEvent(AudioEventType::BonusPickup, bonus.position, bonus.size, "PLASMA_WEAPON");
                }
                
                bool found = false;
                for (auto& timer : activeBonusTimers_) {
                    if (timer.type == "PLASMA_WEAPON") {
                        timer.durationSeconds = std::max(timer.durationSeconds, static_cast<double>(plasmaAmmo_));
                        timer.remainingSeconds = plasmaAmmo_;
                        timer.fadeOutRemainingSeconds = 0.0;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    activeBonusTimers_.push_back(ActiveBonusTimer{
                        .type = "PLASMA_WEAPON",
                        .durationSeconds = 10.0,
                        .remainingSeconds = 10.0,
                        .fadeOutRemainingSeconds = 0.0,
                        .stacks = 1
                    });
                }
                bonus.alive = false;
                despawn(bonus.entity);
                continue;
            } else if (typeToApply == "LEVEL_PASS") {
                for (auto& brick : bricks_) {
                    if (brick.alive) {
                        brick.alive = false;
                        despawn(brick.entity);
                        score_.add(brick.points);
                        if (physics_) {
                            physics_->removeBrick(brick.entity);
                        }
                        queueAudioEvent(AudioEventType::BrickBreak, brick.position, brick.size, "silent," + std::string{levels::toString(brick.color)});
                    }
                }
            }

            queueAudioEvent(AudioEventType::BonusPickup, bonus.position, bonus.size, typeToApply);
            continue;
        }

        if (bonus.position.y > 1080.0f) {
            bonus.alive = false;
            despawn(bonus.entity);
        }
    }

    fallingBonuses_.erase(
        std::remove_if(fallingBonuses_.begin(), fallingBonuses_.end(),
                       [](const FallingBonus& b) { return !b.alive; }),
        fallingBonuses_.end());
}

void GameWorld::updateBonusTimers(double dt) {
    for (auto& timer : activeBonusTimers_) {
        if (timer.type == "PLASMA_WEAPON") {
            if (plasmaAmmo_ <= 0) {
                if (timer.remainingSeconds > 0.0) {
                    timer.remainingSeconds = 0.0;
                    timer.fadeOutRemainingSeconds = bonusTimerFadeOutSeconds;
                } else if (timer.fadeOutRemainingSeconds > 0.0) {
                    timer.fadeOutRemainingSeconds = std::max(0.0, timer.fadeOutRemainingSeconds - dt);
                }
            } else {
                timer.remainingSeconds = plasmaAmmo_;
            }
            continue;
        }
        if (timer.remainingSeconds > 0.0) {
            timer.remainingSeconds = std::max(0.0, timer.remainingSeconds - dt);
            if (timer.remainingSeconds <= 0.0) {
                timer.fadeOutRemainingSeconds = bonusTimerFadeOutSeconds;
            }
        } else if (timer.fadeOutRemainingSeconds > 0.0) {
            timer.fadeOutRemainingSeconds = std::max(0.0, timer.fadeOutRemainingSeconds - dt);
        }
    }

    std::erase_if(activeBonusTimers_, [](const ActiveBonusTimer& timer) {
        return timer.remainingSeconds <= 0.0 && timer.fadeOutRemainingSeconds <= 0.0;
    });
}

void GameWorld::activateTimedBonus(const std::string& type, double durationSeconds) {
    if (type == "BONUS_MAGNET") {
        std::erase_if(activeBonusTimers_, [](const ActiveBonusTimer& t) { return t.type == "PENALTIES_MAGNET"; });
    } else if (type == "PENALTIES_MAGNET") {
        std::erase_if(activeBonusTimers_, [](const ActiveBonusTimer& t) { return t.type == "BONUS_MAGNET"; });
    }

    for (auto& timer : activeBonusTimers_) {
        if (timer.type == type) {
            if (type == "PLASMA_WEAPON") {
                plasmaAmmo_ += 10;
                timer.durationSeconds = std::max(timer.durationSeconds, static_cast<double>(plasmaAmmo_));
                timer.remainingSeconds = plasmaAmmo_;
            } else if (type == "INCREASE_PADDLE") {
                timer.stacks = std::min(6, timer.stacks + 1);
                timer.durationSeconds = durationSeconds;
                timer.remainingSeconds = durationSeconds;
            } else if (type == "DECREASE_PADDLE" || type == "STICKY_PADDLE" || type == "FROZEN_PADDLE" || type == "INVISIBLE_PADDLE" || type == "BONUS_WALL" || type == "DARKNESS" || type == "CHAOTIC_BALLS" || type == "BONUS_MAGNET" || type == "PENALTIES_MAGNET") {
                timer.durationSeconds = durationSeconds;
                timer.remainingSeconds = durationSeconds;
            } else {
                timer.durationSeconds += durationSeconds;
                timer.remainingSeconds += durationSeconds;
            }
            timer.fadeOutRemainingSeconds = 0.0;
            return;
        }
    }

    if (type == "PLASMA_WEAPON") {
        plasmaAmmo_ = 10;
        activeBonusTimers_.push_back(ActiveBonusTimer{
            .type = type,
            .durationSeconds = 10.0,
            .remainingSeconds = 10.0,
            .fadeOutRemainingSeconds = 0.0,
            .stacks = 1,
        });
        return;
    }

    activeBonusTimers_.push_back(ActiveBonusTimer{
        .type = type,
        .durationSeconds = durationSeconds,
        .remainingSeconds = durationSeconds,
        .fadeOutRemainingSeconds = 0.0,
        .stacks = 1,
    });
}

void GameWorld::expireActiveBonusTimers() {
    for (auto& timer : activeBonusTimers_) {
        if (timer.remainingSeconds > 0.0) {
            timer.remainingSeconds = 0.0;
            timer.fadeOutRemainingSeconds = bonusTimerFadeOutSeconds;
        }
    }
    plasmaAmmo_ = 0;
    plasmaBullets_.clear();
}

bool GameWorld::isBonusActive(const std::string& type) const {
    return std::any_of(activeBonusTimers_.begin(), activeBonusTimers_.end(), [&type](const ActiveBonusTimer& timer) {
        if (type == "BONUS_WALL") {
            return timer.type == type && (timer.remainingSeconds > 0.0 || timer.fadeOutRemainingSeconds > 0.0);
        }
        return timer.type == type && timer.remainingSeconds > 0.0;
    });
}

void GameWorld::callBallsToPaddle() {
    bool movedAnyBall = false;
    if (ball_.state == BallState::Launched) {
        ball_.velocity = callBallVelocityFor(ball_);
        if (physics_) {
            physics_->setBallVelocity(ball_.entity, ball_.velocity);
        }
        movedAnyBall = true;
    }

    for (auto& extraBall : extraBalls_) {
        if (extraBall.state == BallState::Launched) {
            extraBall.velocity = callBallVelocityFor(extraBall);
            if (physics_) {
                physics_->setBallVelocity(extraBall.entity, extraBall.velocity);
            }
            movedAnyBall = true;
        }
    }

    if (movedAnyBall) {
        queueAudioEvent(AudioEventType::CallBallPaddle, paddle_.position, paddle_.size, "CALL_BALL");
    }
}

Vec2 GameWorld::callBallVelocityFor(const Ball& ball) const {
    const Vec2 target{
        paddle_.position.x + paddle_.size.w * 0.5f,
        paddle_.position.y - ball.radius - 18.0f,
    };
    Vec2 delta{
        target.x - ball.position.x,
        target.y - ball.position.y,
    };
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length <= 0.001f) {
        return Vec2{0.0f, -callBallPullSpeed};
    }
    return Vec2{
        (delta.x / length) * callBallPullSpeed,
        (delta.y / length) * callBallPullSpeed,
    };
}

bool GameWorld::checkCollision(const Paddle& paddle, const FallingBonus& bonus) const {
    return (paddle.position.x < bonus.position.x + bonus.size.w &&
            paddle.position.x + paddle.size.w > bonus.position.x &&
            paddle.position.y < bonus.position.y + bonus.size.h &&
            paddle.position.y + paddle.size.h > bonus.position.y);
}

bool GameWorld::hasBoss() const noexcept {
    return boss_ != nullptr;
}

const Boss& GameWorld::boss() const noexcept {
    return *boss_;
}

float GameWorld::bossHealthNormalized() const noexcept {
    if (!boss_) return 0.0f;
    return static_cast<float>(boss_->currentHealth) / static_cast<float>(boss_->maxHealth);
}

int GameWorld::bossRemainingHealth() const noexcept {
    if (!boss_) return 0;
    return boss_->currentHealth;
}

const std::vector<DroneEntity>& GameWorld::bossDrones() const noexcept {
    static const std::vector<DroneEntity> empty;
    return boss_ ? boss_->drones : empty;
}

const std::vector<GravityMine>& GameWorld::bossGravityMines() const noexcept {
    static const std::vector<GravityMine> empty;
    return boss_ ? boss_->gravityMines : empty;
}

const SingularityPulse& GameWorld::bossSingularityPulse() const noexcept {
    static const SingularityPulse empty{};
    return boss_ ? boss_->singularityPulse : empty;
}

void GameWorld::setBossShieldForTesting(bool active) {
    if (boss_) boss_->shieldActive = active;
}

void GameWorld::setBossShotTimerForTesting(double timer) {
    if (boss_) boss_->shotTimerSeconds = timer;
}

void GameWorld::setBossShieldTimerForTesting(double timer) {
    if (boss_) boss_->shieldTimerSeconds = timer;
}

void GameWorld::setBossTeleportTimerForTesting(double timer) {
    if (boss_) boss_->teleportTimerSeconds = timer;
}

void GameWorld::setBossLaserTimerForTesting(double timer) {
    if (boss_) boss_->laserTimerSeconds = timer;
}

void GameWorld::setBossLaserStateForTesting(Boss::LaserState state) {
    if (boss_) boss_->laserState = state;
}

void GameWorld::setBossLaserTargetXForTesting(float x) {
    if (boss_) boss_->laserTargetX = x;
}

void GameWorld::setBossLaserAppliedThisCycleForTesting(bool value) {
    if (boss_) boss_->laserAppliedThisCycle = value;
}

void GameWorld::setBossPhaseForTesting(Boss::Phase phase) {
    if (!boss_) return;
    // Phase transitions fire the corresponding `enterBossPhaseN()` so that
    // drones are spawned, tunables are switched, etc. Jumping straight to a
    // later phase must back-fill the earlier ones -- otherwise drones, mines
    // and pulse state aren't populated. The simple assign-only path is the
    // fallback when we're *re-entering* a phase that already transitioned.
    if (boss_->levelNumber == 40) {
        if (phase == Boss::Phase::Two || phase == Boss::Phase::Three ||
            phase == Boss::Phase::Four) {
            if (!boss_->phase2Transitioned) enterBossPhase2();
        }
        if (phase == Boss::Phase::Three || phase == Boss::Phase::Four) {
            if (!boss_->phase3Transitioned) enterBossPhaseThree();
        }
        switch (phase) {
            case Boss::Phase::One:   boss_->phase = Boss::Phase::One;   break;
            case Boss::Phase::Two:   boss_->phase = Boss::Phase::Two;   break;
            case Boss::Phase::Three: boss_->phase = Boss::Phase::Three; break;
            case Boss::Phase::Four:  if (!boss_->phase4Transitioned) enterBossPhaseFour(); else boss_->phase = Boss::Phase::Four;  break;
        }
        return;
    }
    // Bosses 1-3 use only Phase::One / Phase::Two.
    if (phase == Boss::Phase::Two && boss_->phase != Boss::Phase::Two) {
        enterBossPhase2();
    } else {
        boss_->phase = phase;
    }
}

void GameWorld::setBossInvulnTimeRemainingForTesting(double seconds) {
    if (boss_) boss_->invulnTimeRemaining = seconds;
}

void GameWorld::setEnabledBonusesForTesting(std::vector<std::string> bonuses) {
    enabledBonuses_ = std::move(bonuses);
}

void GameWorld::launchBallForTesting() {
    if (!ball_.velocity.x && !ball_.velocity.y) {
        ball_.velocity = Vec2{0.0f, -620.0f};
    }
    ball_.state = BallState::Launched;
    if (physics_) {
        physics_->syncAttachedBall(ball_);
        physics_->setBallVelocity(ball_.entity, ball_.velocity);
    }
}

void GameWorld::triggerBossAttackCooldownForTesting() {
    handleLifeLost();
}

void GameWorld::setBossPostRespawnCooldownRemainingForTesting(double seconds) {
    bossAttackCooldownSeconds_ = std::max(0.0, seconds);
}

void GameWorld::setGamePhaseForTesting(GamePhase phase) {
    phase_ = phase;
}

void GameWorld::activateBonusForTesting(const std::string& type, double duration) {
    activeBonusTimers_.push_back(ActiveBonusTimer{
        .type = type,
        .durationSeconds = duration,
        .remainingSeconds = duration,
        .fadeOutRemainingSeconds = 0.0,
        .stacks = 1
    });
}

double GameWorld::bossAttackCooldownRemainingForTesting() const noexcept {
    return bossAttackCooldownSeconds_;
}

void GameWorld::setBossHitClockForTesting(double seconds) {
    bossHitClockSeconds_ = std::max(0.0, seconds);
    lastBossHitByBall_.clear();
}

double GameWorld::bossHitClockForTesting() const noexcept {
    return bossHitClockSeconds_;
}

void GameWorld::damageBossDroneForTesting(int droneIdx, int damage) {
    if (!boss_) return;
    if (droneIdx < 0 || static_cast<size_t>(droneIdx) >= boss_->drones.size()) return;
    auto& d = boss_->drones[droneIdx];
    if (!d.alive) return;
    d.currentHealth = std::max(0, d.currentHealth - damage);
    d.hitFlashRemainingSeconds = 0.15;
    score_.add(boss_->pointsPerDrone);
    queueAudioEvent(AudioEventType::BossHit, d.position, d.size);
    if (d.currentHealth <= 0) {
        d.currentHealth = 0;
        d.alive = false;
        if (boss_->droneDropOnDestroyChanceEnabled) {
            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> chance(0.0f, 1.0f);
            if (chance(rng) < 0.30f) {
                spawnRandomBonusAt(Vec2{
                    d.position.x + d.size.w * 0.5f,
                    d.position.y + d.size.h * 0.5f
                });
            }
        }
    }
}

void GameWorld::simulateBallHitsBossForTesting(int hits) {
    const int count = std::max(0, hits);
    for (int i = 0; i < count; ++i) {
        applyBallBossHitIfAny();  // exposed via header for tests
    }
}

void GameWorld::damageBossForTesting(int damage) {
    if (!boss_ || boss_->defeated) return;
    // Respect the post-teleport invulnerability window when the test asks for
    // it by leaving invulnTimeRemaining > 0 and using damageBossForTesting.
    if (boss_->levelNumber == 30 && boss_->invulnTimeRemaining > 0.0) {
        queueAudioEvent(AudioEventType::BossShieldBlock, boss_->position, boss_->size);
        return;
    }
    const int prevHp = boss_->currentHealth;
    boss_->currentHealth = std::max(0, boss_->currentHealth - damage);
    boss_->prevHealthAtHitForDropCheck = boss_->currentHealth;
    boss_->crystalFlashAlpha = 1.0f;
    boss_->hitFlashRemainingSeconds = 0.15;
    queueAudioEvent(AudioEventType::BossHit, boss_->position, boss_->size);

    // Phase 2 transition (used by boss 30 and boss 40)
    if ((boss_->levelNumber == 30 || boss_->levelNumber == 40) &&
        !boss_->phase2Transitioned &&
        boss_->currentHealth <= static_cast<int>(
            boss_->maxHealth * boss_->phase2ThresholdFraction)) {
        enterBossPhase2();
    }

    // Threshold-driven bonus drops (the same condition applyBallBossHitIfAny uses).
    if (boss_->levelNumber == 30 || boss_->levelNumber == 40) {
        float fracHi = (boss_->levelNumber == 40) ? 0.66f : 0.70f;
        float fracMid = (boss_->levelNumber == 40) ? 0.33f : 0.40f;
        float fracLo = 0.10f;
        int hpHi = static_cast<int>(boss_->maxHealth * fracHi);
        int hpMid = static_cast<int>(boss_->maxHealth * fracMid);
        int hpLo = static_cast<int>(boss_->maxHealth * fracLo);
        if ((prevHp > hpHi && boss_->currentHealth <= hpHi) ||
            (prevHp > hpMid && boss_->currentHealth <= hpMid) ||
            (prevHp > hpLo && boss_->currentHealth <= hpLo)) {
            onPhaseThresholdCrossed();
        }
    }

    // Boss 4: 4 phases stacked on HP% degradation only (no section damage).
    if (boss_->levelNumber == 40) {
        const int hp33 = static_cast<int>(boss_->maxHealth * 0.33f);
        const int hp10b = static_cast<int>(boss_->maxHealth * 0.10f);
        if (boss_->currentHealth <= hp33 && !boss_->phase3Transitioned) {
            enterBossPhaseThree();
        }
        if (boss_->currentHealth <= hp10b && !boss_->phase4Transitioned) {
            enterBossPhaseFour();
        }
    }

    if (boss_->levelNumber == 50) {
        // Phase transitions
        if (prevHp > boss_->phase2HpThreshold && boss_->currentHealth <= boss_->phase2HpThreshold) {
            enterChronarchPhase2();
        }
        if (prevHp > boss_->phase3HpThreshold && boss_->currentHealth <= boss_->phase3HpThreshold) {
            enterChronarchPhase3();
        }
        if (prevHp > boss_->phase4HpThreshold && boss_->currentHealth <= boss_->phase4HpThreshold) {
            enterChronarchPhase4();
        }

        // Drop check on negative bonuses
        if ((prevHp > boss_->phase2HpThreshold && boss_->currentHealth <= boss_->phase2HpThreshold) ||
            (prevHp > boss_->phase3HpThreshold && boss_->currentHealth <= boss_->phase3HpThreshold) ||
            (prevHp > boss_->phase4HpThreshold && boss_->currentHealth <= boss_->phase4HpThreshold) ||
            (prevHp > 28 && boss_->currentHealth <= 28)) {
            spawnChronarchNegativeBonus(ball_.position);
        }

        score_.add(boss_->pointsPerHit);
        if (boss_->currentHealth <= 0) {
            boss_->currentHealth = 0;
            boss_->defeated = true;
            score_.add(boss_->pointsOnDefeat);
            queueAudioEvent(AudioEventType::BossDefeated, boss_->position, boss_->size);
            clearChronarchState();
            boss_->paradoxShards.clear();
        }
        return;
    }

    score_.add(boss_->pointsPerHit);
    if (boss_->currentHealth <= 0) {
        boss_->currentHealth = 0;
        boss_->defeated = true;
        score_.add(boss_->pointsOnDefeat);
        queueAudioEvent(AudioEventType::BossDefeated, boss_->position, boss_->size);
        // Clear active boss-spawned state right away so subsequent callers
        // (and the LevelComplete state's early-return in update()) see a
        // fully cleaned world.
        boss_->projectiles.clear();
        boss_->drones.clear();
        boss_->laserState = Boss::LaserState::Idle;
        boss_->laserAlpha = 0.0f;
        boss_->invulnTimeRemaining = 0.0;
        if (boss_->levelNumber == 40) {
            boss_->gravityMines.clear();
            boss_->singularityPulse = SingularityPulse{};
        }
    }
}

void GameWorld::setGravityFieldEnabledForTesting(bool enabled) {
    if (!boss_) return;
    boss_->gravityFieldEnabled = enabled;
    if (enabled) {
        // Pick the strength that matches the current phase so the test can
        // simply turn the field on without juggling the strength knob too.
        boss_->gravityFieldStrength =
            (boss_->phase == Boss::Phase::Four)
                ? boss_->gravityFieldStrengthPhase4
                : boss_->gravityFieldStrengthPhase3;
    }
}

void GameWorld::setBossGravityFieldStrengthForTesting(float v) {
    if (!boss_) return;
    boss_->gravityFieldStrength = static_cast<double>(v);
}

void GameWorld::addBossProjectileForTesting(Vec2 position, Vec2 velocity,
                                          float size, bool homing) {
    if (!boss_) return;
    BossProjectile p;
    p.entity = spawn(EntityKind::Bonus);
    p.position = position;
    p.velocity = velocity;
    p.size = Size{size, size};
    p.alive = true;
    p.age = 0.0;
    p.isHoming = homing;
    boss_->projectiles.push_back(p);
}

void GameWorld::setBossGravityMinesForTesting(std::vector<GravityMine> mines) {
    if (!boss_) return;
    boss_->gravityMines = std::move(mines);
}

void GameWorld::spawnBossGravityMineForTesting(Vec2 centerPos) {
    if (!boss_) return;
    GravityMine m;
    m.x = centerPos.x;
    m.y = centerPos.y;
    m.age = 0.0f;
    m.lifetimeRemaining = boss_->gravityMineLifetimeSeconds;
    m.bornAtSeconds = boss_->singularityPulseClockSeconds;
    boss_->gravityMines.push_back(m);
}

void GameWorld::setBossHomingEnabledForTesting(bool enabled) {
    if (!boss_) return;
    if (!enabled) return;
    // Flip every existing projectile to homing so the test does not have to
    // wait for a fresh spawn cadence. New projectiles in phase Four will
    // continue to be marked homing via updateBossShots.
    for (auto& p : boss_->projectiles) {
        p.isHoming = true;
    }
}

void GameWorld::setBossSingularityPulseClockForTesting(double seconds) {
    if (!boss_) return;
    // The semantics: position the boss's logical clock `seconds` into the
    // future (clamped at zero) and guarantee that the very NEXT tick
    // crosses the next-fire threshold, i.e. clockSeconds >= nextFire. We
    // achieve this by setting nextFire to the requested value and the
    // clock just *past* it.
    const double t = std::max(0.0, seconds);
    boss_->singularityPulseNextFireSeconds = t;
    boss_->singularityPulseClockSeconds = t + 1e-3;
}

void GameWorld::setBossGravityMineIntervalForTesting(float seconds) {
    if (!boss_) return;
    boss_->gravityMineIntervalSeconds = std::max(0.0f, seconds);
    // Push the next-fire clock past the new interval so respawns don't
    // happen in tests that explicitly disable auto-spawn.
    boss_->gravityMineNextFireSeconds = std::max(
        boss_->gravityMineNextFireSeconds,
        static_cast<double>(seconds));
}

void GameWorld::setBossMoveSpeedForTesting(float v) {
    if (!boss_) return;
    boss_->moveSpeed = v;
}

void GameWorld::reduceBossHealthToOnePercentForTesting() {
    if (!boss_ || boss_->defeated) return;

    if (boss_->sectionCount > 1) {
        // Boss 2+: Give 1 HP per alive section
        int totalNewHealth = 0;
        for (int i = 0; i < boss_->sectionCount; ++i) {
            if (boss_->sections[i].alive) {
                boss_->sectionHealth[i] = 1;
                totalNewHealth += 1;
            }
        }
        boss_->currentHealth = totalNewHealth;
    } else {
        // Boss 1: 1% of max health, at least 1
        boss_->currentHealth = std::max(1, boss_->maxHealth / 100);
    }
}

void GameWorld::updateBoss(double dt) {
    if (!boss_) return;
    if (boss_->defeated) return;

    if (bossAttackCooldownSeconds_ > 0.0) {
        bossAttackCooldownSeconds_ = std::max(0.0, bossAttackCooldownSeconds_ - dt);
    }

    if (boss_->hitFlashRemainingSeconds > 0.0) {
        boss_->hitFlashRemainingSeconds = std::max(0.0, boss_->hitFlashRemainingSeconds - dt);
    }

    if (boss_->levelNumber == 30) {
        if (boss_->crystalFlashAlpha > 0.0f) {
            boss_->crystalFlashAlpha = std::max(0.0f, boss_->crystalFlashAlpha - static_cast<float>(dt) * 6.6f);
        }
    }

    if (boss_->levelNumber == 50) {
        updateBossMovement(dt);
        updateChronarch(dt);
        return;
    }

    updateBossMovement(dt);
    updateBossDive(dt);
    updateBossShield(dt);
    updateBossShots(dt);
    updateBossProjectiles(dt);

    if (boss_->levelNumber == 30) {
        updateBossTeleport(dt);
        updateBossLaser(dt);
        updateBossDrones(dt);
    }

    if (boss_->levelNumber == 40) {
        updateBossTeleport(dt);
        updateBossLaser(dt);
        updateBossDrones(dt);
        updateSingularityPulses(dt);
        updateGravityMines(dt);
    }
}

void GameWorld::updateBossMovement(double dt) {
    if (!boss_) return;
    auto& b = *boss_;
    if (b.edgePauseRemaining > 0.0) {
        b.edgePauseRemaining = std::max(0.0, b.edgePauseRemaining - dt);
        return;
    }

    const float stageCenter = brickStageLeft + brickStageWidth * 0.5f;
    const float leftEdge  = stageCenter - b.moveAmplitude - b.size.w * 0.5f;
    const float rightEdge = stageCenter + b.moveAmplitude - b.size.w * 0.5f;

    b.position.x += b.moveDirection * b.moveSpeed * static_cast<float>(dt);

    if (b.position.x <= leftEdge) {
        b.position.x = leftEdge;
        b.moveDirection = +1;
        b.edgePauseRemaining = b.edgePauseSeconds;
    } else if (b.position.x >= rightEdge) {
        b.position.x = rightEdge;
        b.moveDirection = -1;
        b.edgePauseRemaining = b.edgePauseSeconds;
    }
}

void GameWorld::updateBossDive(double dt) {
    if (!boss_ || boss_->sectionCount == 1) return;
    auto& b = *boss_;

    if (b.diveState == Boss::DiveState::Idle) {
        b.diveTimerSeconds += dt;
        if (b.diveTimerSeconds >= b.diveIntervalSeconds) {
            b.diveState = Boss::DiveState::Diving;
            b.diveTimerSeconds = 0.0;
            b.diveStateTimer = 0.0;
            queueAudioEvent(AudioEventType::BossDive, b.position, b.size);
        }
    } else {
        b.diveStateTimer += dt;
        const double halfCycle = b.diveDurationSeconds * 0.5;
        if (b.diveStateTimer < halfCycle) {
            b.diveOffsetY = b.diveDepthPx * static_cast<float>(b.diveStateTimer / halfCycle);
        } else if (b.diveStateTimer < b.diveDurationSeconds) {
            b.diveOffsetY = b.diveDepthPx;
        } else if (b.diveStateTimer < b.diveDurationSeconds + halfCycle) {
            const double t = (b.diveStateTimer - b.diveDurationSeconds) / halfCycle;
            b.diveOffsetY = b.diveDepthPx * (1.0f - static_cast<float>(t));
        } else {
            b.diveOffsetY = 0.0f;
            b.diveState = Boss::DiveState::Idle;
            b.diveTimerSeconds = 0.0;
        }
    }
    b.position.y = b.baseY + b.diveOffsetY;
}

void GameWorld::updateBossShield(double dt) {
    if (!boss_ || boss_->defeated || boss_->sectionCount == 1) return;
    auto& b = *boss_;

    b.shieldTimerSeconds += dt;
    if (!b.shieldActive && b.shieldTimerSeconds >= b.shieldCooldownSeconds) {
        b.shieldActive = true;
    } else if (b.shieldActive && b.shieldTimerSeconds >= b.shieldCycleSeconds) {
        b.shieldActive = false;
        b.shieldTimerSeconds = 0.0;
    }

    if (b.shieldActive) {
        b.shieldGlowAlpha = 0.7f;
    } else if (b.shieldTimerSeconds < 0.3) {
        b.shieldGlowAlpha = 0.7f * static_cast<float>(b.shieldTimerSeconds / 0.3);
    } else {
        b.shieldGlowAlpha = 0.0f;
    }
}

void GameWorld::updateBossShots(double dt) {
    if (!boss_ || boss_->defeated) return;
    if (boss_->sectionCount == 1 && boss_->levelNumber != 40) return;
    if (bossAttackCooldownSeconds_ > 0.0) return;  // no new shots during cooldown
    auto& b = *boss_;

    b.shotTimerSeconds += dt;
    const double firstShotDelay = 1.5;
    const double interval = (b.levelNumber == 40 && b.phase == Boss::Phase::Four) ? 1.2 : b.shotIntervalSeconds;
    const double nextShotAt = b.projectiles.empty() ? firstShotDelay : interval;

    if (b.shotTimerSeconds >= nextShotAt) {
        b.shotTimerSeconds = 0.0;
        const float effectiveY = b.position.y + b.size.h;
        const float leftX  = b.position.x + 12.0f;
        const float rightX = b.position.x + b.size.w - 12.0f - b.projectileSize.w;

        auto spawnProj = [&](float x, bool forceHoming) {
            BossProjectile p;
            p.entity = spawn(EntityKind::Bonus);
            p.position = Vec2{x, effectiveY};
            // Boss 4 (Singularity) Phase 4 must fire homing projectiles at
            // roughly half their cadence. Other levels keep the straight
            // downward trajectory.
            if (forceHoming) {
                p.velocity = Vec2{0.0f, b.projectileSpeed};
                p.isHoming = true;
            } else {
                p.velocity = Vec2{0.0f, b.projectileSpeed};
            }
            p.size = b.projectileSize;
            p.alive = true;
            p.age = 0.0;
            b.projectiles.push_back(p);
        };
        if (b.levelNumber == 40 && b.phase == Boss::Phase::Four) {
            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<int> d(0, 1);
            const bool leftHoming  = d(rng) == 1;
            const bool rightHoming = d(rng) == 1;
            spawnProj(leftX, leftHoming);
            spawnProj(rightX, rightHoming);
        } else {
            spawnProj(leftX, false);
            spawnProj(rightX, false);
        }
        queueAudioEvent(AudioEventType::BossShot, b.position, b.size);
    }
}

void GameWorld::updateBossProjectiles(double dt) {
    if (!boss_) return;
    if (boss_->levelNumber == 40) {
        updateHomingLogic(dt);
    }
    for (auto& p : boss_->projectiles) {
        if (!p.alive) continue;
        p.age += dt;
        p.position.x += p.velocity.x * static_cast<float>(dt);
        p.position.y += p.velocity.y * static_cast<float>(dt);

        const Paddle& pad = paddle_;
        if (p.position.y + p.size.h >= pad.position.y &&
            p.position.x < pad.position.x + pad.size.w &&
            p.position.x + p.size.w > pad.position.x) {
            p.alive = false;
            if (!isBonusActive("BONUS_WALL")) {
                queueAudioEvent(AudioEventType::BossProjectileHitPaddle, p.position, p.size);
                handleProjectileLifeLoss();
            } else {
                queueAudioEvent(AudioEventType::BossShieldBlock, p.position, p.size);
            }
        }
        if (p.position.y > 1080.0f) {
            p.alive = false;
        }
    }
    boss_->projectiles.erase(
        std::remove_if(boss_->projectiles.begin(), boss_->projectiles.end(),
                       [](const BossProjectile& p){ return !p.alive; }),
        boss_->projectiles.end());
}

void GameWorld::handleProjectileLifeLoss() {
    // Same grace-period rule as a regular respawn: the boss gets a cooldown
    // and clears active shots so the just-respawned paddle isn't immediately
    // killed by an in-flight projectile.
    bossAttackCooldownSeconds_ = std::max(
        bossAttackCooldownSeconds_,
        respawnLaunchDelaySeconds + 1.5);
    clearBossAttackState();
    if (lives_.loseLife()) {
        queueAudioEvent(AudioEventType::LifeLost, paddle_.position);
        phase_ = GamePhase::LifeLost;
        respawnLaunchDelayRemaining_ = respawnLaunchDelaySeconds;
        autoLaunchCountdown_ = 5.0;
        attachBallToPaddle();
        if (physics_) physics_->syncAttachedBall(ball_);
    } else {
        queueAudioEvent(AudioEventType::GameOver, ball_.position);
        phase_ = GamePhase::GameOver;
        respawnLaunchDelayRemaining_ = 0.0;
        ball_.state = BallState::AttachedToPaddle;
        ball_.velocity = Vec2{};
        if (physics_) physics_->syncAttachedBall(ball_);
    }
}

int GameWorld::findHitSectionIdx() const {
    if (!boss_) return -1;
    const float bx = boss_->position.x;
    const float by = boss_->position.y;
    const float bw = boss_->size.w;
    const float bh = boss_->size.h;

    const float closestX = std::clamp(ball_.position.x, bx, bx + bw);
    const float closestY = std::clamp(ball_.position.y, by, by + bh);
    const float dx = ball_.position.x - closestX;
    const float dy = ball_.position.y - closestY;
    if (dx*dx + dy*dy > ball_.radius * ball_.radius) return -1;

    const float relX = ball_.position.x - bx;
    int bestSection = -1;
    float bestDist = 1e9f;
    for (size_t i = 0; i < boss_->sections.size(); ++i) {
        const auto& s = boss_->sections[i];
        if (!s.alive) continue;
        const float secCenterX = s.localBounds.left + (s.localBounds.right - s.localBounds.left) * 0.5f;
        const float d = std::abs(relX - secCenterX);
        if (d < bestDist) { bestDist = d; bestSection = static_cast<int>(i); }
    }
    return bestSection;
}

// ============================================================================
// Boss 3 (Helios) - new methods
// ============================================================================

void GameWorld::updateBossTeleport(double dt) {
    if (!boss_ || boss_->defeated) return;
    if (boss_->levelNumber != 30 && boss_->levelNumber != 40) return;
    auto& b = *boss_;

    if (b.invulnTimeRemaining > 0.0) {
        b.invulnTimeRemaining = std::max(0.0, b.invulnTimeRemaining - dt);
    }

    b.teleportTimerSeconds += dt;
    const double interval = (b.phase == Boss::Phase::Two)
        ? b.teleportIntervalPhase2Seconds
        : (b.phase == Boss::Phase::Three)
            ? b.teleportIntervalPhase3Seconds
            : (b.phase == Boss::Phase::Four)
                ? b.teleportIntervalPhase4Seconds
                : b.teleportIntervalSeconds;
    const double nextAt = (b.teleportTimerSeconds < b.doubleTeleportFirstDelaySeconds)
        ? b.doubleTeleportFirstDelaySeconds : interval;

    if (b.teleportTimerSeconds >= nextAt) {
        b.teleportTimerSeconds = 0.0;

        const float stageCenter = brickStageLeft + brickStageWidth * 0.5f;
        const float leftEdge  = stageCenter - b.moveAmplitude - b.size.w * 0.5f;
        const float rightEdge = stageCenter + b.moveAmplitude - b.size.w * 0.5f;

        if (b.levelNumber == 40 && b.phase == Boss::Phase::Four) {
            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> offset(-100.0f, 100.0f);
            b.position.x = std::clamp(
                ball_.position.x - b.size.w * 0.5f + offset(rng),
                leftEdge,
                rightEdge);
        } else {
            const float oldX = b.position.x;
            float newX = oldX;
            // Re-pick until we actually change X by at least 30% of amplitude.
            for (int attempt = 0; attempt < 8; ++attempt) {
                thread_local std::mt19937 rng(std::random_device{}());
                std::uniform_real_distribution<float> d(leftEdge, rightEdge);
                newX = d(rng);
                if (std::abs(newX - oldX) > b.moveAmplitude * 0.3f) break;
            }
            b.position.x = newX;
        }
        b.invulnTimeRemaining = b.invulnOnTeleportSeconds;
        b.moveDirection = (b.position.x < stageCenter) ? +1 : -1;

        queueAudioEvent(AudioEventType::BossTeleport, b.position, b.size);
    }
}

void GameWorld::updateBossLaser(double dt) {
    if (!boss_ || boss_->defeated) return;
    if (boss_->levelNumber != 30 && boss_->levelNumber != 40) return;
    if (bossAttackCooldownSeconds_ > 0.0) return;  // no laser updates during cooldown
    auto& b = *boss_;

    b.laserTimerSeconds += dt;
    b.laserStateTimer += dt;

    const double interval = (b.phase == Boss::Phase::Two)
        ? b.laserIntervalPhase2Seconds
        : (b.phase == Boss::Phase::Three)
            ? b.laserIntervalPhase3Seconds
            : (b.phase == Boss::Phase::Four)
                ? b.laserIntervalPhase4Seconds
                : b.laserIntervalSeconds;

    auto enterState = [&](Boss::LaserState s) {
        b.laserState = s;
        b.laserStateTimer = 0.0;
        b.laserAppliedThisCycle = (s != Boss::LaserState::Firing);
    };

    switch (b.laserState) {
        case Boss::LaserState::Idle: {
            // Use firstDelay until the inaugural cycle has fired - then use interval.
            const double effectiveNext = (!b.laserFirstFired && b.laserFirstDelaySeconds > 0.0)
                ? b.laserFirstDelaySeconds
                : interval;
            if (b.laserTimerSeconds >= effectiveNext) {
                // Pick a target X biased toward the paddle's center.
                const float paddleCenter = paddle_.position.x + paddle_.size.w * 0.5f;
                const float paddleHalf = paddle_.size.w * 0.5f;
                thread_local std::mt19937 rng(std::random_device{}());
                std::uniform_real_distribution<float> d(
                    paddleCenter - paddleHalf * 0.4f,
                    paddleCenter + paddleHalf * 0.4f);
                b.laserTargetX = d(rng);
                b.laserTimerSeconds = 0.0;
                b.laserFirstFired = true;
                enterState(Boss::LaserState::Charging);
                queueAudioEvent(AudioEventType::BossLaserCharge, b.position, b.size);
            }
            break;
        }
        case Boss::LaserState::Charging:
            if (b.laserStateTimer >= b.laserChargeSeconds) {
                enterState(Boss::LaserState::Firing);
                queueAudioEvent(AudioEventType::BossLaserFire, b.position, b.size);
            }
            break;
        case Boss::LaserState::Firing:
            if (b.laserStateTimer >= b.laserFiringSeconds) {
                enterState(Boss::LaserState::Cooldown);
            } else if (!b.laserAppliedThisCycle) {
                handleLaserLifeLossIfAny();
                b.laserAppliedThisCycle = true;
            }
            break;
        case Boss::LaserState::Cooldown:
            if (b.laserStateTimer >= b.laserCooldownSeconds) {
                b.laserState = Boss::LaserState::Idle;
                b.laserStateTimer = 0.0;
            }
            break;
    }

        // Update visual alpha (charging pulse + firing solid + cooldown fade)
        if (b.laserState == Boss::LaserState::Charging) {
            const float pulse = 0.5f + 0.5f * std::sin(b.laserStateTimer * 12.0);
            b.laserAlpha = 0.20f + 0.25f * pulse;
        } else if (b.laserState == Boss::LaserState::Firing) {
            b.laserAlpha = 0.95f;
        } else if (b.laserState == Boss::LaserState::Cooldown) {
            const float t = static_cast<float>(b.laserStateTimer / b.laserCooldownSeconds);
            b.laserAlpha = 0.95f * (1.0f - t);
        } else {
            b.laserAlpha = 0.0f;
        }
}

void GameWorld::updateBossDrones(double dt) {
    if (!boss_ || boss_->defeated) return;
    if (boss_->levelNumber != 30 && boss_->levelNumber != 40) return;
    auto& b = *boss_;
    if (b.levelNumber == 30 && b.phase != Boss::Phase::Two) return;
    if (b.levelNumber == 40 && b.phase == Boss::Phase::One) return;

    const float stageCenter = brickStageLeft + brickStageWidth * 0.5f;

    for (auto& d : b.drones) {
        if (!d.alive) continue;

        d.age += dt;
        if (d.hitFlashRemainingSeconds > 0.0) {
            d.hitFlashRemainingSeconds = std::max(0.0, d.hitFlashRemainingSeconds - dt);
        }

        if (d.edgePauseRemaining > 0.0) {
            d.edgePauseRemaining = std::max(0.0, d.edgePauseRemaining - dt);
        } else {
            d.position.x += d.moveDirection * d.moveSpeed * static_cast<float>(dt);
            const float leftEdge  = stageCenter - d.moveAmplitude - d.size.w * 0.5f;
            const float rightEdge = stageCenter + d.moveAmplitude - d.size.w * 0.5f;
            if (d.position.x <= leftEdge) {
                d.position.x = leftEdge;
                d.moveDirection = +1;
                d.edgePauseRemaining = d.edgePauseSeconds;
            } else if (d.position.x >= rightEdge) {
                d.position.x = rightEdge;
                d.moveDirection = -1;
                d.edgePauseRemaining = d.edgePauseSeconds;
            }
        }

        // Honour the boss post-respawn cooldown so a freshly respawned paddle
        // isn't immediately killed by a drone projectile still falling toward it.
        if (bossAttackCooldownSeconds_ <= 0.0) {
            d.shotTimerSeconds += dt;
            if (d.shotTimerSeconds >= b.droneShotIntervalSeconds) {
                d.shotTimerSeconds = 0.0;
                BossProjectile p;
                p.entity = spawn(EntityKind::Bonus);
                p.position = Vec2{
                    d.position.x + d.size.w * 0.5f - b.projectileSize.w * 0.5f,
                    d.position.y + d.size.h + 4.0f
                };
                p.velocity = Vec2{0.0f, 320.0f};
                p.size = b.projectileSize;
                p.alive = true;
                p.age = 0.0;
                b.projectiles.push_back(p);
                queueAudioEvent(AudioEventType::BossShot, d.position, d.size);
            }
        }
    }

    // Clean up dead drones (alive=false) - keep their placeholders so testers can
    // count final population, but we won't update them above.

    if (b.levelNumber == 40) {
        int targetCount = 0;
        if (b.phase == Boss::Phase::Two) {
            targetCount = b.dronesPhaseTwoSpawnCount;
        } else if (b.phase == Boss::Phase::Three || b.phase == Boss::Phase::Four) {
            targetCount = b.dronesPhaseTwoSpawnCount + b.dronesPhaseThreeSpawnCount;
        }

        int aliveCount = 0;
        for (const auto& d : b.drones) {
            if (d.alive) {
                aliveCount++;
            }
        }

        if (aliveCount < targetCount) {
            b.droneRespawnTimerRemaining -= dt;
            if (b.droneRespawnTimerRemaining <= 0.0) {
                const float droneY = 460.0f;
                // Alternate positions or use random positions inside bounds
                thread_local std::mt19937 rng(std::random_device{}());
                std::uniform_real_distribution<float> xDist(stageCenter - 220.0f, stageCenter + 220.0f);
                float spawnX = xDist(rng);
                int dir = (rng() % 2 == 0) ? 1 : -1;

                DroneEntity d;
                d.entity = spawn(EntityKind::Bonus);
                d.position = Vec2{spawnX, droneY};
                d.size = Size{34.0f, 34.0f};
                d.maxHealth = 3;
                d.currentHealth = 3;
                d.moveSpeed = 160.0f;
                d.moveAmplitude = 280.0f;
                d.edgePauseSeconds = 0.4;
                d.moveDirection = dir;
                d.alive = true;
                d.shotTimerSeconds = 0.0;
                b.drones.push_back(d);

                b.droneRespawnTimerRemaining = 7.0;
            }
        } else {
            b.droneRespawnTimerRemaining = 7.0;
        }
    }
}

void GameWorld::enterBossPhase2() {
    if (!boss_) return;
    if (boss_->levelNumber != 30 && boss_->levelNumber != 40) return;
    auto& b = *boss_;
    if (b.phase2Transitioned) return;
    b.phase2Transitioned = true;
    b.phase = Boss::Phase::Two;

    if (b.levelNumber == 30) {
        // Once we cross into phase 2 we want a faster boss and a fatter beam.
        b.moveSpeed = 450.0f;
        b.laserWidth = b.laserWidthPhaseTwo;
        b.teleportIntervalSeconds = b.teleportIntervalPhase2Seconds;
        b.laserIntervalSeconds = b.laserIntervalPhase2Seconds;
    } else {
        // Singularity (boss 4) phase 2 boosts speed and telegraph cadence.
        b.moveSpeed = b.moveSpeedPhase2;
    }

    // Spawn two drones roughly symmetric around the stage center, positioned
    // *below* the boss body so they don't visually clip the portrait sprite.
    const int droneCount = b.dronesPhaseTwoSpawnCount > 0 ? b.dronesPhaseTwoSpawnCount : 2;
    const float stageCenter = brickStageLeft + brickStageWidth * 0.5f;
    const float droneY = (b.levelNumber == 40) ? 460.0f : 460.0f;
    auto spawnDrone = [&](float x, int dir) {
        DroneEntity d;
        d.entity = spawn(EntityKind::Bonus);
        d.position = Vec2{x, droneY};
        d.size = Size{34.0f, 34.0f};
        d.maxHealth = 3;
        d.currentHealth = 3;
        d.moveSpeed = 160.0f;
        d.moveAmplitude = 280.0f;
        d.edgePauseSeconds = 0.4;
        d.moveDirection = dir;
        d.alive = true;
        d.shotTimerSeconds = 0.0;
        b.drones.push_back(d);
    };
    // Symmetric pair left/right of the stage centre.
    spawnDrone(stageCenter - 220.0f, +1);
    spawnDrone(stageCenter + 220.0f, -1);
    // Boss 4 leaves room for extra drones via phase 3 -- these are spawned
    // by enterBossPhaseThree(). Helios leaves the count at exactly 2.
    (void)droneCount;

    queueAudioEvent(AudioEventType::BossPhaseTransition, b.position, b.size);
}

void GameWorld::dropPhaseThresholdBonus() {
    if (!boss_) return;
    // Boss loot is always a penalty - same convention as bosses 1 and 2.
    static const std::array<std::string, 6> kBossNegativeBonuses = {
        "DECREASE_PADDLE", "FAST_BALLS", "WEAK_BALLS",
        "FROZEN_PADDLE", "CHAOTIC_BALLS", "PENALTIES_MAGNET"
    };

    // Filter by currently enabled bonuses (same convention as older bosses).
    std::vector<std::string> valid;
    valid.reserve(kBossNegativeBonuses.size());
    for (const auto& eb : enabledBonuses_) {
        for (const auto& choice : kBossNegativeBonuses) {
            if (eb == choice) valid.push_back(eb);
        }
    }
    if (valid.empty()) return;

    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<std::size_t> d(0, valid.size() - 1);
    const std::string pick = valid[d(rng)];
    Vec2 center{
        boss_->position.x + boss_->size.w * 0.5f,
        boss_->position.y + boss_->size.h * 0.5f
    };
    spawnBonus(pick, center);
}

void GameWorld::onPhaseThresholdCrossed() {
    if (!boss_) return;
    dropPhaseThresholdBonus();
}

void GameWorld::initBossLevelFour_Singularity() {
    boss_ = std::make_unique<Boss>();
    boss_->levelNumber = 40;
    boss_->size = Size{300.0f, 300.0f};
    boss_->baseY = 270.0f;
    boss_->entity = spawn(EntityKind::Boss);

    boss_->maxHealth = 220;
    boss_->currentHealth = 220;

    boss_->pointsPerHit = 130;
    boss_->pointsOnDefeat = 42000;
    boss_->pointsPerDrone = 700;

    boss_->moveAmplitude = 340.0f;
    boss_->edgePauseSeconds = 0.3;

    // Phase-specific tunables
    boss_->moveSpeed = 260.0f;       // phase 1 by default
    boss_->teleportIntervalSeconds = 6.0;
    boss_->teleportIntervalPhase2Seconds = 4.0;
    boss_->teleportIntervalPhase3Seconds = 3.2;
    boss_->teleportIntervalPhase4Seconds = 2.4;
    boss_->invulnOnTeleportSeconds = 0.4;

    boss_->laserIntervalSeconds = 4.5;
    boss_->laserIntervalPhase2Seconds = 4.5;
    boss_->laserIntervalPhase3Seconds = 3.0;
    boss_->laserIntervalPhase4Seconds = 2.0;
    boss_->laserChargeSeconds = 1.0;
    boss_->laserFiringSeconds = 0.45;
    boss_->laserCooldownSeconds = 0.6;
    boss_->laserFirstDelaySeconds = 2.5;
    boss_->laserFirstFired = false;
    boss_->laserWidth = 36.0f;
    boss_->laserWidthPhaseOne = 36.0f;
    boss_->laserWidthPhaseTwo = 40.0f;
    boss_->laserWidthPhase3 = 44.0f;
    boss_->laserWidthPhaseFour = 52.0f;
    boss_->laserWidthPhaseTwo = 40.0f;  // alias for legacy

    boss_->droneShotIntervalSeconds = 2.3;

    // Phase thresholds (4-phase boss)
    boss_->phase2ThresholdFraction = 0.66f;  // 145 HP
    boss_->phase2ThresholdHp = static_cast<int>(boss_->maxHealth * boss_->phase2ThresholdFraction);
    boss_->phase2Transitioned = false;
    boss_->phase3ThresholdFraction = 0.20f;  // 44 HP
    boss_->phase3ThresholdHp = static_cast<int>(boss_->maxHealth * 0.20f);
    boss_->phase3Transitioned = false;
    boss_->phase4ThresholdFraction = 0.10f;  // 22 HP
    boss_->phase4ThresholdHp = static_cast<int>(boss_->maxHealth * 0.10f);
    boss_->phase4Transitioned = false;

    // Singularity-side configurations
    boss_->phase = Boss::Phase::One;
    boss_->gravityFieldEnabled = false;
    boss_->gravityFieldStrengthPhase3 = 180.0f;
    boss_->gravityFieldStrengthPhase4 = 340.0f;
    boss_->gravityFieldRadius = 280.0f;
    boss_->gravityFieldFalloffExponent = 1.0f;

    boss_->singularityPulseIntervalSeconds = 8.0;
    boss_->singularityPulseNextFireSeconds = 8.0;  // first pulse 8s in
    boss_->singularityPulseFlashDurationSeconds = 0.7;
    boss_->singularityPulseRadius = 720.0f;
    boss_->singularityPulseClockSeconds = 0.0;
    boss_->singularityPulse = SingularityPulse{};
    boss_->gravityMineIntervalSeconds = 1.6;
    boss_->gravityMineNextFireSeconds = 0.0;
    boss_->gravityMineLifetimeSeconds = 4.0f;
    boss_->gravityMineRadius = 80.0f;
    boss_->gravityMineStrength = 220.0f;
    boss_->gravityMineVisualRadius = 40.0f;
    boss_->gravityMineSpawnRadiusFromBoss = 350.0f;

    boss_->homingTurnRate = 4.5f;
    boss_->homingTargetSpeed = 380.0f;
    boss_->homingMaxRange = 800.0f;

    boss_->dronesPhaseTwoSpawnCount = 2;
    boss_->dronesPhaseThreeSpawnCount = 3;
    boss_->droneRespawnTimerRemaining = 7.0;

    boss_->sections.clear();
    boss_->sections.push_back(BossSection{
        .entity = spawn(EntityKind::Boss),
        .localBounds = {0, 0, 300, 300}
    });
    boss_->sectionCount = 1;
    boss_->sectionHealth = {220};
    boss_->sectionMaxHealth = {220};
    boss_->prevHealthAtHitForDropCheck = 220;

    boss_->position = Vec2{
        brickStageLeft + brickStageWidth * 0.5f - boss_->size.w * 0.5f,
        boss_->baseY
    };
    boss_->moveDirection = 1;
}

void GameWorld::enterBossPhaseThree() {
    if (!boss_) return;
    if (boss_->levelNumber != 40) return;
    if (boss_->phase3Transitioned) return;
    boss_->phase3Transitioned = true;
    boss_->phase = Boss::Phase::Three;

    boss_->moveSpeed = boss_->moveSpeedPhase3;
    boss_->gravityFieldEnabled = true;
    boss_->gravityFieldStrength = boss_->gravityFieldStrengthPhase3;
    boss_->singularityPulseNextFireSeconds = boss_->singularityPulseIntervalSeconds;
    boss_->teleportIntervalSeconds = boss_->teleportIntervalPhase3Seconds;
    boss_->laserIntervalSeconds = boss_->laserIntervalPhase3Seconds;
    boss_->laserWidth = boss_->laserWidthPhase3;

    // Spawn +3 more drones (added to the 2 already spawned in phase 2).
    const float stageCenter = brickStageLeft + brickStageWidth * 0.5f;
    const float droneY = 460.0f;
    auto spawnDrone = [&](float x, int dir) {
        DroneEntity d;
        d.entity = spawn(EntityKind::Boss);
        d.position = Vec2{x, droneY};
        d.size = Size{34.0f, 34.0f};
        d.maxHealth = 3;
        d.currentHealth = 3;
        d.moveSpeed = 160.0f;
        d.moveAmplitude = 280.0f;
        d.edgePauseSeconds = 0.4;
        d.moveDirection = dir;
        d.alive = true;
        d.shotTimerSeconds = 0.0;
        boss_->drones.push_back(d);
    };
    const float third = boss_->dronesPhaseThreeSpawnCount;
    for (int i = 0; i < third; ++i) {
        // Slot the three new drones across the central band of the stage,
        // alternating move direction to keep the shuttle's coverage dense.
        const float offset =
            220.0f * static_cast<float>(i + 1) / static_cast<float>(third + 1);
        const float x = (i % 2 == 0) ? stageCenter + offset : stageCenter - offset;
        const int dir = (i % 2 == 0) ? +1 : -1;
        spawnDrone(x, dir);
    }

    queueAudioEvent(AudioEventType::BossPhaseTransition, boss_->position, boss_->size);
}

void GameWorld::enterBossPhaseFour() {
    if (!boss_) return;
    if (boss_->levelNumber != 40) return;
    if (boss_->phase4Transitioned) return;
    boss_->phase4Transitioned = true;
    boss_->phase = Boss::Phase::Four;

    boss_->moveSpeed = boss_->moveSpeedPhase4;
    boss_->gravityFieldEnabled = true;
    boss_->gravityFieldStrength = boss_->gravityFieldStrengthPhase4;
    boss_->teleportIntervalSeconds = boss_->teleportIntervalPhase4Seconds;
    boss_->laserIntervalSeconds = boss_->laserIntervalPhase4Seconds;
    boss_->laserWidth = boss_->laserWidthPhaseFour;

    boss_->gravityMineNextFireSeconds = 0.0;  // start firing mines immediately

    queueAudioEvent(AudioEventType::BossPhaseTransition, boss_->position, boss_->size);
}

void GameWorld::updateGravityField(double dt) {
    if (!boss_ || boss_->levelNumber != 40) return;
    if (!boss_->gravityFieldEnabled) return;

    const float cx = boss_->position.x + boss_->size.w * 0.5f;
    const float cy = boss_->position.y + boss_->size.h * 0.5f;
    const float radius = boss_->gravityFieldRadius;

    auto pullOne = [&](Vec2& pos, Vec2& vel) {
        const float dx = cx - pos.x;
        const float dy = cy - pos.y;
        const float dsq = dx*dx + dy*dy;
        if (dsq > radius * radius) return;
        const float d = std::sqrt(dsq);
        if (d < 1.0f) return;
        const float falloff = 1.0f - (d / radius);
        const float force = boss_->gravityFieldStrength * falloff * static_cast<float>(dt);
        const float inv = 1.0f / d;
        vel.x += dx * inv * force;
        vel.y += dy * inv * force;
    };

    pullOne(ball_.position, ball_.velocity);
    for (auto& eb : extraBalls_) {
        pullOne(eb.position, eb.velocity);
    }
}

void GameWorld::updateSingularityPulses(double dt) {
    if (!boss_ || boss_->levelNumber != 40) return;
    if (boss_->defeated) return;

    boss_->singularityPulseClockSeconds += dt;

    // Time for the next pulse?
    if (boss_->singularityPulseNextFireSeconds >= 0.0 &&
        boss_->singularityPulseClockSeconds >= boss_->singularityPulseNextFireSeconds) {
        if (!boss_->singularityPulse.active) {
            fireSingularityPulse();
        }
        boss_->singularityPulseNextFireSeconds =
            boss_->singularityPulseClockSeconds + boss_->singularityPulseIntervalSeconds;
    }

    // Animate the active pulse ring outward.
    if (boss_->singularityPulse.active) {
        boss_->singularityPulse.timeSinceFired += dt;
        const float frac = static_cast<float>(
            boss_->singularityPulse.timeSinceFired / boss_->singularityPulseFlashDurationSeconds);
        boss_->singularityPulse.currentRadius =
            boss_->singularityPulseRadius * std::clamp(frac, 0.0f, 1.0f);

        const float cx = boss_->position.x + boss_->size.w * 0.5f;
        const float cy = boss_->position.y + boss_->size.h * 0.5f;
        const float pulseR = boss_->singularityPulse.currentRadius;

        // The ring sweeps outward and gradually exerts a strong inward force on
        // any ball whose distance from the boss sits within the ring band.
        const float thickness = 30.0f;
        auto sweepPull = [&](Vec2& pos, Vec2& vel) {
            const float dx = cx - pos.x;
            const float dy = cy - pos.y;
            const float d = std::sqrt(dx*dx + dy*dy);
            if (std::abs(d - pulseR) >= thickness) return;
            const float inv = 1.0f / std::max(d, 1.0f);
            const float f = 240.0f * static_cast<float>(dt);
            vel.x += dx * inv * f;
            vel.y += dy * inv * f;
        };
        sweepPull(ball_.position, ball_.velocity);
        for (auto& eb : extraBalls_) {
            sweepPull(eb.position, eb.velocity);
        }

        if (boss_->singularityPulse.timeSinceFired >= boss_->singularityPulseFlashDurationSeconds) {
            boss_->singularityPulse = SingularityPulse{};
        }
    }
}

void GameWorld::fireSingularityPulse() {
    boss_->singularityPulse.active = true;
    boss_->singularityPulse.timeSinceFired = 0.0f;
    boss_->singularityPulse.currentRadius = 0.0f;

    if (boss_->levelNumber == 40 && boss_->phase == Boss::Phase::Four) {
        for (int i = 0; i < 5; ++i) {
            spawnGravityMine();
        }
    }
}

void GameWorld::updateGravityMines(double dt) {
    if (!boss_ || boss_->levelNumber != 40) return;
    if (boss_->defeated) return;

    // Spawn cadence (only in phase 4).
    if (boss_->phase == Boss::Phase::Four) {
        boss_->gravityMineNextFireSeconds -= dt;
        if (boss_->gravityMineNextFireSeconds <= 0.0) {
            spawnGravityMine();
            boss_->gravityMineNextFireSeconds = boss_->gravityMineIntervalSeconds;
        }
    }

    // Step the existing mines.
    for (auto& m : boss_->gravityMines) {
        m.age += static_cast<float>(dt);
        m.lifetimeRemaining -= static_cast<float>(dt);
    }
    boss_->gravityMines.erase(
        std::remove_if(
            boss_->gravityMines.begin(),
            boss_->gravityMines.end(),
            [](const GravityMine& m) { return m.lifetimeRemaining <= 0.0f; }),
        boss_->gravityMines.end());

    // Pull the ball toward each mine still alive.
    applyGravityMinePull(dt);
}

void GameWorld::spawnGravityMine() {
    if (!boss_) return;
    const float cx = boss_->position.x + boss_->size.w * 0.5f;
    const float cy = boss_->position.y + boss_->size.h * 0.5f;

    thread_local std::mt19937 rng(std::random_device{}());
    const float angle = std::uniform_real_distribution<float>(0.0f, 6.2831853f)(rng);
    const float r = std::uniform_real_distribution<float>(
        60.0f, boss_->gravityMineSpawnRadiusFromBoss)(rng);
    GravityMine m;
    m.x = cx + std::cos(angle) * r;
    m.y = cy + std::sin(angle) * r;
    m.age = 0.0f;
    m.lifetimeRemaining = boss_->gravityMineLifetimeSeconds;
    m.bornAtSeconds = boss_->singularityPulseClockSeconds;
    boss_->gravityMines.push_back(m);
}

void GameWorld::applyGravityMinePull(double dt) {
    if (!boss_ || boss_->levelNumber != 40) return;
    if (boss_->phase != Boss::Phase::Four) return;

    auto pullOneTo = [&](Vec2& pos, Vec2& vel, const GravityMine& m) {
        const float dx = m.x - pos.x;
        const float dy = m.y - pos.y;
        const float dsq = dx*dx + dy*dy;
        if (dsq > boss_->gravityMineRadius * boss_->gravityMineRadius) return;
        const float d = std::sqrt(dsq);
        if (d < 1.0f) return;
        const float falloff = 1.0f - (d / boss_->gravityMineRadius);
        const float force = boss_->gravityMineStrength * falloff * static_cast<float>(dt);
        const float inv = 1.0f / d;
        vel.x += dx * inv * force;
        vel.y += dy * inv * force;
    };

    for (const auto& m : boss_->gravityMines) {
        pullOneTo(ball_.position, ball_.velocity, m);
        for (auto& eb : extraBalls_) {
            pullOneTo(eb.position, eb.velocity, m);
        }
    }
}

void GameWorld::updateBallDriftFromGravity(double dt) {
    if (!boss_ || boss_->levelNumber != 40 || boss_->defeated) return;

    const float cx = boss_->position.x + boss_->size.w * 0.5f;
    const float cy = boss_->position.y + boss_->size.h * 0.5f;

    auto updateDriftOne = [&](Ball& ball) {
        if (ball.state == BallState::AttachedToPaddle) {
            ball.gravityExposure = 0.0f;
            return;
        }

        bool inWell = false;

        // 1. Check main gravity field
        if (boss_->gravityFieldEnabled) {
            const float dx = cx - ball.position.x;
            const float dy = cy - ball.position.y;
            const float distSq = dx*dx + dy*dy;
            if (distSq <= boss_->gravityFieldRadius * boss_->gravityFieldRadius) {
                inWell = true;
            }
        }

        // 2. Check gravity mines (only in Phase 4)
        if (boss_->phase == Boss::Phase::Four) {
            for (const auto& m : boss_->gravityMines) {
                const float dx = m.x - ball.position.x;
                const float dy = m.y - ball.position.y;
                if (dx*dx + dy*dy <= boss_->gravityMineRadius * boss_->gravityMineRadius) {
                    inWell = true;
                    break;
                }
            }
        }

        // 3. Check singularity pulse (if active and ball is close to the propagating wave front)
        if (boss_->singularityPulse.active) {
            const float dx = cx - ball.position.x;
            const float dy = cy - ball.position.y;
            const float dist = std::sqrt(dx*dx + dy*dy);
            if (std::abs(dist - boss_->singularityPulse.currentRadius) <= 30.0f) {
                inWell = true;
            }
        }

        if (inWell) {
            // Accumulate exposure: max out at 2.0s
            ball.gravityExposure = std::min(2.0f, ball.gravityExposure + static_cast<float>(dt * 1.5f));
        } else {
            // Decay exposure when safe
            ball.gravityExposure = std::max(0.0f, ball.gravityExposure - static_cast<float>(dt * 0.5f));
        }

        if (ball.gravityExposure > 0.0f) {
            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> distAngle(0.0f, 6.2831853f);
            std::uniform_real_distribution<float> distJitter(-1.0f, 1.0f);

            // A. Chaotic velocity drift: grows with exposure up to 180 px/s^2
            const float angle = distAngle(rng);
            const float driftForce = ball.gravityExposure * 90.0f;
            ball.velocity.x += std::cos(angle) * driftForce * static_cast<float>(dt);
            ball.velocity.y += std::sin(angle) * driftForce * static_cast<float>(dt);

            // B. Visual jitter: direct micro-perturbation of spatial coordinates
            const float jitterAmount = ball.gravityExposure * 2.0f; // up to 4px
            ball.position.x += distJitter(rng) * jitterAmount * static_cast<float>(dt * 60.0f);
            ball.position.y += distJitter(rng) * jitterAmount * static_cast<float>(dt * 60.0f);

            // Sync with physics body
            if (physics_) {
                physics_->teleportBallForTesting(ball.entity, ball.position, ball.velocity);
            }
        }
    };

    updateDriftOne(ball_);
    for (auto& eb : extraBalls_) {
        updateDriftOne(eb);
    }
}

void GameWorld::updateHomingLogic(double dt) {
    if (!boss_ || boss_->levelNumber != 40) return;
    if (boss_->phase != Boss::Phase::Four) return;
    if (boss_->defeated) return;

    auto steerTowards = [&](BossProjectile& projectile, Vec2 targetPos) {
        const float dx = targetPos.x - projectile.position.x;
        const float dy = targetPos.y - projectile.position.y;
        const float d = std::sqrt(dx*dx + dy*dy);
        if (d < 0.001f) return;
        const float tx = (dx / d) * boss_->homingTargetSpeed;
        const float ty = (dy / d) * boss_->homingTargetSpeed;
        const float turn = boss_->homingTurnRate * static_cast<float>(dt);
        projectile.velocity.x += (tx - projectile.velocity.x) * turn;
        projectile.velocity.y += (ty - projectile.velocity.y) * turn;
        projectile.velocity.x = std::clamp(projectile.velocity.x, -600.0f, 600.0f);
        projectile.velocity.y = std::clamp(projectile.velocity.y, -600.0f, 600.0f);
    };

    for (auto& p : boss_->projectiles) {
        if (!p.alive || !p.isHoming) continue;
        const float dx = ball_.position.x - p.position.x;
        const float dy = ball_.position.y - p.position.y;
        const float dist = std::sqrt(dx*dx + dy*dy);
        if (dist > boss_->homingMaxRange) continue;
        steerTowards(p, ball_.position);
    }
}

void GameWorld::applyBallDroneHitIfAny() {
    if (!boss_) return;
    auto& b = *boss_;

    auto checkOne = [&](Ball& ball) {
        for (auto& d : b.drones) {
            if (!d.alive) continue;
            const float dx = ball.position.x - (d.position.x + d.size.w * 0.5f);
            const float dy = ball.position.y - (d.position.y + d.size.h * 0.5f);
            const float r = ball.radius + std::min(d.size.w, d.size.h) * 0.5f;
            if (dx*dx + dy*dy > r * r) continue;

            // Bounce ball
            bool reflectX = std::abs(dx) > std::abs(dy);
            if (reflectX) ball.velocity.x = -ball.velocity.x;
            else          ball.velocity.y = -ball.velocity.y;
            if (physics_) physics_->setBallVelocity(ball.entity, ball.velocity);

            int damage = 1;
            if (isBonusActive("ENERGY_BALLS")) damage = 3;
            if (isBonusActive("WEAK_BALLS")) damage = 0;
            if (damage == 0) return;

            d.currentHealth -= damage;
            score_.add(b.pointsPerDrone);
            d.hitFlashRemainingSeconds = 0.15;
            queueAudioEvent(AudioEventType::BossHit, d.position, d.size);

            if (d.currentHealth <= 0) {
                d.currentHealth = 0;
                d.alive = false;
                if (b.droneDropOnDestroyChanceEnabled) {
                    thread_local std::mt19937 rng(std::random_device{}());
                    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
                    if (chance(rng) < 0.30f) {
                        spawnRandomBonusAt(Vec2{
                            d.position.x + d.size.w * 0.5f,
                            d.position.y + d.size.h * 0.5f
                        });
                    }
                }
            }
            return;  // one hit per tick per ball
        }
    };

    checkOne(ball_);
    for (auto& eb : extraBalls_) checkOne(eb);
}

void GameWorld::spawnRandomBonusAt(Vec2 centerPos) {
    // Boss-3 drone loot follows the same convention as boss 1 and boss 2:
    // exclusively a penalty.
    static const std::array<std::string, 5> kNegativeChoices = {
        "DECREASE_PADDLE", "FAST_BALLS", "WEAK_BALLS",
        "FROZEN_PADDLE", "CHAOTIC_BALLS"
    };
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<std::size_t> d(0, kNegativeChoices.size() - 1);
    const std::string pick = kNegativeChoices[d(rng)];

    // Honour enabledBonuses filter so we don't spawn something the test rig disabled
    for (const auto& e : enabledBonuses_) {
        if (e == pick) {
            spawnBonus(pick, centerPos);
            return;
        }
    }
}

void GameWorld::handleLaserLifeLossIfAny() {
    if (!boss_) return;
    auto& b = *boss_;

    const float laserLeft  = b.laserTargetX - b.laserWidth * 0.5f;
    const float laserRight = b.laserTargetX + b.laserWidth * 0.5f;

    const Paddle& pad = paddle_;
    const float padLeft  = pad.position.x;
    const float padRight = pad.position.x + pad.size.w;

    // Beam is tall enough that the laser always covers the row of the paddle
    // (the boss sits in the upper quarter of the stage, far above the paddle).
    if (laserRight >= padLeft && laserLeft <= padRight) {
        if (isBonusActive("BONUS_WALL")) {
            queueAudioEvent(AudioEventType::BossShieldBlock, paddle_.position, paddle_.size);
        } else {
            queueAudioEvent(AudioEventType::BossProjectileHitPaddle, paddle_.position, paddle_.size);
            handleLaserLifeLoss();
        }
    }
}

void GameWorld::handleLaserLifeLoss() {
    // Same grace-period rule as a regular respawn: pause boss attacks and
    // dismiss the beam so the just-respawned paddle is safe.
    bossAttackCooldownSeconds_ = std::max(
        bossAttackCooldownSeconds_,
        respawnLaunchDelaySeconds + 1.5);
    clearBossAttackState();
    if (lives_.loseLife()) {
        queueAudioEvent(AudioEventType::LifeLost, paddle_.position);
        phase_ = GamePhase::LifeLost;
        respawnLaunchDelayRemaining_ = respawnLaunchDelaySeconds;
        autoLaunchCountdown_ = 5.0;
        attachBallToPaddle();
        if (physics_) physics_->syncAttachedBall(ball_);
    } else {
        queueAudioEvent(AudioEventType::GameOver, ball_.position);
        phase_ = GamePhase::GameOver;
        respawnLaunchDelayRemaining_ = 0.0;
        ball_.state = BallState::AttachedToPaddle;
        ball_.velocity = Vec2{};
        if (physics_) physics_->syncAttachedBall(ball_);
    }
}

void GameWorld::onBossSectionDestroyed(int sectionIdx) {
    if (!boss_) return;
    
    // SFX
    const auto& sec = boss_->sections[sectionIdx];
    queueAudioEvent(AudioEventType::BossSectionDestroyed, 
                    Vec2{boss_->position.x + sec.localBounds.left, boss_->position.y + sec.localBounds.top},
                    Size{sec.localBounds.right - sec.localBounds.left, sec.localBounds.bottom - sec.localBounds.top},
                    std::to_string(sectionIdx + 1));

    if (sectionIdx == 1) return; // Central section exception

    const std::array<const char*, 5> kBossNegativeBonuses = {
        "DECREASE_PADDLE",
        "FAST_BALLS",
        "WEAK_BALLS",
        "FROZEN_PADDLE",
        "CHAOTIC_BALLS",
    };
    std::vector<std::string> valid;
    for (const auto& eb : enabledBonuses_) {
        for (const auto& nb : kBossNegativeBonuses) {
            if (eb == std::string(nb)) valid.push_back(eb);
        }
    }
    
    if (!valid.empty()) {
        thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<std::size_t> d(0, valid.size() - 1);
        const auto& s = boss_->sections[sectionIdx];
        Vec2 center{
            boss_->position.x + s.localBounds.left + (s.localBounds.right - s.localBounds.left) * 0.5f,
            boss_->position.y + s.localBounds.top + (s.localBounds.bottom - s.localBounds.top) * 0.5f
        };
        spawnBonus(valid[d(rng)], center);
    }
}

void GameWorld::applyBallBossHitIfAny() {
    if (!boss_ || boss_->defeated) return;

    auto checkOne = [&](Ball& ball) {
        const double nowSeconds = bossHitClockSeconds_;
        const auto nowIt = lastBossHitByBall_.find(ball.entity);
        const double lastHit = (nowIt == lastBossHitByBall_.end()) ? -1000.0 : nowIt->second;
        const bool throttled = (nowSeconds - lastHit) < bossHitCooldownSeconds;

        if (boss_->levelNumber == 50) {
            Vec2 bossCenter = { boss_->position.x + boss_->size.w * 0.5f, boss_->position.y + boss_->size.h * 0.5f };
            for (auto& shard : boss_->paradoxShards) {
                if (shard.alive) {
                    Vec2 shardPos = bossCenter + shard.orbitOffset;
                    const float bx = shardPos.x - shard.size.w * 0.5f;
                    const float by = shardPos.y - shard.size.h * 0.5f;
                    const float bw = shard.size.w;
                    const float bh = shard.size.h;

                    const float closestX = std::clamp(ball.position.x, bx, bx + bw);
                    const float closestY = std::clamp(ball.position.y, by, by + bh);
                    const float dx = ball.position.x - closestX;
                    const float dy = ball.position.y - closestY;
                    const float distSq = dx*dx + dy*dy;

                    if (distSq <= ball.radius * ball.radius) {
                        float nx = 0.0f;
                        float ny = 0.0f;
                        float penetration = 0.0f;

                        if (distSq > 0.0001f) {
                            float dist = std::sqrt(distSq);
                            nx = dx / dist;
                            ny = dy / dist;
                            penetration = ball.radius - dist;
                        } else {
                            float overlapLeft = ball.position.x - bx;
                            float overlapRight = (bx + bw) - ball.position.x;
                            float overlapTop = ball.position.y - by;
                            float overlapBottom = (by + bh) - ball.position.y;
                            float minOverlap = std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});
                            if (minOverlap == overlapLeft) { nx = -1.0f; ny = 0.0f; penetration = overlapLeft + ball.radius; }
                            else if (minOverlap == overlapRight) { nx = 1.0f; ny = 0.0f; penetration = overlapRight + ball.radius; }
                            else if (minOverlap == overlapTop) { nx = 0.0f; ny = -1.0f; penetration = overlapTop + ball.radius; }
                            else { nx = 0.0f; ny = 1.0f; penetration = overlapBottom + ball.radius; }
                        }

                        ball.position.x += nx * penetration;
                        ball.position.y += ny * penetration;
                        constexpr float kSideWallEpsilon = 1.5f;
                        ball.position.x += nx * kSideWallEpsilon;
                        ball.position.y += ny * kSideWallEpsilon;

                        float dot = ball.velocity.x * nx + ball.velocity.y * ny;
                        if (dot < 0.0f) {
                            ball.velocity.x -= 2.0f * dot * nx;
                            ball.velocity.y -= 2.0f * dot * ny;
                        }

                        if (physics_) {
                            physics_->teleportBallForTesting(ball.entity, ball.position, ball.velocity);
                        }

                        lastBossHitByBall_[ball.entity] = nowSeconds;

                        if (!throttled) {
                            int shardDamage = 1;
                            if (isBonusActive("ENERGY_BALLS")) shardDamage = 10;
                            if (isBonusActive("WEAK_BALLS")) shardDamage = 0;

                            if (shardDamage > 0) {
                                shard.currentHealth = std::max(0, shard.currentHealth - shardDamage);
                                shard.hitFlashRemainingSeconds = 0.15;
                                queueAudioEvent(AudioEventType::BossHit, shardPos, shard.size);

                                if (shard.currentHealth <= 0) {
                                    shard.alive = false;
                                    shard.respawnRemainingSeconds = boss_->shardRespawnSeconds;
                                    score_.add(750);
                                    spawnTimeRift(TimeRiftKind::Rewind, shardPos);
                                    queueAudioEvent(AudioEventType::BossShieldBlock, shardPos, shard.size);
                                }
                            }
                        }
                        return;
                    }
                }
            }
        }

        int hitSection = -1;
        if (boss_->levelNumber == 50) {
            hitSection = 0;
        } else {
            hitSection = findHitSectionIdx();
        }

        // Wait, if it's boss 1, we still need AABB check logic. Boss 1 sections = 1, we still want it to bounce off the main AABB even if all sections are destroyed (though boss 1 section never destroyed).
        // Let's implement the phantom AABB bounce.
        const float bx = boss_->position.x;
        const float by = boss_->position.y;
        const float bw = boss_->size.w;
        const float bh = boss_->size.h;

        const float closestX = std::clamp(ball.position.x, bx, bx + bw);
        const float closestY = std::clamp(ball.position.y, by, by + bh);
        const float dx = ball.position.x - closestX;
        const float dy = ball.position.y - closestY;
        const float distSq = dx*dx + dy*dy;

        bool phantomHit = false;
        if (distSq <= ball.radius * ball.radius) {
            phantomHit = true;
        }

        if (!phantomHit) return;

        // Throttle: if the same ball damaged the boss less than the cooldown
        // window ago, this contact is treated as a grazing bounce rather than
        // a fresh strike. We still apply the bounce so the ball escapes the
        // hitbox cleanly (the position correction below pushes it out), but
        // no HP is removed.

        // Bounce logic (applies even if phantom hit, i.e., hitSection < 0)
        float nx = 0.0f;
        float ny = 0.0f;
        float penetration = 0.0f;

        if (distSq > 0.0001f) {
            float dist = std::sqrt(distSq);
            nx = dx / dist;
            ny = dy / dist;
            penetration = ball.radius - dist;
        } else {
            float overlapLeft = ball.position.x - bx;
            float overlapRight = (bx + bw) - ball.position.x;
            float overlapTop = ball.position.y - by;
            float overlapBottom = (by + bh) - ball.position.y;

            float minOverlap = std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});

            if (minOverlap == overlapLeft) { nx = -1.0f; ny = 0.0f; penetration = overlapLeft + ball.radius; }
            else if (minOverlap == overlapRight) { nx = 1.0f; ny = 0.0f; penetration = overlapRight + ball.radius; }
            else if (minOverlap == overlapTop) { nx = 0.0f; ny = -1.0f; penetration = overlapTop + ball.radius; }
            else { nx = 0.0f; ny = 1.0f; penetration = overlapBottom + ball.radius; }
        }

        ball.position.x += nx * penetration;
        ball.position.y += ny * penetration;

        // Push the ball a touch further out than pure penetration so the next
        // physics tick cannot re-register us inside the hitbox when the
        // collision happens near a flat side wall. This is the cheap analogue
        // of Box2D's CCD and prevents the "stuck-along-the-edge" exploit.
        constexpr float kSideWallEpsilon = 1.5f;
        ball.position.x += nx * kSideWallEpsilon;
        ball.position.y += ny * kSideWallEpsilon;

        float dot = ball.velocity.x * nx + ball.velocity.y * ny;
        if (dot < 0.0f) {
            ball.velocity.x -= 2.0f * dot * nx;
            ball.velocity.y -= 2.0f * dot * ny;
        }

        // Boss 3 phase 2: every other hit returns a 25% faster ball. We use the
        // hit counter measured by a static thread_local so the test framework can
        // observe the multiplier behaviour without time dependence.
        if (boss_->levelNumber == 30 && boss_->phase == Boss::Phase::Two) {
            thread_local int phase2ReflectCounter = 0;
            phase2ReflectCounter++;
            if (phase2ReflectCounter % 2 == 0) {
                ball.velocity.x *= 1.25f;
                ball.velocity.y *= 1.25f;
            }
        }

        if (physics_) {
            physics_->teleportBallForTesting(ball.entity, ball.position, ball.velocity);
        }

        // Record the contact; whether or not HP is deducted this frame, no
        // further damage should land before the throttle window elapses.
        lastBossHitByBall_[ball.entity] = nowSeconds;

        // If the same ball has already damaged the boss very recently
        // (e.g. it grazed along a side wall), do not deduct HP. The bounce
        // and position correction above already prevent visual sticking, so
        // game-feel is preserved without rewarding extreme grazes.
        if (throttled) return;

        if (hitSection < 0) return;

        if (boss_->shieldActive) {
            queueAudioEvent(AudioEventType::BossShieldBlock, boss_->position, boss_->size);
            return;
        }

        // Boss 3: brief invulnerability right after a teleport — ball bounces but
        // does not damage.
        if (boss_->levelNumber == 30 && boss_->invulnTimeRemaining > 0.0) {
            queueAudioEvent(AudioEventType::BossShieldBlock, boss_->position, boss_->size);
            return;
        }

        int damage = 1;
        if (isBonusActive("ENERGY_BALLS")) damage = 10;
        if (isBonusActive("WEAK_BALLS")) damage = 0;

        if (damage == 0) return;

        if (boss_->levelNumber == 50) {
            const int prevHp = boss_->currentHealth;
            boss_->currentHealth = std::max(0, boss_->currentHealth - damage);
            boss_->prevHealthAtHitForDropCheck = boss_->currentHealth;
            score_.add(boss_->pointsPerHit);
            queueAudioEvent(AudioEventType::BossHit, boss_->position, boss_->size);
            boss_->hitFlashRemainingSeconds = 0.15;

            // Phase transitions
            if (prevHp > boss_->phase2HpThreshold && boss_->currentHealth <= boss_->phase2HpThreshold) {
                enterChronarchPhase2();
            }
            if (prevHp > boss_->phase3HpThreshold && boss_->currentHealth <= boss_->phase3HpThreshold) {
                enterChronarchPhase3();
            }
            if (prevHp > boss_->phase4HpThreshold && boss_->currentHealth <= boss_->phase4HpThreshold) {
                enterChronarchPhase4();
            }

            // Drop check on negative bonuses: 75% (210), 50% (140), 25% (70), 10% (28)
            if ((prevHp > boss_->phase2HpThreshold && boss_->currentHealth <= boss_->phase2HpThreshold) ||
                (prevHp > boss_->phase3HpThreshold && boss_->currentHealth <= boss_->phase3HpThreshold) ||
                (prevHp > boss_->phase4HpThreshold && boss_->currentHealth <= boss_->phase4HpThreshold) ||
                (prevHp > 28 && boss_->currentHealth <= 28)) {
                spawnChronarchNegativeBonus(ball.position);
            }

            if (boss_->currentHealth <= 0) {
                boss_->currentHealth = 0;
                boss_->defeated = true;
                score_.add(boss_->pointsOnDefeat);
                queueAudioEvent(AudioEventType::BossDefeated, boss_->position, boss_->size);
                clearChronarchState();
            }
            return;
        }

        // Boss 3/4 dedicated path: single shared HP pool, but with phase logic and
        // threshold-driven bonus drops.
        if (boss_->levelNumber == 30 || boss_->levelNumber == 40) {
            const int prevHp = boss_->currentHealth;
            boss_->currentHealth = std::max(0, boss_->currentHealth - damage);
            boss_->prevHealthAtHitForDropCheck = boss_->currentHealth;
            score_.add(boss_->pointsPerHit);
            queueAudioEvent(AudioEventType::BossHit, boss_->position, boss_->size);
            boss_->hitFlashRemainingSeconds = 0.15;
            boss_->crystalFlashAlpha = 1.0f;

            const float frac = boss_->levelNumber == 40 ? 0.66f : 0.70f;
            const int hi66 = static_cast<int>(boss_->maxHealth * frac);
            const float frac40 = boss_->levelNumber == 40 ? 0.33f : 0.40f;
            const int hi40 = static_cast<int>(boss_->maxHealth * frac40);
            const float frac10 = 0.10f;
            const int hi10 = static_cast<int>(boss_->maxHealth * frac10);

            if ((prevHp > hi66 && boss_->currentHealth <= hi66) ||
                (prevHp > hi40 && boss_->currentHealth <= hi40) ||
                (prevHp > hi10 && boss_->currentHealth <= hi10)) {
                onPhaseThresholdCrossed();
            }

            if (boss_->levelNumber == 40) {
                // Singularity: 4 phases by HP%.
                const int hp66 = static_cast<int>(boss_->maxHealth * 0.66f);
                const int hp33 = static_cast<int>(boss_->maxHealth * 0.33f);
                const int hp10b = static_cast<int>(boss_->maxHealth * 0.10f);
                if (boss_->currentHealth <= hp66 && !boss_->phase2Transitioned) {
                    enterBossPhase2();
                }
                if (boss_->currentHealth <= hp33 && !boss_->phase3Transitioned) {
                    enterBossPhaseThree();
                }
                if (boss_->currentHealth <= hp10b && !boss_->phase4Transitioned) {
                    enterBossPhaseFour();
                }
            } else if (boss_->currentHealth <= static_cast<int>(
                           boss_->maxHealth * boss_->phase2ThresholdFraction) &&
                       !boss_->phase2Transitioned) {
                enterBossPhase2();
            }

            if (boss_->currentHealth <= 0) {
                boss_->currentHealth = 0;
                boss_->defeated = true;
                score_.add(boss_->pointsOnDefeat);
                queueAudioEvent(AudioEventType::BossDefeated, boss_->position, boss_->size);
            }
            return;
        }

        // Boss 1 specific drop logic for backward compatibility
        if (boss_->sectionCount == 1) {
            int oldHealth = boss_->currentHealth;
            boss_->currentHealth -= damage;
            boss_->currentHealth = std::max(0, boss_->currentHealth);
            score_.add(boss_->pointsPerHit);
            queueAudioEvent(AudioEventType::BossHit, boss_->position, boss_->size);
            boss_->hitFlashRemainingSeconds = 0.15;

            auto spawnNegativeBonus = [&]() {
                const std::array<const char*, 5> kBossNegativeBonuses = {
                    "DECREASE_PADDLE", "FAST_BALLS", "WEAK_BALLS", "FROZEN_PADDLE", "CHAOTIC_BALLS",
                };
                std::vector<std::string> valid;
                for (const auto& eb : enabledBonuses_) {
                    for (const auto& nb : kBossNegativeBonuses) {
                        if (eb == std::string(nb)) valid.push_back(eb);
                    }
                }
                if (!valid.empty()) {
                    thread_local std::mt19937 rng(std::random_device{}());
                    std::uniform_int_distribution<std::size_t> d(0, valid.size() - 1);
                    Vec2 center{boss_->position.x + boss_->size.w * 0.5f, boss_->position.y + boss_->size.h * 0.5f};
                    spawnBonus(valid[d(rng)], center);
                }
            };

            if (oldHealth > 20 && boss_->currentHealth <= 20) spawnNegativeBonus();
            if (oldHealth > 10 && boss_->currentHealth <= 10) spawnNegativeBonus();

            if (boss_->currentHealth <= 0) {
                boss_->currentHealth = 0;
                boss_->defeated = true;
                score_.add(boss_->pointsOnDefeat);
                queueAudioEvent(AudioEventType::BossDefeated, boss_->position, boss_->size);
            }
            return;
        }

        // Boss 2 logic
        boss_->sectionHealth[hitSection] -= damage;
        boss_->currentHealth -= damage;
        score_.add(boss_->pointsPerHit);
        queueAudioEvent(AudioEventType::BossHit, boss_->position, boss_->size);
        boss_->hitFlashRemainingSeconds = 0.15;

        if (boss_->sectionHealth[hitSection] <= 0) {
            boss_->sectionHealth[hitSection] = 0;
            boss_->sections[hitSection].alive = false;
            onBossSectionDestroyed(hitSection);
        }

        if (boss_->currentHealth <= 0) {
            boss_->currentHealth = 0;
            boss_->defeated = true;
            score_.add(boss_->pointsOnDefeat);
            queueAudioEvent(AudioEventType::BossDefeated, boss_->position, boss_->size);
        }
    };

    checkOne(ball_);
    for (auto& eb : extraBalls_) checkOne(eb);
}

const char* toString(GamePhase phase) noexcept {
    switch (phase) {
    case GamePhase::Ready:
        return "ready";
    case GamePhase::Playing:
        return "playing";
    case GamePhase::Paused:
        return "paused";
    case GamePhase::LevelComplete:
        return "level_complete";
    case GamePhase::LifeLost:
        return "life_lost";
    case GamePhase::GameOver:
        return "game_over";
    }

    return "unknown";
}

bool isBonusEnabled(const std::vector<std::string>& enabledBonuses, std::string_view type) {
    if (enabledBonuses.empty()) {
        return true;
    }
    return std::find(enabledBonuses.begin(), enabledBonuses.end(), type) != enabledBonuses.end();
}

std::optional<std::string> pickRainbowBountyDrop(const std::vector<std::string>& enabledBonuses) {
    static constexpr std::array<const char*, 16> restrictedPool{{
        "BONUS_SCORE",
        "BONUS_SCORE_200",
        "BONUS_SCORE_500",
        "EXTRA_LIFE",
        "BONUS_BALL",
        "CALL_BALL",
        "SLOW_BALLS",
        "SCORE_RAIN",
        "ENERGY_BALLS",
        "EXPLOSION_BALLS",
        "INCREASE_PADDLE",
        "STICKY_PADDLE",
        "PLASMA_WEAPON",
        "BONUS_WALL",
        "BONUS_MAGNET",
        "ADD_FIVE_SECONDS"
    }};

    std::vector<std::string> candidates;
    candidates.reserve(restrictedPool.size());
    for (const char* b : restrictedPool) {
        if (isBonusEnabled(enabledBonuses, b)) {
            candidates.emplace_back(b);
        }
    }

    if (candidates.empty()) {
        return std::nullopt;
    }

    thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<std::size_t> selectDist(0, candidates.size() - 1);
    return candidates[selectDist(generator)];
}

std::optional<std::string> pickBloodTitheDrop(const std::vector<std::string>& enabledBonuses) {
    static constexpr std::array<const char*, 8> restrictedPool{{
        "CHAOTIC_BALLS",
        "FROZEN_PADDLE",
        "DECREASE_PADDLE",
        "FAST_BALLS",
        "PENALTIES_MAGNET",
        "WEAK_BALLS",
        "INVISIBLE_PADDLE",
        "DARKNESS"
    }};

    std::vector<std::string> candidates;
    candidates.reserve(restrictedPool.size());
    for (const char* b : restrictedPool) {
        if (isBonusEnabled(enabledBonuses, b)) {
            candidates.emplace_back(b);
        }
    }

    if (candidates.empty()) {
        return std::nullopt;
    }

    thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<std::size_t> selectDist(0, candidates.size() - 1);
    return candidates[selectDist(generator)];
}

void GameWorld::initBossLevelFive_Chronarch() {
    boss_ = std::make_unique<Boss>();
    boss_->levelNumber = 50;
    boss_->size = Size{320.0f, 220.0f};
    boss_->entity = spawn(EntityKind::Boss);
    boss_->maxHealth = 280;
    boss_->currentHealth = 280;
    boss_->pointsPerHit = 150;
    boss_->pointsOnDefeat = 55000;
    boss_->moveAmplitude = 420.0f;
    boss_->moveSpeed = 240.0f;
    boss_->edgePauseSeconds = 0.25;

    // Phase thresholds:
    boss_->phase1HpThreshold = 280;
    boss_->phase2HpThreshold = 210; // 75%
    boss_->phase3HpThreshold = 140; // 50%
    boss_->phase4HpThreshold = 70;  // 25%

    // Chronarch fields:
    boss_->timeSnapshotIntervalSeconds = 0.25;
    boss_->timeSnapshotAccumulatorSeconds = 0.0;
    boss_->timeSnapshotHistorySeconds = 6.0;
    boss_->rewindLookbackSeconds = 1.25;

    boss_->timeRiftSpawnIntervalSeconds = 4.5;
    boss_->timeRiftSpawnTimerSeconds = 2.0;
    boss_->maxTimeRifts = 4;
    boss_->timeRiftSlowMultiplier = 0.62f;
    boss_->timeRiftHasteMultiplier = 1.28f;

    boss_->clockHandAttackIntervalSeconds = 5.0;
    boss_->clockHandAttackTimerSeconds = 3.0;
    boss_->clockHandBaseAngularVelocity = 1.45f;

    boss_->shardRespawnSeconds = 8.0;
    boss_->shardOrbitRadius = 190.0f;
    boss_->shardOrbitAngularVelocity = 0.65f;
    boss_->shardOrbitClockSeconds = 0.0;

    boss_->zeroHourTimerSeconds = 0.0;
    boss_->zeroHourIntervalSeconds = 7.0;
    boss_->zeroHourDurationSeconds = 1.15;
    boss_->zeroHourRemainingSeconds = 0.0;
    boss_->zeroHourBallSpeedMultiplier = 0.35f;

    boss_->sections.clear();
    boss_->sections.push_back(BossSection{
        .entity = spawn(EntityKind::Boss),
        .localBounds = {0, 0, 320, 220}
    });
    boss_->sectionCount = 1;
    boss_->sectionHealth = {280};
    boss_->sectionMaxHealth = {280};
    boss_->prevHealthAtHitForDropCheck = 280;

    boss_->position = Vec2{
        brickStageLeft + brickStageWidth * 0.5f - boss_->size.w * 0.5f,
        150.0f
    };
    boss_->moveDirection = 1;

    // 4 paradox shards
    boss_->paradoxShards.clear();
    for (int i = 0; i < 4; ++i) {
        ParadoxShard shard;
        shard.entity = spawn(EntityKind::Boss);
        float angle = i * 1.570796f;
        shard.orbitOffset = { std::cos(angle) * boss_->shardOrbitRadius, std::sin(angle) * boss_->shardOrbitRadius };
        shard.size = Size{46.0f, 46.0f};
        shard.currentHealth = 3;
        shard.maxHealth = 3;
        shard.alive = true;
        shard.respawnRemainingSeconds = 0.0;
        shard.hitFlashRemainingSeconds = 0.0;
        boss_->paradoxShards.push_back(shard);
    }
}

void GameWorld::spawnChronarchNegativeBonus(Vec2 ballPos) {
    static constexpr std::array<const char*, 8> negativePool = {
        "DECREASE_PADDLE",
        "FAST_BALLS",
        "WEAK_BALLS",
        "FROZEN_PADDLE",
        "CHAOTIC_BALLS",
        "PENALTIES_MAGNET",
        "INVISIBLE_PADDLE",
        "DARKNESS"
    };

    std::vector<std::string> valid;
    for (const auto& eb : enabledBonuses_) {
        for (const auto& nb : negativePool) {
            if (eb == std::string(nb)) {
                valid.push_back(eb);
            }
        }
    }

    if (valid.empty()) return;

    Vec2 bossCenter = { boss_->position.x + boss_->size.w * 0.5f, boss_->position.y + boss_->size.h * 0.5f };
    Vec2 spawnPos = bossCenter;
    float minDistSq = 1e9f;

    for (const auto& shard : boss_->paradoxShards) {
        if (shard.alive) {
            Vec2 shardPos = bossCenter + shard.orbitOffset;
            float dx = shardPos.x - ballPos.x;
            float dy = shardPos.y - ballPos.y;
            float distSq = dx*dx + dy*dy;
            if (distSq < minDistSq) {
                minDistSq = distSq;
                spawnPos = shardPos;
            }
        }
    }

    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<std::size_t> d(0, valid.size() - 1);
    spawnBonus(valid[d(rng)], spawnPos);
}

void GameWorld::rewindSingleBall(Ball& ball, size_t extraBallIndex) {
    if (!boss_ || boss_->timeSnapshots.empty()) return;
    auto& b = *boss_;

    double targetAge = 0.75;
    auto closestIt = b.timeSnapshots.begin();
    double minDiff = std::abs(closestIt->ageSeconds - targetAge);

    for (auto it = b.timeSnapshots.begin() + 1; it != b.timeSnapshots.end(); ++it) {
        double diff = std::abs(it->ageSeconds - targetAge);
        if (diff < minDiff) {
            minDiff = diff;
            closestIt = it;
        }
    }

    if (extraBallIndex == static_cast<size_t>(-1)) {
        ball.position = closestIt->ballPosition;
        ball.velocity = closestIt->ballVelocity;
    } else {
        if (extraBallIndex < closestIt->extraBallPositions.size()) {
            ball.position = closestIt->extraBallPositions[extraBallIndex];
            ball.velocity = closestIt->extraBallVelocities[extraBallIndex];
        } else {
            ball.position = closestIt->ballPosition;
            ball.velocity = closestIt->ballVelocity;
        }
    }

    // Safety offset check
    const float bx = b.position.x;
    const float by = b.position.y;
    const float bw = b.size.w;
    const float bh = b.size.h;

    const float closestX = std::clamp(ball.position.x, bx, bx + bw);
    const float closestY = std::clamp(ball.position.y, by, by + bh);
    const float dx = ball.position.x - closestX;
    const float dy = ball.position.y - closestY;
    const float distSq = dx*dx + dy*dy;

    if (distSq <= ball.radius * ball.radius) {
        float nx = 0.0f;
        float ny = 0.0f;
        float penetration = 0.0f;
        if (distSq > 0.0001f) {
            float dist = std::sqrt(distSq);
            nx = dx / dist;
            ny = dy / dist;
            penetration = ball.radius - dist;
        } else {
            nx = 0.0f;
            ny = -1.0f;
            penetration = ball.radius + 1.0f;
        }
        ball.position.x += nx * (penetration + 5.0f);
        ball.position.y += ny * (penetration + 5.0f);
    }

    const float leftLimit = bounds_.bounds.left + ball.radius;
    const float rightLimit = bounds_.bounds.right - ball.radius;
    const float topLimit = bounds_.bounds.top + ball.radius;
    ball.position.x = std::clamp(ball.position.x, leftLimit, rightLimit);
    ball.position.y = std::max(ball.position.y, topLimit);

    if (physics_) {
        physics_->teleportBallForTesting(ball.entity, ball.position, ball.velocity);
    }
}

void GameWorld::updateChronarch(double dt) {
    if (!boss_) return;

    captureTimeSnapshot(dt);
    updateTimeRifts(dt);
    updateClockHands(dt);
    updateParadoxShards(dt);
    updateZeroHour(dt);

    // Update ball speed multipliers
    auto updateBallSpeedMod = [&](Ball& ball, size_t idx) {
        float currentMod = 1.0f;
        auto it = boss_->ballSpeedMultipliers.find(ball.entity);
        if (it != boss_->ballSpeedMultipliers.end()) {
            currentMod = it->second;
        }

        float targetMod = 1.0f;
        for (auto& rift : boss_->timeRifts) {
            if (!rift.alive) continue;
            float dx = ball.position.x - rift.center.x;
            float dy = ball.position.y - rift.center.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist <= rift.radius) {
                if (rift.kind == TimeRiftKind::Slow) {
                    targetMod *= boss_->timeRiftSlowMultiplier;
                } else if (rift.kind == TimeRiftKind::Haste) {
                    targetMod *= boss_->timeRiftHasteMultiplier;
                } else if (rift.kind == TimeRiftKind::Rewind) {
                    rift.alive = false;
                    rewindSingleBall(ball, idx);
                }
            }
        }

        if (boss_->zeroHourRemainingSeconds > 0.0) {
            targetMod *= boss_->zeroHourBallSpeedMultiplier;
        }

        float rate = 2.5f;
        currentMod += (targetMod - currentMod) * static_cast<float>(rate * dt);
        boss_->ballSpeedMultipliers[ball.entity] = currentMod;
    };

    updateBallSpeedMod(ball_, static_cast<size_t>(-1));
    for (size_t i = 0; i < extraBalls_.size(); ++i) {
        updateBallSpeedMod(extraBalls_[i], i);
    }
}

void GameWorld::captureTimeSnapshot(double dt) {
    if (!boss_) return;
    auto& b = *boss_;

    for (auto& snap : b.timeSnapshots) {
        snap.ageSeconds += dt;
    }

    b.timeSnapshots.erase(
        std::remove_if(b.timeSnapshots.begin(), b.timeSnapshots.end(),
                       [&](const TimeSnapshot& snap) {
                           return snap.ageSeconds > b.timeSnapshotHistorySeconds;
                       }),
        b.timeSnapshots.end()
    );

    b.timeSnapshotAccumulatorSeconds += dt;
    if (b.timeSnapshotAccumulatorSeconds >= b.timeSnapshotIntervalSeconds) {
        b.timeSnapshotAccumulatorSeconds = 0.0;
        TimeSnapshot snap;
        snap.ageSeconds = 0.0;
        snap.ballPosition = ball_.position;
        snap.ballVelocity = ball_.velocity;
        snap.paddlePosition = paddle_.position;
        snap.extraBallPositions.clear();
        snap.extraBallVelocities.clear();
        for (const auto& eb : extraBalls_) {
            snap.extraBallPositions.push_back(eb.position);
            snap.extraBallVelocities.push_back(eb.velocity);
        }
        b.timeSnapshots.push_back(snap);
    }
}

void GameWorld::triggerChronarchRewind() {
    if (!boss_ || boss_->timeSnapshots.empty()) return;
    auto& b = *boss_;

    double targetAge = b.rewindLookbackSeconds;
    auto closestIt = b.timeSnapshots.begin();
    double minDiff = std::abs(closestIt->ageSeconds - targetAge);

    for (auto it = b.timeSnapshots.begin() + 1; it != b.timeSnapshots.end(); ++it) {
        double diff = std::abs(it->ageSeconds - targetAge);
        if (diff < minDiff) {
            minDiff = diff;
            closestIt = it;
        }
    }

    ball_.position = closestIt->ballPosition;
    ball_.velocity = closestIt->ballVelocity;

    size_t extraCount = std::min(closestIt->extraBallPositions.size(), extraBalls_.size());
    for (size_t i = 0; i < extraCount; ++i) {
        extraBalls_[i].position = closestIt->extraBallPositions[i];
        extraBalls_[i].velocity = closestIt->extraBallVelocities[i];
    }

    auto preventSticking = [&](Ball& ball) {
        const float bx = b.position.x;
        const float by = b.position.y;
        const float bw = b.size.w;
        const float bh = b.size.h;

        const float closestX = std::clamp(ball.position.x, bx, bx + bw);
        const float closestY = std::clamp(ball.position.y, by, by + bh);
        const float dx = ball.position.x - closestX;
        const float dy = ball.position.y - closestY;
        const float distSq = dx*dx + dy*dy;

        if (distSq <= ball.radius * ball.radius) {
            float nx = 0.0f;
            float ny = 0.0f;
            float penetration = 0.0f;

            if (distSq > 0.0001f) {
                float dist = std::sqrt(distSq);
                nx = dx / dist;
                ny = dy / dist;
                penetration = ball.radius - dist;
            } else {
                nx = 0.0f;
                ny = -1.0f;
                penetration = ball.radius + 1.0f;
            }

            ball.position.x += nx * (penetration + 5.0f);
            ball.position.y += ny * (penetration + 5.0f);
        }

        const float leftLimit = bounds_.bounds.left + ball.radius;
        const float rightLimit = bounds_.bounds.right - ball.radius;
        const float topLimit = bounds_.bounds.top + ball.radius;
        ball.position.x = std::clamp(ball.position.x, leftLimit, rightLimit);
        ball.position.y = std::max(ball.position.y, topLimit);

        if (physics_) {
            physics_->teleportBallForTesting(ball.entity, ball.position, ball.velocity);
        }
    };

    preventSticking(ball_);
    for (auto& eb : extraBalls_) {
        preventSticking(eb);
    }
}

static float distancePointToAABB(Vec2 pt, float left, float top, float right, float bottom) {
    float closestX = std::clamp(pt.x, left, right);
    float closestY = std::clamp(pt.y, top, bottom);
    float dx = pt.x - closestX;
    float dy = pt.y - closestY;
    return std::sqrt(dx*dx + dy*dy);
}

void GameWorld::updateClockHands(double dt) {
    if (!boss_) return;
    auto& b = *boss_;

    b.clockHandAttackTimerSeconds += dt;
    if (b.clockHandAttackTimerSeconds >= b.clockHandAttackIntervalSeconds) {
        b.clockHandAttackTimerSeconds = 0.0;
        int count = 1;
        if (b.currentHealth <= b.phase4HpThreshold) {
            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<int> dist(2, 3);
            count = dist(rng);
        } else if (b.currentHealth <= b.phase3HpThreshold) {
            count = 2;
        } else if (b.currentHealth <= b.phase2HpThreshold) {
            count = 2;
        } else {
            count = 1;
        }
        spawnClockHandAttack(count);
    }

    for (auto& hand : b.clockHands) {
        hand.ageSeconds += dt;
        hand.angleRadians += hand.angularVelocity * static_cast<float>(dt);

        if (hand.telegraphing) {
            if (hand.ageSeconds >= hand.telegraphSeconds) {
                hand.telegraphing = false;
                hand.active = true;
                queueAudioEvent(AudioEventType::BossLaserFire, b.position, b.size);
            }
        } else if (hand.active) {
            if (hand.ageSeconds >= hand.telegraphSeconds + hand.activeSeconds) {
                hand.active = false;
            } else {
                if (!hand.appliedThisCycle) {
                    bool hit = false;
                    Vec2 startPoint = { b.position.x + b.size.w * 0.5f, b.position.y + b.size.h * 0.5f };
                    Vec2 endPoint = { startPoint.x + std::cos(hand.angleRadians) * hand.length, startPoint.y + std::sin(hand.angleRadians) * hand.length };

                    float pLeft = paddle_.position.x;
                    float pRight = paddle_.position.x + paddle_.size.w;
                    float pTop = paddle_.position.y;
                    float pBottom = paddle_.position.y + paddle_.size.h;

                    for (int step = 0; step <= 100; ++step) {
                        float t = step / 100.0f;
                        Vec2 pt = { startPoint.x + t * (endPoint.x - startPoint.x), startPoint.y + t * (endPoint.y - startPoint.y) };
                        if (distancePointToAABB(pt, pLeft, pTop, pRight, pBottom) <= hand.width * 0.5f) {
                            hit = true;
                            break;
                        }
                    }

                    if (hit) {
                        hand.appliedThisCycle = true;
                        if (isBonusActive("BONUS_WALL")) {
                            std::erase_if(activeBonusTimers_, [](const ActiveBonusTimer& t) { return t.type == "BONUS_WALL"; });
                            queueAudioEvent(AudioEventType::BossShieldBlock, paddle_.position, paddle_.size);
                        } else {
                            queueAudioEvent(AudioEventType::BossProjectileHitPaddle, paddle_.position, paddle_.size);
                            handleProjectileLifeLoss();
                            return;
                        }
                    }
                }

                auto applyImpulse = [&](Ball& ball) {
                    Vec2 startPoint = { b.position.x + b.size.w * 0.5f, b.position.y + b.size.h * 0.5f };
                    Vec2 endPoint = { startPoint.x + std::cos(hand.angleRadians) * hand.length, startPoint.y + std::sin(hand.angleRadians) * hand.length };

                    Vec2 segment = endPoint - startPoint;
                    float segLenSq = segment.x * segment.x + segment.y * segment.y;
                    float t = 0.0f;
                    if (segLenSq > 0.0001f) {
                        t = ((ball.position.x - startPoint.x) * segment.x + (ball.position.y - startPoint.y) * segment.y) / segLenSq;
                        t = std::clamp(t, 0.0f, 1.0f);
                    }
                    Vec2 closestPoint = { startPoint.x + segment.x * t, startPoint.y + segment.y * t };
                    float dx = ball.position.x - closestPoint.x;
                    float dy = ball.position.y - closestPoint.y;
                    float distSq = dx*dx + dy*dy;
                    float limitDist = ball.radius + hand.width * 0.5f;

                    if (distSq <= limitDist * limitDist) {
                        float signVel = hand.angularVelocity >= 0.0f ? 1.0f : -1.0f;
                        Vec2 perp = { -std::sin(hand.angleRadians) * signVel, std::cos(hand.angleRadians) * signVel };
                        ball.velocity.x += perp.x * 180.0f;
                        ball.velocity.y += perp.y * 180.0f;
                        if (physics_) {
                            physics_->setBallVelocity(ball.entity, ball.velocity);
                        }
                    }
                };

                applyImpulse(ball_);
                for (auto& eb : extraBalls_) {
                    applyImpulse(eb);
                }
            }
        }
    }

    b.clockHands.erase(
        std::remove_if(b.clockHands.begin(), b.clockHands.end(),
                       [](const ClockHandBeam& hand) {
                           return hand.ageSeconds >= hand.telegraphSeconds + hand.activeSeconds;
                       }),
        b.clockHands.end()
    );
}

void GameWorld::spawnClockHandAttack(int handCount) {
    if (!boss_) return;
    auto& b = *boss_;

    int currentActive = static_cast<int>(b.clockHands.size());
    int maxAllowedNew = 3 - currentActive;
    if (handCount > maxAllowedNew) {
        handCount = maxAllowedNew;
    }
    if (handCount <= 0) return;

    queueAudioEvent(AudioEventType::BossLaserCharge, b.position, b.size);

    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);

    float baseAngle = angleDist(rng);

    for (int i = 0; i < handCount; ++i) {
        ClockHandBeam hand;
        hand.telegraphSeconds = 0.8;
        hand.activeSeconds = 1.4;
        hand.ageSeconds = 0.0;
        hand.telegraphing = true;
        hand.active = false;
        hand.appliedThisCycle = false;
        hand.length = 720.0f;
        hand.width = 18.0f;

        float sign = (i % 2 == 0) ? 1.0f : -1.0f;
        hand.angularVelocity = b.clockHandBaseAngularVelocity * sign;

        if (handCount == 2 && b.currentHealth <= b.phase3HpThreshold && b.currentHealth > b.phase4HpThreshold) {
            hand.angleRadians = baseAngle + i * 1.570796f;
        } else {
            hand.angleRadians = baseAngle + i * (6.28318f / handCount);
        }

        b.clockHands.push_back(hand);
    }
}

void GameWorld::updateTimeRifts(double dt) {
    if (!boss_) return;
    auto& b = *boss_;

    if (b.zeroHourRemainingSeconds <= 0.0) {
        b.timeRiftSpawnTimerSeconds += dt;
        if (b.timeRiftSpawnTimerSeconds >= b.timeRiftSpawnIntervalSeconds) {
            b.timeRiftSpawnTimerSeconds = 0.0;

            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            
            float hasteChance = (b.currentHealth <= b.phase2HpThreshold) ? 0.6f : 0.5f;
            TimeRiftKind kind = (dist(rng) < hasteChance) ? TimeRiftKind::Haste : TimeRiftKind::Slow;

            float radius = 90.0f;
            float minX = bounds_.bounds.left + radius;
            float maxX = bounds_.bounds.right - radius;
            float minY = bounds_.bounds.top + radius + 50.0f;
            float maxY = paddle_.position.y - 120.0f;
            if (maxY < minY) {
                maxY = minY + 10.0f;
            }

            std::uniform_real_distribution<float> posX(minX, maxX);
            std::uniform_real_distribution<float> posY(minY, maxY);
            Vec2 center = { posX(rng), posY(rng) };

            spawnTimeRift(kind, center);
        }
    }

    for (auto& rift : b.timeRifts) {
        rift.ageSeconds += dt;
        if (rift.ageSeconds >= rift.lifetimeSeconds) {
            rift.alive = false;
        }
    }

    b.timeRifts.erase(
        std::remove_if(b.timeRifts.begin(), b.timeRifts.end(),
                       [](const TimeRift& rift) {
                           return !rift.alive;
                       }),
        b.timeRifts.end()
    );
}

void GameWorld::spawnTimeRift(TimeRiftKind kind, Vec2 center) {
    if (!boss_) return;
    auto& b = *boss_;

    TimeRift rift;
    rift.entity = spawn(EntityKind::Boss);
    rift.center = center;
    rift.radius = 90.0f;
    rift.kind = kind;
    rift.ageSeconds = 0.0;
    rift.lifetimeSeconds = 5.0;
    rift.alive = true;

    int limit = 4;
    if (b.currentHealth <= b.phase4HpThreshold) {
        limit = 7;
    } else if (b.currentHealth <= b.phase3HpThreshold) {
        limit = 6;
    }

    if (static_cast<int>(b.timeRifts.size()) >= limit) {
        b.timeRifts.erase(b.timeRifts.begin());
    }

    b.timeRifts.push_back(rift);
}

void GameWorld::updateParadoxShards(double dt) {
    if (!boss_) return;
    auto& b = *boss_;

    b.shardOrbitClockSeconds += dt;

    Vec2 bossCenter = { b.position.x + b.size.w * 0.5f, b.position.y + b.size.h * 0.5f };

    for (size_t i = 0; i < b.paradoxShards.size(); ++i) {
        auto& shard = b.paradoxShards[i];
        
        if (!shard.alive) {
            shard.respawnRemainingSeconds -= dt;
            if (shard.respawnRemainingSeconds <= 0.0) {
                shard.alive = true;
                shard.currentHealth = 3;
                shard.respawnRemainingSeconds = 0.0;
            }
        }

        float angle = i * 1.570796f + static_cast<float>(b.shardOrbitClockSeconds) * b.shardOrbitAngularVelocity;
        shard.orbitOffset = { std::cos(angle) * b.shardOrbitRadius, std::sin(angle) * b.shardOrbitRadius };

        if (shard.hitFlashRemainingSeconds > 0.0) {
            shard.hitFlashRemainingSeconds = std::max(0.0, shard.hitFlashRemainingSeconds - dt);
        }
    }
}

void GameWorld::updateZeroHour(double dt) {
    if (!boss_) return;
    auto& b = *boss_;

    if (b.currentHealth > b.phase4HpThreshold) return;

    if (b.zeroHourRemainingSeconds > 0.0) {
        b.zeroHourRemainingSeconds = std::max(0.0, b.zeroHourRemainingSeconds - dt);
    } else {
        b.zeroHourTimerSeconds += dt;
        if (b.zeroHourTimerSeconds >= b.zeroHourIntervalSeconds) {
            b.zeroHourTimerSeconds = 0.0;
            b.zeroHourRemainingSeconds = b.zeroHourDurationSeconds;
            queueAudioEvent(AudioEventType::BossLaserCharge, b.position, b.size);
        }
    }
}

void GameWorld::enterChronarchPhase2() {
    if (!boss_) return;
    auto& b = *boss_;
    b.phase = Boss::Phase::Two;
    queueAudioEvent(AudioEventType::BossPhaseTransition, b.position, b.size);
    triggerChronarchRewind();
    b.moveSpeed = 310.0f;
    b.clockHandAttackIntervalSeconds = 4.0;
    b.timeRiftSpawnIntervalSeconds = 3.8;
}

void GameWorld::enterChronarchPhase3() {
    if (!boss_) return;
    auto& b = *boss_;
    b.phase = Boss::Phase::Three;
    queueAudioEvent(AudioEventType::BossPhaseTransition, b.position, b.size);
    triggerChronarchRewind();
    b.moveSpeed = 390.0f;
    b.clockHandAttackIntervalSeconds = 3.2;
    b.timeRiftSpawnIntervalSeconds = 3.2;
    b.maxTimeRifts = 6;
}

void GameWorld::enterChronarchPhase4() {
    if (!boss_) return;
    auto& b = *boss_;
    b.phase = Boss::Phase::Four;
    queueAudioEvent(AudioEventType::BossPhaseTransition, b.position, b.size);
    triggerChronarchRewind();
    b.moveSpeed = 480.0f;
    b.clockHandAttackIntervalSeconds = 2.4;
    b.timeRiftSpawnIntervalSeconds = 2.8;
    b.maxTimeRifts = 7;
    for (auto& shard : b.paradoxShards) {
        if (shard.alive) {
            shard.currentHealth = std::max(shard.currentHealth, 2);
        }
    }
}

void GameWorld::clearChronarchState() {
    if (!boss_) return;
    auto& b = *boss_;
    b.clockHands.clear();
    b.timeRifts.clear();
    b.zeroHourRemainingSeconds = 0.0;
}

void GameWorld::setChronarchTimeRiftTimerForTesting(double seconds) {
    if (boss_) {
        boss_->timeRiftSpawnTimerSeconds = seconds;
    }
}

void GameWorld::spawnChronarchTimeRiftForTesting(TimeRiftKind kind, Vec2 center) {
    spawnTimeRift(kind, center);
}

void GameWorld::setChronarchClockHandTimerForTesting(double seconds) {
    if (boss_) {
        boss_->clockHandAttackTimerSeconds = seconds;
    }
}

void GameWorld::setChronarchZeroHourTimerForTesting(double seconds) {
    if (boss_) {
        boss_->zeroHourTimerSeconds = seconds;
    }
}

void GameWorld::spawnChronarchClockHandAtAngleForTesting(float angleRadians) {
    if (!boss_) return;
    ClockHandBeam hand;
    hand.angleRadians = angleRadians;
    hand.angularVelocity = 0.0f; // freeze rotation so angle stays at pi/2 for reliable testing
    hand.telegraphSeconds = 0.8;
    hand.activeSeconds = 3.0; // generous active window
    // Start already at the transition point — next update makes it active
    hand.ageSeconds = hand.telegraphSeconds;
    hand.telegraphing = true;
    hand.active = false;
    hand.appliedThisCycle = false;
    hand.length = 720.0f;
    hand.width = 18.0f;
    boss_->clockHands.push_back(hand);
}

void GameWorld::setPaddleXForTesting(float x) {
    paddle_.position.x = x;
}

void GameWorld::forceChronarchSnapshotForTesting(double lookbackSeconds) {
    if (!boss_) return;
    auto& b = *boss_;
    TimeSnapshot snap;
    snap.ageSeconds = lookbackSeconds;
    snap.ballPosition = ball_.position;
    snap.ballVelocity = ball_.velocity;
    snap.paddlePosition = paddle_.position;
    for (const auto& eb : extraBalls_) {
        snap.extraBallPositions.push_back(eb.position);
        snap.extraBallVelocities.push_back(eb.velocity);
    }
    b.timeSnapshots.push_back(snap);
}

const std::vector<TimeRift>& GameWorld::chronarchTimeRifts() const noexcept {
    static const std::vector<TimeRift> empty;
    return boss_ ? boss_->timeRifts : empty;
}

const std::vector<ClockHandBeam>& GameWorld::chronarchClockHands() const noexcept {
    static const std::vector<ClockHandBeam> empty;
    return boss_ ? boss_->clockHands : empty;
}

const std::vector<ParadoxShard>& GameWorld::chronarchParadoxShards() const noexcept {
    static const std::vector<ParadoxShard> empty;
    return boss_ ? boss_->paradoxShards : empty;
}

} // namespace arcadeblocks::gameplay
