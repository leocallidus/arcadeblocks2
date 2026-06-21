#include "ui/HelpView.hpp"

#include "ui/UiLayout.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace arcadeblocks::ui {
namespace {

struct ShowcaseRow {
    const char* spriteName;
    UiAccent accent;
    const char* titleKey;
    const char* descriptionKey;
};

std::string t(const localization::Localization& localization, settings::Language language, std::string_view key) {
    return localization.text(language, key);
}

std::string labelFor(const localization::Localization& localization, const ShowcaseRow& row, settings::Language language) {
    return t(localization, language, row.titleKey);
}

std::string descriptionFor(const localization::Localization& localization, const ShowcaseRow& row, settings::Language language) {
    return t(localization, language, row.descriptionKey);
}

const char* bindingName(const settings::KeyBinding& binding) {
    return binding.keyName.c_str();
}

bool activationPressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space);
}

void bulletWrapped(const char* text) {
    ImGui::BeginGroup();
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextWrapped("%s", text);
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
}

constexpr std::array<ShowcaseRow, 5> brickRows{{
    {"blue_brick.png", UiAccent::Cyan, "help.bricks.standard.title", "help.bricks.standard.description"},
    {"green_brick.png", UiAccent::Green, "help.bricks.tough.title", "help.bricks.tough.description"},
    {"pink_brick.png", UiAccent::Pink, "help.bricks.combo.title", "help.bricks.combo.description"},
    {"yellow_brick.png", UiAccent::Yellow, "help.bricks.heavy.title", "help.bricks.heavy.description"},
    {"explosive_bricks.png", UiAccent::Red, "help.bricks.explosive.title", "help.bricks.explosive.description"},
}};

constexpr std::array<ShowcaseRow, 18> positiveBonusRows{{
    {"bonus_score.png", UiAccent::Yellow, "help.bonuses.bonus_score.title", "help.bonuses.bonus_score.description"},
    {"bonus_score200.png", UiAccent::Cyan, "help.bonuses.bonus_score_200.title", "help.bonuses.bonus_score_200.description"},
    {"bonus_score500.png", UiAccent::Pink, "help.bonuses.bonus_score_500.title", "help.bonuses.bonus_score_500.description"},
    {"bonus_add_five_second.png", UiAccent::Green, "help.bonuses.add_five_seconds.title", "help.bonuses.add_five_seconds.description"},
    {"call_ball_to_paddle_bonus.png", UiAccent::Pink, "help.bonuses.call_ball.title", "help.bonuses.call_ball.description"},
    {"extra_life.png", UiAccent::Orange, "help.bonuses.extra_life.title", "help.bonuses.extra_life.description"},
    {"increase_paddle.png", UiAccent::Cyan, "help.bonuses.increase_paddle.title", "help.bonuses.increase_paddle.description"},
    {"sticky_paddle.png", UiAccent::Yellow, "help.bonuses.sticky_paddle.title", "help.bonuses.sticky_paddle.description"},
    {"slow_balls.png", UiAccent::Cyan, "help.bonuses.slow_balls.title", "help.bonuses.slow_balls.description"},
    {"energy_balls.png", UiAccent::Purple, "help.bonuses.energy_balls.title", "help.bonuses.energy_balls.description"},
    {"bonus_wall.png", UiAccent::Green, "help.bonuses.bonus_wall.title", "help.bonuses.bonus_wall.description"},
    {"bonus_magnet.png", UiAccent::Purple, "help.bonuses.bonus_magnet.title", "help.bonuses.bonus_magnet.description"},
    {"bonus_ball.png", UiAccent::Cyan, "help.bonuses.bonus_ball.title", "help.bonuses.bonus_ball.description"},
    {"plasma_weapon.png", UiAccent::Red, "help.bonuses.plasma_weapon.title", "help.bonuses.plasma_weapon.description"},
    {"explosion_balls.png", UiAccent::Orange, "help.bonuses.explosion_balls.title", "help.bonuses.explosion_balls.description"},
    {"trickster.png", UiAccent::Yellow, "help.bonuses.trickster.title", "help.bonuses.trickster.description"},
    {"level_complete_bonus.png", UiAccent::Green, "help.bonuses.level_pass.title", "help.bonuses.level_pass.description"},
    {"rainbow_bounty.png", UiAccent::Pink, "help.bonuses.rainbow_bounty.title", "help.bonuses.rainbow_bounty.description"},
}};

