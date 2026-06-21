#include "settings/SettingsStore.hpp"

#include "settings/KeyBinding.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <sstream>
#include <system_error>

namespace arcadeblocks::settings {
namespace {

using json = nlohmann::json;

constexpr int currentSettingsVersion = 1;
constexpr double minVolume = 0.0;
constexpr double maxVolume = 1.0;
constexpr float minUiScale = 0.75f;
constexpr float maxUiScale = 2.0f;
constexpr float minPaddleSpeed = 200.0f;
constexpr float maxPaddleSpeed = 2000.0f;
constexpr float minTurboSpeed = 400.0f;
constexpr float maxTurboSpeed = 12000.0f;

void addDiagnostic(
    SettingsLoadResult& result,
    SettingsDiagnosticSeverity severity,
    std::string message) {
    result.diagnostics.push_back(SettingsDiagnostic{severity, std::move(message)});
}

void addWarning(SettingsLoadResult& result, std::string message) {
    addDiagnostic(result, SettingsDiagnosticSeverity::Warning, std::move(message));
}

void addError(SettingsLoadResult& result, std::string message) {
    addDiagnostic(result, SettingsDiagnosticSeverity::Error, std::move(message));
}

bool loadTextFile(const std::filesystem::path& path, std::string& output, std::string& error) {
    std::ifstream input{path};
    if (!input) {
        error = "Failed to open settings file '" + path.string() + "'";
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    output = std::move(buffer).str();
    return true;
}

json toJson(const GameSettings& settings) {
    return json{
        {"version", settings.version},
        {"language", toString(settings.language)},
        {"audio", {
            {"masterVolume", settings.audio.masterVolume},
            {"musicVolume", settings.audio.musicVolume},
            {"sfxVolume", settings.audio.sfxVolume},
            {"callBallSound", settings.audio.callBallSound},
        }},
        {"video", {
            {"windowMode", toString(settings.video.windowMode)},
            {"resolution", settings.video.resolution},
            {"vsync", settings.video.vsync},
            {"uiScale", settings.video.uiScale},
            {"showLevelBackground", settings.video.showLevelBackground},
            {"fpsLimit", settings.video.fpsLimit},
        }},
        {"gameplay", {
            {"playerName", settings.gameplay.playerName},
            {"paddleSpeed", settings.gameplay.paddleSpeed},
            {"turboSpeed", settings.gameplay.turboSpeed},
            {"difficulty", toString(settings.gameplay.difficulty)},
            {"showLaunchTrajectory", settings.gameplay.showLaunchTrajectory},
        }},
        {"controls", {
            {"moveLeft", settings.controls.moveLeft.keyName},
            {"moveRight", settings.controls.moveRight.keyName},
            {"launch", settings.controls.launch.keyName},
            {"callBall", settings.controls.callBall.keyName},
            {"turbo", settings.controls.turbo.keyName},
            {"turboBall", settings.controls.turboBall.keyName},
            {"plasma", settings.controls.plasma.keyName},
            {"pause", settings.controls.pause.keyName},
        }},
    };
}

bool readInt(const json& object, const char* key, int& out) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        return false;
    }
    out = it->get<int>();
    return true;
}

bool readFloat(const json& object, const char* key, float& out) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number()) {
        return false;
    }
    out = it->get<float>();
    return true;
}

bool readDouble(const json& object, const char* key, double& out) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number()) {
        return false;
    }
    out = it->get<double>();
    return true;
}

bool readBool(const json& object, const char* key, bool& out) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_boolean()) {
        return false;
    }
    out = it->get<bool>();
    return true;
}

bool readString(const json& object, const char* key, std::string& out) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        return false;
    }
    out = it->get<std::string>();
    return true;
}

template <typename T>
void validateRange(
    SettingsLoadResult& result,
    const char* fieldName,
    T minValue,
    T maxValue,
    T defaultValue,
    T& value) {
    if (value < minValue || value > maxValue) {
        addWarning(
            result,
            std::string(fieldName) + " is outside [" + std::to_string(minValue) + ", " + std::to_string(maxValue)
                + "]; using default");
        value = defaultValue;
    }
}

