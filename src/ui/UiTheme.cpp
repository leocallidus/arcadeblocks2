#include "ui/UiTheme.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <unordered_map>

namespace arcadeblocks::ui {
namespace {

float g_scale = 1.0f;

ImVec4 color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    return ImVec4{
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        static_cast<float>(a) / 255.0f,
    };
}

} // namespace

void UiTheme::apply(float scale) {
    g_scale = std::clamp(scale, 0.75f, 2.0f);

    ImGuiStyle style{};
    ImGui::StyleColorsDark(&style);
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.ScaleAllSizes(g_scale);

    style.Colors[ImGuiCol_WindowBg] = color(12, 14, 28, 232);
    style.Colors[ImGuiCol_Border] = color(179, 136, 255, 165);
    style.Colors[ImGuiCol_Text] = textPrimary();
    style.Colors[ImGuiCol_TextDisabled] = color(162, 169, 196);
    style.Colors[ImGuiCol_FrameBg] = color(20, 23, 41, 225);
    style.Colors[ImGuiCol_FrameBgHovered] = color(34, 39, 66, 235);
    style.Colors[ImGuiCol_FrameBgActive] = color(42, 48, 84, 245);
    style.Colors[ImGuiCol_Button] = color(18, 21, 39, 235);
    style.Colors[ImGuiCol_ButtonHovered] = color(44, 52, 88, 245);
    style.Colors[ImGuiCol_ButtonActive] = color(58, 68, 112, 255);
    style.Colors[ImGuiCol_Header] = color(20, 23, 41, 225);
    style.Colors[ImGuiCol_HeaderHovered] = color(34, 39, 66, 235);
    style.Colors[ImGuiCol_HeaderActive] = color(42, 48, 84, 245);
    style.Colors[ImGuiCol_Separator] = color(179, 136, 255, 140);
    style.Colors[ImGuiCol_SeparatorHovered] = accent(UiAccent::Cyan);
    style.Colors[ImGuiCol_SeparatorActive] = accent(UiAccent::Cyan);
    style.Colors[ImGuiCol_ScrollbarBg] = color(9, 10, 22, 180);
    style.Colors[ImGuiCol_ScrollbarGrab] = color(179, 136, 255, 210);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = accent(UiAccent::Cyan);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = accent(UiAccent::Cyan);
    style.Colors[ImGuiCol_SliderGrab] = accent(UiAccent::Cyan);
    style.Colors[ImGuiCol_SliderGrabActive] = accent(UiAccent::Pink);
    style.Colors[ImGuiCol_CheckMark] = accent(UiAccent::Green);

    ImGui::GetStyle() = style;
}

ImVec4 UiTheme::accent(UiAccent accentColor) noexcept {
    switch (accentColor) {
    case UiAccent::Pink:
        return color(255, 110, 199);
    case UiAccent::Cyan:
        return color(126, 232, 250);
    case UiAccent::Purple:
        return color(179, 136, 255);
    case UiAccent::Green:
        return color(127, 255, 127);
    case UiAccent::Yellow:
        return color(255, 255, 127);
    case UiAccent::Orange:
        return color(255, 179, 71);
    case UiAccent::Red:
        return color(255, 68, 68);
    case UiAccent::Lime:
        return color(180, 255, 0);
    case UiAccent::Fuchsia:
        return color(255, 0, 255);
    case UiAccent::Brown:
        return color(190, 120, 50);
    case UiAccent::White:
        return color(240, 245, 255);
    }
    return color(255, 255, 255);
}

ImVec4 UiTheme::textPrimary() noexcept {
    return color(255, 255, 255);
}

ImVec4 UiTheme::textSecondary() noexcept {
    return color(224, 224, 224);
}

ImVec4 UiTheme::panelFill() noexcept {
    return color(15, 15, 28, 224);
}

float UiTheme::overlayOpacity() noexcept {
    return 0.68f;
}

float UiTheme::buttonHeight() {
    return 46.0f * g_scale;
}

