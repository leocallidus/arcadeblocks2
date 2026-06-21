#include "render/SpriteAtlas.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace arcadeblocks::render {

SpriteAtlas SpriteAtlas::loadFromFile(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"Failed to open sprite atlas JSON: " + path.string()};
    }

    nlohmann::json document;
    input >> document;

    SpriteAtlas atlas;
    const auto& frames = document.at("frames");
    if (!frames.is_object()) {
        throw std::runtime_error{"Sprite atlas has no object 'frames': " + path.string()};
    }

    for (const auto& [name, entry] : frames.items()) {
        const auto& frame = entry.at("frame");
        atlas.frames_.emplace(
            name,
            SpriteFrame{
                .x = frame.at("x").get<int>(),
                .y = frame.at("y").get<int>(),
                .w = frame.at("w").get<int>(),
                .h = frame.at("h").get<int>(),
                .rotated = entry.value("rotated", false),
            });
    }

    return atlas;
}

std::optional<SpriteFrame> SpriteAtlas::find(std::string_view name) const {
    const auto found = frames_.find(std::string{name});
    if (found == frames_.end()) {
        return std::nullopt;
    }
    return found->second;
}

bool SpriteAtlas::contains(std::string_view name) const {
    return frames_.contains(std::string{name});
}

std::size_t SpriteAtlas::frameCount() const noexcept {
    return frames_.size();
}

} // namespace arcadeblocks::render
