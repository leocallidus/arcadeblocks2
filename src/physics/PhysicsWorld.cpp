#include "physics/PhysicsWorld.hpp"

#include "core/Log.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <iostream>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <random>

namespace arcadeblocks::physics {
namespace {

constexpr float pixelsPerMeter = 100.0f;
constexpr int subStepCount = 4;
constexpr float targetBallSpeedPixels = 660.0f;
constexpr float minBallSpeedPixels = 620.0f;
constexpr float maxBallSpeedPixels = 720.0f;
constexpr float maxTurboSpeedPixels = 12000.0f;
constexpr float worldMaxLinearSpeedMeters = (maxTurboSpeedPixels / pixelsPerMeter) * 1.25f;
constexpr int brickHitCooldownFrames = 6;
constexpr float minimumSlopeFromHorizontal = 0.363970234f; // LBreakoutHD: twenty degrees.
constexpr float maximumSlopeBeforeVertical = 5.67128182f; // LBreakoutHD: ten degrees from vertical.
constexpr float paddleFriction = 0.15f;
constexpr float paddleMaxReflectionDegrees = 72.0f;
constexpr float pi = 3.14159265358979323846f;

enum Category : std::uint64_t {
    categoryBall = 1ULL << 0U,
    categoryPaddle = 1ULL << 1U,
    categoryBrick = 1ULL << 2U,
    categoryWall = 1ULL << 3U
};

enum class ShapeKind {
    Ball,
    Paddle,
    Brick,
    Wall
};

struct ShapeUserData {
    ShapeKind kind = ShapeKind::Wall;
    gameplay::EntityId entity = 0;
};

struct BrickBody {
    gameplay::EntityId entity = 0;
    b2BodyId body = b2_nullBodyId;
    b2ShapeId shape = b2_nullShapeId;
    gameplay::Size size;
};

b2Vec2 toMeters(gameplay::Vec2 pixels) {
    return b2Vec2{pixels.x / pixelsPerMeter, pixels.y / pixelsPerMeter};
}

gameplay::Vec2 toPixels(b2Vec2 meters) {
    return gameplay::Vec2{meters.x * pixelsPerMeter, meters.y * pixelsPerMeter};
}

b2Vec2 centerMeters(gameplay::Vec2 topLeft, gameplay::Size size) {
    return toMeters(gameplay::Vec2{topLeft.x + size.w * 0.5f, topLeft.y + size.h * 0.5f});
}

b2Polygon boxPolygon(gameplay::Size size) {
    return b2MakeBox(size.w * 0.5f / pixelsPerMeter, size.h * 0.5f / pixelsPerMeter);
}

float length(gameplay::Vec2 vector) {
    return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

gameplay::Vec2 normalized(gameplay::Vec2 vector) {
    const float len = length(vector);
    if (len <= 0.0001f) {
        return gameplay::Vec2{0.35f, -0.94f};
    }
    return gameplay::Vec2{vector.x / len, vector.y / len};
}

gameplay::Vec2 stabilizedVelocity(gameplay::Vec2 velocity, float targetSpeedPixels = targetBallSpeedPixels, bool bypassClamp = false) {
    auto direction = normalized(velocity);
    float speed = targetSpeedPixels;
    if (!bypassClamp) {
        float currentSpeed = std::clamp(length(velocity), minBallSpeedPixels, maxBallSpeedPixels);
        if (currentSpeed > 0.0001f) {
            speed = std::clamp(targetSpeedPixels, minBallSpeedPixels, std::max(maxBallSpeedPixels, targetSpeedPixels));
        }
    }

    if (std::abs(direction.x) < 0.0001f) {
        direction.x = velocity.x < 0.0f ? -0.01f : 0.01f;
    }

    const float slope = std::abs(direction.y / direction.x);
    if (slope < minimumSlopeFromHorizontal) {
        direction.y = (direction.y < 0.0f ? -1.0f : 1.0f) * std::abs(direction.x) * minimumSlopeFromHorizontal;
    } else if (slope > maximumSlopeBeforeVertical) {
        direction.x = (direction.x < 0.0f ? -1.0f : 1.0f) * std::abs(direction.y) / maximumSlopeBeforeVertical;
    }

    if (std::abs(std::abs(direction.x) - std::abs(direction.y)) < 0.002f) {
        direction.x *= 0.98f;
    }

    direction = normalized(direction);
    float angle = std::atan2(direction.y, direction.x);
    constexpr float twoDegrees = pi / 90.0f;
    angle = std::round(angle / twoDegrees) * twoDegrees;
    direction = normalized(gameplay::Vec2{std::cos(angle), std::sin(angle)});
    return gameplay::Vec2{direction.x * speed, direction.y * speed};
}

uint64_t shapeKey(b2ShapeId shapeId) {
    return b2StoreShapeId(shapeId);
}

bool checkCircleRectangleOverlap(gameplay::Vec2 center, float radius, gameplay::Vec2 rectCenter, gameplay::Size rectSize) {
    float halfW = rectSize.w * 0.5f;
    float halfH = rectSize.h * 0.5f;
    
    float closestX = std::clamp(center.x, rectCenter.x - halfW, rectCenter.x + halfW);
    float closestY = std::clamp(center.y, rectCenter.y - halfH, rectCenter.y + halfH);
    
    float distanceX = center.x - closestX;
    float distanceY = center.y - closestY;
    
    float distanceSquared = (distanceX * distanceX) + (distanceY * distanceY);
    return distanceSquared < (radius * radius);
}

} // namespace

class PhysicsWorld::Impl {
public:
    explicit Impl(const gameplay::GameWorld& gameWorld) {
        b2WorldDef worldDef = b2DefaultWorldDef();
        worldDef.gravity = b2Vec2{0.0f, 0.0f};
        worldDef.enableContinuous = true;
        worldDef.maximumLinearSpeed = worldMaxLinearSpeedMeters;
        world_ = b2CreateWorld(&worldDef);
        b2World_SetMaximumLinearSpeed(world_, worldDef.maximumLinearSpeed);

        const auto version = b2GetVersion();
        core::Log::info(
            "Physics initialized: Box2D " + std::to_string(version.major) + "."
            + std::to_string(version.minor) + "." + std::to_string(version.revision)
            + ", fixed timestep substeps=" + std::to_string(subStepCount)
            + ", metersToPixels=" + std::to_string(static_cast<int>(pixelsPerMeter)));

        createWalls(gameWorld.bounds().bounds);
        createPaddle(gameWorld.paddle());
        createBall(gameWorld.ball());
        createBricks(gameWorld.bricks());
    }