constexpr std::array<ShowcaseRow, 11> negativeBonusRows{{
    {"chaotic_balls.png", UiAccent::Pink, "help.bonuses.chaotic_balls.title", "help.bonuses.chaotic_balls.description"},
    {"freeze_paddle.png", UiAccent::Cyan, "help.bonuses.frozen_paddle.title", "help.bonuses.frozen_paddle.description"},
    {"decrease_paddle.png", UiAccent::Red, "help.bonuses.decrease_paddle.title", "help.bonuses.decrease_paddle.description"},
    {"fast_balls.png", UiAccent::Red, "help.bonuses.fast_balls.title", "help.bonuses.fast_balls.description"},
    {"penalties_magnet.png", UiAccent::Orange, "help.bonuses.penalties_magnet.title", "help.bonuses.penalties_magnet.description"},
    {"weak_balls.png", UiAccent::Red, "help.bonuses.weak_balls.title", "help.bonuses.weak_balls.description"},
    {"invisible_paddle.png", UiAccent::Purple, "help.bonuses.invisible_paddle.title", "help.bonuses.invisible_paddle.description"},
    {"darkness.png", UiAccent::Red, "help.bonuses.darkness.title", "help.bonuses.darkness.description"},
    {"bad_luck.png", UiAccent::Red, "help.bonuses.bad_luck.title", "help.bonuses.bad_luck.description"},
    {"reset.png", UiAccent::Red, "help.bonuses.reset.title", "help.bonuses.reset.description"},
    {"blood_tithe.png", UiAccent::Red, "help.bonuses.blood_tithe.title", "help.bonuses.blood_tithe.description"},
}};

constexpr std::array<ShowcaseRow, 1> specialBonusRows{{
    {"random.png", UiAccent::Yellow, "help.bonuses.random_bonus.title", "help.bonuses.random_bonus.description"},
}};

void renderSpriteBadge(const HelpViewAssets& assets, std::string_view spriteName, UiAccent accent, const ImVec2& size) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max{min.x + size.x, min.y + size.y};

    drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4{0.06f, 0.08f, 0.16f, 0.92f}), 8.0f);
    const ImVec4 accentColor = UiTheme::accent(accent);
    drawList->AddRect(min, max, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 0.92f}), 8.0f, 0, 2.0f);

    bool renderedSprite = false;
    if (assets.atlas != nullptr && assets.atlasTexture != nullptr) {
        if (const auto frame = assets.atlas->find(spriteName)) {
            if (!frame->rotated && assets.atlasTexture->width() > 0 && assets.atlasTexture->height() > 0) {
                const ImVec2 uv0{
                    static_cast<float>(frame->x) / static_cast<float>(assets.atlasTexture->width()),
                    static_cast<float>(frame->y) / static_cast<float>(assets.atlasTexture->height()),
                };
                const ImVec2 uv1{
                    static_cast<float>(frame->x + frame->w) / static_cast<float>(assets.atlasTexture->width()),
                    static_cast<float>(frame->y + frame->h) / static_cast<float>(assets.atlasTexture->height()),
                };
                const float inset = 8.0f * UiLayout::viewportScale();
                const float innerWidth = size.x - inset * 2.0f;
                const float innerHeight = size.y - inset * 2.0f;
                const float frameWidth = static_cast<float>(frame->w);
                const float frameHeight = static_cast<float>(frame->h);
                const float scale = std::min(innerWidth / frameWidth, innerHeight / frameHeight);
                const float drawWidth = frameWidth * scale;
                const float drawHeight = frameHeight * scale;
                const ImVec2 drawMin{
                    min.x + inset + (innerWidth - drawWidth) * 0.5f,
                    min.y + inset + (innerHeight - drawHeight) * 0.5f,
                };
                drawList->AddImage(
                    reinterpret_cast<ImTextureID>(assets.atlasTexture->native()),
                    drawMin,
                    ImVec2{drawMin.x + drawWidth, drawMin.y + drawHeight},
                    uv0,
                    uv1);
                renderedSprite = true;
            }
        }
    }

    if (!renderedSprite) {
        drawList->AddCircleFilled(ImVec2{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f}, 12.0f * UiLayout::viewportScale(), ImGui::GetColorU32(accentColor));
    }

    ImGui::Dummy(size);
}

void renderShowcaseRow(const localization::Localization& localization, const HelpViewAssets& assets, settings::Language language, const ShowcaseRow& row) {
    const float scale = UiLayout::viewportScale();
    const ImVec2 badgeSize{112.0f * scale, 72.0f * scale};

    ImGui::PushID(row.titleKey);
    ImGui::BeginGroup();
    renderSpriteBadge(assets, row.spriteName, row.accent, badgeSize);
    ImGui::EndGroup();
    ImGui::SameLine(0.0f, UiTheme::itemSpacing());

    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, UiTheme::accent(row.accent));
    const auto title = labelFor(localization, row, language);
    ImGui::TextUnformatted(title.c_str());
    ImGui::PopStyleColor();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    const auto description = descriptionFor(localization, row, language);
    ImGui::TextWrapped("%s", description.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    ImGui::Spacing();
    ImGui::PopID();
}

