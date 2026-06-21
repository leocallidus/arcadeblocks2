#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arcadeblocks::ui {

struct BonusTimerModel {
    std::string type;
    std::string name;
    double durationSeconds = 0.0;
    double remainingSeconds = 0.0;
    double fadeOutRemainingSeconds = 0.0;
    void* iconTexture = nullptr;
    float iconUv0x = 0.0f;
    float iconUv0y = 0.0f;
    float iconUv1x = 1.0f;
    float iconUv1y = 1.0f;
};

struct HudModel {
    int level = 1;
    int score = 0;
    int lives = 0;
    int activeBricks = 0;
    int entityCount = 0;
    bool hasBoss = false;
    float bossHealthNormalized = 0.0f;
    std::vector<float> bossSectionHealthsNormalized;
    std::string bossHpLabel;
    std::string levelName;
    std::string phase;
    std::string assetStats;
    std::string audioStats;
    double framesPerSecond = 0.0;
    double frameMilliseconds = 0.0;
    std::uint64_t updatesThisFrame = 0;
    bool debug = false;
    bool assetStatsVisible = false;
    bool physicsDebug = false;
    float bonusTimerVisibility = 1.0f;
    std::vector<BonusTimerModel> bonusTimers;
};

class HudView {
public:
    void render(const HudModel& model);
};

} // namespace arcadeblocks::ui
