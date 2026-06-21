#pragma once

#include "localization/Localization.hpp"
#include "render/Texture.hpp"
#include "settings/Settings.hpp"
#include "ui/UiTheme.hpp"

#include <string>

namespace arcadeblocks::ui {

enum class LevelCompleteAction {
    None,
    Restart,
    Continue
};

struct LevelCompleteRenderResult {
    LevelCompleteAction action = LevelCompleteAction::None;
    UiSoundEffect soundEffect = UiSoundEffect::None;
};

struct LevelCompleteStats {
    std::string playerName;
    int levelNumber;
    int score;
    double timeSeconds;
    int livesLost;
    int positiveBonuses = 0;
    int negativeBonuses = 0;
};

class LevelCompleteView {
public:
    LevelCompleteRenderResult render(
        const render::Texture* backgroundTexture,
        const localization::Localization& localization,
        settings::Language language,
        const LevelCompleteStats& stats,
        double activeDuration,
        bool& open);

    void reset() noexcept {
        lastPlayedStar_ = -1;
        selectedIndex_ = 1;
        wasOpen_ = false;
    }

private:
    int lastPlayedStar_ = -1;
    int selectedIndex_ = 1; // Default to 'Continue' button
    bool wasOpen_ = false;
};

} // namespace arcadeblocks::ui
