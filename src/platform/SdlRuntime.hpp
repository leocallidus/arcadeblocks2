#pragma once

#include "assets/AssetRegistry.hpp"
#include "audio/AudioSystem.hpp"
#include "core/Clock.hpp"
#include "core/CommandLine.hpp"
#include "gameplay/GameWorld.hpp"
#include "levels/LevelTypes.hpp"
#include "localization/Localization.hpp"
#include "settings/Settings.hpp"
#include "ui/HudView.hpp"
#include "ui/DebugMenuView.hpp"
#include "ui/DebugBonusesView.hpp"
#include "ui/HelpView.hpp"
#include "ui/LevelCompleteView.hpp"
#include "ui/LevelsMenuView.hpp"
#include "ui/LoadingScreenView.hpp"
#include "ui/MainMenuView.hpp"
#include "ui/PauseView.hpp"
#include "ui/SettingsView.hpp"

#include <SDL3/SDL_keycode.h>

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

struct SDL_Renderer;
struct SDL_Window;

namespace arcadeblocks::render {
class AssetManager;
class Renderer;
}

namespace arcadeblocks::ui {
class ImGuiLayer;
}

namespace arcadeblocks::platform {

struct SdlRuntimeConfig {
    std::filesystem::path assetsDirectory;
    std::filesystem::path settingsFilePath;
    std::shared_ptr<localization::Localization> localization;
    core::WindowMode windowMode = core::WindowMode::Windowed;
    settings::GameSettings settings = settings::defaultSettings();
    int level = 1;
    bool startInLevel = false;
    bool noAudio = false;
    bool debug = false;
    bool perfSummary = false;
    float uiScale = 1.0f;
    int smokeFrames = 0;
    core::SmokeScenario smokeScenario = core::SmokeScenario::None;
    int windowWidth = 1280;
    int windowHeight = 720;
    int logicalWidth = 1920;
    int logicalHeight = 1080;
};

enum class RuntimeScreen {
    MainMenu,
    LevelIntro,
    InGame,
    PauseMenu
};

struct LevelIntroState {
    int levelNumber = 0;
    std::string levelName;
    int chapterIdx = 1;
    std::string chapterTitle;
    ui::UiAccent chapterAccent = ui::UiAccent::White;
    bool fromDebugMenu = false;
    
    double elapsedSeconds = 0.0;
    double durationSeconds = 3.0;
    std::filesystem::path sfxPath;
    bool sfxStarted = false;
    bool fadeOutStarted = false;
};

class SdlRuntime {
public:
    explicit SdlRuntime(SdlRuntimeConfig config);
    ~SdlRuntime();

    SdlRuntime(const SdlRuntime&) = delete;
    SdlRuntime& operator=(const SdlRuntime&) = delete;

    bool initialize();
    int run();
    void shutdown();

private:
    struct InputBindings {
        SDL_Keycode moveLeft = SDLK_LEFT;
        SDL_Keycode moveRight = SDLK_RIGHT;
        SDL_Keycode launch = SDLK_SPACE;
        SDL_Keycode pause = SDLK_ESCAPE;
        SDL_Keycode callBall = SDLK_B;
        SDL_Keycode turbo = SDLK_X;
        SDL_Keycode turboBall = SDLK_V;
        SDL_Keycode plasma = SDLK_Z;
    };

    enum class BrickImpactEffectKind {
        Break,
        Blocked,
        Explosion,
        Dissolve
    };

    struct BrickImpactParticle {
        gameplay::Vec2 position;
        gameplay::Vec2 velocity;
        float size = 0.0f;
        float spin = 0.0f;
        unsigned char r = 255;
        unsigned char g = 255;
        unsigned char b = 255;
    };

    struct BrickImpactEffect {
        BrickImpactEffectKind kind = BrickImpactEffectKind::Break;
        gameplay::Vec2 center;
        gameplay::Size brickSize;
        double age = 0.0;
        double duration = 0.0;
        std::vector<BrickImpactParticle> particles;
    };

    struct LifeLostEffect {
        gameplay::Vec2 center;
        double age = 0.0;
        double duration = 1.1;
        std::vector<BrickImpactParticle> particles;
    };

    struct BonusParticle {
        gameplay::Vec2 position;
        gameplay::Vec2 velocity;
        float size = 0.0f;
        double age = 0.0;
        double duration = 0.0;
        unsigned char r = 255;
        unsigned char g = 255;
        unsigned char b = 255;
        bool isTrail = false;
    };

    struct BonusPickupGlow {
        gameplay::Vec2 center;
        double age = 0.0;
        double duration = 0.0;
        float maxRadius = 0.0f;
        unsigned char r = 255;
        unsigned char g = 255;
        unsigned char b = 255;
    };

