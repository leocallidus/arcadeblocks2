#include "core/Application.hpp"
#include "core/CommandLine.hpp"

#include <iostream>

int main(int argc, char** argv) {
    auto parsed = arcadeblocks::core::parseCommandLine(argc, argv);
    if (!parsed.ok) {
        std::cerr << parsed.error << '\n';
        return 2;
    }

    arcadeblocks::core::Application application{std::move(parsed.options)};
    return application.run();
}
