#pragma once

#include <imgui.h>

namespace arcadeblocks::ui {

enum class UiAccent {
    Pink,
    Cyan,
    Purple,
    Green,
    Yellow,
    Orange,
    Red,
    Lime,
    Fuchsia,
    Brown,
    White
};

enum class UiSoundEffect {
    None,
    Hover,
    Select,
    Back,
    SettingsChange,
    DebugOpen,
    StarGlow,
    LevelCompleteRandom
};

struct UiButtonResult {
    bool pressed = false;
    bool hovered = false;
};

class UiTheme {
public:
    static void apply(float scale);
    [[nodiscard]] static ImVec4 accent(UiAccent accent) noexcept;
    [[nodiscard]] static ImVec4 textPrimary() noexcept;
    [[nodiscard]] static ImVec4 textSecondary() noexcept;
    [[nodiscard]] static ImVec4 panelFill() noexcept;
    [[nodiscard]] static float overlayOpacity() noexcept;
    [[nodiscard]] static float buttonHeight();
    [[nodiscard]] static float itemSpacing();
    [[nodiscard]] static float sectionSpacing();
    static void beginPanelTitle(const char* title, UiAccent accent);
    static void renderCyberpunkPanelTitle(const char* title, UiAccent accent);
    [[nodiscard]] static UiButtonResult renderCyberpunkTabButton(
        const char* id,
        const char* label,
        UiAccent accent,
        bool selected,
        const ImVec2& size);
    static void renderModalOverlay(float opacityScale = 1.0f);
    [[nodiscard]] static UiButtonResult renderNeonButton(
        const char* id,
        const char* label,
        UiAccent accent,
        const ImVec2& size,
        bool selected);
    static void beginSectionHeader(const char* label, UiAccent accent);
    static void settingsRowLabel(const char* label);
    static void beginScrollPanel(const char* id, const ImVec2& size);
    static void endScrollPanel();

    /// Call before ImGui::Begin() to animate a modal window appearing/disappearing.
    /// @param t  Animation progress: 0 = just opened (or just starting to close),
    ///           1 = fully visible.  Pass the result of animateWindow().
    static void pushWindowAnimation(float t);
    static void popWindowAnimation();

    /// Returns an animated t value (0→1) for a window keyed by @p windowId.
    /// @param windowId   Unique string (same every frame for a given window).
    /// @param closing    True when the window is being asked to close (reverse anim).
    /// @param done       Set to true when a close animation finishes.
    /// @param duration   Animation duration in seconds (default 0.18s).
    [[nodiscard]] static float animateWindow(const char* windowId,
                                             bool closing,
                                             bool& done,
                                             float duration = 0.18f);
};

} // namespace arcadeblocks::ui