    ~Impl() {
        if (b2World_IsValid(world_)) {
            b2DestroyWorld(world_);
        }
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    void syncPaddle(const gameplay::Paddle& paddle, double fixedDeltaSeconds) {
        if (paddle.size.w != paddleSizePixels_.w || paddle.size.h != paddleSizePixels_.h) {
            paddleSizePixels_ = paddle.size;
            if (b2Shape_IsValid(paddleShape_)) {
                shapeDataByShape_.erase(shapeKey(paddleShape_));
                b2DestroyShape(paddleShape_, true);
                paddleShape_ = b2_nullShapeId;
            }
            auto def = shapeDef(ShapeKind::Paddle, paddle.entity, categoryPaddle, categoryBall);
            const auto polygon = boxPolygon(paddle.size);
            paddleShape_ = b2CreatePolygonShape(paddleBody_, &def, &polygon);
            registerShape(paddleShape_);
        }
        const b2Transform target{
            centerMeters(paddle.position, paddle.size),
            b2Rot_identity,
        };
        b2Body_SetTargetTransform(paddleBody_, target, static_cast<float>(fixedDeltaSeconds));
    }

    void syncAttachedBall(const gameplay::Ball& ball) {
        if (ball.state == gameplay::BallState::AttachedToPaddle) {
            auto bodyId = findBallBody(ball.entity);
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetTransform(bodyId, toMeters(ball.position), b2Rot_identity);
                b2Body_SetLinearVelocity(bodyId, b2Vec2_zero);
            }
            for (auto& bb : ballBodies_) {
                if (bb.entity == ball.entity) {
                    bb.attached = true;
                    break;
                }
            }
        }
    }

