#include "ui/HudView.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace arcadeblocks::ui {
namespace {

float clamp01(double value) {
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

ImVec4 timerTimeColor(const BonusTimerModel& timer, float alpha) {
    const double ratio = timer.durationSeconds > 0.0 ? timer.remainingSeconds / timer.durationSeconds : 0.0;
    if (ratio > 0.5) {
        return ImVec4{0.45f, 1.0f, 0.55f, alpha};
    }
    if (ratio > 0.2) {
        return ImVec4{1.0f, 0.84f, 0.28f, alpha};
    }
    return ImVec4{1.0f, 0.32f, 0.24f, alpha};
}

void renderBonusTimers(const HudModel& model) {
    if (model.bonusTimers.empty() || model.bonusTimerVisibility <= 0.001f) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return;
    }

    constexpr float fadeOutSeconds = 0.75f;
    const float windowWidth = 360.0f;
    const float rowHeight = 82.0f;
    const ImVec2 basePos{viewport->Pos.x + viewport->Size.x - 18.0f, viewport->Pos.y + 18.0f};
    ImGui::SetNextWindowPos(basePos, ImGuiCond_Always, ImVec2{1.0f, 0.0f});
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin(
        "BonusTimers",
        nullptr,
        ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoFocusOnAppearing);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (const auto& timer : model.bonusTimers) {
        float alpha = 1.0f;
        if (timer.remainingSeconds > 0.0) {
            if (timer.type == "PLASMA_WEAPON") {
                alpha = 1.0f;
            } else {
                alpha = clamp01((timer.durationSeconds - timer.remainingSeconds) / 0.28);
            }
        } else {
            alpha = clamp01(timer.fadeOutRemainingSeconds / fadeOutSeconds);
        }
        alpha *= std::clamp(model.bonusTimerVisibility, 0.0f, 1.0f);
        if (alpha <= 0.001f) {
            continue;
        }

        const float slideOffset = (1.0f - alpha) * 36.0f;
        const ImVec2 rowPos = ImGui::GetCursorScreenPos();
        const ImVec2 cardMin{rowPos.x + slideOffset, rowPos.y};
        const ImVec2 cardMax{cardMin.x + windowWidth, cardMin.y + rowHeight};
        const ImU32 bg = ImGui::GetColorU32(ImVec4{0.025f, 0.04f, 0.10f, 0.84f * alpha});
        const ImU32 border = ImGui::GetColorU32(ImVec4{0.24f, 0.86f, 1.0f, 0.78f * alpha});
        const ImU32 glow = ImGui::GetColorU32(ImVec4{0.10f, 0.58f, 1.0f, 0.22f * alpha});
        drawList->AddRectFilled(cardMin, cardMax, bg, 14.0f);
        drawList->AddRect(cardMin, cardMax, border, 14.0f, 0, 1.6f);
        drawList->AddRectFilled(
            ImVec2{cardMin.x, cardMin.y},
            ImVec2{cardMin.x + 5.0f, cardMax.y},
            glow,
            14.0f);

        const ImVec2 iconCenter{cardMin.x + 42.0f, cardMin.y + 40.0f};
        drawList->AddCircleFilled(iconCenter, 24.0f, ImGui::GetColorU32(ImVec4{0.12f, 0.88f, 1.0f, 0.22f * alpha}), 36);
        drawList->AddCircle(iconCenter, 24.0f, ImGui::GetColorU32(ImVec4{0.55f, 1.0f, 0.88f, 0.92f * alpha}), 36, 2.0f);
        if (timer.iconTexture != nullptr) {
            constexpr float iconW = 52.0f;
            constexpr float iconH = 32.0f;
            drawList->AddImage(
                reinterpret_cast<ImTextureID>(timer.iconTexture),
                ImVec2{iconCenter.x - iconW * 0.5f, iconCenter.y - iconH * 0.5f},
                ImVec2{iconCenter.x + iconW * 0.5f, iconCenter.y + iconH * 0.5f},
                ImVec2{timer.iconUv0x, timer.iconUv0y},
                ImVec2{timer.iconUv1x, timer.iconUv1y},
                ImGui::GetColorU32(ImVec4{1.0f, 1.0f, 1.0f, alpha}));
        } else {
            const char* icon = timer.type == "CALL_BALL" ? "B" : "*";
            const ImVec2 iconSize = ImGui::CalcTextSize(icon);
            drawList->AddText(
                ImVec2{iconCenter.x - iconSize.x * 0.5f, iconCenter.y - iconSize.y * 0.5f},
                ImGui::GetColorU32(ImVec4{0.86f, 1.0f, 0.98f, alpha}),
                icon);
        }

        const float textX = cardMin.x + 82.0f;
        drawList->AddText(
            ImVec2{textX, cardMin.y + 13.0f},
            ImGui::GetColorU32(ImVec4{0.90f, 0.96f, 1.0f, 0.95f * alpha}),
            timer.name.c_str());

        char seconds[32]{};
        if (timer.type == "PLASMA_WEAPON") {
            std::snprintf(seconds, sizeof(seconds), "%d", static_cast<int>(timer.remainingSeconds));
        } else {
            std::snprintf(seconds, sizeof(seconds), "%.1f s", std::max(0.0, timer.remainingSeconds));
        }
        drawList->AddText(
            ImVec2{textX, cardMin.y + 38.0f},
            ImGui::GetColorU32(timerTimeColor(timer, alpha)),
            seconds);

        const float progress = clamp01(timer.durationSeconds > 0.0 ? timer.remainingSeconds / timer.durationSeconds : 0.0);
        const ImVec2 barMin{textX, cardMin.y + 64.0f};
        const ImVec2 barMax{cardMax.x - 18.0f, cardMin.y + 70.0f};
        drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(ImVec4{0.13f, 0.16f, 0.25f, 0.78f * alpha}), 3.0f);
        drawList->AddRectFilled(
            barMin,
            ImVec2{barMin.x + (barMax.x - barMin.x) * progress, barMax.y},
            ImGui::GetColorU32(timerTimeColor(timer, alpha)),
            3.0f);

        ImGui::Dummy(ImVec2{windowWidth, rowHeight + 8.0f});
    }

