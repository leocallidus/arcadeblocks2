#pragma once

#include "levels/LevelTypes.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>
#include <optional>

namespace arcadeblocks::physics {
class PhysicsWorld;
}

namespace arcadeblocks::gameplay {

using EntityId = std::uint32_t;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    friend Vec2 operator+(Vec2 a, Vec2 b) noexcept { return Vec2{a.x + b.x, a.y + b.y}; }
    friend Vec2 operator-(Vec2 a, Vec2 b) noexcept { return Vec2{a.x - b.x, a.y - b.y}; }
};

struct Size {
    float w = 0.0f;
    float h = 0.0f;
};

struct Bounds {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct GameInput {
    bool moveLeft = false;
    bool moveRight = false;
    bool mousePaddleActive = false;
    float mousePaddleCenterX = 0.0f;
    bool launchPressed = false;
    bool callBallPressed = false;
    bool turboBallActive = false;
    bool pausePressed = false;
    bool toggleDebugDrawPressed = false;
    bool shootPlasmaPressed = false;
};

enum class EntityKind {
    Paddle,
    Ball,
    Brick,
    Wall,
    LevelBounds,
    Bonus,
    Boss
};

enum class BallState {
    AttachedToPaddle,
    Launched
};

struct BossSection {
    EntityId entity = 0;
    Bounds localBounds{0,0,0,0};
    bool alive = true;
    levels::BrickColor tint = levels::BrickColor::Red;
};

struct BossProjectile {
    EntityId entity = 0;
    Vec2 position{0, 0};
    Vec2 velocity{0, 0};
    Size size{18.0f, 18.0f};
    bool alive = true;
    double age = 0.0;
    // Boss 4 (Singularity, phase Four) marks some fired projectiles as homing.
    // When true in updateBossProjectiles the projectile continually re-targets
    // the ball. Pre-boss-4 projectiles always leave this false.
    bool isHoming = false;
};

enum class DroneShotState { Idle, Charging, Firing } ;

struct DroneEntity {
    EntityId entity = 0;
    Vec2 position{0, 0};
    Size size{34.0f, 34.0f};
    int currentHealth = 3;
    int maxHealth = 3;
    float moveSpeed = 160.0f;
    float moveAmplitude = 280.0f;
    double edgePauseSeconds = 0.4;
    double edgePauseRemaining = 0.0;
    int moveDirection = 1;
    bool alive = true;
    double shotTimerSeconds = 0.0;
    double age = 0.0;
    double hitFlashRemainingSeconds = 0.0;
};

struct GravityMine {
    // Boss-4 only. A short-lived stationary trap that pulls the ball
    // towards its centre. Stored separately from BossProjectile because
    // mines never move on their own and never damage the paddle.
    float x = 0.0f;
    float y = 0.0f;
    float age = 0.0f;
    float lifetimeRemaining = 0.0f;
    double bornAtSeconds = 0.0;
};

struct SingularityPulse {
    // Band of expanding gravitational ripple fired every
    // singularityPulseIntervalSeconds. The visual radius expands from 0 to
    // singularityPulseRadius over singularityPulseFlashDurationSeconds.
    float currentRadius = 0.0f;
    float timeSinceFired = 0.0f;
    bool active = false;
};

struct TimeSnapshot {
    double ageSeconds = 0.0;
    Vec2 ballPosition{0, 0};
    Vec2 ballVelocity{0, 0};
    std::vector<Vec2> extraBallPositions;
    std::vector<Vec2> extraBallVelocities;
    Vec2 paddlePosition{0, 0};
};

enum class TimeRiftKind {
    Slow,
    Haste,
    Rewind
};

struct TimeRift {
    EntityId entity = 0;
    Vec2 center{0, 0};
    float radius = 90.0f;
    TimeRiftKind kind = TimeRiftKind::Slow;
    double ageSeconds = 0.0;
    double lifetimeSeconds = 5.0;
    bool alive = true;
};

struct ClockHandBeam {
    float angleRadians = 0.0f;
    float angularVelocity = 0.0f;
    float length = 720.0f;
    float width = 18.0f;
    double telegraphSeconds = 0.8;
    double activeSeconds = 1.4;
    double cooldownSeconds = 0.0;
    double ageSeconds = 0.0;
    bool telegraphing = true;
    bool active = false;
    bool appliedThisCycle = false;
};

struct ParadoxShard {
    EntityId entity = 0;
    Vec2 orbitOffset{0, 0};
    Size size{46.0f, 46.0f};
    int currentHealth = 3;
    int maxHealth = 3;
    bool alive = true;
    double respawnRemainingSeconds = 0.0;
    double hitFlashRemainingSeconds = 0.0;
};

struct Boss {
    EntityId entity = 0;
    Vec2 position{0, 0};
    Size size{0, 0};
    int maxHealth = 30;
    int currentHealth = 30;
    int pointsPerHit = 50;
    int pointsOnDefeat = 5000;
    float moveAmplitude = 360.0f;
    float moveSpeed = 220.0f;
    double edgePauseSeconds = 0.8;
    double edgePauseRemaining = 0.0;
    int moveDirection = 1;
    std::vector<BossSection> sections;
    bool defeated = false;
    double hitFlashRemainingSeconds = 0.0;

