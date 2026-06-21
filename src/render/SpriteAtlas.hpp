#pragma once

#include "render/Renderer.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace arcadeblocks::render {

class SpriteAtlas {
public:
    static SpriteAtlas loadFromFile(const std::filesystem::path& path);

    [[nodiscard]] std::optional<SpriteFrame> find(std::string_view name) const;
    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] std::size_t frameCount() const noexcept;

private:
    std::unordered_map<std::string, SpriteFrame> frames_;
};

} // namespace arcadeblocks::render
