#include "core/CommandLine.hpp"

#include <charconv>
#include <cmath>
#include <string_view>

namespace arcadeblocks::core {
namespace {

bool startsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool parsePositiveInt(std::string_view value, int& out) {
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

bool parseScale(std::string_view value, float& out) {
    if (value.empty()) {
        return false;
    }

    float parsed = 0.0f;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(parsed) || parsed < 0.75f || parsed > 2.0f) {
        return false;
    }

    out = parsed;
    return true;
}

std::optional<SmokeScenario> parseSmokeScenario(std::string_view value) {
    if (value == "main-menu") {
        return SmokeScenario::MainMenu;
    }
    if (value == "settings") {
        return SmokeScenario::Settings;
    }
    if (value == "settings-save") {
        return SmokeScenario::SettingsSave;
    }
    if (value == "settings-cycle") {
        return SmokeScenario::SettingsCycle;
    }
    if (value == "help") {
        return SmokeScenario::Help;
    }
    if (value == "help-cycle") {
        return SmokeScenario::HelpCycle;
    }
    if (value == "pause") {
        return SmokeScenario::Pause;
    }
    if (value == "pause-settings") {
        return SmokeScenario::PauseSettings;
    }
    if (value == "pause-help") {
        return SmokeScenario::PauseHelp;
    }
    return std::nullopt;
}

CommandLineParseResult fail(CommandLineOptions options, std::string error) {
    CommandLineParseResult result;
    result.options = std::move(options);
    result.ok = false;
    result.error = std::move(error);
    return result;
}

} // namespace

CommandLineParseResult parseCommandLine(int argc, char** argv) {
    CommandLineOptions options;
    if (argc > 0 && argv[0] != nullptr) {
        options.executablePath = argv[0];
    }

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index] != nullptr ? argv[index] : "";

        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            continue;
        }

        if (argument == "--version") {
            options.showVersion = true;
            continue;
        }

        if (argument == "--reset-settings") {
            options.resetSettings = true;
            continue;
        }

        if (argument == "--open-settings-smoke") {
            options.smokeScenario = SmokeScenario::Settings;
            continue;
        }

        if (argument == "--open-help-smoke") {
            options.smokeScenario = SmokeScenario::Help;
            continue;
        }

        if (argument == "--windowed") {
            options.windowMode = WindowMode::Windowed;
            options.windowModeSpecified = true;
            continue;
        }

        if (argument == "--fullscreen") {
            options.windowMode = WindowMode::Fullscreen;
            options.windowModeSpecified = true;
            continue;
        }

        if (argument == "--no-audio") {
            options.noAudio = true;
            continue;
        }

        if (argument == "--debug") {
            options.debug = true;
            continue;
        }

        if (argument == "--perf-summary") {
            options.perfSummary = true;
            continue;
        }

        if (argument == "--assets-dir") {
            if (index + 1 >= argc || argv[index + 1] == nullptr) {
                return fail(std::move(options), "--assets-dir requires a path argument");
            }
            options.assetsDirectoryOverride = argv[++index];
            continue;
        }

        if (startsWith(argument, "--assets-dir=")) {
            const auto value = argument.substr(std::string_view{"--assets-dir="}.size());
            if (value.empty()) {
                return fail(std::move(options), "--assets-dir requires a non-empty path");
            }
            options.assetsDirectoryOverride = std::string{value};
            continue;
        }

        if (argument == "--settings-file") {
            if (index + 1 >= argc || argv[index + 1] == nullptr || argv[index + 1][0] == '\0') {
                return fail(std::move(options), "--settings-file requires a non-empty path");
            }
            options.settingsFileOverride = argv[++index];
            continue;
        }

        if (startsWith(argument, "--settings-file=")) {
            const auto value = argument.substr(std::string_view{"--settings-file="}.size());
            if (value.empty()) {
                return fail(std::move(options), "--settings-file requires a non-empty path");
            }
            options.settingsFileOverride = std::string{value};
            continue;
        }

        if (argument == "--level") {
            if (index + 1 >= argc || argv[index + 1] == nullptr) {
                return fail(std::move(options), "--level requires a positive integer argument");
            }
            int parsed = 0;
            if (!parsePositiveInt(argv[++index], parsed)) {
                return fail(std::move(options), "--level requires a positive integer argument");
            }
            options.level = parsed;
            options.levelSpecified = true;
            continue;
        }

        if (startsWith(argument, "--level=")) {
            const auto value = argument.substr(std::string_view{"--level="}.size());
            int parsed = 0;
            if (!parsePositiveInt(value, parsed)) {
                return fail(std::move(options), "--level requires a positive integer argument");
            }
            options.level = parsed;
            options.levelSpecified = true;
            continue;
        }

        if (argument == "--smoke-frames") {
            if (index + 1 >= argc || argv[index + 1] == nullptr) {
                return fail(std::move(options), "--smoke-frames requires a positive integer argument");
            }
            int parsed = 0;
            if (!parsePositiveInt(argv[++index], parsed)) {
                return fail(std::move(options), "--smoke-frames requires a positive integer argument");
            }
            options.smokeFrames = parsed;
            continue;
        }

        if (startsWith(argument, "--smoke-frames=")) {
            const auto value = argument.substr(std::string_view{"--smoke-frames="}.size());
            int parsed = 0;
            if (!parsePositiveInt(value, parsed)) {
                return fail(std::move(options), "--smoke-frames requires a positive integer argument");
            }
            options.smokeFrames = parsed;
            continue;
        }

        if (argument == "--smoke-scenario") {
            if (index + 1 >= argc || argv[index + 1] == nullptr) {
                return fail(std::move(options), "--smoke-scenario requires a scenario name");
            }
            const auto scenario = parseSmokeScenario(argv[++index]);
            if (!scenario) {
                return fail(std::move(options), "Unknown smoke scenario: " + std::string{argv[index]});
            }
            options.smokeScenario = *scenario;
            continue;
        }

        if (startsWith(argument, "--smoke-scenario=")) {
            const auto value = argument.substr(std::string_view{"--smoke-scenario="}.size());
            const auto scenario = parseSmokeScenario(value);
            if (!scenario) {
                return fail(std::move(options), "Unknown smoke scenario: " + std::string{value});
            }
            options.smokeScenario = *scenario;
            continue;
        }

        if (argument == "--ui-scale") {
            if (index + 1 >= argc || argv[index + 1] == nullptr) {
                return fail(std::move(options), "--ui-scale requires a number from 0.75 to 2.0");
            }
            float parsed = 0.0f;
            if (!parseScale(argv[++index], parsed)) {
                return fail(std::move(options), "--ui-scale requires a number from 0.75 to 2.0");
            }
            options.uiScale = parsed;
            options.uiScaleSpecified = true;
            continue;
        }

        if (startsWith(argument, "--ui-scale=")) {
            const auto value = argument.substr(std::string_view{"--ui-scale="}.size());
            float parsed = 0.0f;
            if (!parseScale(value, parsed)) {
                return fail(std::move(options), "--ui-scale requires a number from 0.75 to 2.0");
            }
            options.uiScale = parsed;
            options.uiScaleSpecified = true;
            continue;
        }

        return fail(std::move(options), "Unknown argument: " + std::string{argument});
    }

    if (options.smokeScenario != SmokeScenario::None && options.smokeFrames == 0) {
        options.smokeFrames = 3;
    }

    CommandLineParseResult result;
    result.options = std::move(options);
    return result;
}

std::string_view toString(WindowMode mode) noexcept {
    switch (mode) {
    case WindowMode::Windowed:
        return "windowed";
    case WindowMode::Fullscreen:
        return "fullscreen";
    }
    return "unknown";
}

std::string_view toString(SmokeScenario scenario) noexcept {
    switch (scenario) {
    case SmokeScenario::None:
        return "none";
    case SmokeScenario::MainMenu:
        return "main-menu";
    case SmokeScenario::Settings:
        return "settings";
    case SmokeScenario::SettingsSave:
        return "settings-save";
    case SmokeScenario::SettingsCycle:
        return "settings-cycle";
    case SmokeScenario::Help:
        return "help";
    case SmokeScenario::HelpCycle:
        return "help-cycle";
    case SmokeScenario::Pause:
        return "pause";
    case SmokeScenario::PauseSettings:
        return "pause-settings";
    case SmokeScenario::PauseHelp:
        return "pause-help";
    }
    return "unknown";
}

} // namespace arcadeblocks::core
