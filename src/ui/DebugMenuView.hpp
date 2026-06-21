#pragma once

#include "localization/Localization.hpp"
#include "render/Texture.hpp"
#include "settings/Settings.hpp"
#include "ui/UiTheme.hpp"

namespace arcadeblocks::ui {

enum class DebugMenuAction {
    None,
    OpenLevels,
    OpenBonuses,
    Close
};

struct DebugMenuRenderResult {
    DebugMenuAction action = DebugMenuAction::None;
    UiSoundEffect soundEffect = UiSoundEffect::None;
    bool isVisible = false;
};

class DebugMenuView {
public:
    DebugMenuRenderResult render(
        const render::Texture* backgroundTexture,
        const localization::Localization& localization,
        settings::Language language,
        bool& open);

private:
    double animationStartedAt_ = -1.0;
    bool wasOpen_ = false;
    int selectedIndex_ = 0;
};

} // namespace arcadeblocks::ui
