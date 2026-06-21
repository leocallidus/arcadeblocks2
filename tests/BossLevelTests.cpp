#include <catch2/catch_test_macros.hpp>
#include "gameplay/GameWorld.hpp"

using namespace arcadeblocks::gameplay;

TEST_CASE("Boss Level Initialization", "[boss][level]") {
    auto world = GameWorld::createBossLevel(10);
    REQUIRE(world != nullptr);
    REQUIRE(world->hasBoss());
    REQUIRE(world->bossRemainingHealth() == 30);
    REQUIRE(world->boss().maxHealth == 30);
    REQUIRE(world->boss().sections.size() == 1);
}

TEST_CASE("Boss 2 Initialization", "[boss][level]") {
    auto world = GameWorld::createBossLevel(20);
    REQUIRE(world != nullptr);
    REQUIRE(world->hasBoss());
    REQUIRE(world->bossRemainingHealth() == 80);
    REQUIRE(world->boss().maxHealth == 80);
    REQUIRE(world->boss().sections.size() == 3);
}

TEST_CASE("Boss takes damage from ball", "[boss][damage]") {
    auto world = GameWorld::createBossLevel(10);
    REQUIRE(world != nullptr);

    int initialHealth = world->bossRemainingHealth();
    REQUIRE(initialHealth > 0);
    
    auto bossPos = world->boss().position;
    auto sectionBounds = world->boss().sections[0].localBounds;
    Vec2 ballPos{bossPos.x + sectionBounds.left + 10.0f, bossPos.y + sectionBounds.bottom + 1.0f};

    world->teleportBallForTesting(world->ball().entity, ballPos, Vec2{0.0f, -500.0f});
    
    for(int i=0; i<10; ++i) {
        world->update(1.0/60.0, GameInput{});
    }
    
    REQUIRE(world->bossRemainingHealth() <= initialHealth);
}
