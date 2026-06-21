#pragma once

#include <string_view>

namespace arcadeblocks::core {

std::string_view version() noexcept;
std::string_view productName() noexcept;
std::string_view sdlTargetVersion() noexcept;

} // namespace arcadeblocks::core
