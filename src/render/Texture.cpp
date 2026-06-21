#include "render/Texture.hpp"

#include <SDL3/SDL.h>

#include <utility>

namespace arcadeblocks::render {

Texture::Texture(SDL_Texture* texture, int width, int height, std::filesystem::path path)
    : texture_(texture),
      width_(width),
      height_(height),
      path_(std::move(path)) {}

Texture::~Texture() {
    reset();
}

Texture::Texture(Texture&& other) noexcept
    : texture_(std::exchange(other.texture_, nullptr)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)),
      path_(std::move(other.path_)) {}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        reset();
        texture_ = std::exchange(other.texture_, nullptr);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
        path_ = std::move(other.path_);
    }
    return *this;
}

SDL_Texture* Texture::native() const noexcept {
    return texture_;
}

int Texture::width() const noexcept {
    return width_;
}

int Texture::height() const noexcept {
    return height_;
}

std::uint64_t Texture::approximateBytes() const noexcept {
    return static_cast<std::uint64_t>(width_) * static_cast<std::uint64_t>(height_) * 4ULL;
}

const std::filesystem::path& Texture::path() const noexcept {
    return path_;
}

Texture::operator bool() const noexcept {
    return texture_ != nullptr;
}

void Texture::reset() noexcept {
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    width_ = 0;
    height_ = 0;
}

} // namespace arcadeblocks::render
