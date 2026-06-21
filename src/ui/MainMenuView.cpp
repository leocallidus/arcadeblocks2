#include "ui/MainMenuView.hpp"

#include "core/Version.hpp"
#include "ui/UiLayout.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace arcadeblocks::ui {
namespace {

constexpr int exitConfirmItemCount = 2;

struct MenuEntry {
    const char* id;
    const char* labelKey;
    UiAccent accent;
    const char* descriptionKey;
};

constexpr std::array<MenuEntry, 4> menuEntries{{
    {"play", "menu.play.label", UiAccent::Pink, "menu.play.description"},
    {"settings", "menu.settings.label", UiAccent::Purple, "menu.settings.description"},
    {"help", "menu.help.label", UiAccent::Green, "menu.help.description"},
    {"exit", "menu.exit.label", UiAccent::Yellow, "menu.exit.description"},
}};

std::string t(const localization::Localization& localization, settings::Language language, std::string_view key) {
    return localization.text(language, key);
}

std::string labelFor(const localization::Localization& localization, const MenuEntry& entry, settings::Language language) {
    return t(localization, language, entry.labelKey);
}

std::string descriptionFor(const localization::Localization& localization, const MenuEntry& entry, settings::Language language) {
    return t(localization, language, entry.descriptionKey);
}

float clamp01(double value) {
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

ImVec2 scaled(float x, float y) {
    return UiLayout::logicalSize(x, y);
}

ImVec2 toScreen(ImVec2 origin, ImVec2 local) {
    return ImVec2{origin.x + local.x, origin.y + local.y};
}

void drawPanel(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha) {
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4{0.04f, 0.05f, 0.12f, 0.68f * alpha}), 12.0f);
    drawList->AddRectFilled(
        ImVec2{min.x + 14.0f, min.y + 14.0f},
        ImVec2{max.x - 14.0f, max.y - 14.0f},
        ImGui::GetColorU32(ImVec4{0.02f, 0.03f, 0.08f, 0.72f * alpha}),
        10.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(ImVec4{0.44f, 0.84f, 0.98f, 0.78f * alpha}), 12.0f, 0, 2.0f);
}

void drawButtonGlow(float alpha, UiAccent accent) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImVec4 accentColor = UiTheme::accent(accent);

    drawList->AddRect(
        ImVec2{min.x - 4.0f, min.y - 4.0f},
        ImVec2{max.x + 4.0f, max.y + 4.0f},
        ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 0.24f * alpha}),
        10.0f,
        0,
        4.0f);
    drawList->AddRect(
        ImVec2{min.x - 10.0f, min.y - 10.0f},
        ImVec2{max.x + 10.0f, max.y + 10.0f},
        ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 0.1f * alpha}),
        14.0f,
        0,
        8.0f);
}

void drawHintText(ImVec2 position, ImVec4 color, std::string_view text) {
    ImGui::SetCursorPos(position);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopStyleColor();
}

bool activationPressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space);
}

} // namespace