    ImGui::End();
}

} // namespace

void HudView::render(const HudModel& model) {
    ImGui::SetNextWindowPos(ImVec2{16.0f, 16.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin(
        "HUD",
        nullptr,
        ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing);

    ImGui::Text("Level %d", model.level);
    if (!model.levelName.empty()) {
        ImGui::TextUnformatted(model.levelName.c_str());
    }
    ImGui::Separator();
    ImGui::Text("Score: %d", model.score);
    ImGui::Text("Lives: %d", model.lives);
    ImGui::Text("Bricks: %d", model.activeBricks);
    ImGui::Text("State: %s", model.phase.c_str());

    if (model.hasBoss) {
        ImGui::Separator();
        if (model.bossSectionHealthsNormalized.empty()) {
            ImGui::TextUnformatted(model.bossHpLabel.empty() ? "Boss HP:" : model.bossHpLabel.c_str());
            ImGui::ProgressBar(model.bossHealthNormalized, ImVec2(-1.0f, 0.0f));
        } else {
            ImGui::TextUnformatted(model.bossHpLabel.empty() ? "BOSS SECTIONS:" : model.bossHpLabel.c_str());
            for (size_t i = 0; i < model.bossSectionHealthsNormalized.size(); ++i) {
                ImGui::ProgressBar(model.bossSectionHealthsNormalized[i], ImVec2(-1.0f, 0.0f));
            }
        }
    }

    if (model.debug) {
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", model.framesPerSecond);
        ImGui::Text("Frame: %.3f ms", model.frameMilliseconds);
        ImGui::Text("Updates: %llu", static_cast<unsigned long long>(model.updatesThisFrame));
        ImGui::Text("Entities: %d", model.entityCount);
        ImGui::Text("Physics: %s", model.physicsDebug ? "debug" : "normal");
        if (model.assetStatsVisible) {
            ImGui::TextUnformatted(model.assetStats.c_str());
            ImGui::TextUnformatted(model.audioStats.c_str());
        }
    }

    ImGui::End();

    renderBonusTimers(model);
}

} // namespace arcadeblocks::ui
