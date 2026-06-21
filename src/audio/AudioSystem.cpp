#include "audio/AudioSystem.hpp"

#include "core/Log.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <sstream>

namespace arcadeblocks::audio {
namespace {

float clampVolume(float volume) {
    return std::clamp(volume, 0.0f, 1.0f);
}

SDL_PropertiesID playOptions(bool loop) {
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (props != 0) {
        SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0);
    }
    return props;
}

} // namespace

AudioSystem::AudioSystem(AudioSystemConfig config)
    : config_(std::move(config)) {
    config_.masterVolume = clampVolume(config_.masterVolume);
    config_.musicVolume = clampVolume(config_.musicVolume);
    config_.sfxVolume = clampVolume(config_.sfxVolume);
    config_.sfxTrackCount = std::clamp(config_.sfxTrackCount, 1, 64);
}

AudioSystem::~AudioSystem() {
    shutdown();
}

bool AudioSystem::initialize() {
    if (!config_.enabled) {
        core::Log::info("Audio disabled by --no-audio");
        initialized_ = true;
        return true;
    }

    if (!MIX_Init()) {
        core::Log::warn("SDL_mixer initialization failed; audio disabled: " + std::string(SDL_GetError()));
        initialized_ = true;
        return true;
    }
    mixerInitialized_ = true;
    logDecoders();

    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (mixer_ == nullptr) {
        core::Log::warn("Audio device initialization failed; audio disabled: " + std::string(SDL_GetError()));
        initialized_ = true;
        return true;
    }

    musicTrack_ = MIX_CreateTrack(mixer_);
    if (musicTrack_ == nullptr) {
        core::Log::warn("Failed to create music track; audio disabled: " + std::string(SDL_GetError()));
        shutdown();
        initialized_ = true;
        return true;
    }
    MIX_TagTrack(musicTrack_, "music");

    sfxTracks_.reserve(static_cast<std::size_t>(config_.sfxTrackCount));
    for (int i = 0; i < config_.sfxTrackCount; ++i) {
        if (MIX_Track* track = MIX_CreateTrack(mixer_)) {
            MIX_TagTrack(track, "sfx");
            sfxTracks_.push_back(track);
        }
    }

    if (sfxTracks_.empty()) {
        core::Log::warn("Failed to create SFX tracks; SFX playback disabled");
    }

    soundBank_ = std::make_unique<SoundBank>(mixer_, config_.assetsDirectory);
    applyVolumes();

    initialized_ = true;
    core::Log::info(
        "Audio initialized: SDL_mixer "
        + std::to_string(SDL_VERSIONNUM_MAJOR(MIX_Version())) + "."
        + std::to_string(SDL_VERSIONNUM_MINOR(MIX_Version())) + "."
        + std::to_string(SDL_VERSIONNUM_MICRO(MIX_Version()))
        + ", sfxTracks=" + std::to_string(sfxTracks_.size()));
    return true;
}

void AudioSystem::shutdown() {
    if (!initialized_ && !mixerInitialized_) {
        return;
    }

    soundBank_.reset();
    currentMusic_.clear();

    if (musicTrack_ != nullptr) {
        MIX_DestroyTrack(musicTrack_);
        musicTrack_ = nullptr;
    }

    for (auto*& track : sfxTracks_) {
        MIX_DestroyTrack(track);
        track = nullptr;
    }
    sfxTracks_.clear();

    if (mixer_ != nullptr) {
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
    }

    if (mixerInitialized_) {
        MIX_Quit();
        mixerInitialized_ = false;
    }

    initialized_ = false;
}

void AudioSystem::preloadSfx(const std::vector<std::filesystem::path>& sounds) {
    if (!available() || soundBank_ == nullptr) {
        return;
    }
    soundBank_->preload(sounds);
}

