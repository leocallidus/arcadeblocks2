#include "ui/ImGuiLayer.hpp"

#include "core/Log.hpp"
#include "ui/UiLayout.hpp"
#include "ui/UiTheme.hpp"

#include <algorithm>
#include <filesystem>
#include <vector>

#include <imgui.h>
#include <SDL3/SDL_render.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

namespace arcadeblocks::ui {
namespace {

constexpr const char* orbitronFontRelativePath = "fonts/Orbitron-Medium.ttf";
constexpr const char* bundledCyrillicFallbackRelativePath = "fonts/Exo2-Medium.ttf";

std::vector<std::filesystem::path> cyrillicFallbackCandidates(const std::filesystem::path& assetsDirectory) {
    return {
        assetsDirectory / bundledCyrillicFallbackRelativePath,
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/IBMPlexSans-Regular.ttf",
    };
}

std::filesystem::path findExistingFont(const std::vector<std::filesystem::path>& candidates) {
    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }
    }

    return {};
}

} // namespace

ImGuiLayer::~ImGuiLayer() {
    shutdown();
}

bool ImGuiLayer::initialize(SDL_Window* window, SDL_Renderer* renderer, float scale, const std::filesystem::path& assetsDirectory) {
    window_ = window;
    renderer_ = renderer;
    scale_ = std::clamp(scale, 0.75f, 2.0f);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    loadFonts(assetsDirectory);

    setScale(scale_);

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_)) {
        core::Log::error("ImGui SDL3 backend initialization failed");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplSDLRenderer3_Init(renderer_)) {
        core::Log::error("ImGui SDL renderer backend initialization failed");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    initialized_ = true;
    core::Log::info("Dear ImGui initialized with SDL3 renderer backend");
    return true;
}

void ImGuiLayer::shutdown() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

void ImGuiLayer::processEvent(const SDL_Event& event) {
    if (initialized_) {
        SDL_Event imguiEvent = event;
        if (renderer_ != nullptr) {
            SDL_ConvertEventToRenderCoordinates(renderer_, &imguiEvent);
        }
        ImGui_ImplSDL3_ProcessEvent(&imguiEvent);
    }
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    auto& io = ImGui::GetIO();
    actualDisplayWidth_ = io.DisplaySize.x;
    actualDisplayHeight_ = io.DisplaySize.y;
    io.DisplaySize = ImVec2{UiLayout::logicalWidth, UiLayout::logicalHeight};
    io.DisplayFramebufferScale = ImVec2{1.0f, 1.0f};

    ImGui::NewFrame();
}

void ImGuiLayer::endFrame() {
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);

    auto& io = ImGui::GetIO();
    if (actualDisplayWidth_ > 0.0f && actualDisplayHeight_ > 0.0f) {
        io.DisplaySize = ImVec2{actualDisplayWidth_, actualDisplayHeight_};
    }
}

void ImGuiLayer::setScale(float scale) {
    scale_ = std::clamp(scale, 0.75f, 2.0f);

    UiTheme::apply(scale_);

    auto& io = ImGui::GetIO();
    io.FontGlobalScale = scale_;
}

bool ImGuiLayer::initialized() const noexcept {
    return initialized_;
}

float ImGuiLayer::scale() const noexcept {
    return scale_;
}

void ImGuiLayer::loadFonts(const std::filesystem::path& assetsDirectory) {
    auto& io = ImGui::GetIO();
    io.Fonts->Clear();

    const auto fontPath = assetsDirectory / orbitronFontRelativePath;
    if (io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 28.0f) == nullptr) {
        core::Log::warn("ImGui font load failed: " + fontPath.string() + "; using default font");
        io.Fonts->AddFontDefault();
        return;
    }

    const auto fallbackPath = findExistingFont(cyrillicFallbackCandidates(assetsDirectory));
    if (!fallbackPath.empty()) {
        static constexpr ImWchar cyrillicRanges[] = {
            0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
            0x2116, 0x2116, // Numero sign, common in Russian UI
            0,
        };

        ImFontConfig fallbackConfig;
        fallbackConfig.MergeMode = true;
        fallbackConfig.PixelSnapH = true;

        if (io.Fonts->AddFontFromFileTTF(fallbackPath.string().c_str(), 28.0f, &fallbackConfig, cyrillicRanges) == nullptr) {
            core::Log::warn("ImGui Cyrillic fallback font load failed: " + fallbackPath.string());
        } else {
            core::Log::info("Loaded ImGui Cyrillic fallback font: " + fallbackPath.string());
        }
    } else {
        core::Log::warn("No ImGui Cyrillic fallback font found; Russian glyphs may be missing");
    }

    io.FontDefault = io.Fonts->Fonts.back();
    core::Log::info("Loaded ImGui font: " + fontPath.string());
}

} // namespace arcadeblocks::ui