    void launchBall(gameplay::Vec2 velocity) {
        if (!ballBodies_.empty()) {
            const auto stableVelocity = stabilizedVelocity(velocity, targetSpeedPixels());
            b2Body_SetLinearVelocity(ballBodies_.front().body, toMeters(stableVelocity));
        }
    }

    void setTurboBallSpeed(float speedPixelsPerSecond) {
        turboBallSpeedPixels_ = std::clamp(speedPixelsPerSecond, targetBallSpeedPixels, 12000.0f);
        if (turboBallActive_) {
            clampBallSpeed();
        }
    }

    void setTurboBallActive(bool active) {
        if (turboBallActive_ == active) {
            return;
        }
        turboBallActive_ = active;
        clampBallSpeed();
    }

    void setBallVelocity(gameplay::EntityId ballEntity, gameplay::Vec2 velocity) {
        auto bodyId = findBallBody(ballEntity);
        if (!b2Body_IsValid(bodyId)) {
            return;
        }
        b2Body_SetLinearVelocity(bodyId, toMeters(stabilizedVelocity(velocity, targetSpeedPixels())));
    }

    void addBall(const gameplay::Ball& ball) {
        createBall(ball);
    }

    void removeBall(gameplay::EntityId ballEntity) {
        auto it = std::find_if(ballBodies_.begin(), ballBodies_.end(),
                               [ballEntity](const BallBody& bb) { return bb.entity == ballEntity; });
        if (it != ballBodies_.end()) {
            shapeDataByShape_.erase(shapeKey(it->shape));
            if (b2Body_IsValid(it->body)) {
                b2DestroyBody(it->body);
            }
            ballBodies_.erase(it);
        }
    }

    void updateBallFilters(bool energyBallsActive, const gameplay::Ball& ball, const std::vector<gameplay::Ball>& extraBalls) {
        for (auto& bb : ballBodies_) {
            if (b2Shape_IsValid(bb.shape)) {
                bool attached = false;
                if (bb.entity == ball.entity) {
                    attached = (ball.state == gameplay::BallState::AttachedToPaddle);
                } else {
                    for (const auto& eb : extraBalls) {
                        if (bb.entity == eb.entity) {
                            attached = (eb.state == gameplay::BallState::AttachedToPaddle);
                            break;
                        }
                    }
                }

                uint64_t mask = categoryWall;
                if (!attached) {
                    mask |= categoryPaddle;
                }
                if (!energyBallsActive) {
                    mask |= categoryBrick;
                }

                b2Filter filter = b2Shape_GetFilter(bb.shape);
                filter.maskBits = mask;
                b2Shape_SetFilter(bb.shape, filter);
            }
        }
    }