void loadAudio(const json& object, SettingsLoadResult& result, GameSettings& settings, const GameSettings& defaults) {
    double number = 0.0;
    if (readDouble(object, "masterVolume", number)) {
        settings.audio.masterVolume = number;
    } else if (object.contains("masterVolume")) {
        addWarning(result, "audio.masterVolume is invalid; using default");
    }

    if (readDouble(object, "musicVolume", number)) {
        settings.audio.musicVolume = number;
    } else if (object.contains("musicVolume")) {
        addWarning(result, "audio.musicVolume is invalid; using default");
    }

    if (readDouble(object, "sfxVolume", number)) {
        settings.audio.sfxVolume = number;
    } else if (object.contains("sfxVolume")) {
        addWarning(result, "audio.sfxVolume is invalid; using default");
    }

    bool flag = false;
    if (readBool(object, "callBallSound", flag)) {
        settings.audio.callBallSound = flag;
    } else if (object.contains("callBallSound")) {
        addWarning(result, "audio.callBallSound is invalid; using default");
    }

    validateRange(result, "audio.masterVolume", minVolume, maxVolume, defaults.audio.masterVolume, settings.audio.masterVolume);
    validateRange(result, "audio.musicVolume", minVolume, maxVolume, defaults.audio.musicVolume, settings.audio.musicVolume);
    validateRange(result, "audio.sfxVolume", minVolume, maxVolume, defaults.audio.sfxVolume, settings.audio.sfxVolume);
}

void loadVideo(const json& object, SettingsLoadResult& result, GameSettings& settings, const GameSettings& defaults) {
    std::string text;
    if (readString(object, "windowMode", text)) {
        if (const auto value = parseVideoWindowMode(text)) {
            settings.video.windowMode = *value;
        } else {
            addWarning(result, "video.windowMode is invalid; using default");
        }
    } else if (object.contains("windowMode")) {
        addWarning(result, "video.windowMode is invalid; using default");
    }

    if (readString(object, "resolution", text)) {
        settings.video.resolution = std::move(text);
    } else if (object.contains("resolution")) {
        addWarning(result, "video.resolution is invalid; using default");
    }

    bool flag = false;
    if (readBool(object, "vsync", flag)) {
        settings.video.vsync = flag;
    } else if (object.contains("vsync")) {
        addWarning(result, "video.vsync is invalid; using default");
    }

    float scale = 0.0f;
    if (readFloat(object, "uiScale", scale)) {
        settings.video.uiScale = scale;
    } else if (object.contains("uiScale")) {
        addWarning(result, "video.uiScale is invalid; using default");
    }

    if (readBool(object, "showLevelBackground", flag)) {
        settings.video.showLevelBackground = flag;
    } else if (object.contains("showLevelBackground")) {
        addWarning(result, "video.showLevelBackground is invalid; using default");
    }

    int fpsLimit = 0;
    if (readInt(object, "fpsLimit", fpsLimit)) {
        settings.video.fpsLimit = fpsLimit;
    } else if (object.contains("fpsLimit")) {
        addWarning(result, "video.fpsLimit is invalid; using default");
    }

    validateRange(result, "video.uiScale", minUiScale, maxUiScale, defaults.video.uiScale, settings.video.uiScale);
    if (settings.video.resolution.empty()) {
        addWarning(result, "video.resolution is empty; using default");
        settings.video.resolution = defaults.video.resolution;
    }
}

void loadGameplay(const json& object, SettingsLoadResult& result, GameSettings& settings, const GameSettings& defaults) {
    std::string text;
    if (readString(object, "playerName", text)) {
        settings.gameplay.playerName = std::move(text);
    } else if (object.contains("playerName")) {
        addWarning(result, "gameplay.playerName is invalid; using default");
    }

    float number = 0.0f;
    if (readFloat(object, "paddleSpeed", number)) {
        settings.gameplay.paddleSpeed = number;
    } else if (object.contains("paddleSpeed")) {
        addWarning(result, "gameplay.paddleSpeed is invalid; using default");
    }

    if (readFloat(object, "turboSpeed", number)) {
        settings.gameplay.turboSpeed = number;
    } else if (object.contains("turboSpeed")) {
        addWarning(result, "gameplay.turboSpeed is invalid; using default");
    }

    if (readString(object, "difficulty", text)) {
        if (const auto value = parseDifficulty(text)) {
            settings.gameplay.difficulty = *value;
        } else {
            addWarning(result, "gameplay.difficulty is invalid; using default");
        }
    } else if (object.contains("difficulty")) {
        addWarning(result, "gameplay.difficulty is invalid; using default");
    }

    bool flag = false;
    if (readBool(object, "showLaunchTrajectory", flag)) {
        settings.gameplay.showLaunchTrajectory = flag;
    } else if (object.contains("showLaunchTrajectory")) {
        addWarning(result, "gameplay.showLaunchTrajectory is invalid; using default");
    }

    validateRange(
        result,
        "gameplay.paddleSpeed",
        minPaddleSpeed,
        maxPaddleSpeed,
        defaults.gameplay.paddleSpeed,
        settings.gameplay.paddleSpeed);
    validateRange(
        result,
        "gameplay.turboSpeed",
        minTurboSpeed,
        maxTurboSpeed,
        defaults.gameplay.turboSpeed,
        settings.gameplay.turboSpeed);

    if (settings.gameplay.playerName.empty()) {
        addWarning(result, "gameplay.playerName is empty; using default");
        settings.gameplay.playerName = defaults.gameplay.playerName;
    }
}

