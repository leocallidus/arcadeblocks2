#pragma once

#include "localization/Localization.hpp"
#include "render/SpriteAtlas.hpp"
#include "render/Texture.hpp"
#include "settings/Settings.hpp"
#include "ui/UiTheme.hpp"

namespace arcadeblocks::ui {

struct HelpViewAssets {
    const render::SpriteAtlas* atlas = nullptr;
    render::Texture* atlasTexture = nullptr;
};

enum class HelpAction {
    None,
    Close
};

struct HelpRenderResult {
    HelpAction action = HelpAction::None;
    UiSoundEffect soundEffect = UiSoundEffect::None;
};

class HelpView {
public:
    HelpRenderResult render(
        const localization::Localization& localization,
        settings::Language language,
        const settings::GameSettings& settings,
        const HelpViewAssets& assets,
        bool openedFromPause,
        bool openedThisFrame);

private:
    bool closingAnimation_ = false;
};

} // namespace arcadeblocks::ui