    PhysicsStepResult step(double fixedDeltaSeconds, gameplay::Ball& ball, std::vector<gameplay::Ball>& extraBalls, float speedMultiplier, bool energyBallsActive, bool stickyPaddleActive, int attachedCount, bool bonusWallActive, bool chaoticBallsActive, const std::unordered_map<gameplay::EntityId, float>& ballSpeedModifiers) {
        currentSpeedMultiplier_ = speedMultiplier;
        chaoticBallsActive_ = chaoticBallsActive;
        
        // Update attached flag in ballBodies_
        for (auto& bb : ballBodies_) {
            if (bb.entity == ball.entity) {
                bb.attached = (ball.state == gameplay::BallState::AttachedToPaddle);
            } else {
                bool found = false;
                for (const auto& eb : extraBalls) {
                    if (bb.entity == eb.entity) {
                        bb.attached = (eb.state == gameplay::BallState::AttachedToPaddle);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    bb.attached = false;
                }
            }
        }

        syncBallRadius(ball.entity, ball.radius);
        for (auto& eb : extraBalls) {
            syncBallRadius(eb.entity, eb.radius);
        }
        
        updateBallFilters(energyBallsActive, ball, extraBalls);
        
        std::unordered_map<gameplay::EntityId, float> preStepVelocityY;
        auto mainBodyId = findBallBody(ball.entity);
        if (b2Body_IsValid(mainBodyId)) {
            preStepVelocityY[ball.entity] = b2Body_GetLinearVelocity(mainBodyId).y;
        }
        for (const auto& eb : extraBalls) {
            auto bodyId = findBallBody(eb.entity);
            if (b2Body_IsValid(bodyId)) {
                preStepVelocityY[eb.entity] = b2Body_GetLinearVelocity(bodyId).y;
            }
        }

        decrementCooldowns();
        b2World_Step(world_, static_cast<float>(fixedDeltaSeconds), subStepCount);

        PhysicsStepResult result;
        processContactEvents(result, stickyPaddleActive, attachedCount, preStepVelocityY);
        clampBallSpeed(ballSpeedModifiers);

        if (energyBallsActive) {
            auto checkBallOverlaps = [&](const gameplay::Ball& b) {
                auto bodyId = findBallBody(b.entity);
                if (!b2Body_IsValid(bodyId)) return;
                gameplay::Vec2 ballPos = toPixels(b2Body_GetPosition(bodyId));
                float radius = b.radius;
                
                for (const auto& brick : bricks_) {
                    if (!b2Body_IsValid(brick.body)) continue;
                    gameplay::Vec2 brickCenter = toPixels(b2Body_GetPosition(brick.body));
                    if (checkCircleRectangleOverlap(ballPos, radius, brickCenter, brick.size)) {
                        if (brickCooldowns_[brick.entity] == 0) {
                            result.brickHits.push_back(brick.entity);
                            brickCooldowns_[brick.entity] = brickHitCooldownFrames;
                        }
                    }
                }
            };
            checkBallOverlaps(ball);
            for (const auto& eb : extraBalls) {
                checkBallOverlaps(eb);
            }
        }

        auto primaryBodyId = findBallBody(ball.entity);
        if (b2Body_IsValid(primaryBodyId)) {
            ball.position = toPixels(b2Body_GetPosition(primaryBodyId));
            ball.velocity = toPixels(b2Body_GetLinearVelocity(primaryBodyId));
            if (bonusWallActive && ball.position.y + ball.radius > boundsBottomPixels_) {
                ball.position.y = boundsBottomPixels_ - ball.radius;
                ball.velocity.y = -std::abs(ball.velocity.y);
                b2Body_SetTransform(primaryBodyId, toMeters(ball.position), b2Rot_identity);
                b2Body_SetLinearVelocity(primaryBodyId, toMeters(ball.velocity));
                result.hitWall = true;
            }
            result.belowBottom = ball.position.y - ball.radius > boundsBottomPixels_;
        }

        for (auto& eb : extraBalls) {
            auto bodyId = findBallBody(eb.entity);
            if (b2Body_IsValid(bodyId)) {
                eb.position = toPixels(b2Body_GetPosition(bodyId));
                eb.velocity = toPixels(b2Body_GetLinearVelocity(bodyId));
                if (bonusWallActive && eb.position.y + eb.radius > boundsBottomPixels_) {
                    eb.position.y = boundsBottomPixels_ - eb.radius;
                    eb.velocity.y = -std::abs(eb.velocity.y);
                    b2Body_SetTransform(bodyId, toMeters(eb.position), b2Rot_identity);
                    b2Body_SetLinearVelocity(bodyId, toMeters(eb.velocity));
                    result.hitWall = true;
                }
            }
        }

        return result;
    }

    void removeBrick(gameplay::EntityId brickEntity) {
        auto found = brickByEntity_.find(brickEntity);
        if (found == brickByEntity_.end()) {
            return;
        }

        const auto index = found->second;
        if (index >= bricks_.size()) {
            return;
        }

        const auto shapeId = bricks_[index].shape;
        shapeDataByShape_.erase(shapeKey(shapeId));
        if (b2Body_IsValid(bricks_[index].body)) {
            b2DestroyBody(bricks_[index].body);
        }
        brickByEntity_.erase(found);
    }

    void teleportBallForTesting(gameplay::EntityId entity, gameplay::Vec2 position, gameplay::Vec2 velocity) {
        auto bodyId = findBallBody(entity);
        if (b2Body_IsValid(bodyId)) {
            b2Body_SetTransform(bodyId, toMeters(position), b2Rot_identity);
            b2Body_SetLinearVelocity(bodyId, toMeters(velocity));
        }
    }

    void syncBallRadius(gameplay::EntityId entity, float radius) {
        for (auto& bb : ballBodies_) {
            if (bb.entity == entity) {
                if (b2Shape_IsValid(bb.shape)) {
                    b2Circle circle{};
                    circle.center = b2Vec2{0.0f, 0.0f};
                    circle.radius = radius / pixelsPerMeter;
                    b2Shape_SetCircle(bb.shape, &circle);
                }
                break;
            }
        }
    }

    void setDebugDrawEnabled(bool enabled) noexcept {
        debugDrawEnabled_ = enabled;
    }

    bool debugDrawEnabled() const noexcept {
        return debugDrawEnabled_;
    }

private:
    b2BodyId createBody(b2BodyType type, b2Vec2 position, const char* name) {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = type;
        bodyDef.position = position;
        bodyDef.name = name;
        bodyDef.fixedRotation = true;
        bodyDef.enableSleep = false;
        bodyDef.isBullet = type == b2_dynamicBody;
        return b2CreateBody(world_, &bodyDef);
    }

    ShapeUserData* allocateShapeData(ShapeKind kind, gameplay::EntityId entity) {
        auto data = std::make_unique<ShapeUserData>(ShapeUserData{kind, entity});
        auto* result = data.get();
        shapeData_.push_back(std::move(data));
        return result;
    }

    b2ShapeDef shapeDef(ShapeKind kind, gameplay::EntityId entity, std::uint64_t category, std::uint64_t mask) {
        b2ShapeDef def = b2DefaultShapeDef();
        def.userData = allocateShapeData(kind, entity);
        def.material.friction = 0.01f;
        def.material.restitution = kind == ShapeKind::Ball ? 1.0f : 0.05f;
        def.density = kind == ShapeKind::Ball ? 1.0f : 0.0f;
        def.filter.categoryBits = category;
        def.filter.maskBits = mask;
        def.enableContactEvents = true;
        def.enableHitEvents = true;
        return def;
    }

    void registerShape(b2ShapeId shapeId) {
        shapeDataByShape_[shapeKey(shapeId)] = static_cast<ShapeUserData*>(b2Shape_GetUserData(shapeId));
    }

    void createWalls(gameplay::Bounds bounds) {
        boundsBottomPixels_ = bounds.bottom;
        constexpr float wallThickness = 40.0f;
        const gameplay::Size verticalWall{wallThickness, bounds.bottom - bounds.top + wallThickness * 2.0f};
        const gameplay::Size horizontalWall{bounds.right - bounds.left + wallThickness * 2.0f, wallThickness};
        createStaticBox(
            gameplay::Vec2{bounds.left - wallThickness, bounds.top - wallThickness},
            verticalWall,
            "left-wall");
        createStaticBox(
            gameplay::Vec2{bounds.right, bounds.top - wallThickness},
            verticalWall,
            "right-wall");
        createStaticBox(
            gameplay::Vec2{bounds.left - wallThickness, bounds.top - wallThickness},
            horizontalWall,
            "top-wall");
    }

    void createStaticBox(gameplay::Vec2 topLeft, gameplay::Size size, const char* name) {
        const auto body = createBody(b2_staticBody, centerMeters(topLeft, size), name);
        auto def = shapeDef(ShapeKind::Wall, 0, categoryWall, categoryBall);
        const auto polygon = boxPolygon(size);
        const auto shape = b2CreatePolygonShape(body, &def, &polygon);
        registerShape(shape);
    }

    void createPaddle(const gameplay::Paddle& paddle) {
        paddleSizePixels_ = paddle.size;
        paddleBody_ = createBody(b2_kinematicBody, centerMeters(paddle.position, paddle.size), "paddle");
        auto def = shapeDef(ShapeKind::Paddle, paddle.entity, categoryPaddle, categoryBall);
        const auto polygon = boxPolygon(paddle.size);
        paddleShape_ = b2CreatePolygonShape(paddleBody_, &def, &polygon);
        registerShape(paddleShape_);
    }

    void createBall(const gameplay::Ball& ball) {
        auto bodyId = createBody(b2_dynamicBody, toMeters(ball.position), "ball");
        auto def = shapeDef(ShapeKind::Ball, ball.entity, categoryBall, categoryPaddle | categoryBrick | categoryWall);
        b2Circle circle{};
        circle.center = b2Vec2{0.0f, 0.0f};
        circle.radius = ball.radius / pixelsPerMeter;
        auto shapeId = b2CreateCircleShape(bodyId, &def, &circle);
        registerShape(shapeId);

        ballBodies_.push_back(BallBody{ball.entity, bodyId, shapeId, ball.state == gameplay::BallState::AttachedToPaddle});
        b2Body_SetLinearVelocity(bodyId, toMeters(ball.velocity));
    }

    void createBricks(const std::vector<gameplay::Brick>& bricks) {
        bricks_.reserve(bricks.size());
        for (const auto& brick : bricks) {
            const auto body = createBody(b2_staticBody, centerMeters(brick.position, brick.size), "brick");
            auto def = shapeDef(ShapeKind::Brick, brick.entity, categoryBrick, categoryBall);
            const auto polygon = boxPolygon(brick.size);
            const auto shape = b2CreatePolygonShape(body, &def, &polygon);
            registerShape(shape);
            brickByEntity_[brick.entity] = bricks_.size();
            bricks_.push_back(BrickBody{.entity = brick.entity, .body = body, .shape = shape, .size = brick.size});
        }
    }

    ShapeUserData* shapeDataFor(b2ShapeId shapeId) const {
        const auto found = shapeDataByShape_.find(shapeKey(shapeId));
        if (found == shapeDataByShape_.end()) {
            return nullptr;
        }
        return found->second;
    }

    void processContactEvents(PhysicsStepResult& result, bool stickyPaddleActive, int attachedCount, const std::unordered_map<gameplay::EntityId, float>& preStepVelocityY) {
        const auto events = b2World_GetContactEvents(world_);
        for (int i = 0; i < events.beginCount; ++i) {
            const auto& event = events.beginEvents[i];
            auto* a = shapeDataFor(event.shapeIdA);
            auto* b = shapeDataFor(event.shapeIdB);
            if (a == nullptr || b == nullptr) {
                continue;
            }

            const auto* ball = a->kind == ShapeKind::Ball ? a : (b->kind == ShapeKind::Ball ? b : nullptr);
            const auto* other = ball == a ? b : a;
            if (ball == nullptr || other == nullptr) {
                continue;
            }

            if (other->kind == ShapeKind::Paddle) {
                float preVelY = 0.0f;
                if (auto it = preStepVelocityY.find(ball->entity); it != preStepVelocityY.end()) {
                    preVelY = it->second;
                }
                bool isMovingDown = preVelY > 0.0f;
                if (isMovingDown) {
                    result.hitPaddle = true;
                    if (stickyPaddleActive && (attachedCount + static_cast<int>(result.paddleHits.size()) < 4)) {
                        auto bodyId = findBallBody(ball->entity);
                        if (b2Body_IsValid(bodyId)) {
                            b2Body_SetLinearVelocity(bodyId, b2Vec2_zero);
                        }
                        result.paddleHits.push_back(ball->entity);
                    } else {
                        applyPaddleResponse(ball->entity);
                    }
                }
            } else if (other->kind == ShapeKind::Wall) {
                result.hitWall = true;
            } else if (other->kind == ShapeKind::Brick) {
                if (brickCooldowns_[other->entity] == 0) {
                    result.brickHits.push_back(other->entity);
                    brickCooldowns_[other->entity] = brickHitCooldownFrames;
                }
            }
        }
    }

    void applyPaddleResponse(gameplay::EntityId ballEntity) {
        auto bodyId = findBallBody(ballEntity);
        if (!b2Body_IsValid(bodyId)) {
            return;
        }
        const auto ballCenter = toPixels(b2Body_GetPosition(bodyId));
        const auto paddleCenter = toPixels(b2Body_GetPosition(paddleBody_));
        const auto paddleVelocity = toPixels(b2Body_GetLinearVelocity(paddleBody_));
        const float halfWidth = std::max(1.0f, paddleSizePixels_.w * 0.5f);
        const float offset = std::clamp((ballCenter.x - paddleCenter.x) / halfWidth, -1.0f, 1.0f);
        const float angle = (-90.0f + offset * paddleMaxReflectionDegrees) * (pi / 180.0f);
        const float speed = targetSpeedPixels();
        auto velocity = gameplay::Vec2{
            std::cos(angle) * speed + paddleVelocity.x * paddleFriction,
            std::sin(angle) * speed,
        };
        velocity = stabilizedVelocity(velocity, speed);
        if (velocity.y > -minimumSlopeFromHorizontal * std::abs(velocity.x)) {
            velocity.y = -std::max(std::abs(velocity.y), speed * 0.35f);
            velocity = stabilizedVelocity(velocity, speed);
        }

        b2Body_SetLinearVelocity(bodyId, toMeters(velocity));
        const auto paddlePosition = b2Body_GetPosition(paddleBody_);
        const auto ballPosition = b2Body_GetPosition(bodyId);
        b2Body_SetTransform(
            bodyId,
            b2Vec2{
                ballPosition.x,
                paddlePosition.y - ((paddleSizePixels_.h * 0.5f) + 20.0f) / pixelsPerMeter,
            },
            b2Rot_identity);
    }

    void decrementCooldowns() {
        for (auto& [_, frames] : brickCooldowns_) {
            if (frames > 0) {
                --frames;
            }
        }
    }

    gameplay::Vec2 randomizeDirection(gameplay::Vec2 currentVelocity, float speed) {
        thread_local std::mt19937 generator(std::random_device{}());
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
        for (int attempt = 0; attempt < 20; ++attempt) {
            float angle = angleDist(generator);
            gameplay::Vec2 dir{std::cos(angle), std::sin(angle)};
            if (std::abs(dir.y) >= 0.35f && std::abs(dir.x) >= 0.25f) {
                return gameplay::Vec2{dir.x * speed, dir.y * speed};
            }
        }
        return currentVelocity;
    }

    void clampBallSpeed(const std::unordered_map<gameplay::EntityId, float>& ballSpeedModifiers = {}) {
        thread_local std::mt19937 generator(std::random_device{}());
        std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);

        for (const auto& bb : ballBodies_) {
            if (b2Body_IsValid(bb.body)) {
                if (bb.attached) {
                    b2Body_SetLinearVelocity(bb.body, b2Vec2_zero);
                } else {
                    auto velocity = toPixels(b2Body_GetLinearVelocity(bb.body));
                    float normalSpeed = targetSpeedPixels();
                    float speed = normalSpeed;
                    auto it = ballSpeedModifiers.find(bb.entity);
                    if (it != ballSpeedModifiers.end()) {
                        speed *= it->second;
                    }
                    bool bypass = (speed != normalSpeed);

                    if (chaoticBallsActive_ && chanceDist(generator) < 0.015f) {
                        velocity = randomizeDirection(velocity, speed);
                    } else {
                        velocity = stabilizedVelocity(velocity, speed, bypass);
                    }
                    b2Body_SetLinearVelocity(bb.body, toMeters(velocity));
                }
            }
        }
    }