    int sectionCount = 3;
    std::vector<int> sectionHealth;
    std::vector<int> sectionMaxHealth;

    bool shieldActive = false;
    double shieldCycleSeconds = 12.0;
    double shieldDurationSeconds = 2.5;
    double shieldCooldownSeconds = 9.5;
    double shieldTimerSeconds = 0.0;
    float shieldGlowAlpha = 0.0f;

    double shotIntervalSeconds = 3.5;
    double shotTimerSeconds = 0.0;
    float projectileSpeed = 380.0f;
    Size projectileSize{18.0f, 18.0f};
    std::vector<BossProjectile> projectiles;

    double diveIntervalSeconds = 6.0;
    double diveTimerSeconds = 0.0;
    float diveOffsetY = 0.0f;
    float diveDepthPx = 200.0f;
    double diveDurationSeconds = 1.0;
    enum class DiveState { Idle, Diving, Holding, Rising } diveState = DiveState::Idle;
    double diveStateTimer = 0.0;
    float baseY = 170.0f;

    // === Boss 3 fields (Helios) ===
    // levelNumber encodes which boss was spawned; 0 means pre-history values set
    // before boss 3, so we can still share the same struct with the older code.
    int levelNumber = 0;

    // Phase logic
    enum class Phase { One, Two, Three, Four } phase = Phase::One;
    float phase2ThresholdFraction = 0.5f;
    int phase2ThresholdHp = 0;
    bool phase2Transitioned = false;
    float phase3ThresholdFraction = 0.20f;
    int phase3ThresholdHp = 0;
    bool phase3Transitioned = false;
    float phase4ThresholdFraction = 0.10f;
    int phase4ThresholdHp = 0;
    bool phase4Transitioned = false;

    // Teleport
    double teleportIntervalSeconds = 5.5;
    double teleportIntervalPhase2Seconds = 3.6;
    double teleportTimerSeconds = 0.0;
    double doubleTeleportFirstDelaySeconds = 2.0;
    double invulnOnTeleportSeconds = 0.5;
    double invulnTimeRemaining = 0.0;

    // Laser
    enum class LaserState { Idle, Charging, Firing, Cooldown } laserState = LaserState::Idle;
    double laserIntervalSeconds = 4.0;
    double laserIntervalPhase2Seconds = 2.4;
    double laserTimerSeconds = 0.0;
    double laserStateTimer = 0.0;
    double laserChargeSeconds = 1.2;
    double laserFiringSeconds = 0.35;
    double laserCooldownSeconds = 0.6;
    double laserFirstDelaySeconds = 2.5;
    bool laserFirstFired = false;
    float laserWidthPhaseOne = 28.0f;
    float laserWidthPhaseTwo = 44.0f;
    float laserWidthPhaseThree = 44.0f;
    float laserWidthPhaseFour = 52.0f;
    float laserWidth = 28.0f;
    float laserTargetX = 0.0f;
    float laserAlpha = 0.0f;
    bool laserAppliedThisCycle = false;

