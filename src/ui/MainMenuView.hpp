#pragma once

#include "localization/Localization.hpp"
#include "render/Texture.hpp"
#include "settings/Settings.hpp"
#include "ui/UiTheme.hpp"

namespace arcadeblocks::ui {

enum class MainMenuAction {
    None,
    StartLevel1,
    Exit
};

struct MainMenuSceneAssets {
    const render::Texture* logo = nullptr;
    const render::Texture* background = nullptr;
};

struct MainMenuRenderResult {
    MainMenuAction action = MainMenuAction::None;
    UiSoundEffect soundEffect = UiSoundEffect::None;
};

class MainMenuView {
public:
    MainMenuRenderResult render(
        const MainMenuSceneAssets& assets,
        const localization::Localization& localization,
        settings::Language language,
        bool& settingsOpen,
        bool& helpOpen,
        bool& exitConfirmOpen);

private:
    int selectedIndex_ = 0;
    int exitConfirmSelectedIndex_ = 0;
    double animationStartedAt_ = -1.0;
    double lastRenderTime_ = -1.0;
};

} // namespace arcadeblocks::ui
