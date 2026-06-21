#include "ui/UiLayout.hpp"

#include <algorithm>

namespace arcadeblocks::ui {

float UiLayout::viewportScale() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
        return 1.0f;
    }

    return viewport->Size.x / logicalWidth;
}

ImVec2 UiLayout::logicalSize(float width, float height) {
    const float scale = viewportScale();
    return ImVec2{width * scale, height * scale};
}

ImVec2 UiLayout::logicalPosition(float x, float y) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = viewportScale();
    return ImVec2{
        viewport->WorkPos.x + x * scale,
        viewport->WorkPos.y + y * scale,
    };
}

void UiLayout::setNextCenteredWindow(float width, float height, ImGuiCond condition) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center = viewport->GetCenter();
    ImGui::SetNextWindowPos(center, condition, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(logicalSize(width, height), condition);
}

} // namespace arcadeblocks::ui
