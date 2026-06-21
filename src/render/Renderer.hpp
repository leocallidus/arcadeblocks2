#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

struct SDL_Renderer;
struct SDL_Texture;
struct TTF_Font;

namespace arcadeblocks::render {

class Texture;

struct Color {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 255;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct SpriteFrame {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool rotated = false;
};

class Renderer {
public:
    Renderer(SDL_Renderer* renderer, std::filesystem::path assetsDirectory);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void beginFrame(Color clearColor);
    void drawTexture(const Texture& texture, Rect destination);
    void drawSprite(const Texture& atlasTexture, const SpriteFrame& frame, Rect destination, unsigned char alpha = 255);
    void drawRect(Rect rect, Color color);
    void drawLine(float x1, float y1, float x2, float y2, Color color);
    void drawText(float x, float y, std::string_view text, Color color = Color{255, 255, 255, 255});
    void endFrame();

private:
    [[nodiscard]] bool initializeFont();
    [[nodiscard]] std::filesystem::path fontPath() const;

    SDL_Renderer* renderer_ = nullptr;
    std::filesystem::path assetsDirectory_;
    TTF_Font* font_ = nullptr;
    bool ttfInitialized_ = false;
};

} // namespace arcadeblocks::render