    // Drones (only used in Phase 2)
    std::vector<DroneEntity> drones;
    double droneShotIntervalSeconds = 2.8;
    bool droneDropOnDestroyChanceEnabled = true;

    // Boss-3 specific drop counters; phases fire when hp% falls below one of these
    // thresholds. We store the *previous* hp so we can detect downward crossings.
    int prevHealthAtHitForDropCheck = 0;
    int pointsPerDrone = 500;

    // Visual flags (sample-time, not state-machine)
    float crystalFlashAlpha = 0.0f;

    // === Boss 4 fields (Singularity) ===
    // Will be filled by initBossLevelFour_Singularity. Earlier bosses leave
    // them at their defaults so they don't accidentally enable gravity /
    // mine / homing / pulse behaviour on the wrong fight.

    // --- Phase logic for boss 4 (4-phase) ---
    // See `phase` enum above. For boss 4 the value goes One -> Two -> Three -> Four
    // when HP crosses 66% / 33% / 10% of maxHealth respectively.

    // --- Gravity field (phase 3+, phase 4) ---
    bool gravityFieldEnabled = false;
    double gravityFieldStrength = 0.0;   // px/sec^2 toward boss center
    float  gravityFieldRadius = 0.0f;   // max radius of influence in pixels
    float  gravityFieldFalloffExponent = 1.0f;  // 1.0 = linear; >1 = sharper

    // --- Singularity pulse (waves of gravity) ---
    double singularityPulseIntervalSeconds = 8.0;
    double singularityPulseNextFireSeconds = -1.0;  // first pulse triggered at init
    double singularityPulseFlashDurationSeconds = 0.7;
    float singularityPulseRadius = 720.0f;
    double singularityPulseClockSeconds = 0.0;
    SingularityPulse singularityPulse;

    // --- Gravity mines (phase 4 only) ---
    double gravityMineIntervalSeconds = 1.6;   // cadence in phase 4 only
    double gravityMineNextFireSeconds = 0.0;
    float  gravityMineLifetimeSeconds = 4.0f;
    float  gravityMineRadius = 80.0f;         // pull range from mine
    float  gravityMineStrength = 220.0f;      // px/sec^2 toward mine center
    float  gravityMineVisualRadius = 40.0f;
    float  gravityMineSpawnRadiusFromBoss = 350.0f;  // spawn ring radius
    std::vector<GravityMine> gravityMines;

    // --- Homing projectiles (phase 4 only) ---
    float homingTurnRate = 4.5f;             // rad/s turning
    float homingTargetSpeed = 380.0f;        // target speed for home-in
    float homingMaxRange = 800.0f;           // homing switch-off distance

    // --- Per-phase thresholds and movement tunables (boss 4) ---
    float gravityFieldStrengthPhase3 = 180.0f;
    float gravityFieldStrengthPhase4 = 340.0f;
    float moveSpeedPhase1 = 260.0f;
    float moveSpeedPhase2 = 340.0f;
    float moveSpeedPhase3 = 420.0f;
    float moveSpeedPhase4 = 560.0f;
    float laserWidthPhase3 = 44.0f;
    float laserWidthPhase4 = 52.0f;
    float teleportIntervalPhase3Seconds = 3.2;
    float teleportIntervalPhase4Seconds = 2.4;
    double laserIntervalPhase3Seconds = 3.0;
    double laserIntervalPhase4Seconds = 2.0;
    int dronesPhaseTwoSpawnCount = 2;
    int dronesPhaseThreeSpawnCount = 3;
    double droneRespawnTimerRemaining = 7.0;

    // === Boss 5 fields (Chronarch / level 50) ===
    std::vector<TimeSnapshot> timeSnapshots;
    double timeSnapshotIntervalSeconds = 0.25;
    double timeSnapshotAccumulatorSeconds = 0.0;
    double timeSnapshotHistorySeconds = 6.0;
    double rewindLookbackSeconds = 1.25;