    void processEvents(bool& running);
    void fixedUpdate(double fixedDeltaSeconds);
    void render();
    bool loadSelectedLevel();
    void startLevel(int level);
    void initLevelIntro(bool fromDebugMenu);
    [[nodiscard]] std::filesystem::path resolveLevelIntroSfx(int level) const;
    void restartLevel();
    void pauseGame();
    void resumeGame();
    void exitToMenu();
    [[nodiscard]] double resumeCountdownDuration() const;
    void updateResumeCountdown(double deltaSeconds);
    void renderResumeCountdownOverlay();
    void playMenuMusic();
    void playLevelMusic();
    void playGameOverMusic();
    void preloadLevelSfx();
    void handleAudioEvents();
    void playAudioEvent(const gameplay::AudioEvent& event);
    void updateGameOverState(double fixedDeltaSeconds);
    void updateLevelCompleteState(double fixedDeltaSeconds);
    void resetGameOverState();
    void resetLevelCompleteState();
    [[nodiscard]] int continueCost() const noexcept;
    void spawnBrickImpactEffect(const gameplay::AudioEvent& event);
    void spawnLifeLostEffect(const gameplay::AudioEvent& event);
    void updateBrickImpactEffects(double fixedDeltaSeconds);
    void renderBrickImpactEffects();
    void renderLifeLostEffects();
    void renderRespawnChargeEffect();
    void updateBonusEffects(double fixedDeltaSeconds);
    void renderBonusEffects();
    void renderGameOverOverlay();
    void renderLevelCompleteOverlay();
    [[nodiscard]] float gameOverFadeProgress() const noexcept;
    [[nodiscard]] float levelCompleteFadeProgress() const noexcept;
    void playUiSound(ui::UiSoundEffect effect);
    void renderGameScene();
    void renderLevelIntro();
    void renderLevelIntroOverlay();
    void renderMainMenuScene();
    void renderHud();
    void handleMainMenuAction(const ui::MainMenuRenderResult& result, bool& running);
    void handlePauseAction(const ui::PauseRenderResult& result);
    void handleHelpAction(const ui::HelpRenderResult& result);
    void handleSettingsAction(const ui::SettingsRenderResult& result);
    void applyRuntimeSettings(const settings::GameSettings& settings);
    [[nodiscard]] bool saveRuntimeSettings();
    void renderGameWorldBricks();
    void renderBoss();
    void renderBossHelios(const gameplay::Boss& boss, float x, float y,
                          unsigned char alpha, float flashRatio, float t);
    void renderBossSingularity(const gameplay::Boss& boss, float x, float y,
                              unsigned char alpha, float flashRatio, float t);
    void renderBossChronarch(const gameplay::Boss& boss, float x, float y,
                             unsigned char alpha, float flashRatio, float t);
    void renderPhysicsDebugOverlay();
    void drawDebugRect(float x, float y, float w, float h);
    void renderSpriteOrPlaceholder(std::string_view spriteName, float x, float y, float w, float h, unsigned char alpha = 255);
    void renderLaunchTrajectory(const gameplay::Ball& ball, float initialVx, unsigned char gameplayAlpha);
    void drawBeautifulDottedLine(gameplay::Vec2 start, gameplay::Vec2 end, unsigned char baseAlpha, float& patternOffset, float& totalDistanceDrawn);
    void drawLandingGlow(gameplay::Vec2 pos, float radius, unsigned char baseAlpha);
    void logAssetStatsOnce();
    void logPerformanceSummary(double totalRuntimeMilliseconds) const;
    void updateWindowTitle();
    void toggleFullscreen();
    void updateMouseCapture();
    void updateMousePaddleFromWindowX(float windowX);
    [[nodiscard]] bool matchesKey(SDL_Keycode eventKey, SDL_Keycode binding, SDL_Keycode alternate = SDLK_UNKNOWN) const noexcept;
    void applyVideoSettings();
    void applyAudioSettings();
    void applySmokeScenario();
    void updateSmokeCycle();
    [[nodiscard]] bool smokeScenarioNeedsGameplay() const noexcept;
    std::string sdlError(const char* context) const;