    [[nodiscard]] float targetSpeedPixels() const noexcept {
        return (turboBallActive_ ? turboBallSpeedPixels_ : targetBallSpeedPixels) * currentSpeedMultiplier_;
    }

    float currentSpeedMultiplier_ = 1.0f;
    b2WorldId world_ = b2_nullWorldId;
    struct BallBody {
        gameplay::EntityId entity = 0;
        b2BodyId body = b2_nullBodyId;
        b2ShapeId shape = b2_nullShapeId;
        bool attached = false;
    };
    std::vector<BallBody> ballBodies_;

    b2BodyId findBallBody(gameplay::EntityId entity) {
        for (const auto& bb : ballBodies_) {
            if (bb.entity == entity) {
                return bb.body;
            }
        }
        return b2_nullBodyId;
    }
    b2BodyId paddleBody_ = b2_nullBodyId;
    b2ShapeId paddleShape_ = b2_nullShapeId;
    gameplay::Size paddleSizePixels_{400.0f, 34.0f};
    float turboBallSpeedPixels_ = 2000.0f;
    bool turboBallActive_ = false;
    bool chaoticBallsActive_ = false;
    std::vector<BrickBody> bricks_;
    std::unordered_map<gameplay::EntityId, std::size_t> brickByEntity_;
    std::unordered_map<gameplay::EntityId, int> brickCooldowns_;
    std::vector<std::unique_ptr<ShapeUserData>> shapeData_;
    std::unordered_map<std::uint64_t, ShapeUserData*> shapeDataByShape_;
    float boundsBottomPixels_ = 1080.0f;
    bool debugDrawEnabled_ = false;
};

PhysicsWorld::PhysicsWorld(const gameplay::GameWorld& gameWorld)
    : impl_(std::make_unique<Impl>(gameWorld)) {}

PhysicsWorld::~PhysicsWorld() = default;

PhysicsWorld::PhysicsWorld(PhysicsWorld&&) noexcept = default;

PhysicsWorld& PhysicsWorld::operator=(PhysicsWorld&&) noexcept = default;

void PhysicsWorld::syncPaddle(const gameplay::Paddle& paddle, double fixedDeltaSeconds) {
    impl_->syncPaddle(paddle, fixedDeltaSeconds);
}

void PhysicsWorld::syncAttachedBall(const gameplay::Ball& ball) {
    impl_->syncAttachedBall(ball);
}

void PhysicsWorld::launchBall(gameplay::Vec2 velocity) {
    impl_->launchBall(velocity);
}

void PhysicsWorld::setTurboBallSpeed(float speedPixelsPerSecond) {
    impl_->setTurboBallSpeed(speedPixelsPerSecond);
}

void PhysicsWorld::setTurboBallActive(bool active) {
    impl_->setTurboBallActive(active);
}

void PhysicsWorld::setBallVelocity(gameplay::EntityId ballEntity, gameplay::Vec2 velocity) {
    impl_->setBallVelocity(ballEntity, velocity);
}

void PhysicsWorld::addBall(const gameplay::Ball& ball) {
    impl_->addBall(ball);
}

void PhysicsWorld::removeBall(gameplay::EntityId ballEntity) {
    impl_->removeBall(ballEntity);
}

PhysicsStepResult PhysicsWorld::step(double fixedDeltaSeconds, gameplay::Ball& ball, std::vector<gameplay::Ball>& extraBalls, float speedMultiplier, bool energyBallsActive, bool stickyPaddleActive, int attachedCount, bool bonusWallActive, bool chaoticBallsActive, const std::unordered_map<gameplay::EntityId, float>& ballSpeedModifiers) {
    return impl_->step(fixedDeltaSeconds, ball, extraBalls, speedMultiplier, energyBallsActive, stickyPaddleActive, attachedCount, bonusWallActive, chaoticBallsActive, ballSpeedModifiers);
}

void PhysicsWorld::removeBrick(gameplay::EntityId brickEntity) {
    impl_->removeBrick(brickEntity);
}

void PhysicsWorld::teleportBallForTesting(gameplay::EntityId entity, gameplay::Vec2 position, gameplay::Vec2 velocity) {
    impl_->teleportBallForTesting(entity, position, velocity);
}

void PhysicsWorld::setDebugDrawEnabled(bool enabled) noexcept {
    impl_->setDebugDrawEnabled(enabled);
}

bool PhysicsWorld::debugDrawEnabled() const noexcept {
    return impl_->debugDrawEnabled();
}

float PhysicsWorld::metersToPixels() const noexcept {
    return pixelsPerMeter;
}

} // namespace arcadeblocks::physics
