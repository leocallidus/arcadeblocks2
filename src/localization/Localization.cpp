#include "localization/Localization.hpp"

#include "core/Log.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

namespace arcadeblocks::localization {

Localization::Table Localization::loadTable(const std::filesystem::path& path, bool warnOnMissing) {
    Table table;

    std::ifstream input{path};
    if (!input) {
        if (warnOnMissing) {
            core::Log::warn("Localization file missing: " + path.string());
        }
        return table;
    }

    nlohmann::json document;
    try {
        input >> document;
    } catch (const std::exception& error) {
        core::Log::warn("Localization JSON parse failed: " + path.string() + " (" + error.what() + ")");
        return table;
    }

    if (!document.is_object()) {
        core::Log::warn("Localization root must be object: " + path.string());
        return table;
    }

    for (const auto& [key, value] : document.items()) {
        if (!value.is_string()) {
            core::Log::warn("Localization key is not string: " + key + " in " + path.string());
            continue;
        }
        table.emplace(key, value.get<std::string>());
    }

    core::Log::info("Loaded localization file: " + path.string() + ", keys=" + std::to_string(table.size()));
    return table;
}

bool Localization::loadFromDirectory(const std::filesystem::path& directory) {
    english_ = loadTable(directory / "ui_en.json", true);
    russian_ = loadTable(directory / "ui_ru.json", true);

    Table englishChapters = loadTable(directory / "chapter_en.json", false);
    for (auto&& [key, val] : englishChapters) {
        english_[key] = std::move(val);
    }

    Table russianChapters = loadTable(directory / "chapter_ru.json", false);
    for (auto&& [key, val] : russianChapters) {
        russian_[key] = std::move(val);
    }

    return !english_.empty() || !russian_.empty();
}

std::string Localization::text(settings::Language language, std::string_view key) const {
    const auto& primary = tableFor(language);
    if (const auto found = primary.find(std::string{key}); found != primary.end()) {
        return found->second;
    }

    if (const auto fallback = english_.find(std::string{key}); fallback != english_.end()) {
        return fallback->second;
    }

    return std::string{key};
}

const Localization::Table& Localization::tableFor(settings::Language language) const noexcept {
    return language == settings::Language::Russian ? russian_ : english_;
}

} // namespace arcadeblocks::localization
