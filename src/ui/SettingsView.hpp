#pragma once

#include "localization/Localization.hpp"
#include "settings/Settings.hpp"
#include "ui/UiTheme.hpp"

#include <SDL3/SDL_keycode.h>

#include <optional>
#include <string>

namespace arcadeblocks::ui {

enum class SettingsContext {
    MainMenu,
    Pause
};

enum class SettingsTab {
    Audio,
    Video,
    Controls,
    Gameplay,
    Language
};

enum class SettingsAction {
    None,
    Apply,
    Close,
    Discard,
    TestSound
};

struct SettingsRenderResult {
    SettingsAction action = SettingsAction::None;
    UiSoundEffect soundEffect = UiSoundEffect::None;
    std::optional<settings::GameSettings> settings;
    bool audioPreviewChanged = false;
};

class SettingsView {
public:
    SettingsRenderResult render(const localization::Localization& localization, const settings::GameSettings& appliedSettings, SettingsContext context);
    bool handleKeyCapture(SDL_Keycode keycode);
    void resetSession();
    void setStatus(std::string status);

private:
    enum class CaptureTarget {
        None,
        MoveLeft,
        MoveRight,
        Launch,
        CallBall,
        Turbo,
        TurboBall,
        Plasma,
        Pause
    };

    void beginSession(const settings::GameSettings& appliedSettings);
    [[nodiscard]] std::string text(std::string_view key) const;
    [[nodiscard]] bool dirty() const;
    [[nodiscard]] SettingsRenderResult applyAndClose(bool close);
    [[nodiscard]] SettingsRenderResult discardAndClose();
    [[nodiscard]] std::optional<std::string> validateDraft() const;
    void updateHoverSound(SettingsRenderResult& result, int currentHoveredWidget);
    void renderAudio(SettingsRenderResult& result);
    void renderVideo(SettingsRenderResult& result);
    void renderControls(SettingsRenderResult& result);
    void renderGameplay(SettingsRenderResult& result);
    void renderLanguage(SettingsRenderResult& result);
    void renderUnsavedDialog(SettingsRenderResult& result, int& currentHoveredWidget);
    void renderKeyCaptureButton(const char* label, CaptureTarget target, settings::KeyBinding& binding);
    [[nodiscard]] settings::KeyBinding* bindingFor(CaptureTarget target);

    settings::GameSettings baseline_ = settings::defaultSettings();
    settings::GameSettings draft_ = settings::defaultSettings();
    std::string status_;
    CaptureTarget captureTarget_ = CaptureTarget::None;
    bool sessionActive_ = false;
    bool unsavedDialogOpen_ = false;
    bool closingAnimation_ = false;
    int hoveredWidget_ = -1;
    SettingsContext context_ = SettingsContext::MainMenu;
    SettingsTab currentTab_ = SettingsTab::Audio;
    const localization::Localization* localization_ = nullptr;
};

} // namespace arcadeblocks::ui
