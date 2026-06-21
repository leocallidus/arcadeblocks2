#include "ui/DebugBonusesView.hpp"
#include "ui/UiLayout.hpp"
#include "core/Log.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace arcadeblocks::ui {
namespace {

float clamp01(double value) {
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

bool activationPressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space);
}

} // namespace

DebugBonusesView::DebugBonusesView() {
    // Positive bonuses (18)
    positiveBonuses_ = {
        {"BONUS_SCORE", "Extra Score", "Дополнительные очки", true},
        {"BONUS_SCORE_200", "Score +200", "+200 очков", true},
        {"BONUS_SCORE_500", "Score +500", "+500 очков", true},
        {"BONUS_SCORE_10000", "Score +10000", "+10000 очков", true},
        {"ADD_FIVE_SECONDS", "Add 5 Seconds", "+5 секунд", true},
        {"CALL_BALL", "Call Ball", "Притяжение мяча", true},
        {"EXTRA_LIFE", "Extra Life", "Дополнительная жизнь", true},
        {"INCREASE_PADDLE", "Increase Paddle", "Увеличение ракетки", true},
        {"STICKY_PADDLE", "Sticky Paddle", "Липкая ракетка", true},
        {"SLOW_BALLS", "Slow Balls", "Замедление мячей", true},
        {"ENERGY_BALLS", "Energy Balls", "Энергетические мячи", true},
        {"BONUS_WALL", "Safety Wall", "Защитный барьер", true},
        {"BONUS_MAGNET", "Bonus Magnet", "Магнит бонусов", true},
        {"BONUS_BALL", "Extra Ball", "Дополнительный мяч", true},
        {"PLASMA_WEAPON", "Plasma Weapon", "Плазменная пушка", true},
        {"EXPLOSION_BALLS", "Explosive Balls", "Взрывные мячи", true},
        {"TRICKSTER", "Trickster", "Шулер", true},
        {"SCORE_RAIN", "Score Rain", "Дождь очков", true},
        {"LEVEL_PASS", "Level Pass", "Проход уровня", true},
        {"RAINBOW_BOUNTY", "Rainbow Bounty", "Радужная щедрость", true}
    };

    // Negative bonuses (11)
    negativeBonuses_ = {
        {"CHAOTIC_BALLS", "Chaotic Trajectory", "Хаотичные мячи", true},
        {"FROZEN_PADDLE", "Frozen Paddle", "Замороженная ракетка", true},
        {"DECREASE_PADDLE", "Decrease Paddle", "Уменьшение ракетки", true},
        {"FAST_BALLS", "Fast Balls", "Ускорение мячей", true},
        {"PENALTIES_MAGNET", "Penalty Magnet", "Магнит штрафов", true},
        {"WEAK_BALLS", "Weak Balls", "Слабые мячи", true},
        {"INVISIBLE_PADDLE", "Invisible Paddle", "Призрачная ракетка", true},
        {"DARKNESS", "Darkness", "Темнота", true},
        {"BAD_LUCK", "Bad Luck", "Невезуха", true},
        {"RESET", "Reset Bonuses", "Сброс бонусов", true},
        {"BLOOD_TITHE", "Blood Tithe", "Кровавая дань", true}
    };

    // Special bonuses (1)
    specialBonuses_ = {
        {"RANDOM_BONUS", "Random Bonus", "Случайный бонус", true}
    };
}

