#include "levels/LevelTypes.hpp"

namespace arcadeblocks::levels {

std::string_view toString(BrickColor color) noexcept {
    switch (color) {
    case BrickColor::Blue:
        return "blue";
    case BrickColor::Cyan:
        return "cyan";
    case BrickColor::DarkBlue:
        return "dark_blue";
    case BrickColor::Explosive:
        return "explosive";
    case BrickColor::Green:
        return "green";
    case BrickColor::Indestructible:
        return "indestructible";
    case BrickColor::LightGray:
        return "light_gray";
    case BrickColor::Orange:
        return "orange";
    case BrickColor::Pink:
        return "pink";
    case BrickColor::Purple:
        return "purple";
    case BrickColor::Red:
        return "red";
    case BrickColor::Shielded:
        return "shielded";
    case BrickColor::Yellow:
        return "yellow";
    }

    return "blue";
}

BrickColor fallbackBrickColor() noexcept {
    return BrickColor::Blue;
}

std::optional<BrickColor> parseBrickColor(std::string_view color) noexcept {
    if (color == "blue") {
        return BrickColor::Blue;
    }
    if (color == "cyan") {
        return BrickColor::Cyan;
    }
    if (color == "dark_blue") {
        return BrickColor::DarkBlue;
    }
    if (color == "explosive" || color == "*") {
        return BrickColor::Explosive;
    }
    if (color == "green") {
        return BrickColor::Green;
    }
    if (color == "indestructible") {
        return BrickColor::Indestructible;
    }
    if (color == "light_gray") {
        return BrickColor::LightGray;
    }
    if (color == "orange") {
        return BrickColor::Orange;
    }
    if (color == "pink") {
        return BrickColor::Pink;
    }
    if (color == "purple") {
        return BrickColor::Purple;
    }
    if (color == "red") {
        return BrickColor::Red;
    }
    if (color == "shielded") {
        return BrickColor::Shielded;
    }
    if (color == "yellow") {
        return BrickColor::Yellow;
    }

    return std::nullopt;
}

} // namespace arcadeblocks::levels
