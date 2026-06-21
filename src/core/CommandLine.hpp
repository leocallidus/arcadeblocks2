#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace arcadeblocks::core {

enum class WindowMode {
    Windowed,
    Fullscreen
};

enum class SmokeScenario {
    None,
    MainMenu,
    Settings,
    SettingsSave,
    SettingsCycle,
    Help,
    HelpCycle,
    Pause,
    PauseSettings,
    PauseHelp
};

struct CommandLineOptions {
    std::filesystem::path executablePath = "ArcadeBlocksII";
    std::optional<std::filesystem::path> assetsDirectoryOverride;
    std::optional<std::filesystem::path> settingsFileOverride;
    int level = 1;
    bool levelSpecified = false;
    WindowMode windowMode = WindowMode::Windowed;
    bool windowModeSpecified = false;
    bool noAudio = false;
    bool debug = false;
    bool perfSummary = false;
    bool showHelp = false;
    bool showVersion = false;
    bool resetSettings = false;
    int smokeFrames = 0;
    SmokeScenario smokeScenario = SmokeScenario::None;
    float uiScale = 1.0f;
    bool uiScaleSpecified = false;
};

struct CommandLineParseResult {
    CommandLineOptions options;
    bool ok = true;
    std::string error;
};

CommandLineParseResult parseCommandLine(int argc, char** argv);
std::string_view toString(WindowMode mode) noexcept;
std::string_view toString(SmokeScenario scenario) noexcept;

} // namespace arcadeblocks::core
