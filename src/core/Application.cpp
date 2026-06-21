#include "core/Application.hpp"

#include "core/Log.hpp"
#include "core/Version.hpp"
#include "platform/SdlRuntime.hpp"
#include "settings/SettingsStore.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>

namespace arcadeblocks::core {
namespace {

core::WindowMode toRuntimeWindowMode(settings::VideoWindowMode mode) noexcept {
    switch (mode) {
    case settings::VideoWindowMode::Windowed:
        return core::WindowMode::Windowed;
    case settings::VideoWindowMode::Fullscreen:
        return core::WindowMode::Fullscreen;
    }
    return core::WindowMode::Windowed;
}

std::pair<int, int> runtimeWindowSize(const settings::GameSettings& settings) {
    if (const auto parsed = settings::parseResolutionString(settings.video.resolution)) {
        return *parsed;
    }
    return {1280, 720};
}

std::filesystem::path runtimeSettingsPath(
    const CommandLineOptions& options,
    const platform::ResolvedPaths& paths) {
    if (options.settingsFileOverride) {
        return *options.settingsFileOverride;
    }
    return settings::settingsFilePath(paths.writableDataDirectory);
}

} // namespace

Application::Application(CommandLineOptions options)
    : options_(std::move(options)),
      settings_(settings::defaultSettings()) {}

int Application::run() {
    if (options_.showHelp) {
        printUsage();
        return 0;
    }

    if (options_.showVersion) {
        printVersion();
        return 0;
    }

    if (!initialize()) {
        shutdown();
        return 1;
    }

    const auto [windowWidth, windowHeight] = runtimeWindowSize(settings_);
    platform::SdlRuntime runtime{platform::SdlRuntimeConfig{
        .assetsDirectory = paths_.assetsDirectory,
        .settingsFilePath = runtimeSettingsPath(options_, paths_),
        .localization = localization_,
        .windowMode = options_.windowMode,
        .settings = settings_,
        .level = options_.level,
        .startInLevel = options_.levelSpecified,
        .noAudio = options_.noAudio,
        .debug = options_.debug,
        .perfSummary = options_.perfSummary,
        .uiScale = options_.uiScale,
        .smokeFrames = options_.smokeFrames,
        .smokeScenario = options_.smokeScenario,
        .windowWidth = windowWidth,
        .windowHeight = windowHeight,
    }};

    if (!runtime.initialize()) {
        shutdown();
        return 1;
    }

    const int result = runtime.run();

    shutdown();
    return result;
}

bool Application::initialize() {
    auto resolved = platform::resolvePaths(options_.executablePath, options_.assetsDirectoryOverride);
    if (!resolved) {
        std::cerr << resolved.error << '\n';
        return false;
    }

    paths_ = std::move(resolved.paths);

    std::error_code error;
    std::filesystem::create_directories(paths_.writableDataDirectory, error);
    if (error) {
        std::cerr << "Failed to create writable data directory '" << paths_.writableDataDirectory.string()
                  << "': " << error.message() << '\n';
        return false;
    }

    std::filesystem::create_directories(paths_.logDirectory, error);
    if (error) {
        std::cerr << "Failed to create log directory '" << paths_.logDirectory.string()
                  << "': " << error.message() << '\n';
        return false;
    }

    const auto logFile = paths_.logDirectory / "arcadeblocks2.log";
    if (!Log::initialize(logFile, options_.debug ? LogLevel::Trace : LogLevel::Info)) {
        std::cerr << "Failed to open log file '" << logFile.string() << "'\n";
        return false;
    }

    initialized_ = true;

    Log::info(std::string(productName()) + " " + std::string(version()) + " starting");
    Log::info(std::string("SDL target version: ") + std::string(sdlTargetVersion()));
    Log::info("Executable path: " + paths_.executablePath.string());
    Log::info("Assets path: " + paths_.assetsDirectory.string());
    Log::info("Writable data path: " + paths_.writableDataDirectory.string());
    Log::info("Log file: " + logFile.string());

    localization_ = std::make_shared<localization::Localization>();
    localization_->loadFromDirectory(paths_.assetsDirectory / "localization");

    settings::SettingsStore settingsStore{runtimeSettingsPath(options_, paths_)};
    if (options_.resetSettings) {
        const auto resetResult = settingsStore.resetToDefaults();
        if (!resetResult.ok) {
            Log::error(resetResult.error);
            return false;
        }
        Log::info("Settings reset to defaults: " + settingsStore.path().string());
    }

    auto settingsResult = settingsStore.load();
    if (!settingsResult.ok) {
        Log::error(settingsResult.error);
        return false;
    }

    settings_ = std::move(settingsResult.settings);
    for (const auto& diagnostic : settingsResult.diagnostics) {
        if (diagnostic.severity == settings::SettingsDiagnosticSeverity::Error) {
            Log::error("Settings: " + diagnostic.message);
        } else {
            Log::warn("Settings: " + diagnostic.message);
        }
    }

    if (!options_.windowModeSpecified) {
        options_.windowMode = toRuntimeWindowMode(settings_.video.windowMode);
    }
    if (!options_.uiScaleSpecified) {
        options_.uiScale = settings_.video.uiScale;
    }

    std::ostringstream settingsSummary;
    settingsSummary << "Settings: path=" << settingsStore.path().string()
                    << ", loadedFromDisk=" << (settingsResult.loadedFromDisk ? "true" : "false")
                    << ", wroteDefaults=" << (settingsResult.wroteDefaults ? "true" : "false")
                    << ", backedUpCorruptFile=" << (settingsResult.backedUpCorruptFile ? "true" : "false")
                    << ", language=" << settings::toString(settings_.language)
                    << ", windowMode=" << settings::toString(settings_.video.windowMode)
                    << ", uiScale=" << settings_.video.uiScale;
    Log::info(settingsSummary.str());

    std::ostringstream options;
    options << "CLI: level=" << options_.level
            << ", levelSpecified=" << (options_.levelSpecified ? "true" : "false")
            << ", windowMode=" << toString(options_.windowMode)
            << ", audio=" << (options_.noAudio ? "disabled" : "enabled")
            << ", debug=" << (options_.debug ? "true" : "false")
            << ", perfSummary=" << (options_.perfSummary ? "true" : "false")
            << ", uiScale=" << options_.uiScale
            << ", smokeFrames=" << options_.smokeFrames
            << ", smokeScenario=" << toString(options_.smokeScenario);
    Log::info(options.str());

    return true;
}

void Application::shutdown() {
    if (!initialized_) {
        return;
    }

    Log::info("Application shutdown complete");
    Log::shutdown();
    initialized_ = false;
}

void Application::printUsage() const {
    std::cout
        << productName() << " " << version() << '\n'
        << "Usage: " << options_.executablePath.filename().string() << " [options]\n\n"
        << "Options:\n"
        << "  --help, -h              Show this help text.\n"
        << "  --version               Print version information.\n"
        << "  --assets-dir <path>     Override assets directory.\n"
        << "  --settings-file <path>  Override settings.json path; intended for tests and portable runs.\n"
        << "  --level <number>        Select level for the future vertical slice.\n"
        << "  --windowed              Request windowed mode.\n"
        << "  --fullscreen            Request fullscreen mode.\n"
        << "  --reset-settings        Rewrite settings.json with defaults before launch.\n"
        << "  --no-audio              Disable audio initialization.\n"
        << "  --debug                 Enable verbose logging.\n"
        << "  --perf-summary          Log one machine-readable performance summary before exit.\n"
        << "  --ui-scale <number>     Set Dear ImGui UI scale from 0.75 to 2.0.\n"
        << "  --smoke-frames <n>      Run n frames and exit; intended for automated tests.\n"
        << "  --smoke-scenario <name> Open main-menu, settings, settings-save, settings-cycle,\n"
        << "                           help, help-cycle, pause, pause-settings, or pause-help.\n"
        << "  --open-settings-smoke   Alias for --smoke-scenario settings.\n"
        << "  --open-help-smoke       Alias for --smoke-scenario help.\n";
}

void Application::printVersion() const {
    std::cout << productName() << " " << version() << '\n';
}

} // namespace arcadeblocks::core