void AudioSystem::playMusic(const std::filesystem::path& relativePath, bool loop) {
    if (!available() || musicTrack_ == nullptr) {
        return;
    }

    const auto key = relativePath.generic_string();
    if (currentMusic_ == key && MIX_TrackPlaying(musicTrack_)) {
        return;
    }

    stopMusic();

    musicFadeSecondsRemaining_ = 0.0;
    musicFadeTotalSeconds_ = 0.0;

    const auto absolute = resolve(relativePath);
    SDL_IOStream* stream = SDL_IOFromFile(absolute.string().c_str(), "rb");
    if (stream == nullptr) {
        core::Log::warn("Failed to open music '" + absolute.string() + "': " + SDL_GetError());
        return;
    }

    if (!MIX_SetTrackIOStream(musicTrack_, stream, true)) {
        core::Log::warn("Failed to assign music stream '" + absolute.string() + "': " + SDL_GetError());
        SDL_CloseIO(stream);
        return;
    }

    MIX_SetTrackGain(musicTrack_, config_.musicVolume);
    const SDL_PropertiesID props = playOptions(loop);
    const bool started = MIX_PlayTrack(musicTrack_, props);
    if (props != 0) {
        SDL_DestroyProperties(props);
    }

    if (!started) {
        core::Log::warn("Failed to play music '" + absolute.string() + "': " + SDL_GetError());
        return;
    }

    currentMusic_ = key;
    core::Log::debug("Playing music '" + key + "'");
}

void AudioSystem::stopMusic() {
    if (!available() || musicTrack_ == nullptr) {
        return;
    }

    musicPausedByApp_ = false;
    MIX_StopTrack(musicTrack_, 0);
    currentMusic_ = "";
}

void AudioSystem::playSfx(const std::filesystem::path& relativePath, float pan) {
    if (!available() || soundBank_ == nullptr) {
        return;
    }

    MIX_Audio* loaded = soundBank_->audio(relativePath);
    if (loaded == nullptr) {
        return;
    }

    MIX_Track* track = availableSfxTrack();
    if (track == nullptr) {
        core::Log::debug("No available SFX track for '" + relativePath.generic_string() + "'");
        return;
    }

    if (!MIX_SetTrackAudio(track, loaded)) {
        core::Log::warn("Failed to assign SFX '" + relativePath.generic_string() + "': " + SDL_GetError());
        return;
    }

    const float effectiveVolume = sfxFadeSecondsRemaining_ > 0.0 && sfxFadeToVolume_ < sfxFadeFromVolume_
                                      ? sfxFadeToVolume_
                                      : config_.sfxVolume;
    MIX_SetTrackGain(track, effectiveVolume);
    const float clampedPan = std::clamp(pan, -1.0f, 1.0f);
    const MIX_StereoGains gains{
        .left = clampedPan <= 0.0f ? 1.0f : 1.0f - clampedPan,
        .right = clampedPan >= 0.0f ? 1.0f : 1.0f + clampedPan,
    };
    MIX_SetTrackStereo(track, &gains);

    if (!MIX_PlayTrack(track, 0)) {
        core::Log::warn("Failed to play SFX '" + relativePath.generic_string() + "': " + SDL_GetError());
    }
}

void AudioSystem::playSfxWithPitch(const std::filesystem::path& relativePath, float pan, float pitch) {
    if (!available() || soundBank_ == nullptr) {
        return;
    }

    MIX_Audio* loaded = soundBank_->audio(relativePath);
    if (loaded == nullptr) {
        return;
    }

    MIX_Track* track = availableSfxTrack();
    if (track == nullptr) {
        core::Log::debug("No available SFX track for '" + relativePath.generic_string() + "'");
        return;
    }

    if (!MIX_SetTrackAudio(track, loaded)) {
        core::Log::warn("Failed to assign SFX '" + relativePath.generic_string() + "': " + SDL_GetError());
        return;
    }

    const float effectiveVolume = sfxFadeSecondsRemaining_ > 0.0 && sfxFadeToVolume_ < sfxFadeFromVolume_
                                      ? sfxFadeToVolume_
                                      : config_.sfxVolume;
    MIX_SetTrackGain(track, effectiveVolume);
    const float clampedPan = std::clamp(pan, -1.0f, 1.0f);
    const MIX_StereoGains gains{
        .left = clampedPan <= 0.0f ? 1.0f : 1.0f - clampedPan,
        .right = clampedPan >= 0.0f ? 1.0f : 1.0f + clampedPan,
    };
    MIX_SetTrackStereo(track, &gains);

    MIX_SetTrackFrequencyRatio(track, std::clamp(pitch, 0.01f, 100.0f));

    if (!MIX_PlayTrack(track, 0)) {
        core::Log::warn("Failed to play SFX '" + relativePath.generic_string() + "': " + SDL_GetError());
    }
}

