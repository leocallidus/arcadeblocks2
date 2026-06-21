#pragma once

#include <imgui.h>

namespace arcadeblocks::ui {

class UiLayout {
public:
    static constexpr float logicalWidth = 1920.0f;
    static constexpr float logicalHeight = 1080.0f;

    [[nodiscard]] static float viewportScale();
    [[nodiscard]] static ImVec2 logicalSize(float width, float height);
    [[nodiscard]] static ImVec2 logicalPosition(float x, float y);
    static void setNextCenteredWindow(float logicalWidth, float logicalHeight, ImGuiCond condition = ImGuiCond_Always);
};

} // namespace arcadeblocks::ui
