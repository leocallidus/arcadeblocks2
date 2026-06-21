#pragma once

#include <cstdint>
#include <filesystem>

struct SDL_Texture;

namespace arcadeblocks::render {

class Texture {
public:
    Texture() = default;
    Texture(SDL_Texture* texture, int width, int height, std::filesystem::path path);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] SDL_Texture* native() const noexcept;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] std::uint64_t approximateBytes() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

private:
    void reset() noexcept;

    SDL_Texture* texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    std::filesystem::path path_;
};

} // namespace arcadeblocks::render
