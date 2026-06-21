#pragma once

#include "assets/AssetRegistry.hpp"
#include "localization/Localization.hpp"
#include "render/Texture.hpp"
#include "settings/Settings.hpp"
#include "ui/UiTheme.hpp"

#include <vector>

namespace arcadeblocks::ui {

enum class LevelsMenuAction {
    None,
    StartLevel,
    Close
};

struct LevelsMenuRenderResult {
    LevelsMenuAction action = LevelsMenuAction::None;
    UiSoundEffect soundEffect = UiSoundEffect::None;
    int selectedLevel = 1;
    bool isVisible = false;
};

class LevelsMenuView {
public:
    LevelsMenuRenderResult render(
        const render::Texture* backgroundTexture,
        const assets::AssetRegistry& assetRegistry,
        const localization::Localization& localization,
        settings::Language language,
        bool& open);

private:
    double animationStartedAt_ = -1.0;
    bool wasOpen_ = false;
    int selectedIndex_ = 0;
    bool levelsDiscovered_ = false;
    struct LevelEntry {
        int index;
        std::string name;
    };
    std::vector<LevelEntry> availableLevels_;

    void discoverLevels(const assets::AssetRegistry& assetRegistry);
};

} // namespace arcadeblocks::ui