    std::vector<TimeRift> timeRifts;
    double timeRiftSpawnIntervalSeconds = 4.5;
    double timeRiftSpawnTimerSeconds = 2.0;
    int maxTimeRifts = 4;
    float timeRiftSlowMultiplier = 0.62f;
    float timeRiftHasteMultiplier = 1.28f;

    std::vector<ClockHandBeam> clockHands;
    double clockHandAttackIntervalSeconds = 5.0;
    double clockHandAttackTimerSeconds = 3.0;
    float clockHandBaseAngularVelocity = 1.45f;

    std::vector<ParadoxShard> paradoxShards;
    double shardRespawnSeconds = 8.0;
    float shardOrbitRadius = 190.0f;
    float shardOrbitAngularVelocity = 0.65f;
    double shardOrbitClockSeconds = 0.0;

    double zeroHourTimerSeconds = 0.0;
    double zeroHourIntervalSeconds = 7.0;
    double zeroHourDurationSeconds = 1.15;
    double zeroHourRemainingSeconds = 0.0;
    float zeroHourBallSpeedMultiplier = 0.35f;

    int phase1HpThreshold = 0;
    int phase2HpThreshold = 0;
    int phase3HpThreshold = 0;
    int phase4HpThreshold = 0;
    std::unordered_map<EntityId, float> ballSpeedMultipliers;
};

enum class GamePhase {
    Ready,
    Playing,
    Paused,
    LevelComplete,
    LifeLost,
    GameOver
};

enum class AudioEventType {
    PaddleSpawn,
    BallLaunch,
    PaddleHit,
    WallHit,
    BrickHit,
    BrickBreak,
    LifeLost,
    GameOver,
    LevelComplete,
    BonusSpawn,
    BonusPickup,
    CallBallPaddle,
    Explosion,
    PlasmaShot,
    PlasmaBrickHit,
    BossHit,
    BossDefeated,
    BossLoading,
    BossShot,
    BossProjectileHitPaddle,
    BossShieldBlock,
    BossDive,
    BossSectionDestroyed,
    BossTeleport,
    BossLaserCharge,
    BossLaserFire,
    BossPhaseTransition
};

struct AudioEvent {
    AudioEventType type = AudioEventType::WallHit;
    Vec2 position;
    Size size;
    std::string detail;
};

struct Entity {
    EntityId id = 0;
    EntityKind kind = EntityKind::Brick;
    bool alive = true;
};

struct Paddle {
    EntityId entity = 0;
    Vec2 position;
    Size size;
    float speed = 760.0f;
};

struct Ball {
    EntityId entity = 0;
    Vec2 position;
    Vec2 velocity;
    float radius = 12.0f;
    BallState state = BallState::AttachedToPaddle;
    float attachOffsetX = 0.0f;
    float gravityExposure = 0.0f; // cumulative gravity well exposure
};

struct Brick {
    EntityId entity = 0;
    Vec2 position;
    Size size;
    levels::BrickColor color = levels::BrickColor::Blue;
    int health = 1;
    int maxHealth = 1;
    int points = 0;
    bool alive = true;
};

struct Wall {
    EntityId entity = 0;
    Bounds bounds;
};

struct LevelBounds {
    EntityId entity = 0;
    Bounds bounds;
};

struct PlasmaBullet {
    EntityId entity = 0;
    Vec2 position;
    Vec2 velocity;
    Size size{16.0f, 32.0f};
    bool alive = true;
};

struct FallingBonus {
    EntityId entity = 0;
    Vec2 position;
    Size size;
    std::string type;
    Vec2 velocity;
    bool alive = true;
    double age = 0.0;
    double fadeOutRemainingSeconds = 0.0;
};

struct ActiveBonusTimer {
    std::string type;
    double durationSeconds = 0.0;
    double remainingSeconds = 0.0;
    double fadeOutRemainingSeconds = 0.0;
    int stacks = 1;
};

struct ScoreSystem {
    int score = 0;

