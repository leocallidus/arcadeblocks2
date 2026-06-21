#pragma once

#include "audio/SoundBank.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct MIX_Mixer;
struct MIX_Track;

namespace arcadeblocks::audio {

struct AudioSystemConfig {
    std::filesystem::path assetsDirectory;
    bool enabled = true;
    float masterVolume = 0.85f;
    float musicVolume = 0.58f;
    float sfxVolume = 0.82f;
    int sfxTrackCount = 32;
};

struct AudioStats {
    bool enabled = false;
    bool initialized = false;
    int sfxLoaded = 0;
    std::uintmax_t approximateSfxBytes = 0;
    std::string currentMusic;
};

class AudioSystem {
public:
    explicit AudioSystem(AudioSystemConfig config);
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    [[nodiscard]] bool initialize();
    void shutdown();

    void preloadSfx(const std::vector<std::filesystem::path>& sounds);
    void playMusic(const std::filesystem::path& relativePath, bool loop);
    void stopMusic();
    void playSfx(const std::filesystem::path& relativePath, float pan = 0.0f);
    void playSfxWithPitch(const std::filesystem::path& relativePath, float pan = 0.0f, float pitch = 1.0f);
    void fadeSfxOut(double seconds);
    void stopAllSfx();
    [[nodiscard]] double getSfxDuration(const std::filesystem::path& relativePath);

    void pauseAll(double seconds);
    void resumeAll(double seconds);
    void update(double deltaSeconds);

    void setMasterVolume(float volume);
    void setMusicVolume(float volume);
    void setSfxVolume(float volume);

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] AudioStats stats() const;

private:
    [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& relativePath) const;
    [[nodiscard]] MIX_Track* availableSfxTrack();
    void applyVolumes();
    void logDecoders() const;

    AudioSystemConfig config_;
    MIX_Mixer* mixer_ = nullptr;
    MIX_Track* musicTrack_ = nullptr;
    std::vector<MIX_Track*> sfxTracks_;
    std::unique_ptr<SoundBank> soundBank_;
    std::string currentMusic_;
    bool initialized_ = false;
    bool mixerInitialized_ = false;
    bool musicPausedByApp_ = false;
    double musicFadeSecondsRemaining_ = 0.0;
    double musicFadeTotalSeconds_ = 0.0;
    float musicFadeFromVolume_ = 0.0f;
    float musicFadeToVolume_ = 0.0f;
    double sfxFadeSecondsRemaining_ = 0.0;
    double sfxFadeTotalSeconds_ = 0.0;
    float sfxFadeFromVolume_ = 0.0f;
    float sfxFadeToVolume_ = 0.0f;
};

} // namespace arcadeblocks::audio
