#pragma once

#include <chrono>
#include <cstdint>

namespace arcadeblocks::core {

class Clock {
public:
    using Seconds = std::chrono::duration<double>;

    void reset();
    Seconds tick();

private:
    std::chrono::steady_clock::time_point previous_ = std::chrono::steady_clock::now();
};

struct FrameStats {
    double frameSeconds = 0.0;
    double accumulatorSeconds = 0.0;
    double interpolationAlpha = 0.0;
    double framesPerSecond = 0.0;
    std::uint64_t frameCount = 0;
    std::uint64_t updateCount = 0;
    std::uint64_t updatesThisFrame = 0;

    void beginFrame(double deltaSeconds, double accumulator);
    void recordUpdate();
    void endFrame(double alpha);
};

} // namespace arcadeblocks::core
