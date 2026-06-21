#pragma once

#include "localization/Localization.hpp"
#include "render/Texture.hpp"
#include "settings/Settings.hpp"
#include "ui/UiTheme.hpp"

#include <random>
#include <string>
#include <vector>

namespace arcadeblocks::ui {

/// Assets required by the loading screen overlay.
struct LoadingScreenAssets {
    const render::Texture* gameLogo = nullptr;       ///< Arcade Blocks II logo (lower-left).
    const render::Texture* studioLogo = nullptr;     ///< Leocallidus Games logo.
    const render::Texture* menuBackground = nullptr; ///< Menu background drawn behind the overlay.
};

/// Result returned each frame by LoadingScreenView::render().
struct LoadingScreenResult {
    bool dismissed = false; ///< True when the loading screen should be removed.
};

/// A beautiful, one-shot loading/splash screen shown at game launch.
///
/// Shows:
///   - Menu background (behind everything)
///   - Game logo (lower-left corner)
///   - Studio logo (centered, large)
///   - Animated loading bar
///   - Randomized welcome message (localized)
///
/// The screen auto-closes after ~1 second (customizable) or when the user
/// presses ESC.  On close it fades out smoothly over a short duration.
class LoadingScreenView {
public:
    /// Seed the random welcome-message index once at construction.
    explicit LoadingScreenView(unsigned int seed);

    /// Call once per frame.  Returns `dismissed == true` when the loading
    /// screen has fully faded out and should be removed from the render loop.
    LoadingScreenResult render(
        const LoadingScreenAssets& assets,
        const localization::Localization& localization,
        settings::Language language);

    /// True while the loading screen is still visible (including fade-out).
    [[nodiscard]] bool active() const noexcept;

    /// True once the loading screen is fully done (post-fade).
    [[nodiscard]] bool finished() const noexcept;

    /// Force-dismiss (ESC pressed externally).
    void dismiss();

private:
    static constexpr double displayDuration_ = 1.0;   ///< seconds before auto-dismiss
    static constexpr double fadeOutDuration_ = 0.5;    ///< seconds to fade out
    static constexpr int welcomeMessageCount_ = 15;

    [[nodiscard]] std::string welcomeKey() const;

    std::mt19937 rng_;
    int welcomeIndex_ = 0;
    double startTime_ = -1.0;
    double dismissedAt_ = -1.0;
    bool dismissed_ = false;
    bool finished_ = false;
};

} // namespace arcadeblocks::ui
