#include "ui/LoadingScreenView.hpp"

#include "core/Version.hpp"
#include "ui/UiLayout.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace arcadeblocks::ui {
namespace {

/// Helper: localization shorthand.
std::string t(const localization::Localization& loc, settings::Language lang, std::string_view key) {
    return loc.text(lang, key);
}

float clamp01(double v) {
    return static_cast<float>(std::clamp(v, 0.0, 1.0));
}

ImVec2 toScreen(ImVec2 origin, ImVec2 local) {
    return ImVec2{origin.x + local.x, origin.y + local.y};
}

/// Ease-out quad for smooth progress bar.
float easeOutQuad(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return 1.0f - (1.0f - x) * (1.0f - x);
}

/// Ease-in-out sine for smooth pulsing.
float pulse(double time, float frequency = 2.0f) {
    return 0.5f + 0.5f * std::sin(static_cast<float>(time * frequency * 3.14159265));
}

void drawImageCover(ImDrawList* drawList, const render::Texture* tex, ImVec2 areaMin, ImVec2 areaMax, float alpha = 1.0f) {
    if (!tex || !*tex || !tex->native()) return;
    const float texW = static_cast<float>(tex->width());
    const float texH = static_cast<float>(tex->height());
    const float areaW = areaMax.x - areaMin.x;
    const float areaH = areaMax.y - areaMin.y;
    const float scaleVal = std::max(areaW / texW, areaH / texH);
    const float drawW = texW * scaleVal;
    const float drawH = texH * scaleVal;
    const float offX = (areaW - drawW) * 0.5f;
    const float offY = (areaH - drawH) * 0.5f;
    drawList->AddImage(
        reinterpret_cast<ImTextureID>(tex->native()),
        ImVec2{areaMin.x + offX, areaMin.y + offY},
        ImVec2{areaMin.x + offX + drawW, areaMin.y + offY + drawH},
        ImVec2{0, 0}, ImVec2{1, 1},
        ImGui::GetColorU32(ImVec4{1, 1, 1, alpha}));
}

} // namespace

LoadingScreenView::LoadingScreenView(unsigned int seed)
    : rng_(seed) {
    std::uniform_int_distribution<int> dist(0, welcomeMessageCount_ - 1);
    welcomeIndex_ = dist(rng_);
}

std::string LoadingScreenView::welcomeKey() const {
    return "loading.welcome." + std::to_string(welcomeIndex_);
}

bool LoadingScreenView::active() const noexcept {
    return !finished_;
}

bool LoadingScreenView::finished() const noexcept {
    return finished_;
}

void LoadingScreenView::dismiss() {
    if (!dismissed_) {
        dismissed_ = true;
        dismissedAt_ = ImGui::GetTime();
    }
}