void loadKeyBinding(
    const json& object,
    const char* key,
    SettingsLoadResult& result,
    KeyBinding& binding,
    const KeyBinding& defaultBinding) {
    std::string text;
    if (readString(object, key, text)) {
        if (const auto canonical = canonicalizeKeyName(text)) {
            binding.keyName = *canonical;
        } else {
            addWarning(result, std::string("controls.") + key + " is invalid; using default");
            binding = defaultBinding;
        }
    } else if (object.contains(key)) {
        addWarning(result, std::string("controls.") + key + " is invalid; using default");
        binding = defaultBinding;
    }
}

void validateBindings(SettingsLoadResult& result, GameSettings& settings, const GameSettings& defaults) {
    if (const auto conflict = findDuplicateBinding(settings.controls)) {
        addWarning(
            result,
            "controls." + std::string{conflict->firstAction} + " and controls."
                + std::string{conflict->secondAction}
                + " must not use the same key; restoring control defaults");
        settings.controls = defaults.controls;
    }
}

void loadControls(const json& object, SettingsLoadResult& result, GameSettings& settings, const GameSettings& defaults) {
    loadKeyBinding(object, "moveLeft", result, settings.controls.moveLeft, defaults.controls.moveLeft);
    loadKeyBinding(object, "moveRight", result, settings.controls.moveRight, defaults.controls.moveRight);
    loadKeyBinding(object, "launch", result, settings.controls.launch, defaults.controls.launch);
    loadKeyBinding(object, "callBall", result, settings.controls.callBall, defaults.controls.callBall);
    loadKeyBinding(object, "turbo", result, settings.controls.turbo, defaults.controls.turbo);
    loadKeyBinding(object, "turboBall", result, settings.controls.turboBall, defaults.controls.turboBall);
    loadKeyBinding(object, "plasma", result, settings.controls.plasma, defaults.controls.plasma);
    loadKeyBinding(object, "pause", result, settings.controls.pause, defaults.controls.pause);
    validateBindings(result, settings, defaults);
}

SettingsLoadResult parseSettingsDocument(const json& document) {
    SettingsLoadResult result;
    const GameSettings defaults = defaultSettings();
    result.settings = defaults;

    if (!document.is_object()) {
        result.ok = false;
        result.error = "Settings root must be a JSON object";
        return result;
    }

    int version = 0;
    if (readInt(document, "version", version)) {
        result.settings.version = version;
    } else if (document.contains("version")) {
        addWarning(result, "version is invalid; using default");
    }

    if (result.settings.version != currentSettingsVersion) {
        addWarning(result, "settings version is unsupported; preserving known fields with current defaults");
        result.settings.version = currentSettingsVersion;
    }

    std::string text;
    if (readString(document, "language", text)) {
        if (const auto language = parseLanguage(text)) {
            result.settings.language = *language;
        } else {
            addWarning(result, "language is invalid; using default");
        }
    } else if (document.contains("language")) {
        addWarning(result, "language is invalid; using default");
    }

    const auto audioIt = document.find("audio");
    if (audioIt != document.end()) {
        if (audioIt->is_object()) {
            loadAudio(*audioIt, result, result.settings, defaults);
        } else {
            addWarning(result, "audio must be an object; using defaults");
        }
    }

    const auto videoIt = document.find("video");
    if (videoIt != document.end()) {
        if (videoIt->is_object()) {
            loadVideo(*videoIt, result, result.settings, defaults);
        } else {
            addWarning(result, "video must be an object; using defaults");
        }
    }

    const auto gameplayIt = document.find("gameplay");
    if (gameplayIt != document.end()) {
        if (gameplayIt->is_object()) {
            loadGameplay(*gameplayIt, result, result.settings, defaults);
        } else {
            addWarning(result, "gameplay must be an object; using defaults");
        }
    }

    const auto controlsIt = document.find("controls");
    if (controlsIt != document.end()) {
        if (controlsIt->is_object()) {
            loadControls(*controlsIt, result, result.settings, defaults);
        } else {
            addWarning(result, "controls must be an object; using defaults");
        }
    }

    return result;
}

