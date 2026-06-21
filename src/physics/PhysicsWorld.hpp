#pragma once

#include "gameplay/GameWorld.hpp"

#include <memory>
#include <vector>
#include <unordered_map>

namespace arcadeblocks::physics {

struct PhysicsStepResult {
    std::vector<gameplay::EntityId> brickHits;
    std::vector<gameplay::EntityId> paddleHits;
    bool hitPaddle = false;
    bool hitWall = false;
    bool belowBottom = false;
};

class PhysicsWorld {
public:
    explicit PhysicsWorld(const gameplay::GameWorld& gameWorld);
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    PhysicsWorld(PhysicsWorld&&) noexcept;
    PhysicsWorld& operator=(PhysicsWorld&&) noexcept;

    void syncPaddle(const gameplay::Paddle& paddle, double fixedDeltaSeconds);
    void syncAttachedBall(const gameplay::Ball& ball);
    void launchBall(gameplay::Vec2 velocity);
    void setTurboBallSpeed(float speedPixelsPerSecond);
    void setTurboBallActive(bool active);
    void setBallVelocity(gameplay::EntityId ballEntity, gameplay::Vec2 velocity);
    void addBall(const gameplay::Ball& ball);
    void removeBall(gameplay::EntityId ballEntity);
    PhysicsStepResult step(double fixedDeltaSeconds, gameplay::Ball& ball, std::vector<gameplay::Ball>& extraBalls, float speedMultiplier = 1.0f, bool energyBallsActive = false, bool stickyPaddleActive = false, int attachedCount = 0, bool bonusWallActive = false, bool chaoticBallsActive = false, const std::unordered_map<gameplay::EntityId, float>& ballSpeedModifiers = {});
    void removeBrick(gameplay::EntityId brickEntity);
    void teleportBallForTesting(gameplay::EntityId entity, gameplay::Vec2 position, gameplay::Vec2 velocity = gameplay::Vec2{});

    void setDebugDrawEnabled(bool enabled) noexcept;
    [[nodiscard]] bool debugDrawEnabled() const noexcept;
    [[nodiscard]] float metersToPixels() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace arcadeblocks::physics
