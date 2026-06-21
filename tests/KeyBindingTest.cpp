#include <catch2/catch_test_macros.hpp>

#include "settings/KeyBinding.hpp"
#include "settings/Settings.hpp"

TEST_CASE("key bindings parse and format through SDL canonical names", "[settings][key-binding]") {
    const auto escape = arcadeblocks::settings::canonicalizeKeyName("escape");
    REQUIRE(escape);
    REQUIRE(*escape == "Escape");
    REQUIRE(arcadeblocks::settings::isValidKeyName("Left"));
    REQUIRE_FALSE(arcadeblocks::settings::isValidKeyName("not-a-real-key"));
    REQUIRE(arcadeblocks::settings::keyNamesEqual("escape", "Escape"));
}

TEST_CASE("duplicate binding validation covers every control action", "[settings][key-binding]") {
    auto controls = arcadeblocks::settings::defaultSettings().controls;
    REQUIRE_FALSE(arcadeblocks::settings::findDuplicateBinding(controls));

    controls.plasma.keyName = "b";
    const auto conflict = arcadeblocks::settings::findDuplicateBinding(controls);
    REQUIRE(conflict);
    REQUIRE(conflict->firstAction == "callBall");
    REQUIRE(conflict->secondAction == "plasma");
}
