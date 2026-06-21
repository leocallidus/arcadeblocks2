#pragma once

#include "core/CommandLine.hpp"
#include "localization/Localization.hpp"
#include "platform/Paths.hpp"
#include "settings/Settings.hpp"

#include <memory>
#include <string>

namespace arcadeblocks::core {

class Application {
public:
    explicit Application(CommandLineOptions options);

    int run();

private:
    bool initialize();
    void shutdown();
    void printUsage() const;
    void printVersion() const;

    CommandLineOptions options_;
    platform::ResolvedPaths paths_;
    settings::GameSettings settings_;
    std::shared_ptr<localization::Localization> localization_;
    bool initialized_ = false;
};

} // namespace arcadeblocks::core
