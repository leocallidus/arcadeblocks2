#include "render/AssetManager.hpp"

#include "core/Log.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <sstream>
#include <stdexcept>

namespace arcadeblocks::render {

namespace {

std::string formatBytes(std::uint64_t bytes) {
    std::ostringstream output;
    output.setf(std::ios::fixed);
    output.precision(2);
    output << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MiB";
    return output.str();
}

} // namespace

AssetManager::AssetManager(SDL_Renderer* renderer, std::filesystem::path assetsDirectory)
    : renderer_(renderer),
      assetsDirectory_(std::move(assetsDirectory)) {}

Texture* AssetManager::texture(std::string_view relativePath) {
    ++textureRequests_;
    const auto key = normalizedKey(relativePath);
    if (const auto cached = textureCache_.find(key); cached != textureCache_.end()) {
        ++cacheHits_;
        return cached->second.get();
    }
    if (failedTextures_.contains(key)) {
        ++cacheHits_;
        return nullptr;
    }

    ++loadAttempts_;
    const auto fullPath = resolve(relativePath);
    SDL_Texture* native = IMG_LoadTexture(renderer_, fullPath.string().c_str());
    if (native == nullptr) {
        ++failedLoads_;
        failedTextures_.insert(key);
        core::Log::warn("Texture load failed: " + fullPath.string() + " (" + SDL_GetError() + ")");
        return nullptr;
    }

    float width = 0.0f;
    float height = 0.0f;
    if (!SDL_GetTextureSize(native, &width, &height)) {
        ++failedLoads_;
        SDL_DestroyTexture(native);
        failedTextures_.insert(key);
        core::Log::warn("Texture size query failed: " + fullPath.string() + " (" + SDL_GetError() + ")");
        return nullptr;
    }

    auto texture = std::make_unique<Texture>(native, static_cast<int>(width), static_cast<int>(height), fullPath);
    auto* result = texture.get();
    core::Log::info(
        "Loaded texture '" + key + "' "
        + std::to_string(result->width()) + "x" + std::to_string(result->height())
        + ", approx " + formatBytes(result->approximateBytes()));
    textureCache_.emplace(key, std::move(texture));
    return result;
}

Texture* AssetManager::spriteAtlasTexture() {
    return texture("sprites/sprite_atlas.png");
}

Texture* AssetManager::spriteAtlasTextureIfLoaded() {
    const auto key = normalizedKey("sprites/sprite_atlas.png");
    if (const auto cached = textureCache_.find(key); cached != textureCache_.end()) {
        return cached->second.get();
    }

    return nullptr;
}

const SpriteAtlas* AssetManager::spriteAtlas() {
    if (spriteAtlasLoadFailed_) {
        return nullptr;
    }

    if (!spriteAtlas_) {
        const auto path = resolve("sprites/sprite_atlas.json");
        try {
            spriteAtlas_ = SpriteAtlas::loadFromFile(path);
            core::Log::info("Loaded sprite atlas metadata: " + std::to_string(spriteAtlas_->frameCount()) + " frames");
        } catch (const std::exception& error) {
            spriteAtlasLoadFailed_ = true;
            core::Log::warn(error.what());
            return nullptr;
        }
    }
    return &*spriteAtlas_;
}

AssetStats AssetManager::stats() const {
    AssetStats result{};
    result.loadedTextures = textureCache_.size();
    result.textureRequests = textureRequests_;
    result.cacheHits = cacheHits_;
    result.loadAttempts = loadAttempts_;
    result.failedLoads = failedLoads_;
    for (const auto& [_, texture] : textureCache_) {
        result.approximateTextureBytes += texture->approximateBytes();
    }
    return result;
}

void AssetManager::releaseUnused() {
    core::Log::debug("AssetManager releaseUnused: retained " + std::to_string(textureCache_.size()) + " cached textures");
}

std::filesystem::path AssetManager::resolve(std::string_view relativePath) const {
    return assetsDirectory_ / std::filesystem::path{relativePath};
}

std::string AssetManager::normalizedKey(std::string_view relativePath) const {
    return std::filesystem::path{relativePath}.generic_string();
}

} // namespace arcadeblocks::render
