#include "settings/KeyBinding.hpp"

#include "settings/Settings.hpp"

#include <SDL3/SDL.h>

#include <array>

namespace arcadeblocks::settings {

std::optional<std::string> canonicalizeKeyName(std::string_view keyName) {
    if (keyName.empty()) {
        return std::nullopt;
    }

    std::string normalized{keyName};
    const SDL_Keycode keycode = SDL_GetKeyFromName(normalized.c_str());
    if (keycode == SDLK_UNKNOWN) {
        return std::nullopt;
    }

    const char* canonical = SDL_GetKeyName(keycode);
    if (canonical == nullptr || canonical[0] == '\0') {
        return std::nullopt;
    }

    return std::string{canonical};
}

bool isValidKeyName(std::string_view keyName) {
    return canonicalizeKeyName(keyName).has_value();
}

bool keyNamesEqual(std::string_view left, std::string_view right) {
    const auto leftCanonical = canonicalizeKeyName(left);
    if (!leftCanonical) {
        return false;
    }

    const auto rightCanonical = canonicalizeKeyName(right);
    if (!rightCanonical) {
        return false;
    }

    return *leftCanonical == *rightCanonical;
}

std::optional<KeyBindingConflict> findDuplicateBinding(const ControlSettings& controls) {
    struct NamedBinding {
        std::string_view action;
        const KeyBinding* binding;
    };

    const std::array<NamedBinding, 8> bindings{{
        {"moveLeft", &controls.moveLeft},
        {"moveRight", &controls.moveRight},
        {"launch", &controls.launch},
        {"callBall", &controls.callBall},
        {"turbo", &controls.turbo},
        {"turboBall", &controls.turboBall},
        {"plasma", &controls.plasma},
        {"pause", &controls.pause},
    }};

    for (std::size_t left = 0; left < bindings.size(); ++left) {
        for (std::size_t right = left + 1; right < bindings.size(); ++right) {
            if (keyNamesEqual(bindings[left].binding->keyName, bindings[right].binding->keyName)) {
                return KeyBindingConflict{bindings[left].action, bindings[right].action};
            }
        }
    }
    return std::nullopt;
}

} // namespace arcadeblocks::settings