    void add(int points);
    [[nodiscard]] bool spend(int points);
};

struct LivesSystem {
    int lives = 3;

    [[nodiscard]] bool loseLife();
    void restore(int value);
};

class GameWorld {
public:
    static GameWorld fromLevel(const levels::LevelDefinition& level);
    static std::unique_ptr<GameWorld> createBossLevel(int levelNumber);
    ~GameWorld();

    GameWorld() = default;
    GameWorld(GameWorld&&) noexcept;
    GameWorld& operator=(GameWorld&&) noexcept;

    GameWorld(const GameWorld&) = delete;
    GameWorld& operator=(const GameWorld&) = delete;

    [[nodiscard]] EntityId spawn(EntityKind kind);
    void despawn(EntityId id);

    void update(double fixedDeltaSeconds, const GameInput& input, const std::vector<std::string>& enabledBonuses = {});
    void setPaddleSpeed(float speed);
    void setTurboBallSpeed(float speed);
    [[nodiscard]] bool continueAfterGameOver(int cost, int restoredLives);
    void requestPause();
    void resumeFromPause();
    void demolishBricksExceptOne();
    void damageBrickForTesting(EntityId entity, int damage);
    
    // Chronarch (Boss 5) public testing helpers & getters
    void setChronarchTimeRiftTimerForTesting(double seconds);
    void spawnChronarchTimeRiftForTesting(TimeRiftKind kind, Vec2 center);
    void setChronarchClockHandTimerForTesting(double seconds);
    void spawnChronarchClockHandAtAngleForTesting(float angleRadians);
    void setChronarchZeroHourTimerForTesting(double seconds);
    void setPaddleXForTesting(float x);
    void forceChronarchSnapshotForTesting(double lookbackSeconds);
    [[nodiscard]] const std::vector<TimeRift>& chronarchTimeRifts() const noexcept;
    [[nodiscard]] const std::vector<ClockHandBeam>& chronarchClockHands() const noexcept;
    [[nodiscard]] const std::vector<ParadoxShard>& chronarchParadoxShards() const noexcept;
    void applyBrickHit(EntityId brickEntity);
    void teleportBallForTesting(EntityId entity, Vec2 position, Vec2 velocity = Vec2{0.0f, 300.0f});
    void setBossShieldForTesting(bool active);
    void setBossShotTimerForTesting(double timer);
    void setBossShieldTimerForTesting(double timer);
    void setBossTeleportTimerForTesting(double timer);
    void setBossLaserTimerForTesting(double timer);
    void setBossLaserStateForTesting(Boss::LaserState state);
    void setBossLaserTargetXForTesting(float x);
    void setBossLaserAppliedThisCycleForTesting(bool value);
    void setBossPhaseForTesting(Boss::Phase phase);
    void setBossInvulnTimeRemainingForTesting(double seconds);
    void damageBossForTesting(int damage);
    void damageBossDroneForTesting(int droneIdx, int damage);
    void simulateBallHitsBossForTesting(int hits);
    void setEnabledBonusesForTesting(std::vector<std::string> bonuses);
    void launchBallForTesting();
    void triggerBossAttackCooldownForTesting();
    void setBossPostRespawnCooldownRemainingForTesting(double seconds);
    void setGamePhaseForTesting(GamePhase phase);
    void activateBonusForTesting(const std::string& type, double duration);
    void setBossHitClockForTesting(double seconds);
    [[nodiscard]] double bossAttackCooldownRemainingForTesting() const noexcept;
    [[nodiscard]] double bossHitClockForTesting() const noexcept;
    void setGravityFieldEnabledForTesting(bool enabled);
    void setBossGravityFieldStrengthForTesting(float v);
    void setBossGravityMinesForTesting(std::vector<GravityMine> mines);
    void spawnBossGravityMineForTesting(Vec2 centerPos);
    void setBossHomingEnabledForTesting(bool enabled);
    void setBossSingularityPulseClockForTesting(double seconds);
    void setBossGravityMineIntervalForTesting(float seconds);
    void setBossMoveSpeedForTesting(float v);
    void reduceBossHealthToOnePercentForTesting();
    void spawnBonus(const std::string& type, Vec2 centerPos);
    [[nodiscard]] std::vector<AudioEvent> consumeAudioEvents();

