#include "audio/SoundBank.hpp"

#include "core/Log.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <system_error>

namespace arcadeblocks::audio {

SoundBank::SoundBank(MIX_Mixer* mixer, std::filesystem::path assetsDirectory)
    : mixer_(mixer),
      assetsDirectory_(std::move(assetsDirectory)) {}

SoundBank::~SoundBank() {
    clear();
}

MIX_Audio* SoundBank::audio(const std::filesystem::path& relativePath) {
    const auto key = keyFor(relativePath);
    if (const auto found = sounds_.find(key); found != sounds_.end()) {
        return found->second.audio;
    }

    const auto path = resolve(relativePath);
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        core::Log::warn("Audio asset size unavailable for '" + path.string() + "': " + error.message());
    }

    MIX_Audio* loaded = MIX_LoadAudio(mixer_, path.string().c_str(), false);
    if (loaded == nullptr) {
        core::Log::warn("Failed to load SFX '" + path.string() + "': " + SDL_GetError());
        return nullptr;
    }

    sounds_.emplace(key, Entry{.audio = loaded, .approximateBytes = error ? 0U : size});
    core::Log::debug("Loaded SFX '" + key + "'");
    return loaded;
}

void SoundBank::preload(const std::vector<std::filesystem::path>& sounds) {
    for (const auto& sound : sounds) {
        (void)audio(sound);
    }
}

void SoundBank::clear() {
    for (auto& [_, entry] : sounds_) {
        MIX_DestroyAudio(entry.audio);
        entry.audio = nullptr;
    }
    sounds_.clear();
}

SoundBankStats SoundBank::stats() const noexcept {
    SoundBankStats result;
    result.loadedSounds = static_cast<int>(sounds_.size());
    for (const auto& [_, entry] : sounds_) {
        result.approximateBytes += entry.approximateBytes;
    }
    return result;
}

std::filesystem::path SoundBank::resolve(const std::filesystem::path& relativePath) const {
    if (relativePath.is_absolute()) {
        return relativePath;
    }
    return assetsDirectory_ / relativePath;
}

std::string SoundBank::keyFor(const std::filesystem::path& relativePath) const {
    return relativePath.generic_string();
}

} // namespace arcadeblocks::audio
