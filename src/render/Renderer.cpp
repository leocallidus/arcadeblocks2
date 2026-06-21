#include "render/Renderer.hpp"

#include "core/Log.hpp"
#include "render/Texture.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <string>

namespace arcadeblocks::render {

namespace {

constexpr const char* orbitronFontRelativePath = "fonts/Orbitron-Medium.ttf";
constexpr float defaultFontSize = 22.0f;

SDL_FRect toSdl(Rect rect) {
    return SDL_FRect{rect.x, rect.y, rect.w, rect.h};
}

} // namespace

Renderer::Renderer(SDL_Renderer* renderer, std::filesystem::path assetsDirectory)
    : renderer_(renderer),
      assetsDirectory_(std::move(assetsDirectory)) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    const bool fontReady = initializeFont();
    (void)fontReady;
}

Renderer::~Renderer() {
    if (font_ != nullptr) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }

    if (ttfInitialized_) {
        TTF_Quit();
        ttfInitialized_ = false;
    }
}

void Renderer::beginFrame(Color clearColor) {
    SDL_SetRenderDrawColor(renderer_, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    SDL_RenderClear(renderer_);
}

void Renderer::drawTexture(const Texture& texture, Rect destination) {
    if (!texture) {
        return;
    }
    auto dst = toSdl(destination);
    SDL_RenderTexture(renderer_, texture.native(), nullptr, &dst);
}

void Renderer::drawSprite(const Texture& atlasTexture, const SpriteFrame& frame, Rect destination, unsigned char alpha) {
    if (!atlasTexture || frame.w <= 0 || frame.h <= 0 || frame.rotated) {
        return;
    }

    SDL_FRect src{
        static_cast<float>(frame.x),
        static_cast<float>(frame.y),
        static_cast<float>(frame.w),
        static_cast<float>(frame.h),
    };
    auto dst = toSdl(destination);
    SDL_SetTextureBlendMode(atlasTexture.native(), SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(atlasTexture.native(), alpha);
    SDL_RenderTexture(renderer_, atlasTexture.native(), &src, &dst);
    SDL_SetTextureAlphaMod(atlasTexture.native(), 255);
}

void Renderer::drawRect(Rect rect, Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    auto dst = toSdl(rect);
    SDL_RenderFillRect(renderer_, &dst);
}

void Renderer::drawLine(float x1, float y1, float x2, float y2, Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderLine(renderer_, x1, y1, x2, y2);
}

void Renderer::drawText(float x, float y, std::string_view text, Color color) {
    if (font_ == nullptr) {
        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        const std::string copy{text};
        SDL_RenderDebugText(renderer_, x, y, copy.c_str());
        return;
    }

    const std::string copy{text};
    SDL_Surface* surface = TTF_RenderText_Blended(
        font_,
        copy.c_str(),
        copy.size(),
        SDL_Color{color.r, color.g, color.b, color.a});
    if (surface == nullptr) {
        core::Log::warn("TTF_RenderText_Blended failed: " + std::string(SDL_GetError()));
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (texture == nullptr) {
        core::Log::warn("SDL_CreateTextureFromSurface failed: " + std::string(SDL_GetError()));
        SDL_DestroySurface(surface);
        return;
    }

    SDL_FRect destination{
        x,
        y,
        static_cast<float>(surface->w),
        static_cast<float>(surface->h),
    };
    SDL_RenderTexture(renderer_, texture, nullptr, &destination);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

bool Renderer::initializeFont() {
    if (!TTF_Init()) {
        core::Log::warn("SDL_ttf initialization failed: " + std::string(SDL_GetError()));
        return false;
    }
    ttfInitialized_ = true;

    const auto path = fontPath();
    font_ = TTF_OpenFont(path.string().c_str(), defaultFontSize);
    if (font_ == nullptr) {
        core::Log::warn("Game font load failed: " + path.string() + " (" + SDL_GetError() + ")");
        return false;
    }

    core::Log::info("Loaded game font: " + path.string());
    return true;
}

std::filesystem::path Renderer::fontPath() const {
    return assetsDirectory_ / orbitronFontRelativePath;
}

void Renderer::endFrame() {
    SDL_RenderPresent(renderer_);
}

} // namespace arcadeblocks::render
