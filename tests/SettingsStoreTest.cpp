#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "settings/Settings.hpp"
#include "settings/SettingsStore.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

class ScopedTempDirectory {
public:
    ScopedTempDirectory() {
        path_ = std::filesystem::temp_directory_path() / ("arcadeblocks2-settings-test-" + std::to_string(counter_++));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~ScopedTempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    inline static int counter_ = 0;
    std::filesystem::path path_;
};

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input{path};
    REQUIRE(input.good());
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

} // namespace

TEST_CASE("settings store creates defaults on first load", "[settings]") {
    ScopedTempDirectory tempDirectory;
    const auto settingsPath = arcadeblocks::settings::settingsFilePath(tempDirectory.path());
    arcadeblocks::settings::SettingsStore store{settingsPath};

    const auto result = store.load();
    REQUIRE(result.ok);
    REQUIRE(!result.loadedFromDisk);
    REQUIRE(result.wroteDefaults);
    REQUIRE(std::filesystem::exists(settingsPath));
    REQUIRE(result.settings.version == 1);
    REQUIRE(result.settings.video.uiScale == Catch::Approx(1.0f));
    REQUIRE(result.settings.gameplay.paddleSpeed == Catch::Approx(760.0f));
    REQUIRE(result.settings.controls.moveLeft.keyName == "Left");
}

TEST_CASE("settings store persists and reloads valid values", "[settings]") {
    ScopedTempDirectory tempDirectory;
    const auto settingsPath = arcadeblocks::settings::settingsFilePath(tempDirectory.path());
    arcadeblocks::settings::SettingsStore store{settingsPath};

    auto settings = arcadeblocks::settings::defaultSettings();
    settings.language = arcadeblocks::settings::Language::English;
    settings.audio.masterVolume = 0.4;
    settings.video.windowMode = arcadeblocks::settings::VideoWindowMode::Fullscreen;
    settings.video.uiScale = 1.25f;
    settings.gameplay.playerName = "Verifier";
    settings.gameplay.showLaunchTrajectory = false;
    settings.controls.pause.keyName = "P";

    const auto saveResult = store.save(settings);
    REQUIRE(saveResult.ok);

    const auto loadResult = store.load();
    REQUIRE(loadResult.ok);
    REQUIRE(loadResult.loadedFromDisk);
    REQUIRE(loadResult.settings.language == arcadeblocks::settings::Language::English);
    REQUIRE(loadResult.settings.audio.masterVolume == Catch::Approx(0.4));
    REQUIRE(loadResult.settings.video.windowMode == arcadeblocks::settings::VideoWindowMode::Fullscreen);
    REQUIRE(loadResult.settings.video.uiScale == Catch::Approx(1.25f));
    REQUIRE(loadResult.settings.gameplay.playerName == "Verifier");
    REQUIRE(loadResult.settings.gameplay.showLaunchTrajectory == false);
    REQUIRE(loadResult.settings.controls.pause.keyName == "P");
}

