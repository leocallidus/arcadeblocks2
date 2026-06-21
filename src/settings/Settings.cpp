#include "settings/Settings.hpp"

#include <charconv>

namespace arcadeblocks::settings {
namespace {

bool parsePositiveInt(std::string_view value, int& out) noexcept {
    if (value.empty()) {
        return false;
    }

    int parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed <= 0) {
        return false;
    }

    out = parsed;
    return true;
}

} // namespace

GameSettings defaultSettings() {
    return GameSettings{};
}

std::filesystem::path settingsFilePath(const std::filesystem::path& writableDataDirectory) {
    return writableDataDirectory / "settings.json";
}

std::optional<std::pair<int, int>> parseResolutionString(std::string_view resolution) noexcept {
    const auto separator = resolution.find('x');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    int width = 0;
    int height = 0;
    if (!parsePositiveInt(resolution.substr(0, separator), width)) {
        return std::nullopt;
    }
    if (!parsePositiveInt(resolution.substr(separator + 1), height)) {
        return std::nullopt;
    }

    return std::pair<int, int>{width, height};
}

std::string_view toString(Language language) noexcept {
    switch (language) {
    case Language::Russian:
        return "ru";
    case Language::English:
        return "en";
    }
    return "ru";
}

std::string_view toString(VideoWindowMode windowMode) noexcept {
    switch (windowMode) {
    case VideoWindowMode::Windowed:
        return "windowed";
    case VideoWindowMode::Fullscreen:
        return "fullscreen";
    }
    return "windowed";
}

std::string_view toString(Difficulty difficulty) noexcept {
    switch (difficulty) {
    case Difficulty::Easy:
        return "easy";
    case Difficulty::Normal:
        return "normal";
    case Difficulty::Hard:
        return "hard";
    case Difficulty::Hardcore:
        return "hardcore";
    }
    return "normal";
}

std::optional<Language> parseLanguage(std::string_view language) noexcept {
    if (language == "ru") {
        return Language::Russian;
    }
    if (language == "en") {
        return Language::English;
    }
    return std::nullopt;
}

std::optional<VideoWindowMode> parseVideoWindowMode(std::string_view windowMode) noexcept {
    if (windowMode == "windowed") {
        return VideoWindowMode::Windowed;
    }
    if (windowMode == "fullscreen") {
        return VideoWindowMode::Fullscreen;
    }
    return std::nullopt;
}

std::optional<Difficulty> parseDifficulty(std::string_view difficulty) noexcept {
    if (difficulty == "easy") {
        return Difficulty::Easy;
    }
    if (difficulty == "normal") {
        return Difficulty::Normal;
    }
    if (difficulty == "hard") {
        return Difficulty::Hard;
    }
    if (difficulty == "hardcore") {
        return Difficulty::Hardcore;
    }
    return std::nullopt;
}

} // namespace arcadeblocks::settings
