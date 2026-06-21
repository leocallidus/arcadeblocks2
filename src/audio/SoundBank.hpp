#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct MIX_Audio;
struct MIX_Mixer;

namespace arcadeblocks::audio {

struct SoundBankStats {
    int loadedSounds = 0;
    std::uintmax_t approximateBytes = 0;
};

class SoundBank {
public:
    SoundBank(MIX_Mixer* mixer, std::filesystem::path assetsDirectory);
    ~SoundBank();

    SoundBank(const SoundBank&) = delete;
    SoundBank& operator=(const SoundBank&) = delete;

    [[nodiscard]] MIX_Audio* audio(const std::filesystem::path& relativePath);
    void preload(const std::vector<std::filesystem::path>& sounds);
    void clear();

    [[nodiscard]] SoundBankStats stats() const noexcept;

private:
    struct Entry {
        MIX_Audio* audio = nullptr;
        std::uintmax_t approximateBytes = 0;
    };

    [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& relativePath) const;
    [[nodiscard]] std::string keyFor(const std::filesystem::path& relativePath) const;

    MIX_Mixer* mixer_ = nullptr;
    std::filesystem::path assetsDirectory_;
    std::unordered_map<std::string, Entry> sounds_;
};

} // namespace arcadeblocks::audio