DebugBonusesRenderResult DebugBonusesView::render(
    const render::Texture* backgroundTexture,
    const localization::Localization& localization,
    settings::Language language,
    bool& open) {

    DebugBonusesRenderResult result;
    const double now = ImGui::GetTime();

    if (open != wasOpen_) {
        animationStartedAt_ = now;
        wasOpen_ = open;
        if (open) {
            selectedIndex_ = 3; // Default to 'Apply' button
        }
    }

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

    auto triggerSound = [&](UiSoundEffect sfx) {
        if (sfx == UiSoundEffect::None) return;
        if (result.soundEffect == UiSoundEffect::Select) {
            return;
        }
        if (sfx == UiSoundEffect::Select) {
            result.soundEffect = sfx;
            return;
        }
        result.soundEffect = sfx;
    };

    // Keyboard navigation for bottom buttons
    if (open) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            selectedIndex_ = selectedIndex_ > 0 ? selectedIndex_ - 1 : 4;
            triggerSound(UiSoundEffect::Hover);
        } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            selectedIndex_ = selectedIndex_ < 4 ? selectedIndex_ + 1 : 0;
            triggerSound(UiSoundEffect::Hover);
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            open = false;
            triggerSound(UiSoundEffect::Back);
            result.action = DebugBonusesAction::Back;
        }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});
    
    ImGui::Begin(
        "DebugBonusesFullscreen",
        nullptr,
        ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings);

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
        drawList->AddRectFilled(
            origin,
            ImVec2{origin.x + viewport->Size.x, origin.y + viewport->Size.y},
            ImGui::GetColorU32(ImVec4{0.01f, 0.02f, 0.08f, fadeAlpha}));
    }

    // Add a dark overlay
    drawList->AddRectFilled(
        origin,
        ImVec2{origin.x + viewport->Size.x, origin.y + viewport->Size.y},
        ImGui::GetColorU32(ImVec4{0.02f, 0.03f, 0.1f, 0.75f * fadeAlpha}));

    const float scale = UiLayout::viewportScale();
    const float centerX = viewport->Size.x * 0.5f;
    const float centerY = viewport->Size.y * 0.5f;

    auto getLocText = [&](const char* key, const char* fallback) -> std::string {
        std::string t = localization.text(language, key);
        return t.empty() ? fallback : t;
    };

    // Title
    const float fontSize = 54.0f * scale;
    const float titleY = centerY - 440.0f * scale;
    const std::string titleText = getLocText("debug.bonuses.title", "BONUS MANAGEMENT");
    const char* title = titleText.c_str();
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    
    // Title shadow
    drawList->AddText(nullptr, fontSize, ImVec2{origin.x + centerX - titleSize.x * 0.5f + 4.0f, origin.y + titleY + 4.0f},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.8f * fadeAlpha}), title);
    // Title
    drawList->AddText(nullptr, fontSize, ImVec2{origin.x + centerX - titleSize.x * 0.5f, origin.y + titleY},
        ImGui::GetColorU32(ImVec4{0.46f, 0.86f, 0.98f, fadeAlpha}), title);

    // Subtitle
    const float subFontSize = 18.0f * scale;
    const float subY = titleY + 70.0f * scale;
    const std::string subText = getLocText("debug.bonuses.subtitle", "Enable or disable bonuses to test gameplay weight mechanics");
    const char* subtitle = subText.c_str();
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    drawList->AddText(nullptr, subFontSize, ImVec2{origin.x + centerX - subSize.x * 0.5f, origin.y + subY},
        ImGui::GetColorU32(ImVec4{0.75f, 0.8f, 0.9f, 0.9f * fadeAlpha}), subtitle);

    // 2. Render Panels
    const float panelHeight = 630.0f;
    const float panelSpacing = 30.0f;

    const float positiveWidth = 560.0f;
    const float negativeWidth = 560.0f;
    const float specialWidth = 320.0f;

    const float totalPanelsWidth = positiveWidth + negativeWidth + specialWidth + 2.0f * panelSpacing;
    const float panelsStartX = centerX - totalPanelsWidth * scale * 0.5f;
    const float panelsStartY = centerY - 320.0f * scale;

    auto drawPanel = [&](float panelStartX, float width, const char* heading, ImVec4 accentColor) {
        ImVec2 pMin{origin.x + panelStartX, origin.y + panelsStartY};
        ImVec2 pMax{pMin.x + width * scale, pMin.y + panelHeight * scale};

        // Panel background
        drawList->AddRectFilled(pMin, pMax, ImGui::GetColorU32(ImVec4{0.04f, 0.05f, 0.15f, 0.85f * fadeAlpha}), 12.0f * scale);
        // Neon border
        drawList->AddRect(pMin, pMax, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, fadeAlpha}), 12.0f * scale, 0, 2.0f * scale);

        // Header text inside panel
        const float headerFontSize = 22.0f * scale;
        ImVec2 headSize = ImGui::CalcTextSize(heading);
        drawList->AddText(nullptr, headerFontSize, ImVec2{pMin.x + (width * scale - headSize.x) * 0.5f, pMin.y + 16.0f * scale},
            ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, fadeAlpha}), heading);

        return std::pair<ImVec2, ImVec2>{pMin, pMax};
    };

    auto drawCheckboxes = [&](const std::pair<ImVec2, ImVec2>& bounds, std::vector<BonusInfo>& list, ImVec4 accentColor, const std::string& prefix) {
        float startX = bounds.first.x + 16.0f * scale;
        float startY = bounds.first.y + 64.0f * scale;
        float itemSpacingY = 29.0f * scale;
        float columnSpacingX = 265.0f * scale;

        for (size_t i = 0; i < list.size(); ++i) {
            auto& bonus = list[i];
            float xOffset = 0.0f;
            float yOffset = static_cast<float>(i) * itemSpacingY;
            if (list.size() > 10) {
                size_t col = i / 10;
                size_t row = i % 10;
                xOffset = static_cast<float>(col) * columnSpacingX;
                yOffset = static_cast<float>(row) * itemSpacingY;
            }
            ImGui::SetCursorPos(ImVec2{startX + xOffset - origin.x, startY + yOffset - origin.y});

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{0.06f, 0.08f, 0.2f, fadeAlpha});
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{0.1f, 0.15f, 0.35f, fadeAlpha});
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{0.2f, 0.3f, 0.6f, fadeAlpha});
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4{accentColor.x, accentColor.y, accentColor.z, fadeAlpha});
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * scale);

            std::string label = (language == settings::Language::Russian) ? bonus.nameRu : bonus.nameEn;
            std::string checkboxId = label + "##" + prefix + std::to_string(i);

            ImGui::Checkbox(checkboxId.c_str(), &bonus.enabled);

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
    };

    // Draw Positive (Green)
    std::string posHeader = getLocText("debug.bonuses.positive", "🟢 POSITIVE / ПОЛЕЗНЫЕ");
    auto posBounds = drawPanel(panelsStartX, positiveWidth, posHeader.c_str(), ImVec4{0.0f, 1.0f, 0.5f, 1.0f});
    drawCheckboxes(posBounds, positiveBonuses_, ImVec4{0.0f, 1.0f, 0.5f, 1.0f}, "pos");

    // Draw Negative (Red)
    std::string negHeader = getLocText("debug.bonuses.negative", "🔴 NEGATIVE / ШТРАФЫ");
    auto negBounds = drawPanel(panelsStartX + (positiveWidth + panelSpacing) * scale, negativeWidth, negHeader.c_str(), ImVec4{1.0f, 0.3f, 0.3f, 1.0f});
    drawCheckboxes(negBounds, negativeBonuses_, ImVec4{1.0f, 0.3f, 0.3f, 1.0f}, "neg");

    // Draw Special (Yellow)
    std::string specHeader = getLocText("debug.bonuses.special", "🟡 SPECIAL / ОСОБЫЕ");
    auto specBounds = drawPanel(panelsStartX + (positiveWidth + negativeWidth + 2.0f * panelSpacing) * scale, specialWidth, specHeader.c_str(), ImVec4{1.0f, 0.85f, 0.0f, 1.0f});
    drawCheckboxes(specBounds, specialBonuses_, ImVec4{1.0f, 0.85f, 0.0f, 1.0f}, "spec");

    // 3. Action Buttons
    const float buttonWidth = 230.0f;
    const float buttonHeight = 52.0f;
    const float buttonSpacing = 35.0f;
    const ImVec2 buttonSize = UiLayout::logicalSize(buttonWidth, buttonHeight);
    const float totalButtonsWidth = 5.0f * buttonSize.x + 4.0f * buttonSpacing * scale;
    const float buttonStartX = centerX - totalButtonsWidth * 0.5f;
    const float buttonStartY = centerY + 340.0f * scale;

    struct ActionBtn {
        const char* id;
        std::string label;
        UiAccent accent;
    };

    std::vector<ActionBtn> buttons = {
        {"btn-back", getLocText("debug.bonuses.back", "Back"), UiAccent::Purple},
        {"btn-enable-all", getLocText("debug.bonuses.enable_all", "Enable All"), UiAccent::Green},
        {"btn-disable-all", getLocText("debug.bonuses.disable_all", "Disable All"), UiAccent::Orange},
        {"btn-apply", getLocText("debug.bonuses.apply", "Apply"), UiAccent::Cyan},
        {"btn-close", getLocText("debug.bonuses.close", "Close"), UiAccent::Red}
    };

    for (int index = 0; index < 5; ++index) {
        const auto& btn = buttons[static_cast<std::size_t>(index)];
        ImGui::SetCursorPos(ImVec2{buttonStartX + static_cast<float>(index) * (buttonSize.x + buttonSpacing * scale), buttonStartY});
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeAlpha);

        const auto renderBtn = UiTheme::renderNeonButton(
            btn.id,
            btn.label.c_str(),
            btn.accent,
            buttonSize,
            open && selectedIndex_ == index
        );

        ImGui::PopStyleVar();

        if (open) {
            // Mouse hover updates selection
            if (renderBtn.hovered && selectedIndex_ != index) {
                selectedIndex_ = index;
                triggerSound(UiSoundEffect::Hover);
            }

            const bool activate = renderBtn.pressed || (selectedIndex_ == index && activationPressed());
            if (activate) {
                triggerSound(UiSoundEffect::Select);

                switch (index) {
                case 0: // Back
                    open = false;
                    result.action = DebugBonusesAction::Back;
                    break;
                case 1: // Enable All
                    for (auto& b : positiveBonuses_) b.enabled = true;
                    for (auto& b : negativeBonuses_) b.enabled = true;
                    for (auto& b : specialBonuses_) b.enabled = true;
                    break;
                case 2: // Disable All
                    for (auto& b : positiveBonuses_) b.enabled = false;
                    for (auto& b : negativeBonuses_) b.enabled = false;
                    for (auto& b : specialBonuses_) b.enabled = false;
                    break;
                case 3: // Apply
                    // Log current debug selections (simulating applying settings)
                    core::Log::info("Applied debug bonus settings:");
                    for (const auto& b : positiveBonuses_) {
                        core::Log::info("  " + b.id + ": " + (b.enabled ? "Enabled" : "Disabled"));
                    }
                    for (const auto& b : negativeBonuses_) {
                        core::Log::info("  " + b.id + ": " + (b.enabled ? "Enabled" : "Disabled"));
                    }
                    for (const auto& b : specialBonuses_) {
                        core::Log::info("  " + b.id + ": " + (b.enabled ? "Enabled" : "Disabled"));
                    }
                    break;
                case 4: // Close
                    open = false;
                    result.action = DebugBonusesAction::Close;
                    break;
                }
            }
        }
    }

    ImGui::End();
    return result;
}

std::vector<std::string> DebugBonusesView::getEnabledBonuses() const {
    std::vector<std::string> enabled;
    for (const auto& b : positiveBonuses_) {
        if (b.enabled) enabled.push_back(b.id);
    }
    for (const auto& b : negativeBonuses_) {
        if (b.enabled) enabled.push_back(b.id);
    }
    for (const auto& b : specialBonuses_) {
        if (b.enabled) enabled.push_back(b.id);
    }
    return enabled;
}

} // namespace arcadeblocks::ui
