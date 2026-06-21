#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace arcadeblocks::settings {

struct KeyBinding {
    std::string keyName;
};

struct ControlSettings;

struct KeyBindingConflict {
    std::string_view firstAction;
    std::string_view secondAction;
};

[[nodiscard]] std::optional<std::string> canonicalizeKeyName(std::string_view keyName);
[[nodiscard]] bool isValidKeyName(std::string_view keyName);
[[nodiscard]] bool keyNamesEqual(std::string_view left, std::string_view right);
[[nodiscard]] std::optional<KeyBindingConflict> findDuplicateBinding(const ControlSettings& controls);

} // namespace arcadeblocks::settings