void renderControls(const localization::Localization& localization, settings::Language language, const settings::GameSettings& settings) {
    const auto title = t(localization, language, "help.controls.title");
    UiTheme::beginSectionHeader(title.c_str(), UiAccent::Cyan);
    ImGui::Spacing();
    bulletWrapped((t(localization, language, "help.controls.move_left_prefix") + bindingName(settings.controls.moveLeft)).c_str());
    bulletWrapped((t(localization, language, "help.controls.move_right_prefix") + bindingName(settings.controls.moveRight)).c_str());
    bulletWrapped((t(localization, language, "help.controls.launch_prefix") + bindingName(settings.controls.launch) + t(localization, language, "help.controls.launch_suffix")).c_str());
    bulletWrapped((t(localization, language, "help.controls.pause_prefix") + bindingName(settings.controls.pause) + t(localization, language, "help.controls.pause_suffix")).c_str());
    bulletWrapped(t(localization, language, "help.controls.mouse_paddle").c_str());
    bulletWrapped((t(localization, language, "help.controls.call_ball_prefix") + bindingName(settings.controls.callBall)).c_str());
    bulletWrapped((t(localization, language, "help.controls.turbo_prefix") + bindingName(settings.controls.turbo)).c_str());
    bulletWrapped((t(localization, language, "help.controls.turbo_ball_prefix") + bindingName(settings.controls.turboBall)).c_str());
    bulletWrapped((t(localization, language, "help.controls.plasma_prefix") + bindingName(settings.controls.plasma)).c_str());
}

void renderBricks(const localization::Localization& localization, settings::Language language, const HelpViewAssets& assets) {
    const auto title = t(localization, language, "help.bricks.title");
    UiTheme::beginSectionHeader(title.c_str(), UiAccent::Pink);
    ImGui::Spacing();
    for (const auto& row : brickRows) {
        renderShowcaseRow(localization, assets, language, row);
    }

    bulletWrapped(t(localization, language, "help.bricks.indestructible").c_str());
    bulletWrapped(t(localization, language, "help.bricks.shielded").c_str());
}

void renderBonuses(const localization::Localization& localization, settings::Language language, const HelpViewAssets& assets) {
    const auto title = t(localization, language, "help.bonuses.title");
    UiTheme::beginSectionHeader(title.c_str(), UiAccent::Green);
    ImGui::Spacing();

    // Positive block
    const auto positiveTitle = t(localization, language, "help.bonuses.positive.title");
    ImGui::TextColored(UiTheme::accent(UiAccent::Green), "%s", positiveTitle.c_str());
    ImGui::Separator();
    ImGui::Spacing();
    for (const auto& row : positiveBonusRows) {
        renderShowcaseRow(localization, assets, language, row);
    }
    ImGui::Spacing();

    // Negative block
    const auto negativeTitle = t(localization, language, "help.bonuses.negative.title");
    ImGui::TextColored(UiTheme::accent(UiAccent::Red), "%s", negativeTitle.c_str());
    ImGui::Separator();
    ImGui::Spacing();
    for (const auto& row : negativeBonusRows) {
        renderShowcaseRow(localization, assets, language, row);
    }
    ImGui::Spacing();

    // Special block
    const auto specialTitle = t(localization, language, "help.bonuses.special.title");
    ImGui::TextColored(UiTheme::accent(UiAccent::Yellow), "%s", specialTitle.c_str());
    ImGui::Separator();
    ImGui::Spacing();
    for (const auto& row : specialBonusRows) {
        renderShowcaseRow(localization, assets, language, row);
    }
    ImGui::Spacing();

    bulletWrapped(t(localization, language, "help.bonuses.footer").c_str());
}

void renderGoal(const localization::Localization& localization, settings::Language language, bool openedFromPause) {
    const auto title = t(localization, language, "help.goal.title");
    UiTheme::beginSectionHeader(title.c_str(), UiAccent::Yellow);
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    const auto paragraph1 = t(localization, language, "help.goal.paragraph1");
    ImGui::TextWrapped("%s", paragraph1.c_str());
    ImGui::Spacing();
    const auto paragraph2 = t(localization, language, "help.goal.paragraph2");
    ImGui::TextWrapped("%s", paragraph2.c_str());
    ImGui::Spacing();
    const auto returnText = openedFromPause
        ? t(localization, language, "help.goal.return_pause")
        : t(localization, language, "help.goal.return_menu");
    ImGui::TextWrapped("%s", returnText.c_str());
    ImGui::PopTextWrapPos();
}