void AudioSystem::fadeSfxOut(double seconds) {
    if (!available()) {
        return;
    }
    const double duration = std::max(0.0, seconds);
    sfxFadeSecondsRemaining_ = duration;
    sfxFadeTotalSeconds_ = duration;
    sfxFadeFromVolume_ = config_.sfxVolume;
    sfxFadeToVolume_ = 0.0f;
    if (duration <= 0.0) {
        for (auto* track : sfxTracks_) {
            if (MIX_TrackPlaying(track)) {
                MIX_StopTrack(track, 0);
            }
        }
    }
}

void AudioSystem::stopAllSfx() {
    if (!available()) {
        return;
    }
    sfxFadeSecondsRemaining_ = 0.0;
    for (auto* track : sfxTracks_) {
        if (MIX_TrackPlaying(track)) {
            MIX_StopTrack(track, 0);
        }
    }
}

double AudioSystem::getSfxDuration(const std::filesystem::path& relativePath) {
    if (!available() || soundBank_ == nullptr) {
        return 0.0;
    }
    MIX_Audio* loaded = soundBank_->audio(relativePath);
    if (loaded == nullptr) {
        return 0.0;
    }
    Sint64 frames = MIX_GetAudioDuration(loaded);
    Sint64 ms = MIX_AudioFramesToMS(loaded, frames);
    return static_cast<double>(ms) / 1000.0;
}


void AudioSystem::pauseAll(double seconds) {
    if (!available()) {
        return;
    }
    const double duration = std::max(0.0, seconds);

    musicFadeSecondsRemaining_ = duration;
    musicFadeTotalSeconds_ = duration;
    musicFadeFromVolume_ = musicTrack_ != nullptr ? config_.musicVolume : 0.0f;
    musicFadeToVolume_ = 0.0f;
    if (musicTrack_ != nullptr) {
        musicPausedByApp_ = true;
        if (duration <= 0.0) {
            MIX_SetTrackGain(musicTrack_, 0.0f);
            if (!MIX_TrackPaused(musicTrack_)) {
                MIX_PauseTrack(musicTrack_);
            }
        }
    }

    sfxFadeSecondsRemaining_ = duration;
    sfxFadeTotalSeconds_ = duration;
    sfxFadeFromVolume_ = config_.sfxVolume;
    sfxFadeToVolume_ = 0.0f;
    for (auto* track : sfxTracks_) {
        if (MIX_TrackPlaying(track)) {
            if (duration <= 0.0) {
                MIX_StopTrack(track, 0);
            } else {
                MIX_SetTrackGain(track, sfxFadeFromVolume_);
            }
        }
    }
}

void AudioSystem::resumeAll(double seconds) {
    if (!available()) {
        return;
    }
    const double duration = std::max(0.0, seconds);

    if (musicTrack_ != nullptr && musicPausedByApp_) {
        if (MIX_TrackPaused(musicTrack_)) {
            MIX_ResumeTrack(musicTrack_);
        }
        musicPausedByApp_ = false;
    }

    musicFadeSecondsRemaining_ = duration;
    musicFadeTotalSeconds_ = duration;
    musicFadeFromVolume_ = 0.0f;
    musicFadeToVolume_ = config_.musicVolume;
    if (musicTrack_ != nullptr) {
        if (duration <= 0.0) {
            MIX_SetTrackGain(musicTrack_, config_.musicVolume);
        } else {
            MIX_SetTrackGain(musicTrack_, 0.0f);
        }
    }
    sfxFadeSecondsRemaining_ = 0.0;
    sfxFadeTotalSeconds_ = 0.0;
}