float UiTheme::itemSpacing() {
    return 14.0f * g_scale;
}

float UiTheme::sectionSpacing() {
    return 22.0f * g_scale;
}

void UiTheme::beginPanelTitle(const char* title, UiAccent accentColor) {
    ImGui::PushStyleColor(ImGuiCol_Text, accent(accentColor));
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(title).x) * 0.5f);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void UiTheme::renderCyberpunkPanelTitle(const char* title, UiAccent accentColor) {
    const ImVec4 accentValue = accent(accentColor);
    const ImU32 accentU32 = ImGui::GetColorU32(accentValue);
    const ImU32 accentFaintU32 = ImGui::GetColorU32(ImVec4{accentValue.x, accentValue.y, accentValue.z, accentValue.w * 0.35f});
    const ImU32 textU32 = ImGui::GetColorU32(textPrimary());

    const ImVec2 textSize = ImGui::CalcTextSize(title);
    const float windowWidth = ImGui::GetWindowSize().x;
    const float titleY = ImGui::GetCursorPosY();
    const float baselineY = titleY + textSize.y;
    const float textX = (windowWidth - textSize.x) * 0.5f;
    const float bracketSize = textSize.y * 0.65f;
    const float bracketGap = 14.0f;
    const float bracketThickness = 2.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 windowPos = ImGui::GetWindowPos();

    const auto bracketPoints = [&](float cx, bool leftSide) {
        const float x = cx;
        const float y0 = baselineY - textSize.y;
        const float y1 = baselineY + 4.0f;
        if (leftSide) {
            drawList->AddLine(ImVec2{x, y0}, ImVec2{x + bracketSize, y0}, accentU32, bracketThickness);
            drawList->AddLine(ImVec2{x, y0}, ImVec2{x, y1}, accentU32, bracketThickness);
            drawList->AddLine(ImVec2{x, y0}, ImVec2{x + bracketSize, y0}, accentFaintU32, bracketThickness + 4.0f);
        } else {
            drawList->AddLine(ImVec2{x, y0}, ImVec2{x - bracketSize, y0}, accentU32, bracketThickness);
            drawList->AddLine(ImVec2{x, y0}, ImVec2{x, y1}, accentU32, bracketThickness);
            drawList->AddLine(ImVec2{x, y0}, ImVec2{x - bracketSize, y0}, accentFaintU32, bracketThickness + 4.0f);
        }
    };

    bracketPoints(windowPos.x + textX - bracketGap, true);
    bracketPoints(windowPos.x + textX + textSize.x + bracketGap, false);

    drawList->AddText(
        ImVec2{windowPos.x + textX + 1.0f, baselineY + 1.0f},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.6f}),
        title);
    drawList->AddText(
        ImVec2{windowPos.x + textX, baselineY},
        accentFaintU32,
        title);
    drawList->AddText(
        ImVec2{windowPos.x + textX, baselineY},
        textU32,
        title);

    const float underlineY = baselineY + 6.0f;
    const float underlinePad = 40.0f;
    drawList->AddLine(
        ImVec2{windowPos.x + (windowWidth - textSize.x) * 0.5f - underlinePad, underlineY},
        ImVec2{windowPos.x + (windowWidth + textSize.x) * 0.5f + underlinePad, underlineY},
        accentFaintU32,
        1.0f);
    drawList->AddLine(
        ImVec2{windowPos.x + (windowWidth - textSize.x) * 0.5f - 4.0f, underlineY},
        ImVec2{windowPos.x + (windowWidth - textSize.x) * 0.5f + 24.0f, underlineY},
        accentU32,
        2.0f);
    drawList->AddLine(
        ImVec2{windowPos.x + (windowWidth + textSize.x) * 0.5f - 24.0f, underlineY},
        ImVec2{windowPos.x + (windowWidth + textSize.x) * 0.5f + 4.0f, underlineY},
        accentU32,
        2.0f);

    ImGui::SetCursorPosY(underlineY + 10.0f);
}

