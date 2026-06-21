#pragma once

#include "settings/Settings.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace arcadeblocks::settings {

enum class SettingsDiagnosticSeverity {
    Warning,
    Error
};

struct SettingsDiagnostic {
    SettingsDiagnosticSeverity severity = SettingsDiagnosticSeverity::Warning;
    std::string message;
};

struct SettingsLoadResult {
    GameSettings settings = defaultSettings();
    std::vector<SettingsDiagnostic> diagnostics;
    bool ok = true;
    std::string error;
    bool loadedFromDisk = false;
    bool wroteDefaults = false;
    bool backedUpCorruptFile = false;
};

struct SettingsSaveResult {
    bool ok = true;
    std::string error;
};

class SettingsStore {
public:
    explicit SettingsStore(std::filesystem::path path);

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] SettingsLoadResult load() const;
    [[nodiscard]] SettingsSaveResult save(const GameSettings& settings) const;
    [[nodiscard]] SettingsSaveResult resetToDefaults() const;

private:
    [[nodiscard]] SettingsSaveResult writeAtomically(const GameSettings& settings) const;
    [[nodiscard]] SettingsSaveResult backupCorruptFile() const;

    std::filesystem::path path_;
};

} // namespace arcadeblocks::settings