    [[nodiscard]] bool hasBoss() const noexcept;
    [[nodiscard]] const Boss& boss() const noexcept;
    [[nodiscard]] float bossHealthNormalized() const noexcept;
    [[nodiscard]] int bossRemainingHealth() const noexcept;
    [[nodiscard]] const std::vector<DroneEntity>& bossDrones() const noexcept;
    [[nodiscard]] const std::vector<GravityMine>& bossGravityMines() const noexcept;
    [[nodiscard]] const SingularityPulse& bossSingularityPulse() const noexcept;
    void addBossProjectileForTesting(Vec2 position, Vec2 velocity,
                                     float size, bool homing);

    [[nodiscard]] const std::vector<Entity>& entities() const noexcept;
    [[nodiscard]] const Paddle& paddle() const noexcept;
    [[nodiscard]] const Ball& ball() const noexcept;
    [[nodiscard]] const std::vector<Ball>& extraBalls() const noexcept;
    [[nodiscard]] const std::vector<Brick>& bricks() const noexcept;
    [[nodiscard]] const Wall& wall() const noexcept;
    [[nodiscard]] const LevelBounds& bounds() const noexcept;
    [[nodiscard]] const std::vector<FallingBonus>& fallingBonuses() const noexcept;
    [[nodiscard]] GamePhase phase() const noexcept;
    [[nodiscard]] int score() const noexcept;
    [[nodiscard]] int lives() const noexcept;
    [[nodiscard]] int activeBrickCount() const noexcept;
    [[nodiscard]] bool physicsDebugDrawEnabled() const noexcept;
    [[nodiscard]] double respawnLaunchDelayRemaining() const noexcept;
    [[nodiscard]] double paddleSpawnAge() const noexcept;
    [[nodiscard]] const std::vector<ActiveBonusTimer>& activeBonusTimers() const noexcept;
    [[nodiscard]] bool isBonusActive(const std::string& type) const;
    [[nodiscard]] const std::vector<PlasmaBullet>& plasmaBullets() const noexcept;
    [[nodiscard]] int plasmaAmmo() const noexcept;

private:
    void spawnPaddle();
    void spawnBall();
    void spawnBounds();
    void spawnBricks(const levels::LevelDefinition& level);
    void attachBallToPaddle();
    void launchBall();
    void releaseAttachedBalls();
    void updatePaddle(double dt, const GameInput& input);
    void updateFallingBonuses(double dt);
    void updateBonusTimers(double dt);
    void activateTimedBonus(const std::string& type, double durationSeconds);
    void expireActiveBonusTimers();
    void callBallsToPaddle();
    [[nodiscard]] Vec2 callBallVelocityFor(const Ball& ball) const;
    void applyBrickDamage(EntityId brickEntity, int damage, bool playImpactSound);
    void explodeNearbyBricks(const Brick& sourceBrick);
    bool checkCollision(const Paddle& paddle, const FallingBonus& bonus) const;
    void handleLifeLost();
    void updateCompletionState();
    void setPhysicsDebugDrawEnabled(bool enabled);
    void queueAudioEvent(AudioEventType type, Vec2 position = Vec2{}, Size size = Size{}, std::string detail = "");

    void updateBoss(double dt);
    void applyBallBossHitIfAny();
    void initBossLevelOne();
    void initBossLevelTwo_Kaira();
    void initBossLevelThree_Helios();
    void initBossLevelFour_Singularity();
    void updateBossMovement(double dt);
    void updateBossDive(double dt);
    void updateBossShield(double dt);
    void updateBossShots(double dt);
    void updateBossProjectiles(double dt);
    void updateBossTeleport(double dt);
    void updateBossLaser(double dt);
    void updateBossDrones(double dt);
    void enterBossPhase2();
    void onPhaseThresholdCrossed();
    void enterBossPhaseThree();
    void enterBossPhaseFour();
    void applyBallDroneHitIfAny();
    void handleLaserLifeLossIfAny();
    void handleLaserLifeLoss();
    void dropPhaseThresholdBonus();
    void clearBossAttackState();
    void spawnRandomBonusAt(Vec2 centerPos);

