#include "ui/LevelCompleteView.hpp"
#include "ui/UiLayout.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace arcadeblocks::ui {
namespace {

constexpr float pi = 3.14159265358979323846f;

std::string formatTitle(std::string_view format, const std::string& name, int level) {
    std::string result{format};
    size_t pos0 = result.find("{0}");
    if (pos0 != std::string::npos) {
        result.replace(pos0, 3, name);
    }
    size_t pos1 = result.find("{1}");
    if (pos1 != std::string::npos) {
        result.replace(pos1, 3, std::to_string(level));
    }
    return result;
}

bool activationPressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space);
}

} // namespace

LevelCompleteRenderResult LevelCompleteView::render(
    const render::Texture* backgroundTexture,
    const localization::Localization& localization,
    settings::Language language,
    const LevelCompleteStats& stats,
    double activeDuration,
    bool& open) {

    LevelCompleteRenderResult result;

    auto triggerSound = [&](UiSoundEffect sfx) {
        if (sfx == UiSoundEffect::None) return;
        if (result.soundEffect == UiSoundEffect::Select) {
            return;
        }
        if (sfx == UiSoundEffect::Select) {
            result.soundEffect = sfx;
            return;
        }
        if (result.soundEffect == UiSoundEffect::StarGlow) {
            return;
        }
        result.soundEffect = sfx;
    };

    if (open != wasOpen_) {
        if (open) {
            lastPlayedStar_ = -1;
            selectedIndex_ = 1; // Default to 'Continue' button for better flow
        }
        wasOpen_ = open;
    }

    // Smooth window opening animation
    float fadeAlpha = std::clamp(static_cast<float>(activeDuration / 0.25), 0.0f, 1.0f);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});

    ImGui::Begin(
        "LevelCompleteFullscreen",
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
        // Fallback dark background
        drawList->AddRectFilled(
            origin,
            ImVec2{origin.x + viewport->Size.x, origin.y + viewport->Size.y},
            ImGui::GetColorU32(ImVec4{0.01f, 0.02f, 0.08f, fadeAlpha}));
    }

    // Semi-transparent overlay to increase contrast
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

    // 2. Localized Title
    const float fontSize = 48.0f * scale;
    const float titleY = centerY - 320.0f * scale;
    std::string rawTitleFormat = getLocText("debug.level_complete.title_format", "{0} Level {1} completed successfully!");
    const std::string titleText = formatTitle(rawTitleFormat, stats.playerName, stats.levelNumber);
    const char* title = titleText.c_str();
    ImVec2 titleSize = ImGui::CalcTextSize(title);

    // Title shadow
    drawList->AddText(nullptr, fontSize, ImVec2{origin.x + centerX - titleSize.x * 0.5f + 4.0f, origin.y + titleY + 4.0f},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.8f * fadeAlpha}), title);
    // Gold neon title text
    drawList->AddText(nullptr, fontSize, ImVec2{origin.x + centerX - titleSize.x * 0.5f, origin.y + titleY},
        ImGui::GetColorU32(ImVec4{1.0f, 0.84f, 0.0f, fadeAlpha}), title);

    // Keyboard navigation
    if (open) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            if (selectedIndex_ > 0) {
                selectedIndex_ = 0;
                triggerSound(UiSoundEffect::Hover);
            }
        } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            if (selectedIndex_ < 1) {
                selectedIndex_ = 1;
                triggerSound(UiSoundEffect::Hover);
            }
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            open = false;
            triggerSound(UiSoundEffect::Back);
            result.action = LevelCompleteAction::None;
        }
    }

    // 3. Mathematical Star Drawing
    int starsToLight = std::clamp(5 - stats.livesLost, 0, 5);
    int activeStars = 0;
    for (int i = 0; i < starsToLight; ++i) {
        if (activeDuration >= 1.0 + i * 0.5) {
            activeStars = i + 1;
        }
    }

    // Star glow sound triggers when a new star lights up
    if (activeStars > lastPlayedStar_) {
        if (lastPlayedStar_ >= 0) {
            triggerSound(UiSoundEffect::StarGlow);
        }
        lastPlayedStar_ = activeStars;
    }

    float outerRadius = 26.0f * scale;
    float innerRadius = outerRadius * 0.382f;
    float starSpacing = 24.0f * scale;
    float totalStarsWidth = 5.0f * (2.0f * outerRadius) + 4.0f * starSpacing;
    float starStartX = centerX - totalStarsWidth * 0.5f + outerRadius;
    float starY = centerY - 210.0f * scale;

    for (int i = 0; i < 5; ++i) {
        float starX = starStartX + i * (2.0f * outerRadius + starSpacing);
        ImVec2 starCenter{origin.x + starX, origin.y + starY};

        // Generates a 10-point star shape mathematically
        ImVec2 points[10];
        for (int j = 0; j < 10; ++j) {
            float angle = -pi / 2.0f + j * (pi / 5.0f);
            float r = (j % 2 == 0) ? outerRadius : innerRadius;
            points[j] = ImVec2{starCenter.x + r * std::cos(angle), starCenter.y + r * std::sin(angle)};
        }

        bool isLit = (i < activeStars);
        ImU32 colorFilled;
        ImU32 colorOutline;

        if (isLit) {
            colorFilled = ImGui::GetColorU32(ImVec4{1.0f, 0.85f, 0.1f, fadeAlpha});
            colorOutline = ImGui::GetColorU32(ImVec4{1.0f, 1.0f, 0.7f, fadeAlpha});
        } else {
            colorFilled = ImGui::GetColorU32(ImVec4{0.08f, 0.1f, 0.2f, 0.4f * fadeAlpha});
            colorOutline = ImGui::GetColorU32(ImVec4{0.3f, 0.35f, 0.5f, 0.6f * fadeAlpha});
        }

        // Draw filled background using central pentagon + 5 tip triangles
        ImVec2 pentagon[5] = { points[1], points[3], points[5], points[7], points[9] };
        drawList->AddConvexPolyFilled(pentagon, 5, colorFilled);
        for (int j = 0; j < 5; ++j) {
            drawList->AddTriangleFilled(points[2*j], points[(2*j + 9) % 10], points[(2*j + 1) % 10], colorFilled);
        }

        // Outlines
        drawList->AddPolyline(points, 10, colorOutline, ImDrawFlags_Closed, 2.0f * scale);
    }

    // 4. Statistics Card Display
    float cardWidth = 580.0f * scale;
    float cardHeight = 270.0f * scale;
    float cardMinX = centerX - cardWidth * 0.5f;
    float cardMinY = centerY - 130.0f * scale;

    ImVec2 cardMin{origin.x + cardMinX, origin.y + cardMinY};
    ImVec2 cardMax{origin.x + cardMinX + cardWidth, origin.y + cardMinY + cardHeight};

    // Draw card background
    drawList->AddRectFilled(
        cardMin,
        cardMax,
        ImGui::GetColorU32(ImVec4{0.04f, 0.06f, 0.16f, 0.85f * fadeAlpha}),
        12.0f * scale
    );

    // Neon glowing purple/violet border
    drawList->AddRect(
        cardMin,
        cardMax,
        ImGui::GetColorU32(ImVec4{0.6f, 0.15f, 0.95f, fadeAlpha}),
        12.0f * scale,
        0,
        2.5f * scale
    );

    float leftMargin = 32.0f * scale;
    float rightMargin = 32.0f * scale;
    float startRowY = cardMin.y + 24.0f * scale;
    float rowHeight = 44.0f * scale;
    float textFontSize = 21.0f * scale;

    struct StatRow {
        std::string label;
        std::string value;
        ImVec4 valueColor;
    };

    int totalSec = static_cast<int>(stats.timeSeconds);
    int min = totalSec / 60;
    int sec = totalSec % 60;
    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", min, sec);

    std::vector<StatRow> rows;
    rows.push_back({
        getLocText("debug.level_complete.score", "Score"),
        std::to_string(stats.score),
        ImVec4{0.0f, 0.95f, 1.0f, fadeAlpha}
    });
    rows.push_back({
        getLocText("debug.level_complete.time", "Time"),
        timeBuf,
        ImVec4{0.0f, 1.0f, 0.5f, fadeAlpha}
    });
    rows.push_back({
        getLocText("debug.level_complete.lives_lost", "Lives Lost"),
        std::to_string(stats.livesLost),
        stats.livesLost > 0 ? ImVec4{1.0f, 0.35f, 0.35f, fadeAlpha} : ImVec4{0.0f, 1.0f, 0.5f, fadeAlpha}
    });
    rows.push_back({
        getLocText("debug.level_complete.positive_bonuses", "Positive Bonuses"),
        std::to_string(stats.positiveBonuses),
        ImVec4{0.0f, 1.0f, 0.5f, fadeAlpha}
    });
    rows.push_back({
        getLocText("debug.level_complete.negative_bonuses", "Negative Bonuses"),
        std::to_string(stats.negativeBonuses),
        ImVec4{1.0f, 0.35f, 0.35f, fadeAlpha}
    });

    for (size_t rowIdx = 0; rowIdx < rows.size(); ++rowIdx) {
        const auto& row = rows[rowIdx];
        float currentY = startRowY + static_cast<float>(rowIdx) * rowHeight;

        // Label
        drawList->AddText(
            nullptr,
            textFontSize,
            ImVec2{cardMin.x + leftMargin, currentY},
            ImGui::GetColorU32(ImVec4{0.75f, 0.8f, 0.9f, fadeAlpha}),
            row.label.c_str()
        );

        // Value
        ImVec2 valSize = ImGui::CalcTextSize(row.value.c_str());
        drawList->AddText(
            nullptr,
            textFontSize,
            ImVec2{cardMax.x - rightMargin - valSize.x, currentY},
            ImGui::GetColorU32(row.valueColor),
            row.value.c_str()
        );
    }

    // 5. Interactive Buttons
    const float buttonWidth = 260.0f;
    const float buttonHeight = 54.0f;
    const float buttonSpacing = 40.0f;
    const ImVec2 buttonSize = UiLayout::logicalSize(buttonWidth, buttonHeight);
    const float totalButtonsWidth = 2.0f * buttonSize.x + buttonSpacing * scale;
    const float buttonStartX = centerX - totalButtonsWidth * 0.5f;
    const float buttonStartY = centerY + 180.0f * scale;

    ImGui::SetCursorPos(ImVec2{buttonStartX, buttonStartY});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeAlpha);

    std::string restartLabel = getLocText("debug.level_complete.restart", "Restart Level");
    const auto restartBtn = UiTheme::renderNeonButton(
        "restart",
        restartLabel.c_str(),
        UiAccent::Orange,
        buttonSize,
        open && selectedIndex_ == 0
    );

    ImGui::SetCursorPos(ImVec2{buttonStartX + buttonSize.x + buttonSpacing * scale, buttonStartY});
    std::string continueLabel = getLocText("debug.level_complete.continue", "Continue");
    const auto continueBtn = UiTheme::renderNeonButton(
        "continue",
        continueLabel.c_str(),
        UiAccent::Green,
        buttonSize,
        open && selectedIndex_ == 1
    );

    ImGui::PopStyleVar();

    if (open) {
        // Handle button mouse hover events
        if (restartBtn.hovered && selectedIndex_ != 0) {
            selectedIndex_ = 0;
            triggerSound(UiSoundEffect::Hover);
        } else if (continueBtn.hovered && selectedIndex_ != 1) {
            selectedIndex_ = 1;
            triggerSound(UiSoundEffect::Hover);
        }

        // Trigger actions on hover/select or enter keypress
        if (restartBtn.pressed || (selectedIndex_ == 0 && activationPressed())) {
            triggerSound(UiSoundEffect::Select);
            open = false;
            result.action = LevelCompleteAction::Restart;
        } else if (continueBtn.pressed || (selectedIndex_ == 1 && activationPressed())) {
            triggerSound(UiSoundEffect::Select);
            open = false;
            result.action = LevelCompleteAction::Continue;
        }
    }

    ImGui::End();
    return result;
}

} // namespace arcadeblocks::ui