UiButtonResult UiTheme::renderCyberpunkTabButton(
    const char* id,
    const char* label,
    UiAccent accentColor,
    bool selected,
    const ImVec2& size) {
    UiButtonResult result;
    const ImVec4 accentValue = accent(accentColor);
    const ImU32 accentU32 = ImGui::GetColorU32(accentValue);
    const ImU32 accentGlowU32 = ImGui::GetColorU32(ImVec4{accentValue.x, accentValue.y, accentValue.z, accentValue.w * 0.5f});
    const ImU32 accentFaintU32 = ImGui::GetColorU32(ImVec4{accentValue.x, accentValue.y, accentValue.z, accentValue.w * 0.25f});

    const ImVec4 idleBg = color(18, 21, 39, 220);
    const ImVec4 hoverBg = color(34, 39, 66, 235);
    const ImVec4 selectedBg = ImVec4{
        std::min(accentValue.x * 0.35f + 0.06f, 0.45f),
        std::min(accentValue.y * 0.35f + 0.06f, 0.45f),
        std::min(accentValue.z * 0.35f + 0.12f, 0.55f),
        0.95f,
    };

    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? selectedBg : idleBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? selectedBg : hoverBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, selected ? selectedBg : hoverBg);
    ImGui::PushStyleColor(ImGuiCol_Text, selected ? textPrimary() : textSecondary());

    result.pressed = ImGui::Button(label, size);
    result.hovered = ImGui::IsItemHovered();

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (selected) {
        const float glowThickness = 4.0f;
        drawList->AddRect(
            ImVec2{itemMin.x - 2.0f, itemMin.y - 2.0f},
            ImVec2{itemMax.x + 2.0f, itemMax.y + 2.0f},
            accentGlowU32,
            0.0f,
            0,
            glowThickness);
        drawList->AddRect(itemMin, itemMax, accentU32, 0.0f, 0, 1.5f);
    } else if (result.hovered) {
        drawList->AddRect(itemMin, itemMax, accentFaintU32, 0.0f, 0, 1.0f);
    }

    const float notch = std::min(size.x, size.y) * 0.18f;
    drawList->AddLine(ImVec2{itemMin.x, itemMin.y}, ImVec2{itemMin.x + notch, itemMin.y}, accentU32, 2.0f);
    drawList->AddLine(ImVec2{itemMin.x, itemMin.y}, ImVec2{itemMin.x, itemMin.y + notch}, accentU32, 2.0f);
    drawList->AddLine(ImVec2{itemMax.x, itemMin.y}, ImVec2{itemMax.x - notch, itemMin.y}, accentU32, 2.0f);
    drawList->AddLine(ImVec2{itemMax.x, itemMin.y}, ImVec2{itemMax.x, itemMin.y + notch}, accentU32, 2.0f);
    drawList->AddLine(ImVec2{itemMin.x, itemMax.y}, ImVec2{itemMin.x + notch, itemMax.y}, accentU32, 2.0f);
    drawList->AddLine(ImVec2{itemMin.x, itemMax.y}, ImVec2{itemMin.x, itemMax.y - notch}, accentU32, 2.0f);
    drawList->AddLine(ImVec2{itemMax.x, itemMax.y}, ImVec2{itemMax.x - notch, itemMax.y}, accentU32, 2.0f);
    drawList->AddLine(ImVec2{itemMax.x, itemMax.y}, ImVec2{itemMax.x, itemMax.y - notch}, accentU32, 2.0f);

    ImGui::PopID();
    return result;
}

void UiTheme::renderModalOverlay(float opacityScale) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return;
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddRectFilled(
        viewport->Pos,
        ImVec2{viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y},
        ImGui::GetColorU32(ImVec4{0.02f, 0.03f, 0.08f, overlayOpacity() * std::clamp(opacityScale, 0.0f, 1.0f)}));
}

