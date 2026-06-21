#pragma once

#include "render/SpriteAtlas.hpp"
#include "render/Texture.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct SDL_Renderer;

namespace arcadeblocks::render {

struct AssetStats {
    std::size_t loadedTextures = 0;
    std::uint64_t approximateTextureBytes = 0;
    std::uint64_t textureRequests = 0;
    std::uint64_t cacheHits = 0;
    std::uint64_t loadAttempts = 0;
    std::uint64_t failedLoads = 0;
};

class AssetManager {
public:
    AssetManager(SDL_Renderer* renderer, std::filesystem::path assetsDirectory);

    [[nodiscard]] Texture* texture(std::string_view relativePath);
    [[nodiscard]] Texture* spriteAtlasTexture();
    [[nodiscard]] Texture* spriteAtlasTextureIfLoaded();
    [[nodiscard]] const SpriteAtlas* spriteAtlas();
    [[nodiscard]] AssetStats stats() const;

    void releaseUnused();

private:
    [[nodiscard]] std::filesystem::path resolve(std::string_view relativePath) const;
    [[nodiscard]] std::string normalizedKey(std::string_view relativePath) const;

    SDL_Renderer* renderer_ = nullptr;
    std::filesystem::path assetsDirectory_;
    std::unordered_map<std::string, std::unique_ptr<Texture>> textureCache_;
    std::unordered_set<std::string> failedTextures_;
    std::optional<SpriteAtlas> spriteAtlas_;
    bool spriteAtlasLoadFailed_ = false;
    std::uint64_t textureRequests_ = 0;
    std::uint64_t cacheHits_ = 0;
    std::uint64_t loadAttempts_ = 0;
    std::uint64_t failedLoads_ = 0;
};

} // namespace arcadeblocks::render
