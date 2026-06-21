#include "ui/DebugMenuView.hpp"

#include "ui/UiLayout.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace arcadeblocks::ui {
namespace {

struct MenuEntry {
    const char* id;
    const char* label;
    UiAccent accent;
};

constexpr std::array<MenuEntry, 4> menuEntries{{
    {"bonuses", "Bonuses", UiAccent::Pink},
    {"levels", "Levels", UiAccent::Cyan},
    {"poem", "Poem", UiAccent::Purple},
    {"credits", "Credits", UiAccent::Green},
}};

float clamp01(double value) {
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

bool activationPressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space);
}

} // namespace

DebugMenuRenderResult DebugMenuView::render(
    const render::Texture* backgroundTexture,
    const localization::Localization& localization,
    settings::Language language,
    bool& open) {

    DebugMenuRenderResult result;
    const double now = ImGui::GetTime();

    if (open != wasOpen_) {
        // State changed
        animationStartedAt_ = now;
        wasOpen_ = open;
    }

    // Handle alpha for smooth opening and closing.
    // Let's assume a 0.25s animation duration.
    double elapsed = now - animationStartedAt_;
    float fadeAlpha = 0.0f;
    if (open) {
        fadeAlpha = clamp01(elapsed / 0.25);
    } else {
        fadeAlpha = 1.0f - clamp01(elapsed / 0.25);
    }

    if (fadeAlpha <= 0.0f && !open) {
        result.isVisible = false;
        return result; // Fully closed
    }
    result.isVisible = true;

    // Keyboard navigation
    if (open) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            selectedIndex_ = selectedIndex_ > 0 ? selectedIndex_ - 1 : static_cast<int>(menuEntries.size()) - 1;
            result.soundEffect = UiSoundEffect::Hover;
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            selectedIndex_ = selectedIndex_ < static_cast<int>(menuEntries.size()) - 1 ? selectedIndex_ + 1 : 0;
            result.soundEffect = UiSoundEffect::Hover;
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            open = false;
            result.soundEffect = UiSoundEffect::Back;
            result.action = DebugMenuAction::Close;
        }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});
    
    ImGui::Begin(
        "DebugMenuFullscreen",
        nullptr,
        ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings);

    // Keep debug menu window focused every frame while open, so it renders
    // on top of MainMenuFullscreen (which has NoBringToFrontOnFocus).
    if (open) {
        ImGui::SetWindowFocus();
    }
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();

    // 1. Draw background image
    if (backgroundTexture != nullptr && (*backgroundTexture) && backgroundTexture->native() != nullptr) {
        const float bgWidth = static_cast<float>(backgroundTexture->width());
        const float bgHeight = static_cast<float>(backgroundTexture->height());
        const float bgScaleX = viewport->Size.x / bgWidth;
        const float bgScaleY = viewport->Size.y / bgHeight;
        const float bgScaleVal = std::max(bgScaleX, bgScaleY);
        const float drawW = bgWidth * bgScaleVal;
        const float drawH = bgHeight * bgScaleVal;
        const float offsetX = (viewport->Size.x - drawW) * 0.5f;
        const float offsetY = (viewport->Size.y - drawH) * 0.5f;

        drawList->AddImage(
            reinterpret_cast<ImTextureID>(backgroundTexture->native()),
            ImVec2{origin.x + offsetX, origin.y + offsetY},
            ImVec2{origin.x + offsetX + drawW, origin.y + offsetY + drawH},
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::GetColorU32(ImVec4{1.0f, 1.0f, 1.0f, fadeAlpha})
        );
    } else {
        // Fallback dark background
        drawList->AddRectFilled(
            origin,
            ImVec2{origin.x + viewport->Size.x, origin.y + viewport->Size.y},
            ImGui::GetColorU32(ImVec4{0.01f, 0.02f, 0.08f, fadeAlpha}));
    }

    // Add a dark overlay for readability
    drawList->AddRectFilled(
        origin,
        ImVec2{origin.x + viewport->Size.x, origin.y + viewport->Size.y},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.5f * fadeAlpha}));

    const float scale = UiLayout::viewportScale();
    const float centerX = viewport->Size.x * 0.5f;
    const float centerY = viewport->Size.y * 0.5f;

    // Title
    const float fontSize = 64.0f * scale;
    const float titleY = centerY - 300.0f * scale;
    const std::string titleText = localization.text(language, "debug.menu.title");
    const char* title = titleText.empty() ? "DEBUG MENU" : titleText.c_str();
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    
    // Draw title shadow
    drawList->AddText(nullptr, fontSize, ImVec2{origin.x + centerX - titleSize.x * 0.5f + 4.0f, origin.y + titleY + 4.0f},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.8f * fadeAlpha}), title);
    // Draw title
    drawList->AddText(nullptr, fontSize, ImVec2{origin.x + centerX - titleSize.x * 0.5f, origin.y + titleY},
        ImGui::GetColorU32(ImVec4{0.46f, 0.86f, 0.98f, fadeAlpha}), title);

    // Centered colored helper text
    const float hintFontSize = 20.0f * scale;
    const float hintY = titleY + 100.0f * scale;
    const std::string hintText = localization.text(language, "debug.menu.hint");
    const char* hint = hintText.empty() 
        ? "Welcome to the debug menu! Select an option to continue ESC - close menu | Arrows - navigate | Enter/Space Select" 
        : hintText.c_str();
    ImVec2 hintSize = ImGui::CalcTextSize(hint);

    // Draw hint shadow
    drawList->AddText(nullptr, hintFontSize, ImVec2{origin.x + centerX - hintSize.x * 0.5f + 2.0f, origin.y + hintY + 2.0f},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.8f * fadeAlpha}), hint);
    // Draw hint
    drawList->AddText(nullptr, hintFontSize, ImVec2{origin.x + centerX - hintSize.x * 0.5f, origin.y + hintY},
        ImGui::GetColorU32(ImVec4{1.0f, 0.73f, 0.2f, fadeAlpha}), hint);

    // Buttons
    const float buttonWidth = 400.0f;
    const float buttonHeight = 60.0f;
    const float buttonSpacing = 80.0f;
    const ImVec2 buttonSize = UiLayout::logicalSize(buttonWidth, buttonHeight);
    const float buttonStartX = centerX - buttonSize.x * 0.5f;
    const float buttonStartY = centerY - 60.0f * scale;

    for (int index = 0; index < static_cast<int>(menuEntries.size()); ++index) {
        const auto& entry = menuEntries[static_cast<std::size_t>(index)];
        
        ImGui::SetCursorPos(ImVec2{buttonStartX, buttonStartY + buttonSpacing * scale * static_cast<float>(index)});
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeAlpha);
        
        const std::string localizedLabel = localization.text(language, std::string("debug.menu.") + entry.id);
        const char* label = localizedLabel.empty() ? entry.label : localizedLabel.c_str();

        const auto button = UiTheme::renderNeonButton(
            entry.id, 
            label, 
            entry.accent, 
            buttonSize, 
            open && selectedIndex_ == index);
            
        ImGui::PopStyleVar();

        if (open && button.hovered && selectedIndex_ != index) {
            selectedIndex_ = index;
            result.soundEffect = UiSoundEffect::Hover;
        }

        if (!open) {
            continue;
        }

        const bool activate = button.pressed || (selectedIndex_ == index && activationPressed());
        if (!activate) {
            continue;
        }

        result.soundEffect = UiSoundEffect::Select;
        
        switch (index) {
        case 0:
            // Bonuses
            result.action = DebugMenuAction::OpenBonuses;
            open = false; // Transition away
            break;
        case 1:
            // Levels
            result.action = DebugMenuAction::OpenLevels;
            open = false; // Transition away
            break;
        case 2:
            // Poem - no action for now
            break;
        case 3:
            // Credits - no action for now
            break;
        }
    }

    ImGui::End();

    return result;
}

} // namespace arcadeblocks::ui
