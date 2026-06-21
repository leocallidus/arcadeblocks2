#include "ui/SettingsView.hpp"

#include "settings/KeyBinding.hpp"
#include "ui/UiLayout.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace arcadeblocks::ui {
namespace {

constexpr int hoverWidgetNone = -1;
constexpr int hoverWidgetTabAudio = 100;
constexpr int hoverWidgetTabVideo = 101;
constexpr int hoverWidgetTabControls = 102;
constexpr int hoverWidgetTabGameplay = 103;
constexpr int hoverWidgetTabLanguage = 104;
constexpr int hoverWidgetApply = 200;
constexpr int hoverWidgetTest = 201;
constexpr int hoverWidgetReset = 202;
constexpr int hoverWidgetBack = 203;
constexpr int hoverWidgetSave = 204;
constexpr int hoverWidgetDiscard = 205;
constexpr int hoverWidgetCancel = 206;

constexpr std::array<std::string_view, 5> resolutionOptions{{
    "1280x720",
    "1600x900",
    "1920x1080",
    "2560x1440",
    "3840x2160",
}};

bool nearlyEqual(double left, double right) {
    return std::abs(left - right) < 0.0001;
}

bool sameAudio(const settings::AudioSettings& left, const settings::AudioSettings& right) {
    return nearlyEqual(left.masterVolume, right.masterVolume)
        && nearlyEqual(left.musicVolume, right.musicVolume)
        && nearlyEqual(left.sfxVolume, right.sfxVolume)
        && left.callBallSound == right.callBallSound;
}

bool sameVideo(const settings::VideoSettings& left, const settings::VideoSettings& right) {
    return left.windowMode == right.windowMode
        && left.resolution == right.resolution
        && left.vsync == right.vsync
        && std::abs(left.uiScale - right.uiScale) < 0.0001f
        && left.showLevelBackground == right.showLevelBackground
        && left.fpsLimit == right.fpsLimit;
}

bool sameGameplay(const settings::GameplaySettings& left, const settings::GameplaySettings& right) {
    return left.playerName == right.playerName
        && std::abs(left.paddleSpeed - right.paddleSpeed) < 0.0001f
        && std::abs(left.turboSpeed - right.turboSpeed) < 0.0001f
        && left.difficulty == right.difficulty
        && left.showLaunchTrajectory == right.showLaunchTrajectory;
}

bool sameControls(const settings::ControlSettings& left, const settings::ControlSettings& right) {
    return left.moveLeft.keyName == right.moveLeft.keyName
        && left.moveRight.keyName == right.moveRight.keyName
        && left.launch.keyName == right.launch.keyName
        && left.callBall.keyName == right.callBall.keyName
        && left.turbo.keyName == right.turbo.keyName
        && left.turboBall.keyName == right.turboBall.keyName
        && left.plasma.keyName == right.plasma.keyName
        && left.pause.keyName == right.pause.keyName;
}

bool isReservedKey(std::string_view keyName) {
    return settings::keyNamesEqual(keyName, "F1")
        || settings::keyNamesEqual(keyName, "F2")
        || settings::keyNamesEqual(keyName, "F3")
        || settings::keyNamesEqual(keyName, "F4")
        || settings::keyNamesEqual(keyName, "F11");
}

std::string difficultyLabel(const localization::Localization& localization, settings::Language language, settings::Difficulty difficulty) {
    switch (difficulty) {
    case settings::Difficulty::Easy:
        return localization.text(language, "settings.gameplay.difficulty.easy");
    case settings::Difficulty::Normal:
        return localization.text(language, "settings.gameplay.difficulty.normal");
    case settings::Difficulty::Hard:
        return localization.text(language, "settings.gameplay.difficulty.hard");
    case settings::Difficulty::Hardcore:
        return localization.text(language, "settings.gameplay.difficulty.hardcore");
    }
    return localization.text(language, "settings.gameplay.difficulty.normal");
}

bool volumeSlider(const char* label, double& value) {
    float asFloat = static_cast<float>(value);
    const bool changed = ImGui::SliderFloat(label, &asFloat, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (changed) {
        value = static_cast<double>(std::clamp(asFloat, 0.0f, 1.0f));
    }
    return changed;
}

void statusText(const std::string& status) {
    if (status.empty()) {
        ImGui::TextUnformatted(" ");
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::accent(UiAccent::Yellow));
    ImGui::TextWrapped("%s", status.c_str());
    ImGui::PopStyleColor();
}

} // namespace

SettingsRenderResult SettingsView::render(const localization::Localization& localization, const settings::GameSettings& appliedSettings, SettingsContext context) {
    if (!sessionActive_) {
        beginSession(appliedSettings);
    }
    localization_ = &localization;
    context_ = context;

    SettingsRenderResult result;
    int currentHoveredWidget = hoverWidgetNone;

    // Animate window open/close.
    bool animDone = false;
    const float animT = UiTheme::animateWindow("SettingsWindow", closingAnimation_, animDone);
    if (closingAnimation_ && animDone) {
        result.action = SettingsAction::Close;
        resetSession();
        return result;
    }

    UiTheme::renderModalOverlay(0.92f * animT);
    UiTheme::pushWindowAnimation(animT);
    UiLayout::setNextCenteredWindow(940.0f, 680.0f);
    ImGui::SetNextWindowBgAlpha(0.97f);
    const auto windowTitle = text("settings.window_title") + "###SettingsWindow";
    ImGui::Begin(
        windowTitle.c_str(),
        nullptr,
        ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoSavedSettings);

    const auto title = text("settings.title");
    UiTheme::renderCyberpunkPanelTitle(title.c_str(), UiAccent::Purple);

    const float scale = UiLayout::viewportScale();
    UiTheme::beginScrollPanel("settings-audio-video", ImVec2{0.0f, 490.0f * scale});

    struct TabSpec {
        SettingsTab tab;
        const char* key;
        UiAccent accent;
        int hoverIndex;
    };
    static constexpr std::array<TabSpec, 5> tabSpecs{{
        {SettingsTab::Audio, "settings.tabs.audio", UiAccent::Cyan, hoverWidgetTabAudio},
        {SettingsTab::Video, "settings.tabs.video", UiAccent::Purple, hoverWidgetTabVideo},
        {SettingsTab::Controls, "settings.tabs.controls", UiAccent::Green, hoverWidgetTabControls},
        {SettingsTab::Gameplay, "settings.tabs.gameplay", UiAccent::Pink, hoverWidgetTabGameplay},
        {SettingsTab::Language, "settings.tabs.language", UiAccent::Yellow, hoverWidgetTabLanguage},
    }};

    const float tabBarWidth = ImGui::GetContentRegionAvail().x;
    const float tabSpacing = 8.0f * scale;
    const ImVec2 tabSize{(tabBarWidth - tabSpacing * static_cast<float>(tabSpecs.size() - 1)) / static_cast<float>(tabSpecs.size()), 44.0f * scale};

    for (std::size_t i = 0; i < tabSpecs.size(); ++i) {
        const auto& spec = tabSpecs[i];
        if (i > 0) {
            ImGui::SameLine(0.0f, tabSpacing);
        }
        const auto label = text(spec.key);
        const bool selected = currentTab_ == spec.tab;
        const auto button = UiTheme::renderCyberpunkTabButton(spec.key, label.c_str(), spec.accent, selected, tabSize);
        if (button.hovered) {
            currentHoveredWidget = spec.hoverIndex;
            if (!selected && hoveredWidget_ != spec.hoverIndex) {
                result.soundEffect = UiSoundEffect::Hover;
            }
        }
        if (button.pressed && !selected) {
            currentTab_ = spec.tab;
            result.soundEffect = UiSoundEffect::Select;
        }
    }

    ImGui::Dummy(ImVec2{0.0f, 12.0f * scale});

    switch (currentTab_) {
    case SettingsTab::Audio:
        renderAudio(result);
        break;
    case SettingsTab::Video:
        renderVideo(result);
        break;
    case SettingsTab::Controls:
        renderControls(result);
        break;
    case SettingsTab::Gameplay:
        renderGameplay(result);
        break;
    case SettingsTab::Language:
        renderLanguage(result);
        break;
    }

    UiTheme::endScrollPanel();

    statusText(status_);

    const float buttonWidth = (ImGui::GetContentRegionAvail().x - UiTheme::itemSpacing() * 3.0f) * 0.25f;
    const ImVec2 buttonSize{buttonWidth, UiTheme::buttonHeight()};

    const auto applyLabel = text("common.apply");
    const auto applyButton = UiTheme::renderNeonButton("settings-apply", applyLabel.c_str(), UiAccent::Green, buttonSize, dirty());
    if (applyButton.hovered) {
        currentHoveredWidget = hoverWidgetApply;
    }
    if (applyButton.pressed) {
        result = applyAndClose(false);
    }

    ImGui::SameLine(0.0f, UiTheme::itemSpacing());
    const auto testLabel = text("settings.buttons.test_sound");
    const auto testButton = UiTheme::renderNeonButton("settings-test-sound", testLabel.c_str(), UiAccent::Cyan, buttonSize, false);
    if (testButton.hovered) {
        currentHoveredWidget = hoverWidgetTest;
    }
    if (testButton.pressed) {
        result.action = SettingsAction::TestSound;
        result.soundEffect = UiSoundEffect::Select;
    }

    ImGui::SameLine(0.0f, UiTheme::itemSpacing());
    const auto resetLabel = text("common.reset");
    const auto resetButton = UiTheme::renderNeonButton("settings-reset", resetLabel.c_str(), UiAccent::Orange, buttonSize, false);
    if (resetButton.hovered) {
        currentHoveredWidget = hoverWidgetReset;
    }
    if (resetButton.pressed) {
        draft_ = settings::defaultSettings();
        result.audioPreviewChanged = true;
        result.settings = draft_;
        result.soundEffect = UiSoundEffect::SettingsChange;
        status_ = text("settings.status.defaults_staged");
    }

    ImGui::SameLine(0.0f, UiTheme::itemSpacing());
    const auto backLabel = text("common.back");
    const auto backButton = UiTheme::renderNeonButton("settings-back", backLabel.c_str(), UiAccent::Yellow, buttonSize, false);
    if (backButton.hovered) {
        currentHoveredWidget = hoverWidgetBack;
    }
    if (backButton.pressed || (ImGui::IsKeyPressed(ImGuiKey_Escape) && !closingAnimation_)) {
        if (dirty()) {
            unsavedDialogOpen_ = true;
            result.soundEffect = UiSoundEffect::Back;
        } else {
            closingAnimation_ = true;
            result.soundEffect = UiSoundEffect::Back;
        }
    }

    renderUnsavedDialog(result, currentHoveredWidget);
    updateHoverSound(result, currentHoveredWidget);

    ImGui::End();
    UiTheme::popWindowAnimation();
    return result;
}

bool SettingsView::handleKeyCapture(SDL_Keycode keycode) {
    if (!sessionActive_ || captureTarget_ == CaptureTarget::None) {
        return false;
    }

    if (keycode == SDLK_ESCAPE) {
        captureTarget_ = CaptureTarget::None;
        status_ = text("settings.status.key_capture_cancelled");
        return true;
    }

    const char* keyName = SDL_GetKeyName(keycode);
    if (keyName == nullptr || keyName[0] == '\0') {
        status_ = text("settings.status.unknown_key");
        return true;
    }

    const auto canonical = settings::canonicalizeKeyName(keyName);
    if (!canonical) {
        status_ = text("settings.status.unsupported_key");
        return true;
    }

    if (isReservedKey(*canonical)) {
        status_ = text("settings.validation.reserved_keys");
        return true;
    }

    if (settings::KeyBinding* binding = bindingFor(captureTarget_)) {
        binding->keyName = *canonical;
        status_ = text("settings.status.key_assigned");
        captureTarget_ = CaptureTarget::None;
        return true;
    }

    captureTarget_ = CaptureTarget::None;
    return true;
}

void SettingsView::resetSession() {
    sessionActive_ = false;
    unsavedDialogOpen_ = false;
    closingAnimation_ = false;
    status_.clear();
    hoveredWidget_ = hoverWidgetNone;
}

void SettingsView::setStatus(std::string status) {
    status_ = std::move(status);
}

std::string SettingsView::text(std::string_view key) const {
    if (localization_ == nullptr) {
        return std::string{key};
    }
    return localization_->text(draft_.language, key);
}

void SettingsView::beginSession(const settings::GameSettings& appliedSettings) {
    baseline_ = appliedSettings;
    draft_ = appliedSettings;
    status_.clear();
    unsavedDialogOpen_ = false;
    sessionActive_ = true;
    hoveredWidget_ = hoverWidgetNone;
}

bool SettingsView::dirty() const {
    return draft_.language != baseline_.language
        || !sameAudio(draft_.audio, baseline_.audio)
        || !sameVideo(draft_.video, baseline_.video)
        || !sameGameplay(draft_.gameplay, baseline_.gameplay)
        || !sameControls(draft_.controls, baseline_.controls);
}

SettingsRenderResult SettingsView::applyAndClose(bool close) {
    if (const auto error = validateDraft()) {
        SettingsRenderResult result;
        result.soundEffect = UiSoundEffect::Back;
        status_ = *error;
        return result;
    }

    baseline_ = draft_;
    SettingsRenderResult result;
    result.action = close ? SettingsAction::Close : SettingsAction::Apply;
    result.settings = draft_;
    result.audioPreviewChanged = true;
    result.soundEffect = UiSoundEffect::Select;
    status_ = text("settings.status.applied");
    if (close) {
        resetSession();
    }
    return result;
}

std::optional<std::string> SettingsView::validateDraft() const {
    if (draft_.gameplay.playerName.empty()) {
        return text("settings.validation.player_name_empty");
    }
    if (draft_.gameplay.paddleSpeed < 200.0f || draft_.gameplay.paddleSpeed > 2000.0f) {
        return text("settings.validation.paddle_speed_range");
    }
    if (draft_.gameplay.turboSpeed < 400.0f || draft_.gameplay.turboSpeed > 12000.0f) {
        return text("settings.validation.turbo_speed_range");
    }
    if (settings::findDuplicateBinding(draft_.controls)) {
        return text("settings.validation.duplicate_keys");
    }

    const std::array<std::string_view, 8> keys{{
        draft_.controls.moveLeft.keyName,
        draft_.controls.moveRight.keyName,
        draft_.controls.launch.keyName,
        draft_.controls.callBall.keyName,
        draft_.controls.turbo.keyName,
        draft_.controls.turboBall.keyName,
        draft_.controls.plasma.keyName,
        draft_.controls.pause.keyName,
    }};
    for (const auto key : keys) {
        if (isReservedKey(key)) {
            return text("settings.validation.reserved_keys");
        }
    }

    return std::nullopt;
}

SettingsRenderResult SettingsView::discardAndClose() {
    draft_ = baseline_;
    SettingsRenderResult result;
    result.settings = baseline_;
    result.audioPreviewChanged = true;
    result.soundEffect = UiSoundEffect::Back;
    closingAnimation_ = true; // Will emit Close once the animation finishes.
    return result;
}

void SettingsView::updateHoverSound(SettingsRenderResult& result, int currentHoveredWidget) {
    if (currentHoveredWidget != hoverWidgetNone
        && currentHoveredWidget != hoveredWidget_
        && result.soundEffect == UiSoundEffect::None) {
        result.soundEffect = UiSoundEffect::Hover;
    }
    hoveredWidget_ = currentHoveredWidget;
}

void SettingsView::renderAudio(SettingsRenderResult& result) {
    const auto title = text("settings.audio.title");
    UiTheme::beginSectionHeader(title.c_str(), UiAccent::Cyan);
    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const auto masterVolume = text("settings.audio.master_volume");
    if (volumeSlider(masterVolume.c_str(), draft_.audio.masterVolume)) {
        result.audioPreviewChanged = true;
        result.settings = draft_;
    }
    const auto musicVolume = text("settings.audio.music_volume");
    if (volumeSlider(musicVolume.c_str(), draft_.audio.musicVolume)) {
        result.audioPreviewChanged = true;
        result.settings = draft_;
    }
    const auto sfxVolume = text("settings.audio.sfx_volume");
    if (volumeSlider(sfxVolume.c_str(), draft_.audio.sfxVolume)) {
        result.audioPreviewChanged = true;
        result.settings = draft_;
    }

    bool callBallSound = draft_.audio.callBallSound;
    const auto callBallSoundLabel = text("settings.audio.call_ball_sound");
    if (ImGui::Checkbox(callBallSoundLabel.c_str(), &callBallSound)) {
        draft_.audio.callBallSound = callBallSound;
        result.audioPreviewChanged = true;
        result.settings = draft_;
        result.soundEffect = UiSoundEffect::SettingsChange;
    }
}

void SettingsView::renderVideo(SettingsRenderResult& result) {
    const auto title = text("settings.video.title");
    UiTheme::beginSectionHeader(title.c_str(), UiAccent::Pink);
    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const bool restrictedInPause = context_ == SettingsContext::Pause;
    if (restrictedInPause) {
        ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::accent(UiAccent::Yellow));
        const auto notice = text("settings.video.pause_restricted");
        ImGui::TextWrapped("%s", notice.c_str());
        ImGui::PopStyleColor();
        ImGui::BeginDisabled();
    }