    // Boss 4 (Singularity) — gravity field / pulses / mines / homing.
    void updateGravityField(double dt);
    void updateSingularityPulses(double dt);
    void updateGravityMines(double dt);
    void updateHomingLogic(double dt);
    void updateBallDriftFromGravity(double dt);
    void spawnGravityMine();
    void fireSingularityPulse();
    void applyGravityMinePull(double dt);
    [[nodiscard]] int findHitSectionIdx() const;
    void onBossSectionDestroyed(int sectionIdx);
    void handleProjectileLifeLoss();

    // Chronarch (Boss 5) private methods
    void initBossLevelFive_Chronarch();
    void updateChronarch(double dt);
    void captureTimeSnapshot(double dt);
    void updateTimeRifts(double dt);
    void updateClockHands(double dt);
    void updateParadoxShards(double dt);
    void updateZeroHour(double dt);
    void triggerChronarchRewind();
    void spawnTimeRift(TimeRiftKind kind, Vec2 center);
    void spawnClockHandAttack(int handCount);
    void enterChronarchPhase2();
    void enterChronarchPhase3();
    void enterChronarchPhase4();
    void clearChronarchState();
    void spawnChronarchNegativeBonus(Vec2 ballPos);
    void rewindSingleBall(Ball& ball, size_t extraBallIndex);

    EntityId nextEntityId_ = 1;
    std::vector<Entity> entities_;
    Paddle paddle_;
    Ball ball_;
    std::vector<Ball> extraBalls_;
    std::vector<Brick> bricks_;
    Wall wall_;
    LevelBounds bounds_;
    std::vector<FallingBonus> fallingBonuses_;
    std::vector<ActiveBonusTimer> activeBonusTimers_;
    std::vector<std::string> enabledBonuses_;
    ScoreSystem score_;
    LivesSystem lives_;
    std::unique_ptr<physics::PhysicsWorld> physics_;
    GamePhase phase_ = GamePhase::Ready;
    GamePhase phaseBeforePause_ = GamePhase::Ready;
    std::vector<AudioEvent> audioEvents_;
    double respawnLaunchDelayRemaining_ = 0.0;
    double autoLaunchCountdown_ = 0.0;
    double paddleSpawnAge_ = 0.0;
    bool physicsDebugDrawEnabled_ = false;
    bool levelPassBonusSpawned_ = false;
    std::vector<PlasmaBullet> plasmaBullets_;
    int plasmaAmmo_ = 0;
    std::unique_ptr<Boss> boss_;
    int bossHitSoundRollSeed_ = 0;
    unsigned int bossRngSeed_ = 0;
    bool bossHasExplicitRngSeed_ = false;
    double bossAttackCooldownSeconds_ = 0.0;
    // Tracks the last time each ball entity damaged the boss, so a ball that
    // stays pressed against the boss side wall cannot register repeated hits
    // across consecutive ticks.
    std::unordered_map<EntityId, double> lastBossHitByBall_;
    // Monotonic clock the per-ball throttle uses. The world updates it from
    // `update()`; tests can override it via `setBossHitClockForTesting`.
    double bossHitClockSeconds_ = 0.0;
};

[[nodiscard]] const char* toString(GamePhase phase) noexcept;

bool isBonusEnabled(const std::vector<std::string>& enabledBonuses, std::string_view type);
std::optional<std::string> pickRainbowBountyDrop(const std::vector<std::string>& enabledBonuses);
std::optional<std::string> pickBloodTitheDrop(const std::vector<std::string>& enabledBonuses);
bool isPositiveBonus(const std::string& type);

} // namespace arcadeblocks::gameplay