void AudioSystem::update(double deltaSeconds) {
    if (!available() || deltaSeconds <= 0.0) {
        return;
    }

    if (musicFadeSecondsRemaining_ > 0.0) {
        musicFadeSecondsRemaining_ = std::max(0.0, musicFadeSecondsRemaining_ - deltaSeconds);
        const float t = musicFadeTotalSeconds_ > 0.0
                            ? static_cast<float>(1.0 - musicFadeSecondsRemaining_ / musicFadeTotalSeconds_)
                            : 1.0f;
        const float volume = musicFadeFromVolume_ + (musicFadeToVolume_ - musicFadeFromVolume_) * std::clamp(t, 0.0f, 1.0f);
        if (musicTrack_ != nullptr) {
            MIX_SetTrackGain(musicTrack_, std::max(0.0f, volume));
            if (musicFadeToVolume_ <= 0.0f && musicFadeSecondsRemaining_ <= 0.0 && musicPausedByApp_) {
                if (!MIX_TrackPaused(musicTrack_)) {
                    MIX_PauseTrack(musicTrack_);
                }
            }
        }
    }

    if (sfxFadeSecondsRemaining_ > 0.0) {
        sfxFadeSecondsRemaining_ = std::max(0.0, sfxFadeSecondsRemaining_ - deltaSeconds);
        const float t = sfxFadeTotalSeconds_ > 0.0
                            ? static_cast<float>(1.0 - sfxFadeSecondsRemaining_ / sfxFadeTotalSeconds_)
                            : 1.0f;
        const float volume = sfxFadeFromVolume_ + (sfxFadeToVolume_ - sfxFadeFromVolume_) * std::clamp(t, 0.0f, 1.0f);
        for (auto* track : sfxTracks_) {
            if (MIX_TrackPlaying(track)) {
                MIX_SetTrackGain(track, std::max(0.0f, volume));
                if (sfxFadeToVolume_ <= 0.0f && sfxFadeSecondsRemaining_ <= 0.0) {
                    MIX_StopTrack(track, 0);
                }
            }
        }
    }
}

void AudioSystem::setMasterVolume(float volume) {
    config_.masterVolume = clampVolume(volume);
    applyVolumes();
}

void AudioSystem::setMusicVolume(float volume) {
    config_.musicVolume = clampVolume(volume);
    applyVolumes();
}

void AudioSystem::setSfxVolume(float volume) {
    config_.sfxVolume = clampVolume(volume);
    applyVolumes();
}

bool AudioSystem::available() const noexcept {
    return initialized_ && mixer_ != nullptr;
}

AudioStats AudioSystem::stats() const {
    AudioStats result;
    result.enabled = config_.enabled;
    result.initialized = available();
    result.currentMusic = currentMusic_;

    if (soundBank_) {
        const auto bankStats = soundBank_->stats();
        result.sfxLoaded = bankStats.loadedSounds;
        result.approximateSfxBytes = bankStats.approximateBytes;
    }

    return result;
}

std::filesystem::path AudioSystem::resolve(const std::filesystem::path& relativePath) const {
    if (relativePath.is_absolute()) {
        return relativePath;
    }
    return config_.assetsDirectory / relativePath;
}

MIX_Track* AudioSystem::availableSfxTrack() {
    for (auto* track : sfxTracks_) {
        if (!MIX_TrackPlaying(track)) {
            MIX_SetTrackFrequencyRatio(track, 1.0f);
            return track;
        }
    }
    return nullptr;
}

void AudioSystem::applyVolumes() {
    if (mixer_ == nullptr) {
        return;
    }

    MIX_SetMixerGain(mixer_, config_.masterVolume);
    if (musicTrack_ != nullptr) {
        if (musicPausedByApp_) {
            MIX_SetTrackGain(musicTrack_, 0.0f);
        } else if (musicFadeSecondsRemaining_ <= 0.0) {
            MIX_SetTrackGain(musicTrack_, config_.musicVolume);
        }
    }
    if (sfxFadeSecondsRemaining_ <= 0.0) {
        for (auto* track : sfxTracks_) {
            MIX_SetTrackGain(track, config_.sfxVolume);
        }
    }
}

void AudioSystem::logDecoders() const {
    std::ostringstream output;
    output << "SDL_mixer decoders:";
    const int decoderCount = MIX_GetNumAudioDecoders();
    for (int i = 0; i < decoderCount; ++i) {
        if (const char* decoder = MIX_GetAudioDecoder(i)) {
            output << ' ' << decoder;
        }
    }
    core::Log::info(output.str());
}

} // namespace arcadeblocks::audio
