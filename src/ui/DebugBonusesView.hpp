#pragma once

#include "localization/Localization.hpp"
#include "render/Texture.hpp"
#include "settings/Settings.hpp"
#include "ui/UiTheme.hpp"

#include <string>
#include <vector>

namespace arcadeblocks::ui {

enum class DebugBonusesAction {
    None,
    Back,
    Close
};

struct DebugBonusesRenderResult {
    DebugBonusesAction action = DebugBonusesAction::None;
    UiSoundEffect soundEffect = UiSoundEffect::None;
    bool isVisible = false;
};

struct BonusInfo {
    std::string id;
    std::string nameEn;
    std::string nameRu;
    bool enabled = true;
};

class DebugBonusesView {
public:
    DebugBonusesView();

    DebugBonusesRenderResult render(
        const render::Texture* backgroundTexture,
        const localization::Localization& localization,
        settings::Language language,
        bool& open);

    [[nodiscard]] std::vector<std::string> getEnabledBonuses() const;

private:
    std::vector<BonusInfo> positiveBonuses_;
    std::vector<BonusInfo> negativeBonuses_;
    std::vector<BonusInfo> specialBonuses_;

    int selectedIndex_ = 0; // Bottom action buttons selection
    bool wasOpen_ = false;
    double animationStartedAt_ = -1.0;
};

} // namespace arcadeblocks::ui