MainMenuRenderResult MainMenuView::render(
    const MainMenuSceneAssets& assets,
    const localization::Localization& localization,
    settings::Language language,
    bool& settingsOpen,
    bool& helpOpen,
    bool& exitConfirmOpen) {
    MainMenuRenderResult result;
    bool exitConfirmOpenedThisFrame = false;
    const double now = ImGui::GetTime();
    if (lastRenderTime_ < 0.0 || (now - lastRenderTime_) > 0.25) {
        animationStartedAt_ = now;
    }
    lastRenderTime_ = now;

    selectedIndex_ = std::clamp(selectedIndex_, 0, static_cast<int>(menuEntries.size()) - 1);
    exitConfirmSelectedIndex_ = std::clamp(exitConfirmSelectedIndex_, 0, exitConfirmItemCount - 1);

    const bool menuBlocked = settingsOpen || helpOpen || exitConfirmOpen;
    if (exitConfirmOpen) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            exitConfirmSelectedIndex_ = exitConfirmSelectedIndex_ > 0 ? exitConfirmSelectedIndex_ - 1 : exitConfirmItemCount - 1;
            result.soundEffect = UiSoundEffect::Hover;
        } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            exitConfirmSelectedIndex_ = exitConfirmSelectedIndex_ < exitConfirmItemCount - 1 ? exitConfirmSelectedIndex_ + 1 : 0;
            result.soundEffect = UiSoundEffect::Hover;
        }
    } else if (!helpOpen) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            selectedIndex_ = selectedIndex_ > 0 ? selectedIndex_ - 1 : static_cast<int>(menuEntries.size()) - 1;
            result.soundEffect = UiSoundEffect::Hover;
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            selectedIndex_ = selectedIndex_ < static_cast<int>(menuEntries.size()) - 1 ? selectedIndex_ + 1 : 0;
            result.soundEffect = UiSoundEffect::Hover;
        }
    }

    const float fadeAlpha = clamp01((now - animationStartedAt_) / 0.55);
    const float scale = UiLayout::viewportScale();
    const float pulse = 0.55f + 0.45f * std::sin(static_cast<float>(now * 4.8));

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});
    ImGui::Begin(
        "MainMenuFullscreen",
        nullptr,
        ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float centerX = viewport->Size.x * 0.5f;
    const float centerY = viewport->Size.y * 0.5f;

    // Draw background texture if available, otherwise dark fill
    if (assets.background != nullptr && (*assets.background) && assets.background->native() != nullptr) {
        const float bgWidth = static_cast<float>(assets.background->width());
        const float bgHeight = static_cast<float>(assets.background->height());
        const float bgScaleX = viewport->Size.x / bgWidth;
        const float bgScaleY = viewport->Size.y / bgHeight;
        const float bgScaleVal = std::max(bgScaleX, bgScaleY);
        const float drawW = bgWidth * bgScaleVal;
        const float drawH = bgHeight * bgScaleVal;
        const float offsetX = (viewport->Size.x - drawW) * 0.5f;
        const float offsetY = (viewport->Size.y - drawH) * 0.5f;
        drawList->AddImage(
            reinterpret_cast<ImTextureID>(assets.background->native()),
            ImVec2{origin.x + offsetX, origin.y + offsetY},
            ImVec2{origin.x + offsetX + drawW, origin.y + offsetY + drawH});
    } else {
        drawList->AddRectFilled(
            origin,
            ImVec2{origin.x + viewport->Size.x, origin.y + viewport->Size.y},
            ImGui::GetColorU32(ImVec4{0.01f, 0.02f, 0.08f, 0.40f + 0.18f * fadeAlpha}));
    }

    // Dark overlay for readability
    drawList->AddRectFilled(
        origin,
        ImVec2{origin.x + viewport->Size.x, origin.y + viewport->Size.y},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.35f}));

    // Decorative circles
    drawList->AddCircleFilled(
        toScreen(origin, ImVec2{centerX + 450.0f * scale, centerY - 150.0f * scale}),
        228.0f * scale,
        ImGui::GetColorU32(ImVec4{0.18f, 0.56f, 0.94f, 0.07f * fadeAlpha}));
    drawList->AddCircleFilled(
        toScreen(origin, ImVec2{centerX + 580.0f * scale, centerY + 220.0f * scale}),
        168.0f * scale,
        ImGui::GetColorU32(ImVec4{0.99f, 0.22f, 0.76f, 0.08f * fadeAlpha}));

    // Static neon title — "ARCADE" (pink) + "BLOCKS II" (cyan)
    {
        const float fontSize = 76.0f * scale;
        const float titleY = centerY - 380.0f * scale;

        const ImVec4 arcadeColor{0.96f, 0.28f, 0.82f, fadeAlpha};
        const ImVec4 blocksColor{0.42f, 0.86f, 1.0f, fadeAlpha};

        drawList->AddText(nullptr, fontSize, toScreen(origin, ImVec2{centerX - 280.0f * scale, titleY}),
            ImGui::GetColorU32(arcadeColor), "ARCADE");
        drawList->AddText(nullptr, fontSize, toScreen(origin, ImVec2{centerX + 20.0f * scale, titleY}),
            ImGui::GetColorU32(blocksColor), "BLOCKS II");
    }

    // Centered button panel
    const float panelHalfWidth = 330.0f * scale;
    const float panelHalfHeight = 270.0f * scale;
    const ImVec2 panelMin{centerX - panelHalfWidth, centerY - panelHalfHeight};
    const ImVec2 panelMax{centerX + panelHalfWidth, centerY + panelHalfHeight};
    drawPanel(drawList, toScreen(origin, panelMin), toScreen(origin, panelMax), fadeAlpha);

    // Title label above buttons
    const float labelX = centerX - panelHalfWidth + 80.0f * scale;
    const float labelY = panelMin.y + 18.0f * scale;
    const auto campaignLabel = t(localization, language, "menu.campaign");
    drawHintText(
        ImVec2{labelX, labelY},
        ImVec4{0.46f, 0.86f, 0.98f, fadeAlpha},
        campaignLabel);

    // Version text
    const std::string versionLine = t(localization, language, "menu.version_prefix") + " " + std::string(core::version()) + "  SDL " + std::string(core::sdlTargetVersion());
    drawHintText(
        ImVec2{labelX, labelY + 34.0f * scale},
        ImVec4{0.68f, 0.74f, 0.86f, 0.78f * fadeAlpha},
        versionLine);

    // Menu buttons centered horizontally within panel
    const float buttonWidth = 468.0f;
    const float buttonHeight = 68.0f;
    const float buttonSpacing = 88.0f;
    const ImVec2 buttonSize = scaled(buttonWidth, buttonHeight);
    const float buttonStartX = centerX - buttonSize.x * 0.5f;
    const float buttonStartY = panelMin.y + 90.0f * scale;

    for (int index = 0; index < static_cast<int>(menuEntries.size()); ++index) {
        const auto& entry = menuEntries[static_cast<std::size_t>(index)];
        const auto label = labelFor(localization, entry, language);
        ImGui::SetCursorPos(ImVec2{buttonStartX, buttonStartY + buttonSpacing * scale * static_cast<float>(index)});
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeAlpha);
        const auto button = UiTheme::renderNeonButton(entry.id, label.c_str(), entry.accent, buttonSize, !menuBlocked && selectedIndex_ == index);
        ImGui::PopStyleVar();

        if (!menuBlocked && button.hovered && selectedIndex_ != index) {
            selectedIndex_ = index;
            result.soundEffect = UiSoundEffect::Hover;
        }

        if (!menuBlocked && (button.hovered || selectedIndex_ == index)) {
            drawButtonGlow(0.65f + 0.35f * pulse, entry.accent);
        }

        if (menuBlocked) {
            continue;
        }

        const bool activate = button.pressed || (selectedIndex_ == index && activationPressed());
        if (!activate) {
            continue;
        }

        switch (index) {
        case 0:
            result.action = MainMenuAction::StartLevel1;
            result.soundEffect = UiSoundEffect::Select;
            break;
        case 1:
            settingsOpen = true;
            result.soundEffect = UiSoundEffect::Select;
            break;
        case 2:
            helpOpen = true;
            result.soundEffect = UiSoundEffect::Select;
            break;
        case 3:
            exitConfirmOpen = true;
            exitConfirmSelectedIndex_ = 0;
            exitConfirmOpenedThisFrame = true;
            result.soundEffect = UiSoundEffect::Select;
            break;
        }
    }

    // Description panel at bottom of button panel
    const float descY = panelMax.y - 82.0f * scale;
    drawList->AddRectFilled(
        toScreen(origin, ImVec2{panelMin.x + 30.0f * scale, descY}),
        toScreen(origin, ImVec2{panelMax.x - 30.0f * scale, descY + 62.0f * scale}),
        ImGui::GetColorU32(ImVec4{0.06f, 0.08f, 0.16f, 0.84f * fadeAlpha}),
        10.0f);
    drawHintText(
        ImVec2{panelMin.x + 48.0f * scale, descY + 8.0f * scale},
        ImVec4{0.98f, 0.34f, 0.86f, fadeAlpha},
        labelFor(localization, menuEntries[static_cast<std::size_t>(selectedIndex_)], language));
    drawHintText(
        ImVec2{panelMin.x + 48.0f * scale, descY + 32.0f * scale},
        ImVec4{0.84f, 0.87f, 0.96f, 0.92f * fadeAlpha},
        descriptionFor(localization, menuEntries[static_cast<std::size_t>(selectedIndex_)], language));

    // Hint at bottom
    const auto inputHint = t(localization, language, "menu.input_hint");
    drawHintText(
        ImVec2{centerX - 210.0f * scale, centerY + 380.0f * scale},
        ImVec4{0.70f, 0.76f, 0.86f, 0.78f * fadeAlpha},
        inputHint);
    const auto footerHint = t(localization, language, "menu.footer_hint");
    drawHintText(
        ImVec2{centerX - 210.0f * scale, centerY + 412.0f * scale},
        ImVec4{0.64f, 0.70f, 0.82f, 0.70f * fadeAlpha},
        footerHint);

    ImGui::End();

    if (exitConfirmOpen) {
        UiTheme::renderModalOverlay(0.96f);
        UiLayout::setNextCenteredWindow(560.0f, 220.0f);
        ImGui::SetNextWindowBgAlpha(0.97f);
        const auto exitWindowTitle = t(localization, language, "menu.exit.window_title") + "###ExitConfirmation";
        ImGui::Begin(
            exitWindowTitle.c_str(),
            nullptr,
            ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoSavedSettings);

        const auto exitTitle = t(localization, language, "menu.exit.title");
        UiTheme::renderCyberpunkPanelTitle(exitTitle.c_str(), UiAccent::Yellow);
        const auto exitPrompt = t(localization, language, "menu.exit.prompt");
        ImGui::TextWrapped("%s", exitPrompt.c_str());
        ImGui::Dummy(ImVec2{0.0f, UiTheme::sectionSpacing()});

        const float width = (ImGui::GetContentRegionAvail().x - UiTheme::itemSpacing()) * 0.5f;
        const ImVec2 confirmSize{width, UiTheme::buttonHeight()};
        const auto cancelLabel = t(localization, language, "common.cancel");
        const auto cancelButton = UiTheme::renderNeonButton(
            "exit-cancel",
            cancelLabel.c_str(),
            UiAccent::Cyan,
            confirmSize,
            exitConfirmSelectedIndex_ == 0);
        if (cancelButton.hovered && exitConfirmSelectedIndex_ != 0) {
            exitConfirmSelectedIndex_ = 0;
            result.soundEffect = UiSoundEffect::Hover;
        }

        ImGui::SameLine(0.0f, UiTheme::itemSpacing());

        const auto exitLabel = t(localization, language, "common.exit");
        const auto exitButton = UiTheme::renderNeonButton(
            "exit-confirm",
            exitLabel.c_str(),
            UiAccent::Red,
            confirmSize,
            exitConfirmSelectedIndex_ == 1);
        if (exitButton.hovered && exitConfirmSelectedIndex_ != 1) {
            exitConfirmSelectedIndex_ = 1;
            result.soundEffect = UiSoundEffect::Hover;
        }

        if (cancelButton.pressed
            || (!exitConfirmOpenedThisFrame && exitConfirmSelectedIndex_ == 0 && activationPressed())
            || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            exitConfirmOpen = false;
            result.soundEffect = UiSoundEffect::Back;
        } else if (exitButton.pressed || (!exitConfirmOpenedThisFrame && exitConfirmSelectedIndex_ == 1 && activationPressed())) {
            exitConfirmOpen = false;
            result.action = MainMenuAction::Exit;
            result.soundEffect = UiSoundEffect::Select;
        }

        ImGui::End();
    }

    return result;
}

} // namespace arcadeblocks::ui
