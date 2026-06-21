#pragma once

#include "localization/Localization.hpp"
#include "settings/Settings.hpp"
#include "ui/UiTheme.hpp"

namespace arcadeblocks::ui {

enum class PauseAction {
    None,
    Resume,
    Settings,
    Help,
    Restart,
    ExitToMenu
};

struct PauseRenderResult {
    PauseAction action = PauseAction::None;
    UiSoundEffect soundEffect = UiSoundEffect::None;
};

class PauseView {
public:
    PauseRenderResult render(const localization::Localization& localization, settings::Language language, bool blocked, bool openedThisFrame, bool pauseRequested);

private:
    int selectedIndex_ = 0;
    bool closingAnimation_ = false;
    PauseAction pendingAction_ = PauseAction::None;
};

} // namespace arcadeblocks::ui