TEST_CASE("settings store ignores unknown fields and falls back on invalid values", "[settings]") {
    ScopedTempDirectory tempDirectory;
    const auto settingsPath = arcadeblocks::settings::settingsFilePath(tempDirectory.path());
    std::ofstream output{settingsPath};
    output << R"json(
{
  "version": 99,
  "language": "de",
  "audio": {
    "masterVolume": 4.0,
    "musicVolume": 0.5,
    "sfxVolume": "bad"
  },
  "video": {
    "windowMode": "fullscreen",
    "resolution": "1920x1080",
    "uiScale": 9.0
  },
  "gameplay": {
    "playerName": "",
    "paddleSpeed": 50.0,
    "turboSpeed": 99999.0,
    "difficulty": "impossible"
  },
  "controls": {
    "moveLeft": "Left",
    "moveRight": "Left",
    "launch": "Escape",
    "pause": "Escape",
    "callBall": "B"
  },
  "futureField": "ignored"
}
)json";
    output.close();

    arcadeblocks::settings::SettingsStore store{settingsPath};
    const auto result = store.load();
    REQUIRE(result.ok);
    REQUIRE(result.loadedFromDisk);
    REQUIRE(result.settings.language == arcadeblocks::settings::Language::Russian);
    REQUIRE(result.settings.audio.masterVolume == Catch::Approx(0.85));
    REQUIRE(result.settings.audio.musicVolume == Catch::Approx(0.5));
    REQUIRE(result.settings.audio.sfxVolume == Catch::Approx(0.82));
    REQUIRE(result.settings.video.windowMode == arcadeblocks::settings::VideoWindowMode::Fullscreen);
    REQUIRE(result.settings.video.uiScale == Catch::Approx(1.0f));
    REQUIRE(result.settings.gameplay.playerName == "Player");
    REQUIRE(result.settings.gameplay.paddleSpeed == Catch::Approx(760.0f));
    REQUIRE(result.settings.gameplay.turboSpeed == Catch::Approx(2000.0f));
    REQUIRE(result.settings.controls.moveLeft.keyName == "Left");
    REQUIRE(result.settings.controls.moveRight.keyName == "Right");
    REQUIRE(result.settings.controls.pause.keyName == "Escape");
    REQUIRE(!result.diagnostics.empty());
}

TEST_CASE("settings store migrates known values from unsupported versions", "[settings]") {
    ScopedTempDirectory tempDirectory;
    const auto settingsPath = arcadeblocks::settings::settingsFilePath(tempDirectory.path());
    std::ofstream output{settingsPath};
    output << R"json({
  "version": 0,
  "language": "en",
  "audio": {"masterVolume": 0.25},
  "future": {"ignored": true}
})json";
    output.close();

    arcadeblocks::settings::SettingsStore store{settingsPath};
    const auto result = store.load();
    REQUIRE(result.ok);
    REQUIRE(result.loadedFromDisk);
    REQUIRE(result.settings.version == 1);
    REQUIRE(result.settings.language == arcadeblocks::settings::Language::English);
    REQUIRE(result.settings.audio.masterVolume == Catch::Approx(0.25));
    REQUIRE_FALSE(result.diagnostics.empty());
}

TEST_CASE("settings store restores all control defaults for duplicate bindings", "[settings]") {
    ScopedTempDirectory tempDirectory;
    const auto settingsPath = arcadeblocks::settings::settingsFilePath(tempDirectory.path());
    std::ofstream output{settingsPath};
    output << R"json({
  "version": 1,
  "controls": {
    "moveLeft": "Left",
    "moveRight": "Right",
    "launch": "Space",
    "callBall": "B",
    "turbo": "B",
    "turboBall": "V",
    "plasma": "Z",
    "pause": "Escape"
  }
})json";
    output.close();

    arcadeblocks::settings::SettingsStore store{settingsPath};
    const auto result = store.load();
    REQUIRE(result.ok);
    REQUIRE(result.settings.controls.callBall.keyName == "B");
    REQUIRE(result.settings.controls.turbo.keyName == "X");
    REQUIRE_FALSE(result.diagnostics.empty());
}

TEST_CASE("settings store backs up broken json and recreates defaults", "[settings]") {
    ScopedTempDirectory tempDirectory;
    const auto settingsPath = arcadeblocks::settings::settingsFilePath(tempDirectory.path());
    std::ofstream output{settingsPath};
    output << "{ broken json";
    output.close();

    arcadeblocks::settings::SettingsStore store{settingsPath};
    const auto result = store.load();
    REQUIRE(result.ok);
    REQUIRE(result.backedUpCorruptFile);
    REQUIRE(result.wroteDefaults);
    REQUIRE(result.settings.controls.launch.keyName == "Space");

    bool foundBackup = false;
    for (const auto& entry : std::filesystem::directory_iterator{tempDirectory.path()}) {
        if (entry.path().filename().string().find("settings.json.corrupt-") == 0) {
            foundBackup = true;
            break;
        }
    }
    REQUIRE(foundBackup);

    const auto currentText = readTextFile(settingsPath);
    REQUIRE(currentText.find("\"language\": \"ru\"") != std::string::npos);
}