    SdlRuntimeConfig config_;
    assets::AssetRegistry assetRegistry_;
    assets::LevelAssetMapping currentLevelAssets_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<render::Renderer> rendererApi_;
    std::unique_ptr<render::AssetManager> assets_;
    render::Texture* menuBackgroundTexture_ = nullptr;
    render::Texture* levelBackgroundTexture_ = nullptr;
    render::Texture* debugBackgroundTexture_ = nullptr;
    render::Texture* gameplayAtlasTexture_ = nullptr;
    const render::SpriteAtlas* gameplayAtlas_ = nullptr;
    std::unique_ptr<audio::AudioSystem> audio_;
    std::unique_ptr<ui::ImGuiLayer> imgui_;
    ui::HudView hudView_;
    ui::HelpView helpView_;
    ui::MainMenuView mainMenuView_;
    ui::DebugMenuView debugMenuView_;
    ui::DebugBonusesView debugBonusesView_;
    ui::LevelCompleteView levelCompleteView_;
    ui::LevelsMenuView levelsMenuView_;
    ui::PauseView pauseView_;
    ui::SettingsView settingsView_;
    std::optional<gameplay::GameWorld> world_;
    std::string levelName_;
    core::Clock clock_;
    core::FrameStats frameStats_;
    double accumulatorSeconds_ = 0.0;
    double startupMilliseconds_ = 0.0;
    double frameMillisecondsSum_ = 0.0;
    double frameMillisecondsMin_ = 0.0;
    double frameMillisecondsMax_ = 0.0;
    std::uint64_t measuredFrames_ = 0;
    std::uint64_t frameLimitUs_ = 0; ///< Frame interval cap in microseconds (0 = unlimited).
    std::chrono::steady_clock::time_point initializeStarted_{};
    std::chrono::steady_clock::time_point runStarted_{};
    bool fullscreen_ = false;
    bool focused_ = true;
    bool leftPressed_ = false;
    bool rightPressed_ = false;
    bool spacePressed_ = false;
    bool mousePaddleActive_ = false;
    bool mouseCaptureEnabled_ = false;
    float mousePaddleCenterX_ = 0.0f;
    bool launchRequested_ = false;
    bool callBallRequested_ = false;
    bool shootPlasmaRequested_ = false;
    bool pauseRequested_ = false;
    bool physicsDebugToggleRequested_ = false;
    bool turboBallPressed_ = false;
    bool turboBallMouseButtonPressed_ = false;
    double turboBallPulseSeconds_ = 0.0;
    bool assetStatsLogged_ = false;
    bool debugOverlayVisible_ = false;
    bool assetStatsVisible_ = false;
    bool settingsOpen_ = false;
    bool helpOpen_ = false;
    bool exitConfirmOpen_ = false;
    bool debugMenuOpen_ = false;
    bool debugBonusesMenuOpen_ = false;
    bool levelsMenuOpen_ = false;
    bool debugSpawnMenuOpen_ = false;
    bool pauseMenuWasOpen_ = false;
    float uiScale_ = 1.0f;
    RuntimeScreen screen_ = RuntimeScreen::MainMenu;
    std::uint64_t resizeEvents_ = 0;
    bool showLevelBackground_ = true;
    bool vsyncEnabled_ = true;
    bool smokeScenarioFailed_ = false;
    bool smokeCycleOpen_ = false;
    std::uint64_t smokeCycleTransitions_ = 0;
    InputBindings inputBindings_{};
    std::vector<BrickImpactEffect> brickImpactEffects_;
    std::vector<LifeLostEffect> lifeLostEffects_;
    std::vector<BonusParticle> bonusParticles_;
    std::vector<BonusPickupGlow> bonusPickupGlows_;
    std::mt19937 effectRng_;
    double gameOverAge_ = 0.0;
    int gameOverSelectedIndex_ = 0;
    int continueSelectedIndex_ = 0;
    int continuePurchaseCount_ = 0;
    bool gameOverActive_ = false;
    bool gameOverMusicStarted_ = false;
    bool continuePromptOpen_ = false;
    bool resumeCountdownActive_ = false;
    double resumeCountdownRemaining_ = 0.0;
    int resumeCountdownLastTick_ = -1;
    double paddleControlLockRemaining_ = 0.0;
    float bonusTimerHudVisibility_ = 0.0f;
    float paddleFrozenBlend_ = 0.0f;
    float paddlePlasmaBlend_ = 0.0f;
    float paddleStickyBlend_ = 0.0f;
    float paddleInvisibleBlend_ = 0.0f;
    float paddleDarknessBlend_ = 0.0f;


    // Level complete state
    bool levelCompleteActive_ = false;
    double levelCompleteAge_ = 0.0;
    double levelCompleteFadeOutDuration_ = 0.75;
    double levelPlayTime_ = 0.0;
    int livesLostThisLevel_ = 0;
    bool levelCompleteSoundPlayed_ = false;
    bool levelCompleteMenuOpen_ = false;

    // Loading screen state
    std::optional<ui::LoadingScreenView> loadingScreenView_;
    render::Texture* gameLogoTexture_ = nullptr;
    render::Texture* studioLogoTexture_ = nullptr;
    std::filesystem::path welcomeSoundPath_;
    bool loadingScreenActive_ = false;
    std::chrono::steady_clock::time_point lastExplosionTime_{};
    std::chrono::steady_clock::time_point lastEnergyBallHitTime_{};

    LevelIntroState levelIntroState_;
    bool lastStartWasDebugMenu_ = false;
};

} // namespace arcadeblocks::platform
