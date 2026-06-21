#pragma once

#include <SDL3/SDL_events.h>

#include <filesystem>

struct SDL_Renderer;
struct SDL_Window;

namespace arcadeblocks::ui {

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    bool initialize(SDL_Window* window, SDL_Renderer* renderer, float scale, const std::filesystem::path& assetsDirectory);
    void shutdown();

    void processEvent(const SDL_Event& event);
    void beginFrame();
    void endFrame();
    void setScale(float scale);

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] float scale() const noexcept;

private:
    void loadFonts(const std::filesystem::path& assetsDirectory);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    float scale_ = 1.0f;
    float actualDisplayWidth_ = 0.0f;
    float actualDisplayHeight_ = 0.0f;
    bool initialized_ = false;
};

} // namespace arcadeblocks::ui
