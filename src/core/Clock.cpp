#include "core/Clock.hpp"

namespace arcadeblocks::core {

void Clock::reset() {
    previous_ = std::chrono::steady_clock::now();
}

Clock::Seconds Clock::tick() {
    const auto now = std::chrono::steady_clock::now();
    const auto delta = now - previous_;
    previous_ = now;
    return std::chrono::duration_cast<Seconds>(delta);
}

void FrameStats::beginFrame(double deltaSeconds, double accumulator) {
    frameSeconds = deltaSeconds;
    accumulatorSeconds = accumulator;
    updatesThisFrame = 0;
    ++frameCount;

    if (deltaSeconds > 0.000001) {
        framesPerSecond = 1.0 / deltaSeconds;
    }
}

void FrameStats::recordUpdate() {
    ++updateCount;
    ++updatesThisFrame;
}

void FrameStats::endFrame(double alpha) {
    interpolationAlpha = alpha;
}

} // namespace arcadeblocks::core
