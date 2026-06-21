#include "core/Version.hpp"

namespace arcadeblocks::core {

std::string_view version() noexcept {
    return ARCADEBLOCKS_VERSION;
}

std::string_view productName() noexcept {
    return "Arcade Blocks II";
}

std::string_view sdlTargetVersion() noexcept {
    return ARCADEBLOCKS_SDL_VERSION;
}

} // namespace arcadeblocks::core
