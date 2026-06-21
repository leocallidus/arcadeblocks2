#pragma once

#include "settings/KeyBinding.hpp"

#include <filesystem>
#include <optional>
#include <utility>
#include <string>
#include <string_view>

namespace arcadeblocks::settings {

enum class Language {
    Russian,
    English
};

enum class VideoWindowMode {
    Windowed,
    Fullscreen
};

enum class Difficulty {
    Easy,
    Normal,
    Hard,
    Hardcore
};

struct AudioSettings {
    double masterVolume = 0.85;
    double musicVolume = 0.58;
    double sfxVolume = 0.82;
    bool callBallSound = true;
};

struct VideoSettings {
    VideoWindowMode windowMode = VideoWindowMode::Windowed;
    std::string resolution = "1280x720";
    bool vsync = true;
    float uiScale = 1.0f;
    bool showLevelBackground = true;
    int fpsLimit = 0; ///< 0 = unlimited. Active when vsync is off.
};

struct GameplaySettings {
    std::string playerName = "Player";
    float paddleSpeed = 760.0f;
    float turboSpeed = 2000.0f;
    Difficulty difficulty = Difficulty::Normal;
    bool showLaunchTrajectory = true;
};

struct ControlSettings {
    KeyBinding moveLeft{"Left"};
    KeyBinding moveRight{"Right"};
    KeyBinding launch{"Space"};
    KeyBinding callBall{"B"};
    KeyBinding turbo{"X"};
    KeyBinding turboBall{"V"};
    KeyBinding plasma{"Z"};
    KeyBinding pause{"Escape"};
};

struct GameSettings {
    int version = 1;
    Language language = Language::Russian;
    AudioSettings audio;
    VideoSettings video;
    GameplaySettings gameplay;
    ControlSettings controls;
};

[[nodiscard]] GameSettings defaultSettings();
[[nodiscard]] std::filesystem::path settingsFilePath(const std::filesystem::path& writableDataDirectory);
[[nodiscard]] std::optional<std::pair<int, int>> parseResolutionString(std::string_view resolution) noexcept;

[[nodiscard]] std::string_view toString(Language language) noexcept;
[[nodiscard]] std::string_view toString(VideoWindowMode windowMode) noexcept;
[[nodiscard]] std::string_view toString(Difficulty difficulty) noexcept;

[[nodiscard]] std::optional<Language> parseLanguage(std::string_view language) noexcept;
[[nodiscard]] std::optional<VideoWindowMode> parseVideoWindowMode(std::string_view windowMode) noexcept;
[[nodiscard]] std::optional<Difficulty> parseDifficulty(std::string_view difficulty) noexcept;

} // namespace arcadeblocks::settings