void handleScrollShortcuts() {
    if (!ImGui::IsWindowFocused() && !ImGui::IsWindowHovered()) {
        return;
    }

    const float step = 56.0f * UiLayout::viewportScale();
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        ImGui::SetScrollY(ImGui::GetScrollY() + step);
    } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - step));
    } else if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
        ImGui::SetScrollY(ImGui::GetScrollY() + step * 4.0f);
    } else if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
        ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - step * 4.0f));
    } else if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        ImGui::SetScrollY(0.0f);
    } else if (ImGui::IsKeyPressed(ImGuiKey_End)) {
        ImGui::SetScrollY(ImGui::GetScrollMaxY());
    }
}

} // namespace

HelpRenderResult HelpView::render(
    const localization::Localization& localization,
    settings::Language language,
    const settings::GameSettings& settings,
    const HelpViewAssets& assets,
    bool openedFromPause,
    bool openedThisFrame) {
    HelpRenderResult result;

    // Reset close animation when newly opened.
    if (openedThisFrame) {
        closingAnimation_ = false;
    }

    bool animDone = false;
    const float animT = UiTheme::animateWindow("HelpWindow", closingAnimation_, animDone);
    if (closingAnimation_ && animDone) {
        result.action = HelpAction::Close;
        closingAnimation_ = false;
        return result;
    }

    UiTheme::renderModalOverlay(0.92f * animT);
    UiTheme::pushWindowAnimation(animT);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = UiLayout::viewportScale();
    const ImVec2 desiredSize = UiLayout::logicalSize(1180.0f, 760.0f);
    const ImVec2 maxSize{
        std::max(360.0f * scale, viewport->Size.x - 64.0f * scale),
        std::max(320.0f * scale, viewport->Size.y - 64.0f * scale),
    };
    const ImVec2 windowSize{
        std::min(desiredSize.x, maxSize.x),
        std::min(desiredSize.y, maxSize.y),
    };

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    const auto windowTitle = t(localization, language, "help.window_title") + "###HelpWindow";
    ImGui::Begin(
        windowTitle.c_str(),
        nullptr,
        ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoSavedSettings);

    const auto title = t(localization, language, "help.title");
    UiTheme::renderCyberpunkPanelTitle(title.c_str(), UiAccent::Green);
    const auto contextText = openedFromPause
        ? t(localization, language, "help.context.pause")
        : t(localization, language, "help.context.menu");
    ImGui::TextWrapped("%s", contextText.c_str());
    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const float footerHeight = UiTheme::buttonHeight() + UiTheme::sectionSpacing() + 8.0f * scale;
    UiTheme::beginScrollPanel("help-scroll", ImVec2{0.0f, std::max(220.0f * scale, ImGui::GetContentRegionAvail().y - footerHeight)});
    handleScrollShortcuts();

    const bool wideLayout = ImGui::GetContentRegionAvail().x >= 860.0f * scale;
    if (wideLayout && ImGui::BeginTable("help-columns", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("help-left", ImGuiTableColumnFlags_WidthStretch, 0.48f);
        ImGui::TableSetupColumn("help-right", ImGuiTableColumnFlags_WidthStretch, 0.52f);
        ImGui::TableNextColumn();
        renderControls(localization, language, settings);
        ImGui::Dummy(ImVec2{0.0f, UiTheme::sectionSpacing()});
        renderBricks(localization, language, assets);

        ImGui::TableNextColumn();
        renderBonuses(localization, language, assets);
        ImGui::Dummy(ImVec2{0.0f, UiTheme::sectionSpacing()});
        renderGoal(localization, language, openedFromPause);
        ImGui::EndTable();
    } else {
        renderControls(localization, language, settings);
        ImGui::Dummy(ImVec2{0.0f, UiTheme::sectionSpacing()});
        renderBricks(localization, language, assets);
        ImGui::Dummy(ImVec2{0.0f, UiTheme::sectionSpacing()});
        renderBonuses(localization, language, assets);
        ImGui::Dummy(ImVec2{0.0f, UiTheme::sectionSpacing()});
        renderGoal(localization, language, openedFromPause);
    }

    UiTheme::endScrollPanel();
    ImGui::Dummy(ImVec2{0.0f, UiTheme::itemSpacing() * 0.5f});

    const auto backButton = UiTheme::renderNeonButton(
        "help-back",
        t(localization, language, "common.back").c_str(),
        UiAccent::Cyan,
        ImVec2{ImGui::GetContentRegionAvail().x, UiTheme::buttonHeight()},
        true);
    if (backButton.pressed || (ImGui::IsKeyPressed(ImGuiKey_Escape) && !closingAnimation_) || (!openedThisFrame && activationPressed())) {
        closingAnimation_ = true;
        result.soundEffect = UiSoundEffect::Back;
    }

    ImGui::End();
    UiTheme::popWindowAnimation();
    return result;
}

} // namespace arcadeblocks::ui
