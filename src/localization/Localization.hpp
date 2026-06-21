#pragma once

#include "settings/Settings.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace arcadeblocks::localization {

class Localization {
public:
    bool loadFromDirectory(const std::filesystem::path& directory);
    [[nodiscard]] std::string text(settings::Language language, std::string_view key) const;

private:
    using Table = std::unordered_map<std::string, std::string>;

    [[nodiscard]] static Table loadTable(const std::filesystem::path& path, bool warnOnMissing = true);
    [[nodiscard]] const Table& tableFor(settings::Language language) const noexcept;

    Table english_;
    Table russian_;
};

} // namespace arcadeblocks::localization