std::string tempSuffix() {
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    return std::to_string(stamp);
}

} // namespace

SettingsStore::SettingsStore(std::filesystem::path path)
    : path_(std::move(path)) {}

const std::filesystem::path& SettingsStore::path() const noexcept {
    return path_;
}

SettingsLoadResult SettingsStore::load() const {
    SettingsLoadResult result;
    result.settings = defaultSettings();

    std::error_code error;
    const bool exists = std::filesystem::exists(path_, error);
    if (error) {
        result.ok = false;
        result.error = "Failed to inspect settings file '" + path_.string() + "': " + error.message();
        return result;
    }

    if (!exists) {
        const auto saveResult = writeAtomically(result.settings);
        result.wroteDefaults = saveResult.ok;
        if (!saveResult.ok) {
            result.ok = false;
            result.error = saveResult.error;
        }
        return result;
    }

    std::string text;
    if (!loadTextFile(path_, text, result.error)) {
        result.ok = false;
        return result;
    }

    json document;
    try {
        document = json::parse(text);
    } catch (const json::parse_error& exception) {
        addError(result, "Failed to parse settings JSON: " + std::string{exception.what()});
        const auto backupResult = backupCorruptFile();
        if (!backupResult.ok) {
            result.ok = false;
            result.error = backupResult.error;
            return result;
        }
        result.backedUpCorruptFile = true;
        const auto saveResult = writeAtomically(result.settings);
        result.wroteDefaults = saveResult.ok;
        if (!saveResult.ok) {
            result.ok = false;
            result.error = saveResult.error;
        }
        return result;
    }

    auto parsed = parseSettingsDocument(document);
    if (!parsed.ok) {
        addError(result, parsed.error);
        const auto backupResult = backupCorruptFile();
        if (!backupResult.ok) {
            result.ok = false;
            result.error = backupResult.error;
            return result;
        }
        result.backedUpCorruptFile = true;
        const auto saveResult = writeAtomically(result.settings);
        result.wroteDefaults = saveResult.ok;
        if (!saveResult.ok) {
            result.ok = false;
            result.error = saveResult.error;
        }
        return result;
    }

    result = std::move(parsed);
    result.loadedFromDisk = true;
    return result;
}

SettingsSaveResult SettingsStore::save(const GameSettings& settings) const {
    return writeAtomically(settings);
}

SettingsSaveResult SettingsStore::resetToDefaults() const {
    return writeAtomically(defaultSettings());
}

SettingsSaveResult SettingsStore::writeAtomically(const GameSettings& settings) const {
    std::error_code error;
    const auto directory = path_.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
        if (error) {
            return SettingsSaveResult{
                false,
                "Failed to create settings directory '" + directory.string() + "': " + error.message()};
        }
    }

    const auto temporaryPath = path_.string() + ".tmp";
    {
        std::ofstream output{temporaryPath, std::ios::out | std::ios::trunc};
        if (!output) {
            return SettingsSaveResult{false, "Failed to open temporary settings file '" + temporaryPath + "'"};
        }
        output << toJson(settings).dump(2) << '\n';
        output.flush();
        if (!output) {
            return SettingsSaveResult{false, "Failed to write temporary settings file '" + temporaryPath + "'"};
        }
    }

    std::filesystem::rename(temporaryPath, path_, error);
    if (!error) {
        return SettingsSaveResult{};
    }

    error.clear();
    std::filesystem::remove(path_, error);
    error.clear();
    std::filesystem::rename(temporaryPath, path_, error);
    if (!error) {
        return SettingsSaveResult{};
    }

    std::filesystem::remove(temporaryPath);
    return SettingsSaveResult{
        false,
        "Failed to replace settings file '" + path_.string() + "': " + error.message()};
}

SettingsSaveResult SettingsStore::backupCorruptFile() const {
    std::error_code error;
    if (!std::filesystem::exists(path_, error)) {
        return SettingsSaveResult{};
    }
    if (error) {
        return SettingsSaveResult{
            false,
            "Failed to inspect corrupt settings file '" + path_.string() + "': " + error.message()};
    }

    const auto backupPath = path_.string() + ".corrupt-" + tempSuffix() + ".bak";
    std::filesystem::rename(path_, backupPath, error);
    if (!error) {
        return SettingsSaveResult{};
    }

    return SettingsSaveResult{
        false,
        "Failed to back up corrupt settings file '" + path_.string() + "': " + error.message()};
}

} // namespace arcadeblocks::settings
