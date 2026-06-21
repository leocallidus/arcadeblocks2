#include <catch2/catch_test_macros.hpp>

#include "localization/Localization.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

class ScopedTempDirectory {
public:
    ScopedTempDirectory() {
        path_ = std::filesystem::temp_directory_path() / ("arcadeblocks2-localization-test-" + std::to_string(counter_++));
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

void writeTextFile(const std::filesystem::path& path, const char* contents) {
    std::ofstream output{path};
    REQUIRE(output.good());
    output << contents;
    output.close();
}

} // namespace

TEST_CASE("localization falls back from russian to english to key", "[localization]") {
    ScopedTempDirectory tempDirectory;
    writeTextFile(
        tempDirectory.path() / "ui_en.json",
        R"json({
  "common.back": "Back",
  "menu.play.label": "Play"
})json");
    writeTextFile(
        tempDirectory.path() / "ui_ru.json",
        R"json({
  "common.back": "Назад"
})json");

    arcadeblocks::localization::Localization localization;
    REQUIRE(localization.loadFromDirectory(tempDirectory.path()));

    REQUIRE(localization.text(arcadeblocks::settings::Language::Russian, "common.back") == "Назад");
    REQUIRE(localization.text(arcadeblocks::settings::Language::Russian, "menu.play.label") == "Play");
    REQUIRE(localization.text(arcadeblocks::settings::Language::English, "menu.play.label") == "Play");
    REQUIRE(localization.text(arcadeblocks::settings::Language::Russian, "missing.key") == "missing.key");
}

TEST_CASE("localization tolerates missing and invalid files", "[localization]") {
    ScopedTempDirectory tempDirectory;

    arcadeblocks::localization::Localization emptyLocalization;
    REQUIRE_FALSE(emptyLocalization.loadFromDirectory(tempDirectory.path()));
    REQUIRE(emptyLocalization.text(arcadeblocks::settings::Language::Russian, "missing.key") == "missing.key");

    writeTextFile(
        tempDirectory.path() / "ui_en.json",
        R"json({
  "common.back": "Back"
})json");
    writeTextFile(
        tempDirectory.path() / "ui_ru.json",
        "{ broken json");

    arcadeblocks::localization::Localization localization;
    REQUIRE(localization.loadFromDirectory(tempDirectory.path()));
    REQUIRE(localization.text(arcadeblocks::settings::Language::Russian, "common.back") == "Back");
}