UiButtonResult UiTheme::renderNeonButton(
    const char* id,
    const char* label,
    UiAccent accentColor,
    const ImVec2& size,
    bool selected) {
    UiButtonResult result;
    const ImVec4 accentValue = accent(accentColor);
    const ImVec4 baseBg = color(18, 21, 39, 235);
    const ImVec4 selectedBg = ImVec4{
        std::min(accentValue.x + 0.14f, 1.0f),
        std::min(accentValue.y + 0.14f, 1.0f),
        std::min(accentValue.z + 0.14f, 1.0f),
        0.95f,
    };

    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 2.0f : 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? selectedBg : baseBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? selectedBg : color(42, 48, 84, 245));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, selected ? selectedBg : color(58, 68, 112, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, selected ? accent(UiAccent::Cyan) : accentValue);
    ImGui::PushStyleColor(ImGuiCol_Text, textPrimary());

    result.pressed = ImGui::Button(label, size);
    result.hovered = ImGui::IsItemHovered();

    if (selected) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        drawList->AddRect(min, max, ImGui::GetColorU32(accent(UiAccent::Cyan)), 6.0f, 0, 2.0f);
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar();
    ImGui::PopID();
    return result;
}

void UiTheme::beginSectionHeader(const char* label, UiAccent accentColor) {
    ImGui::PushStyleColor(ImGuiCol_Text, accent(accentColor));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
}

void UiTheme::settingsRowLabel(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, textSecondary());
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::SameLine(170.0f * g_scale);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

void UiTheme::beginScrollPanel(const char* id, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, color(24, 26, 47, 214));
    ImGui::BeginChild(id, size, true);
}

void UiTheme::endScrollPanel() {
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ── Window open/close animation ────────────────────────────────────────────

namespace {

struct WindowAnimState {
    double startTime = -1.0;
    bool   wasClosing = false;
};

// Per-window animation state (keyed by window ID string).
std::unordered_map<std::string, WindowAnimState>& windowAnimMap() {
    static std::unordered_map<std::string, WindowAnimState> s_map;
    return s_map;
}

// Ease-out-back: overshoot slightly on open, snap on close.
float easeOutBack(float x) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float x1 = x - 1.0f;
    return 1.0f + c3 * x1 * x1 * x1 + c1 * x1 * x1;
}

} // namespace

float UiTheme::animateWindow(const char* windowId, bool closing, bool& done, float duration) {
    done = false;
    const double now = ImGui::GetTime();
    auto& state = windowAnimMap()[windowId];

    // Detect direction change (re-open while closing, or start closing).
    if (closing != state.wasClosing) {
        // Mirror: restart from the symmetric point so there's no jump.
        const double elapsed = (state.startTime < 0.0) ? 0.0 : now - state.startTime;
        const double progress = (duration > 0.0f) ? std::clamp(elapsed / static_cast<double>(duration), 0.0, 1.0) : 1.0;
        const double mirror = closing ? progress : (1.0 - progress);
        state.startTime = now - mirror * static_cast<double>(duration);
        state.wasClosing = closing;
    }

    if (state.startTime < 0.0) {
        state.startTime = now;
        state.wasClosing = closing;
    }

    const double elapsed = now - state.startTime;
    const float rawT = (duration > 0.0f)
        ? static_cast<float>(std::clamp(elapsed / static_cast<double>(duration), 0.0, 1.0))
        : 1.0f;

    float t;
    if (!closing) {
        t = easeOutBack(rawT);
        t = std::clamp(t, 0.0f, 1.0f);
    } else {
        // Closing: linear fade-out feels cleaner than back-ease.
        t = 1.0f - rawT;
        t = std::clamp(t, 0.0f, 1.0f);
        if (rawT >= 1.0f) {
            done = true;
            windowAnimMap().erase(windowId);
        }
    }

    return t;
}

void UiTheme::pushWindowAnimation(float t) {
    // Alpha: fade in/out.
    const float alpha = std::clamp(t, 0.0f, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
}

void UiTheme::popWindowAnimation() {
    ImGui::PopStyleVar(); // Alpha
}

} // namespace arcadeblocks::ui
