#include "ui/PauseView.hpp"

#include "ui/UiLayout.hpp"

#include <imgui.h>

#include <algorithm>

namespace arcadeblocks::ui {
namespace {

std::string t(const localization::Localization& localization, settings::Language language, std::string_view key) {
    return localization.text(language, key);
}

bool activationPressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space);
}

} // namespace

PauseRenderResult PauseView::render(const localization::Localization& localization, settings::Language language, bool blocked, bool openedThisFrame, bool pauseRequested) {
    PauseRenderResult result;
    constexpr int itemCount = 5;
    selectedIndex_ = std::clamp(selectedIndex_, 0, itemCount - 1);

    if (openedThisFrame) {
        closingAnimation_ = false;
        pendingAction_ = PauseAction::None;
        selectedIndex_ = 0;
    }

    if (!blocked && pauseRequested && !closingAnimation_) {
        closingAnimation_ = true;
        pendingAction_ = PauseAction::Resume;
        result.soundEffect = UiSoundEffect::Back;
    }

    bool animDone = false;
    const float animT = UiTheme::animateWindow("PauseWindow", closingAnimation_, animDone);
    if (closingAnimation_ && animDone) {
        result.action = pendingAction_;
        closingAnimation_ = false;
        return result;
    }

    UiTheme::renderModalOverlay(0.9f * animT);
    UiTheme::pushWindowAnimation(animT);

    if (!blocked) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            selectedIndex_ = selectedIndex_ > 0 ? selectedIndex_ - 1 : itemCount - 1;
            result.soundEffect = UiSoundEffect::Hover;
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            selectedIndex_ = selectedIndex_ < itemCount - 1 ? selectedIndex_ + 1 : 0;
            result.soundEffect = UiSoundEffect::Hover;
        }
    }

    UiLayout::setNextCenteredWindow(460.0f, 404.0f);
    ImGui::SetNextWindowBgAlpha(0.88f);
    const auto windowTitle = t(localization, language, "pause.window_title") + "###PauseWindow";
    ImGui::Begin(
        windowTitle.c_str(),
        nullptr,
        ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoSavedSettings);

    const auto panelTitle = t(localization, language, "pause.title");
    UiTheme::renderCyberpunkPanelTitle(panelTitle.c_str(), UiAccent::Cyan);
    const ImVec2 buttonSize{ImGui::GetContentRegionAvail().x, UiTheme::buttonHeight()};

    const auto resumeLabel = t(localization, language, "pause.resume");
    const auto resumeButton = UiTheme::renderNeonButton("resume", resumeLabel.c_str(), UiAccent::Cyan, buttonSize, selectedIndex_ == 0);
    if (!blocked && resumeButton.hovered && selectedIndex_ != 0) {
        selectedIndex_ = 0;
        result.soundEffect = UiSoundEffect::Hover;
    }
    if (!blocked && (resumeButton.pressed || (selectedIndex_ == 0 && activationPressed()))) {
        pendingAction_ = PauseAction::Resume;
        closingAnimation_ = true;
        result.soundEffect = UiSoundEffect::Back;
    }

    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const auto settingsLabel = t(localization, language, "pause.settings");
    const auto settingsButton = UiTheme::renderNeonButton("pause-settings", settingsLabel.c_str(), UiAccent::Purple, buttonSize, selectedIndex_ == 1);
    if (!blocked && settingsButton.hovered && selectedIndex_ != 1) {
        selectedIndex_ = 1;
        result.soundEffect = UiSoundEffect::Hover;
    }
    if (!blocked && (settingsButton.pressed || (selectedIndex_ == 1 && activationPressed()))) {
        result.action = PauseAction::Settings;
        result.soundEffect = UiSoundEffect::Select;
    }

    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const auto helpLabel = t(localization, language, "pause.help");
    const auto helpButton = UiTheme::renderNeonButton("pause-help", helpLabel.c_str(), UiAccent::Green, buttonSize, selectedIndex_ == 2);
    if (!blocked && helpButton.hovered && selectedIndex_ != 2) {
        selectedIndex_ = 2;
        result.soundEffect = UiSoundEffect::Hover;
    }
    if (!blocked && (helpButton.pressed || (selectedIndex_ == 2 && activationPressed()))) {
        result.action = PauseAction::Help;
        result.soundEffect = UiSoundEffect::Select;
    }

    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const auto restartLabel = t(localization, language, "pause.restart");
    const auto restartButton = UiTheme::renderNeonButton("restart", restartLabel.c_str(), UiAccent::Orange, buttonSize, selectedIndex_ == 3);
    if (!blocked && restartButton.hovered && selectedIndex_ != 3) {
        selectedIndex_ = 3;
        result.soundEffect = UiSoundEffect::Hover;
    }
    if (!blocked && (restartButton.pressed || (selectedIndex_ == 3 && activationPressed()))) {
        pendingAction_ = PauseAction::Restart;
        closingAnimation_ = true;
        result.soundEffect = UiSoundEffect::Select;
    }

    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const auto exitLabel = t(localization, language, "pause.exit_to_menu");
    const auto exitButton = UiTheme::renderNeonButton("exit", exitLabel.c_str(), UiAccent::Yellow, buttonSize, selectedIndex_ == 4);
    if (!blocked && exitButton.hovered && selectedIndex_ != 4) {
        selectedIndex_ = 4;
        result.soundEffect = UiSoundEffect::Hover;
    }
    if (!blocked && (exitButton.pressed || (selectedIndex_ == 4 && activationPressed()))) {
        pendingAction_ = PauseAction::ExitToMenu;
        closingAnimation_ = true;
        result.soundEffect = UiSoundEffect::Select;
    }

    ImGui::End();
    UiTheme::popWindowAnimation();
    return result;
}

} // namespace arcadeblocks::ui