    int mode = draft_.video.windowMode == settings::VideoWindowMode::Fullscreen ? 1 : 0;
    const auto windowedLabel = text("settings.video.windowed");
    if (ImGui::RadioButton(windowedLabel.c_str(), mode == 0)) {
        draft_.video.windowMode = settings::VideoWindowMode::Windowed;
        result.soundEffect = UiSoundEffect::SettingsChange;
    }
    ImGui::SameLine();
    const auto fullscreenLabel = text("settings.video.fullscreen");
    if (ImGui::RadioButton(fullscreenLabel.c_str(), mode == 1)) {
        draft_.video.windowMode = settings::VideoWindowMode::Fullscreen;
        result.soundEffect = UiSoundEffect::SettingsChange;
    }

    const char* preview = draft_.video.resolution.c_str();
    const auto resolutionLabel = text("settings.video.resolution");
    if (ImGui::BeginCombo(resolutionLabel.c_str(), preview)) {
        for (const auto option : resolutionOptions) {
            const bool selected = draft_.video.resolution == option;
            if (ImGui::Selectable(option.data(), selected)) {
                draft_.video.resolution = std::string{option};
                result.soundEffect = UiSoundEffect::SettingsChange;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    bool vsync = draft_.video.vsync;
    const auto vsyncLabel = text("settings.video.vsync");
    if (ImGui::Checkbox(vsyncLabel.c_str(), &vsync)) {
        draft_.video.vsync = vsync;
        result.soundEffect = UiSoundEffect::SettingsChange;
    }

    float uiScale = draft_.video.uiScale;
    const auto uiScaleLabel = text("settings.video.ui_scale");
    if (ImGui::SliderFloat(uiScaleLabel.c_str(), &uiScale, 0.75f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        draft_.video.uiScale = std::clamp(uiScale, 0.75f, 2.0f);
    }

    bool showBackground = draft_.video.showLevelBackground;
    const auto levelBackgroundLabel = text("settings.video.level_background");
    if (ImGui::Checkbox(levelBackgroundLabel.c_str(), &showBackground)) {
        draft_.video.showLevelBackground = showBackground;
        result.soundEffect = UiSoundEffect::SettingsChange;
    }

    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const auto fpsLimitLabel = text("settings.video.fps_limit");
    int fpsLimit = draft_.video.fpsLimit;
    ImGui::SetNextItemWidth(120.0f * UiLayout::viewportScale());
    if (ImGui::InputInt(fpsLimitLabel.c_str(), &fpsLimit, 10, 60)) {
        // 0 = unlimited; otherwise clamp to a sane range
        if (fpsLimit < 0) fpsLimit = 0;
        if (fpsLimit > 0 && fpsLimit < 10) fpsLimit = 10;
        if (fpsLimit > 999) fpsLimit = 999;
        draft_.video.fpsLimit = fpsLimit;
        result.soundEffect = UiSoundEffect::SettingsChange;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::accent(UiAccent::Cyan));
    const auto fpsHint = text("settings.video.fps_limit_hint");
    ImGui::TextWrapped("%s", fpsHint.c_str());
    ImGui::PopStyleColor();

    if (restrictedInPause) {
        ImGui::EndDisabled();
    }
}

void SettingsView::renderControls(SettingsRenderResult&) {
    const auto title = text("settings.controls.title");
    UiTheme::beginSectionHeader(title.c_str(), UiAccent::Green);
    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    renderKeyCaptureButton(text("settings.controls.move_left").c_str(), CaptureTarget::MoveLeft, draft_.controls.moveLeft);
    renderKeyCaptureButton(text("settings.controls.move_right").c_str(), CaptureTarget::MoveRight, draft_.controls.moveRight);
    renderKeyCaptureButton(text("settings.controls.launch").c_str(), CaptureTarget::Launch, draft_.controls.launch);
    renderKeyCaptureButton(text("settings.controls.call_ball").c_str(), CaptureTarget::CallBall, draft_.controls.callBall);
    renderKeyCaptureButton(text("settings.controls.turbo_paddle").c_str(), CaptureTarget::Turbo, draft_.controls.turbo);
    renderKeyCaptureButton(text("settings.controls.turbo_ball").c_str(), CaptureTarget::TurboBall, draft_.controls.turboBall);
    renderKeyCaptureButton(text("settings.controls.plasma").c_str(), CaptureTarget::Plasma, draft_.controls.plasma);
    renderKeyCaptureButton(text("settings.controls.pause").c_str(), CaptureTarget::Pause, draft_.controls.pause);

    if (captureTarget_ != CaptureTarget::None) {
        ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::accent(UiAccent::Yellow));
        const auto prompt = text("settings.controls.capture_prompt");
        ImGui::TextWrapped("%s", prompt.c_str());
        ImGui::PopStyleColor();
    }
}

void SettingsView::renderGameplay(SettingsRenderResult&) {
    const auto title = text("settings.gameplay.title");
    UiTheme::beginSectionHeader(title.c_str(), UiAccent::Orange);
    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    char playerName[64]{};
    const auto copyLength = std::min(draft_.gameplay.playerName.size(), sizeof(playerName) - 1);
    draft_.gameplay.playerName.copy(playerName, copyLength);
    const auto playerNameLabel = text("settings.gameplay.player_name");
    if (ImGui::InputText(playerNameLabel.c_str(), playerName, sizeof(playerName))) {
        draft_.gameplay.playerName = playerName;
    }

    float paddleSpeed = draft_.gameplay.paddleSpeed;
    const auto paddleSpeedLabel = text("settings.gameplay.paddle_speed");
    if (ImGui::SliderFloat(paddleSpeedLabel.c_str(), &paddleSpeed, 200.0f, 2000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) {
        draft_.gameplay.paddleSpeed = std::clamp(paddleSpeed, 200.0f, 2000.0f);
    }

    float turboSpeed = draft_.gameplay.turboSpeed;
    const auto turboSpeedLabel = text("settings.gameplay.turbo_speed");
    if (ImGui::SliderFloat(turboSpeedLabel.c_str(), &turboSpeed, 400.0f, 12000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) {
        draft_.gameplay.turboSpeed = std::clamp(turboSpeed, 400.0f, 12000.0f);
    }

    const auto difficultyFieldLabel = text("settings.gameplay.difficulty");
    const auto currentDifficulty = difficultyLabel(*localization_, draft_.language, draft_.gameplay.difficulty);
    if (ImGui::BeginCombo(difficultyFieldLabel.c_str(), currentDifficulty.c_str())) {
        constexpr std::array<settings::Difficulty, 4> difficulties{{
            settings::Difficulty::Easy,
            settings::Difficulty::Normal,
            settings::Difficulty::Hard,
            settings::Difficulty::Hardcore,
        }};
        for (const auto difficulty : difficulties) {
            const bool selected = draft_.gameplay.difficulty == difficulty;
            const auto difficultyName = difficultyLabel(*localization_, draft_.language, difficulty);
            if (ImGui::Selectable(difficultyName.c_str(), selected)) {
                draft_.gameplay.difficulty = difficulty;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    bool showLaunchTrajectory = draft_.gameplay.showLaunchTrajectory;
    const auto trajectoryLabel = text("settings.gameplay.show_launch_trajectory");
    if (ImGui::Checkbox(trajectoryLabel.c_str(), &showLaunchTrajectory)) {
        draft_.gameplay.showLaunchTrajectory = showLaunchTrajectory;
    }
}

void SettingsView::renderLanguage(SettingsRenderResult& result) {
    const auto title = text("settings.language.title");
    UiTheme::beginSectionHeader(title.c_str(), UiAccent::Cyan);
    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const auto russianLabel = text("language.russian");
    if (ImGui::RadioButton(russianLabel.c_str(), draft_.language == settings::Language::Russian)) {
        draft_.language = settings::Language::Russian;
        result.settings = draft_;
        result.action = SettingsAction::None;
        result.soundEffect = UiSoundEffect::SettingsChange;
    }
    const auto englishLabel = text("language.english");
    if (ImGui::RadioButton(englishLabel.c_str(), draft_.language == settings::Language::English)) {
        draft_.language = settings::Language::English;
        result.settings = draft_;
        result.action = SettingsAction::None;
        result.soundEffect = UiSoundEffect::SettingsChange;
    }
}

void SettingsView::renderUnsavedDialog(SettingsRenderResult& result, int& currentHoveredWidget) {
    if (!unsavedDialogOpen_) {
        return;
    }

    const auto popupTitle = text("settings.unsaved.window_title") + "###UnsavedSettings";
    ImGui::OpenPopup(popupTitle.c_str());
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(UiLayout::logicalSize(520.0f, 210.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(popupTitle.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    const auto unsavedTitle = text("settings.unsaved.title");
    UiTheme::renderCyberpunkPanelTitle(unsavedTitle.c_str(), UiAccent::Yellow);
    const auto unsavedPrompt = text("settings.unsaved.prompt");
    ImGui::TextWrapped("%s", unsavedPrompt.c_str());
    ImGui::Dummy(ImVec2{0.0f, UiTheme::sectionSpacing()});

    const float width = (ImGui::GetContentRegionAvail().x - UiTheme::itemSpacing() * 2.0f) / 3.0f;
    const ImVec2 size{width, UiTheme::buttonHeight()};
    const auto saveLabel = text("common.save");
    const auto saveButton = UiTheme::renderNeonButton("settings-save-close", saveLabel.c_str(), UiAccent::Green, size, true);
    if (saveButton.hovered) {
        currentHoveredWidget = hoverWidgetSave;
    }
    if (saveButton.hovered && hoveredWidget_ != hoverWidgetSave) {
        result.soundEffect = UiSoundEffect::Hover;
    }
    if (saveButton.pressed) {
        result = applyAndClose(true);
        unsavedDialogOpen_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine(0.0f, UiTheme::itemSpacing());
    const auto discardLabel = text("common.discard");
    const auto discardButton = UiTheme::renderNeonButton("settings-discard-close", discardLabel.c_str(), UiAccent::Red, size, false);
    if (discardButton.hovered) {
        currentHoveredWidget = hoverWidgetDiscard;
    }
    if (discardButton.hovered && hoveredWidget_ != hoverWidgetDiscard && result.soundEffect == UiSoundEffect::None) {
        result.soundEffect = UiSoundEffect::Hover;
    }
    if (discardButton.pressed) {
        result = discardAndClose();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine(0.0f, UiTheme::itemSpacing());
    const auto cancelLabel = text("common.cancel");
    const auto cancelButton = UiTheme::renderNeonButton("settings-cancel-close", cancelLabel.c_str(), UiAccent::Cyan, size, false);
    if (cancelButton.hovered) {
        currentHoveredWidget = hoverWidgetCancel;
    }
    if (cancelButton.hovered && hoveredWidget_ != hoverWidgetCancel && result.soundEffect == UiSoundEffect::None) {
        result.soundEffect = UiSoundEffect::Hover;
    }
    if (cancelButton.pressed
        || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        unsavedDialogOpen_ = false;
        result.soundEffect = UiSoundEffect::Back;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void SettingsView::renderKeyCaptureButton(const char* label, CaptureTarget target, settings::KeyBinding& binding) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(220.0f * UiLayout::viewportScale());

    const bool selected = captureTarget_ == target;
    const auto captureButtonLabel = text("settings.controls.capture_button");
    const char* buttonText = selected ? captureButtonLabel.c_str() : binding.keyName.c_str();
    if (UiTheme::renderNeonButton(label, buttonText, selected ? UiAccent::Yellow : UiAccent::Purple, ImVec2{220.0f * UiLayout::viewportScale(), UiTheme::buttonHeight()}, selected).pressed) {
        captureTarget_ = target;
        status_ = text("settings.controls.capture_prompt");
    }
}

settings::KeyBinding* SettingsView::bindingFor(CaptureTarget target) {
    switch (target) {
    case CaptureTarget::MoveLeft:
        return &draft_.controls.moveLeft;
    case CaptureTarget::MoveRight:
        return &draft_.controls.moveRight;
    case CaptureTarget::Launch:
        return &draft_.controls.launch;
    case CaptureTarget::CallBall:
        return &draft_.controls.callBall;
    case CaptureTarget::Turbo:
        return &draft_.controls.turbo;
    case CaptureTarget::TurboBall:
        return &draft_.controls.turboBall;
    case CaptureTarget::Plasma:
        return &draft_.controls.plasma;
    case CaptureTarget::Pause:
        return &draft_.controls.pause;
    case CaptureTarget::None:
        return nullptr;
    }
    return nullptr;
}

} // namespace arcadeblocks::ui