LoadingScreenResult LoadingScreenView::render(
    const LoadingScreenAssets& assets,
    const localization::Localization& localization,
    settings::Language language) {

    LoadingScreenResult result;

    if (finished_) {
        result.dismissed = true;
        return result;
    }

    const double now = ImGui::GetTime();

    // First frame — record start time.
    if (startTime_ < 0.0) {
        startTime_ = now;
    }

    const double elapsed = now - startTime_;

    // ESC to dismiss.
    if (!dismissed_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        dismiss();
    }

    // Auto-dismiss after displayDuration_.
    if (!dismissed_ && elapsed >= displayDuration_) {
        dismiss();
    }

    // Fade-out progress (0 = fully visible, 1 = fully invisible).
    float fadeOut = 0.0f;
    if (dismissed_) {
        const double fadeElapsed = now - dismissedAt_;
        fadeOut = clamp01(fadeElapsed / fadeOutDuration_);
        if (fadeOut >= 1.0f) {
            finished_ = true;
            result.dismissed = true;
            return result;
        }
    }

    const float alpha = 1.0f - fadeOut;
    const float scale = UiLayout::viewportScale();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0, 0, 0, 0});
    ImGui::Begin(
        "LoadingScreen",
        nullptr,
        ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float vpW = viewport->Size.x;
    const float vpH = viewport->Size.y;
    const float centerX = vpW * 0.5f;
    const float centerY = vpH * 0.5f;

    // ── 1. Menu background behind everything ──
    drawImageCover(dl, assets.menuBackground, origin, ImVec2{origin.x + vpW, origin.y + vpH}, alpha);

    // ── 2. Dark cinematic overlay ──
    dl->AddRectFilled(
        origin,
        ImVec2{origin.x + vpW, origin.y + vpH},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.72f * alpha}));

    // ── 3. Decorative gradient strips (top & bottom) ──
    {
        const float stripH = 6.0f * scale;
        // Top neon strip — cyan
        dl->AddRectFilledMultiColor(
            origin,
            ImVec2{origin.x + vpW, origin.y + stripH},
            ImGui::GetColorU32(ImVec4{0.22f, 0.82f, 0.98f, 0.92f * alpha}),
            ImGui::GetColorU32(ImVec4{0.62f, 0.28f, 0.96f, 0.92f * alpha}),
            ImGui::GetColorU32(ImVec4{0.62f, 0.28f, 0.96f, 0.0f}),
            ImGui::GetColorU32(ImVec4{0.22f, 0.82f, 0.98f, 0.0f}));
        // Bottom neon strip — pink
        dl->AddRectFilledMultiColor(
            ImVec2{origin.x, origin.y + vpH - stripH},
            ImVec2{origin.x + vpW, origin.y + vpH},
            ImGui::GetColorU32(ImVec4{0.96f, 0.28f, 0.82f, 0.0f}),
            ImGui::GetColorU32(ImVec4{0.42f, 0.86f, 1.0f, 0.0f}),
            ImGui::GetColorU32(ImVec4{0.42f, 0.86f, 1.0f, 0.92f * alpha}),
            ImGui::GetColorU32(ImVec4{0.96f, 0.28f, 0.82f, 0.92f * alpha}));
    }

    // ── 4. Soft ambient circles (decorative bokeh) ──
    {
        const float p1 = pulse(now, 1.2f);
        const float p2 = pulse(now + 1.5, 0.9f);
        dl->AddCircleFilled(
            toScreen(origin, ImVec2{vpW * 0.15f, vpH * 0.3f}),
            180.0f * scale,
            ImGui::GetColorU32(ImVec4{0.18f, 0.56f, 0.94f, 0.06f * alpha * p1}));
        dl->AddCircleFilled(
            toScreen(origin, ImVec2{vpW * 0.85f, vpH * 0.7f}),
            220.0f * scale,
            ImGui::GetColorU32(ImVec4{0.96f, 0.22f, 0.76f, 0.06f * alpha * p2}));
        dl->AddCircleFilled(
            toScreen(origin, ImVec2{vpW * 0.5f, vpH * 0.2f}),
            140.0f * scale,
            ImGui::GetColorU32(ImVec4{0.42f, 0.86f, 1.0f, 0.04f * alpha * p1}));
    }

    // ── 5. Game logo — centered, prominent ──
    if (assets.gameLogo && *assets.gameLogo && assets.gameLogo->native()) {
        const float logoMaxW = 520.0f * scale;
        const float logoMaxH = 340.0f * scale;
        const float texW = static_cast<float>(assets.gameLogo->width());
        const float texH = static_cast<float>(assets.gameLogo->height());
        const float logoScale = std::min(logoMaxW / texW, logoMaxH / texH);
        const float drawW = texW * logoScale;
        const float drawH = texH * logoScale;
        const float logoX = centerX - drawW * 0.5f;
        const float logoY = centerY - drawH * 0.5f - 60.0f * scale;

        // Subtle glow behind logo
        dl->AddCircleFilled(
            toScreen(origin, ImVec2{centerX, logoY + drawH * 0.5f}),
            (drawW * 0.5f + 40.0f * scale),
            ImGui::GetColorU32(ImVec4{0.22f, 0.52f, 0.88f, 0.08f * alpha}));

        dl->AddImage(
            reinterpret_cast<ImTextureID>(assets.gameLogo->native()),
            toScreen(origin, ImVec2{logoX, logoY}),
            toScreen(origin, ImVec2{logoX + drawW, logoY + drawH}),
            ImVec2{0, 0}, ImVec2{1, 1},
            ImGui::GetColorU32(ImVec4{1, 1, 1, alpha}));
    }

    // ── 6. Studio logo — bottom-left corner ──
    if (assets.studioLogo && *assets.studioLogo && assets.studioLogo->native()) {
        const float logoMaxW = 320.0f * scale;
        const float logoMaxH = 140.0f * scale;
        const float texW = static_cast<float>(assets.studioLogo->width());
        const float texH = static_cast<float>(assets.studioLogo->height());
        const float logoScale = std::min(logoMaxW / texW, logoMaxH / texH);
        const float drawW = texW * logoScale;
        const float drawH = texH * logoScale;
        const float margin = 32.0f * scale;
        const float logoX = margin;
        const float logoY = vpH - drawH - margin;

        dl->AddImage(
            reinterpret_cast<ImTextureID>(assets.studioLogo->native()),
            toScreen(origin, ImVec2{logoX, logoY}),
            toScreen(origin, ImVec2{logoX + drawW, logoY + drawH}),
            ImVec2{0, 0}, ImVec2{1, 1},
            ImGui::GetColorU32(ImVec4{1, 1, 1, 0.88f * alpha}));
    }

    // ── 7. Loading bar ──
    {
        const float barW = 480.0f * scale;
        const float barH = 8.0f * scale;
        const float barX = centerX - barW * 0.5f;
        const float barY = centerY + 140.0f * scale;
        const float progress = dismissed_ ? 1.0f : easeOutQuad(clamp01(elapsed / displayDuration_));

        // Bar background (dark glass)
        dl->AddRectFilled(
            toScreen(origin, ImVec2{barX, barY}),
            toScreen(origin, ImVec2{barX + barW, barY + barH}),
            ImGui::GetColorU32(ImVec4{0.1f, 0.12f, 0.2f, 0.7f * alpha}),
            barH * 0.5f);

        // Bar fill — gradient cyan → pink
        if (progress > 0.001f) {
            const float fillW = barW * progress;
            dl->AddRectFilledMultiColor(
                toScreen(origin, ImVec2{barX, barY}),
                toScreen(origin, ImVec2{barX + fillW, barY + barH}),
                ImGui::GetColorU32(ImVec4{0.22f, 0.82f, 0.98f, 0.95f * alpha}),
                ImGui::GetColorU32(ImVec4{0.96f, 0.32f, 0.84f, 0.95f * alpha}),
                ImGui::GetColorU32(ImVec4{0.96f, 0.32f, 0.84f, 0.95f * alpha}),
                ImGui::GetColorU32(ImVec4{0.22f, 0.82f, 0.98f, 0.95f * alpha}));

            // Glow on tip of bar
            const float glowR = 18.0f * scale;
            dl->AddCircleFilled(
                toScreen(origin, ImVec2{barX + fillW, barY + barH * 0.5f}),
                glowR,
                ImGui::GetColorU32(ImVec4{0.96f, 0.32f, 0.84f, 0.35f * alpha * pulse(now, 3.0f)}));
        }

        // Bar outline
        dl->AddRect(
            toScreen(origin, ImVec2{barX - 1.0f, barY - 1.0f}),
            toScreen(origin, ImVec2{barX + barW + 1.0f, barY + barH + 1.0f}),
            ImGui::GetColorU32(ImVec4{0.44f, 0.84f, 0.98f, 0.4f * alpha}),
            barH * 0.5f, 0, 1.0f);
    }

    // ── 8. Welcome message ──
    {
        const auto welcomeText = t(localization, language, welcomeKey());
        const float textY = centerY + 170.0f * scale;
        const ImVec2 textSize = ImGui::CalcTextSize(welcomeText.c_str());
        const float textX = centerX - textSize.x * 0.5f;

        dl->AddText(
            nullptr, 0.0f,
            toScreen(origin, ImVec2{textX, textY}),
            ImGui::GetColorU32(ImVec4{0.82f, 0.86f, 0.94f, 0.92f * alpha}),
            welcomeText.c_str());
    }

    // ── 9. "Press ESC to skip" hint ──
    if (!dismissed_) {
        const auto skipHint = t(localization, language, "loading.skip_hint");
        const ImVec2 hintSize = ImGui::CalcTextSize(skipHint.c_str());
        const float hintX = vpW - hintSize.x - 28.0f * scale;
        const float hintY = vpH - hintSize.y - 20.0f * scale;
        const float hintAlpha = 0.5f + 0.2f * pulse(now, 1.5f);

        dl->AddText(
            nullptr, 0.0f,
            toScreen(origin, ImVec2{hintX, hintY}),
            ImGui::GetColorU32(ImVec4{0.7f, 0.74f, 0.84f, hintAlpha * alpha}),
            skipHint.c_str());
    }

    // ── 10. Version watermark ──
    {
        const std::string ver = std::string(core::version());
        const ImVec2 verSize = ImGui::CalcTextSize(ver.c_str());
        const float verX = vpW - verSize.x - 28.0f * scale;
        const float verY = 14.0f * scale;
        dl->AddText(
            nullptr, 0.0f,
            toScreen(origin, ImVec2{verX, verY}),
            ImGui::GetColorU32(ImVec4{0.5f, 0.55f, 0.65f, 0.5f * alpha}),
            ver.c_str());
    }

    ImGui::End();

    return result;
}

} // namespace arcadeblocks::ui
