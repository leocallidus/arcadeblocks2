#include "platform/SdlRuntime.hpp"

#include "audio/AudioSystem.hpp"
#include "core/Log.hpp"
#include "core/Version.hpp"
#include "gameplay/GameWorld.hpp"
#include "levels/LevelLoader.hpp"
#include "render/AssetManager.hpp"
#include "render/Renderer.hpp"
#include "settings/SettingsStore.hpp"
#include "ui/ImGuiLayer.hpp"
#include "ui/UiLayout.hpp"
#include "ui/UiTheme.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>
#include <utility>

namespace arcadeblocks::platform {
namespace {

constexpr double fixedDeltaSeconds = 1.0 / 60.0;

int getIntroChapterIndex(int level) {
    if (level >= 1 && level <= 10) return 1;
    if (level >= 11 && level <= 20) return 2;
    if (level >= 21 && level <= 30) return 3;
    if (level == 31) return 4;
    if (level >= 32 && level <= 40) return 5;
    if (level >= 41 && level <= 50) return 6;
    if (level >= 51 && level <= 60) return 7;
    if (level >= 61 && level <= 70) return 8;
    if (level >= 71 && level <= 80) return 9;
    if (level >= 81 && level <= 90) return 10;
    if (level >= 91 && level <= 100) return 11;
    if (level >= 101 && level <= 116) return 12;
    return 13;
}

ui::UiAccent getIntroChapterAccent(int chapterIdx) {
    switch (chapterIdx) {
        case 1: return ui::UiAccent::Pink;
        case 2: return ui::UiAccent::Cyan;
        case 3: return ui::UiAccent::Purple;
        case 4: return ui::UiAccent::Lime;
        case 5: return ui::UiAccent::Fuchsia;
        case 6: return ui::UiAccent::Cyan;
        case 7: return ui::UiAccent::Brown;
        case 8: return ui::UiAccent::Cyan;
        case 9: return ui::UiAccent::White;
        case 10: return ui::UiAccent::Purple;
        case 11: return ui::UiAccent::White;
        case 12: return ui::UiAccent::Purple;
        default: return ui::UiAccent::White;
    }
}

std::string getIntroChapterTitle(int chapterIdx, const localization::Localization& localization, settings::Language language) {
    if (chapterIdx >= 1 && chapterIdx <= 12) {
        std::string key = "chapter.title." + std::to_string(chapterIdx);
        std::string localized = localization.text(language, key);
        if (localized != key && !localized.empty()) {
            return localized;
        }
    }
    if (language == settings::Language::Russian) {
        if (chapterIdx == 13) return "Дополнительные уровни";
        return "Глава " + std::to_string(chapterIdx);
    } else {
        if (chapterIdx == 13) return "Extra Levels";
        return "Chapter " + std::to_string(chapterIdx);
    }
}

bool isIntroBossLevel(int level) {
    switch (level) {
        case 10:
        case 20:
        case 30:
        case 40:
        case 50:
        case 60:
        case 70:
        case 80:
        case 90:
        case 100:
        case 114:
        case 115:
            return true;
        default:
            return false;
    }
}
constexpr double maxFrameDeltaSeconds = 0.25;
constexpr const char* windowIconRelativePath = "sprites/favicon-192.png";
constexpr double brickBreakEffectDuration = 0.64;
constexpr double brickBlockedEffectDuration = 0.32;
constexpr double paddleControlLockAfterResumeSeconds = 1.0;
constexpr double lifeLostEffectDuration = 1.1;
constexpr double respawnLaunchDelaySeconds = 0.9;
constexpr float pi = 3.14159265358979323846f;

std::string formatString(const char* format, double a, double b, unsigned long long c, double d) {
    char buffer[256]{};
    std::snprintf(buffer, sizeof(buffer), format, a, b, c, d);
    return buffer;
}

std::string textureStatsLine(const render::AssetStats& stats) {
    std::ostringstream output;
    output.setf(std::ios::fixed);
    output.precision(2);
    output << "textures " << stats.loadedTextures
           << "  approx " << (static_cast<double>(stats.approximateTextureBytes) / (1024.0 * 1024.0)) << " MiB";
    return output.str();
}

std::string formatNumber(int value) {
    const std::string digits = std::to_string(std::max(0, value));
    std::string result;
    result.reserve(digits.size() + digits.size() / 3);
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            result.push_back(' ');
        }
        result.push_back(digits[i]);
    }
    return result;
}

void applyWindowIcon(SDL_Window* window, const std::filesystem::path& assetsDirectory) {
    if (window == nullptr) {
        return;
    }

    const auto iconPath = assetsDirectory / windowIconRelativePath;
    SDL_Surface* icon = IMG_Load(iconPath.string().c_str());
    if (icon == nullptr) {
        core::Log::warn("Window icon load failed: " + iconPath.string() + " (" + SDL_GetError() + ")");
        return;
    }

    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
    core::Log::info("Loaded window icon: " + iconPath.string());
}

float panFromLogicalX(float x) {
    return std::clamp((x / 1920.0f) * 2.0f - 1.0f, -1.0f, 1.0f);
}

float easeOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

unsigned char fadeAlpha(float progress, float maxAlpha) {
    const float alpha = std::clamp((1.0f - progress) * maxAlpha, 0.0f, 255.0f);
    return static_cast<unsigned char>(alpha);
}

SDL_Keycode keycodeOrDefault(std::string_view keyName, SDL_Keycode fallback) {
    std::string copy{keyName};
    const SDL_Keycode keycode = SDL_GetKeyFromName(copy.c_str());
    return keycode == SDLK_UNKNOWN ? fallback : keycode;
}

const localization::Localization& runtimeLocalization(const SdlRuntimeConfig& config) {
    static const auto fallback = std::make_shared<localization::Localization>();
    return config.localization ? *config.localization : *fallback;
}

float safe_div(float num, float denom) {
    if (std::abs(denom) < 1e-6f) {
        return denom > 0.0f ? 1e9f : -1e9f;
    }
    return num / denom;
}

int getUtf8Length(const std::string& str) {
    int length = 0;
    size_t byteIdx = 0;
    while (byteIdx < str.size()) {
        unsigned char c = str[byteIdx];
        if (c < 0x80) {
            byteIdx += 1;
        } else if ((c & 0xE0) == 0xC0) {
            byteIdx += 2;
        } else if ((c & 0xF0) == 0xE0) {
            byteIdx += 3;
        } else if ((c & 0xF8) == 0xF0) {
            byteIdx += 4;
        } else {
            byteIdx += 1;
        }
        length++;
    }
    return length;
}

std::string getUtf8Prefix(const std::string& str, int numChars) {
    if (numChars <= 0) return "";
    int charsCounted = 0;
    size_t byteIdx = 0;
    while (byteIdx < str.size() && charsCounted < numChars) {
        unsigned char c = str[byteIdx];
        if (c < 0x80) {
            byteIdx += 1;
        } else if ((c & 0xE0) == 0xC0) {
            byteIdx += 2;
        } else if ((c & 0xF0) == 0xE0) {
            byteIdx += 3;
        } else if ((c & 0xF8) == 0xF0) {
            byteIdx += 4;
        } else {
            byteIdx += 1;
        }
        charsCounted++;
    }
    return str.substr(0, byteIdx);
}

struct Ray {
    gameplay::Vec2 origin;
    gameplay::Vec2 dir;
};

float intersectAABB(const Ray& ray, float xmin, float xmax, float ymin, float ymax, gameplay::Vec2& outNormal) {
    float t1 = safe_div(xmin - ray.origin.x, ray.dir.x);
    float t2 = safe_div(xmax - ray.origin.x, ray.dir.x);
    float t3 = safe_div(ymin - ray.origin.y, ray.dir.y);
    float t4 = safe_div(ymax - ray.origin.y, ray.dir.y);

    float tmin = std::max(std::min(t1, t2), std::min(t3, t4));
    float tmax = std::min(std::max(t1, t2), std::max(t3, t4));

    if (tmax < 0.0f || tmin > tmax) {
        return -1.0f;
    }
    if (tmin < 0.05f) {
        return -1.0f;
    }

    if (std::abs(tmin - t1) < 1e-4f) outNormal = gameplay::Vec2{-1.0f, 0.0f};
    else if (std::abs(tmin - t2) < 1e-4f) outNormal = gameplay::Vec2{1.0f, 0.0f};
    else if (std::abs(tmin - t3) < 1e-4f) outNormal = gameplay::Vec2{0.0f, -1.0f};
    else if (std::abs(tmin - t4) < 1e-4f) outNormal = gameplay::Vec2{0.0f, 1.0f};

    return tmin;
}

} // namespace

SdlRuntime::SdlRuntime(SdlRuntimeConfig config)
    : config_(config),
      assetRegistry_(config_.assetsDirectory),
      currentLevelAssets_(assetRegistry_.level(config_.level)),
      fullscreen_(config.windowMode == core::WindowMode::Fullscreen),
      debugOverlayVisible_(config.debug),
      assetStatsVisible_(config.debug),
      uiScale_(std::clamp(config.uiScale, 0.75f, 2.0f)),
      showLevelBackground_(config_.settings.video.showLevelBackground),
      vsyncEnabled_(config_.settings.video.vsync),
      inputBindings_(InputBindings{
          .moveLeft = keycodeOrDefault(config_.settings.controls.moveLeft.keyName, SDLK_LEFT),
          .moveRight = keycodeOrDefault(config_.settings.controls.moveRight.keyName, SDLK_RIGHT),
          .launch = keycodeOrDefault(config_.settings.controls.launch.keyName, SDLK_SPACE),
          .pause = keycodeOrDefault(config_.settings.controls.pause.keyName, SDLK_ESCAPE),
          .callBall = keycodeOrDefault(config_.settings.controls.callBall.keyName, SDLK_B),
          .turbo = keycodeOrDefault(config_.settings.controls.turbo.keyName, SDLK_X),
          .turboBall = keycodeOrDefault(config_.settings.controls.turboBall.keyName, SDLK_V),
          .plasma = keycodeOrDefault(config_.settings.controls.plasma.keyName, SDLK_Z),
      }),
      effectRng_(std::random_device{}()) {}

SdlRuntime::~SdlRuntime() {
    shutdown();
}

bool SdlRuntime::initialize() {
    initializeStarted_ = std::chrono::steady_clock::now();
    core::Log::info("Initializing SDL " + std::string(core::sdlTargetVersion()));

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        core::Log::error(sdlError("SDL_Init"));
        return false;
    }

    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | (fullscreen_ ? SDL_WINDOW_FULLSCREEN : 0);
    if (!SDL_CreateWindowAndRenderer(
            "Arcade Blocks II",
            config_.windowWidth,
            config_.windowHeight,
            flags,
            &window_,
            &renderer_)) {
        core::Log::error(sdlError("SDL_CreateWindowAndRenderer"));
        return false;
    }

    applyWindowIcon(window_, config_.assetsDirectory);

    if (!SDL_SetRenderLogicalPresentation(
            renderer_,
            config_.logicalWidth,
            config_.logicalHeight,
            SDL_LOGICAL_PRESENTATION_STRETCH)) {
        core::Log::warn(sdlError("SDL_SetRenderLogicalPresentation"));
    }

    applyVideoSettings();

    rendererApi_ = std::make_unique<render::Renderer>(renderer_, config_.assetsDirectory);
    assets_ = std::make_unique<render::AssetManager>(renderer_, config_.assetsDirectory);
    audio_ = std::make_unique<audio::AudioSystem>(audio::AudioSystemConfig{
        .assetsDirectory = config_.assetsDirectory,
        .enabled = !config_.noAudio,
        .masterVolume = static_cast<float>(config_.settings.audio.masterVolume),
        .musicVolume = static_cast<float>(config_.settings.audio.musicVolume),
        .sfxVolume = static_cast<float>(config_.settings.audio.sfxVolume),
    });
    if (!audio_->initialize()) {
        return false;
    }
    applyAudioSettings();

    imgui_ = std::make_unique<ui::ImGuiLayer>();
    if (!imgui_->initialize(window_, renderer_, uiScale_, config_.assetsDirectory)) {
        return false;
    }

    if ((config_.startInLevel || smokeScenarioNeedsGameplay()) && !loadSelectedLevel()) {
        return false;
    }
    screen_ = config_.startInLevel ? RuntimeScreen::InGame : RuntimeScreen::MainMenu;
    applySmokeScenario();
    updateMouseCapture();
    if (screen_ != RuntimeScreen::MainMenu) {
        preloadLevelSfx();
        playLevelMusic();
    } else if (config_.smokeFrames > 0) {
        // Skip loading screen in smoke tests — go straight to menu music.
        playMenuMusic();
    } else {
        // Show loading screen and play welcome sound.
        loadingScreenView_.emplace(ui::LoadingScreenView{std::random_device{}()});
        loadingScreenActive_ = true;
        welcomeSoundPath_ = assetRegistry_.menu().welcomeSound;
        if (audio_) {
            audio_->playSfx(welcomeSoundPath_);
        }
        core::Log::info("Loading screen started, welcome sound: " + welcomeSoundPath_.generic_string());
    }

    updateWindowTitle();
    clock_.reset();

    core::Log::info(
        "SDL window initialized: window=" + std::to_string(config_.windowWidth) + "x" + std::to_string(config_.windowHeight)
        + ", logical=" + std::to_string(config_.logicalWidth) + "x" + std::to_string(config_.logicalHeight)
        + ", fullscreen=" + (fullscreen_ ? "true" : "false"));

    startupMilliseconds_ = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - initializeStarted_).count();
    core::Log::info("Startup ready in " + std::to_string(startupMilliseconds_) + " ms");

    return true;
}

int SdlRuntime::run() {
    bool running = true;
    int framesRemaining = config_.smokeFrames;
    runStarted_ = std::chrono::steady_clock::now();

    while (running) {
        const std::uint64_t frameStartNs = SDL_GetTicksNS();

        auto delta = clock_.tick().count();
        delta = std::min(delta, maxFrameDeltaSeconds);
        accumulatorSeconds_ += delta;
        frameStats_.beginFrame(delta, accumulatorSeconds_);

        processEvents(running);

        while (accumulatorSeconds_ >= fixedDeltaSeconds) {
            fixedUpdate(fixedDeltaSeconds);
            accumulatorSeconds_ -= fixedDeltaSeconds;
        }

        const double alpha = std::clamp(accumulatorSeconds_ / fixedDeltaSeconds, 0.0, 1.0);
        frameStats_.endFrame(alpha);

        render();

        // FPS cap: sleep for remaining frame time if vsync is off and a limit is set.
        if (frameLimitUs_ > 0) {
            const std::uint64_t elapsed = (SDL_GetTicksNS() - frameStartNs) / 1000ull;
            if (elapsed < frameLimitUs_) {
                SDL_DelayNS((frameLimitUs_ - elapsed) * 1000ull);
            }
        }

        const double frameMilliseconds = frameStats_.frameSeconds * 1000.0;
        frameMillisecondsSum_ += frameMilliseconds;
        if (measuredFrames_ == 0) {
            frameMillisecondsMin_ = frameMilliseconds;
            frameMillisecondsMax_ = frameMilliseconds;
        } else {
            frameMillisecondsMin_ = std::min(frameMillisecondsMin_, frameMilliseconds);
            frameMillisecondsMax_ = std::max(frameMillisecondsMax_, frameMilliseconds);
        }
        ++measuredFrames_;

        if (config_.smokeFrames > 0 && --framesRemaining <= 0) {
            running = false;
        }
    }

    const double totalRuntimeMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - runStarted_).count();
    if (config_.perfSummary) {
        logPerformanceSummary(totalRuntimeMilliseconds);
    }

    core::Log::info(
        "SDL runtime stopped after " + std::to_string(frameStats_.frameCount)
        + " frames and " + std::to_string(frameStats_.updateCount) + " fixed updates");
    return smokeScenarioFailed_ ? 1 : 0;
}

void SdlRuntime::shutdown() {
    if (window_ != nullptr) {
        SDL_ShowCursor();
        SDL_SetWindowMouseGrab(window_, false);
        SDL_SetWindowMouseRect(window_, nullptr);
    }

    audio_.reset();
    imgui_.reset();
    assets_.reset();
    rendererApi_.reset();

    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

void SdlRuntime::processEvents(bool& running) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (imgui_) {
            imgui_->processEvent(event);
        }

        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            running = false;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            ++resizeEvents_;
            core::Log::debug(
                "Window resized: " + std::to_string(event.window.data1) + "x" + std::to_string(event.window.data2)
                + " (" + std::to_string(resizeEvents_) + " resize events)");
            updateMouseCapture();
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            focused_ = true;
            core::Log::info("Window focus gained");
            updateMouseCapture();
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            focused_ = false;
            mousePaddleActive_ = false;
            updateMouseCapture();
            core::Log::info("Window focus lost");
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (screen_ == RuntimeScreen::InGame && focused_ && !resumeCountdownActive_) {
                updateMousePaddleFromWindowX(event.motion.x);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (screen_ == RuntimeScreen::InGame && !resumeCountdownActive_) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    updateMousePaddleFromWindowX(event.button.x);
                    launchRequested_ = true;
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    shootPlasmaRequested_ = true;
                } else if (event.button.button == SDL_BUTTON_MIDDLE
                           || event.button.button == SDL_BUTTON_X1
                           || event.button.button == SDL_BUTTON_X2) {
                    turboBallMouseButtonPressed_ = true;
                }
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_MIDDLE
                || event.button.button == SDL_BUTTON_X1
                || event.button.button == SDL_BUTTON_X2) {
                turboBallMouseButtonPressed_ = false;
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            if (screen_ == RuntimeScreen::InGame && world_ && world_->phase() == gameplay::GamePhase::Playing && !resumeCountdownActive_) {
                turboBallPulseSeconds_ = 0.45;
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) {
                // During loading screen, block all key handling except ImGui
                // (LoadingScreenView catches ESC via ImGui::IsKeyPressed).
                if (loadingScreenActive_) {
                    break;
                }
                if (settingsOpen_ && settingsView_.handleKeyCapture(event.key.key)) {
                    break;
                }
                if ((settingsOpen_ || helpOpen_ || exitConfirmOpen_) && matchesKey(event.key.key, inputBindings_.pause)) {
                    break;
                }
                if (event.key.key == SDLK_Q) {
                    running = false;
                } else if (matchesKey(event.key.key, inputBindings_.pause)) {
                    if (screen_ == RuntimeScreen::InGame) {
                        pauseGame();
                    } else if (screen_ == RuntimeScreen::PauseMenu) {
                        pauseRequested_ = true;
                    } else if (settingsOpen_ || helpOpen_ || exitConfirmOpen_) {
                        // Menu overlays own Escape handling in the UI layer.
                    } else {
                        running = false;
                    }
                } else if (event.key.key == SDLK_F1) {
                    if (screen_ == RuntimeScreen::MainMenu) {
                        if (debugMenuOpen_ || debugBonusesMenuOpen_ || levelsMenuOpen_) {
                            debugMenuOpen_ = false;
                            debugBonusesMenuOpen_ = false;
                            levelsMenuOpen_ = false;
                            playUiSound(ui::UiSoundEffect::Back);
                            core::Log::info("Debug menu closed");
                        } else {
                            debugMenuOpen_ = true;
                            playUiSound(ui::UiSoundEffect::DebugOpen);
                            core::Log::info("Debug menu opened");
                        }
                    } else {
                        debugOverlayVisible_ = !debugOverlayVisible_;
                        core::Log::info(std::string("Debug overlay ") + (debugOverlayVisible_ ? "shown" : "hidden"));
                    }
                } else if (event.key.key == SDLK_F2) {
                    physicsDebugToggleRequested_ = true;
                } else if (event.key.key == SDLK_F3) {
                    assetStatsVisible_ = !assetStatsVisible_;
                    core::Log::info(std::string("Asset/audio stats overlay ") + (assetStatsVisible_ ? "shown" : "hidden"));
                    core::Log::info("Asset stats: " + textureStatsLine(assets_->stats()));
                    if (audio_) {
                        const auto stats = audio_->stats();
                        core::Log::info(
                            "Audio stats: initialized=" + std::string(stats.initialized ? "true" : "false")
                            + ", sfxLoaded=" + std::to_string(stats.sfxLoaded)
                            + ", currentMusic=" + stats.currentMusic);
                    }
                } else if (event.key.key == SDLK_F4) {
                    if (screen_ != RuntimeScreen::MainMenu) {
                        restartLevel();
                    }
                } else if (event.key.key == SDLK_P) {
                    if (screen_ == RuntimeScreen::InGame && world_) {
                        if (world_->hasBoss()) {
                            world_->reduceBossHealthToOnePercentForTesting();
                        } else {
                            world_->demolishBricksExceptOne();
                        }
                    }
                } else if (event.key.key == SDLK_O) {
                    if (screen_ == RuntimeScreen::InGame && world_) {
                        debugSpawnMenuOpen_ = !debugSpawnMenuOpen_;
                        updateMouseCapture();
                        if (debugSpawnMenuOpen_) {
                            playUiSound(ui::UiSoundEffect::DebugOpen);
                        } else {
                            playUiSound(ui::UiSoundEffect::Back);
                        }
                    }
                } else if (event.key.key == SDLK_F11 || (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT) != 0)) {
                    toggleFullscreen();
                } else if (matchesKey(event.key.key, inputBindings_.moveLeft, SDLK_A)) {
                    if (!resumeCountdownActive_) {
                        leftPressed_ = true;
                        mousePaddleActive_ = false;
                    }
                } else if (matchesKey(event.key.key, inputBindings_.moveRight, SDLK_D)) {
                    if (!resumeCountdownActive_) {
                        rightPressed_ = true;
                        mousePaddleActive_ = false;
                    }
                } else if (matchesKey(event.key.key, inputBindings_.launch)) {
                    if (!resumeCountdownActive_) {
                        spacePressed_ = true;
                        launchRequested_ = true;
                    }
                } else if (matchesKey(event.key.key, inputBindings_.callBall)) {
                    if (!resumeCountdownActive_ && screen_ == RuntimeScreen::InGame) {
                        callBallRequested_ = true;
                    }
                } else if (matchesKey(event.key.key, inputBindings_.plasma)) {
                    if (!resumeCountdownActive_ && screen_ == RuntimeScreen::InGame) {
                        shootPlasmaRequested_ = true;
                    }
                } else if (matchesKey(event.key.key, inputBindings_.turboBall)) {
                    if (!resumeCountdownActive_) {
                        turboBallPressed_ = true;
                    }
                }
            }
            break;

        case SDL_EVENT_KEY_UP:
            if (matchesKey(event.key.key, inputBindings_.moveLeft, SDLK_A)) {
                leftPressed_ = false;
            } else if (matchesKey(event.key.key, inputBindings_.moveRight, SDLK_D)) {
                rightPressed_ = false;
            } else if (matchesKey(event.key.key, inputBindings_.launch)) {
                spacePressed_ = false;
            } else if (matchesKey(event.key.key, inputBindings_.turboBall)) {
                turboBallPressed_ = false;
            }
            break;

        default:
            break;
        }
    }
}

void SdlRuntime::fixedUpdate(double) {
    frameStats_.recordUpdate();
    if (turboBallPulseSeconds_ > 0.0) {
        turboBallPulseSeconds_ = std::max(0.0, turboBallPulseSeconds_ - fixedDeltaSeconds);
    }
    if (audio_) {
        audio_->update(fixedDeltaSeconds);
    }

    if (screen_ == RuntimeScreen::LevelIntro) {
        levelIntroState_.elapsedSeconds += fixedDeltaSeconds;
        if (!levelIntroState_.sfxStarted) {
            levelIntroState_.sfxStarted = true;
            if (audio_ && !isIntroBossLevel(levelIntroState_.levelNumber)) {
                audio_->playSfx(levelIntroState_.sfxPath);
            }
        }
        if (levelIntroState_.durationSeconds - levelIntroState_.elapsedSeconds <= 0.5 && !levelIntroState_.fadeOutStarted) {
            levelIntroState_.fadeOutStarted = true;
            if (audio_) {
                audio_->fadeSfxOut(0.5);
            }
        }
        if (levelIntroState_.elapsedSeconds >= levelIntroState_.durationSeconds) {
            screen_ = RuntimeScreen::InGame;
            updateMouseCapture();
            preloadLevelSfx();
            playLevelMusic();
            core::Log::info("Level intro finished, starting gameplay for level " + std::to_string(config_.level));
        }
        return;
    }
    if (resumeCountdownActive_) {
        updateResumeCountdown(fixedDeltaSeconds);
    }
    if (paddleControlLockRemaining_ > 0.0) {
        paddleControlLockRemaining_ = std::max(0.0, paddleControlLockRemaining_ - fixedDeltaSeconds);
    }
    const bool shouldShowBonusTimers =
        world_
        && screen_ == RuntimeScreen::InGame
        && !resumeCountdownActive_
        && (world_->phase() == gameplay::GamePhase::Ready || world_->phase() == gameplay::GamePhase::Playing);
    const float timerFadeStep = static_cast<float>(fixedDeltaSeconds / 0.32);
    if (shouldShowBonusTimers) {
        bonusTimerHudVisibility_ = std::min(1.0f, bonusTimerHudVisibility_ + timerFadeStep);
    } else {
        bonusTimerHudVisibility_ = std::max(0.0f, bonusTimerHudVisibility_ - timerFadeStep);
    }
    if (world_ && screen_ == RuntimeScreen::InGame && !resumeCountdownActive_ && !debugSpawnMenuOpen_) {
        if (world_->phase() == gameplay::GamePhase::Playing) {
            levelPlayTime_ += fixedDeltaSeconds;
        }
        const bool paddleControlLocked = paddleControlLockRemaining_ > 0.0;
        world_->update(
            fixedDeltaSeconds,
            gameplay::GameInput{
                .moveLeft = !paddleControlLocked && leftPressed_,
                .moveRight = !paddleControlLocked && rightPressed_,
                .mousePaddleActive = !paddleControlLocked && mousePaddleActive_,
                .mousePaddleCenterX = mousePaddleCenterX_,
                .launchPressed = launchRequested_,
                .callBallPressed = callBallRequested_,
                .turboBallActive = turboBallPressed_ || turboBallMouseButtonPressed_ || turboBallPulseSeconds_ > 0.0,
                .pausePressed = pauseRequested_,
                .toggleDebugDrawPressed = physicsDebugToggleRequested_,
                .shootPlasmaPressed = shootPlasmaRequested_,
            },
            debugBonusesView_.getEnabledBonuses());
        handleAudioEvents();

        if (world_->phase() == gameplay::GamePhase::Playing) {
            for (const auto& bonus : world_->fallingBonuses()) {
                if (bonus.alive) {
                    unsigned char r = 100, g = 255, b = 100;
                    if (bonus.type == "BONUS_SCORE") {
                        r = 255; g = 200; b = 50;
                    } else if (bonus.type == "BONUS_SCORE_200") {
                        r = 50; g = 200; b = 255;
                    } else if (bonus.type == "BONUS_SCORE_500") {
                        r = 255; g = 80; b = 200;
                    } else if (bonus.type == "BONUS_SCORE_10000") {
                        r = 255; g = 170; b = 170;
                    } else if (bonus.type == "EXTRA_LIFE") {
                        r = 255; g = 120; b = 0;
                    } else if (bonus.type == "BONUS_BALL") {
                        r = 170; g = 255; b = 0;
                    } else if (bonus.type == "CALL_BALL") {
                        r = 90; g = 235; b = 255;
                    } else if (bonus.type == "SLOW_BALLS") {
                        r = 0; g = 191; b = 255;
                    } else if (bonus.type == "FAST_BALLS") {
                        r = 255; g = 50; b = 50;
                    } else if (bonus.type == "SCORE_RAIN") {
                        r = 255; g = 215; b = 0;
                    } else if (bonus.type == "WEAK_BALLS") {
                        r = 160; g = 160; b = 160;
                    } else if (bonus.type == "ENERGY_BALLS") {
                        r = 180; g = 0; b = 255;
                    } else if (bonus.type == "EXPLOSION_BALLS") {
                        r = 255; g = 96; b = 30;
                    } else if (bonus.type == "INCREASE_PADDLE") {
                        r = 0; g = 255; b = 128;
                    } else if (bonus.type == "DECREASE_PADDLE") {
                        r = 255; g = 50; b = 50;
                    } else if (bonus.type == "STICKY_PADDLE") {
                        r = 50; g = 220; b = 50;
                    } else if (bonus.type == "FROZEN_PADDLE") {
                        r = 160; g = 220; b = 255;
                    } else if (bonus.type == "INVISIBLE_PADDLE") {
                        r = 200; g = 170; b = 255;
                    } else if (bonus.type == "BONUS_WALL") {
                        r = 0; g = 180; b = 255;
                    } else if (bonus.type == "DARKNESS") {
                        r = 60; g = 30; b = 100;
                    } else if (bonus.type == "CHAOTIC_BALLS") {
                        r = 255; g = 60; b = 180;
                    } else if (bonus.type == "BONUS_MAGNET") {
                        r = 0; g = 255; b = 200;
                    } else if (bonus.type == "PENALTIES_MAGNET") {
                        r = 255; g = 100; b = 0;
                    } else if (bonus.type == "BAD_LUCK") {
                        r = 220; g = 0; b = 30;
                    } else if (bonus.type == "TRICKSTER") {
                        r = 255; g = 215; b = 0;
                    } else if (bonus.type == "ADD_FIVE_SECONDS") {
                        r = 50; g = 255; b = 150;
                    } else if (bonus.type == "RESET") {
                        r = 220; g = 220; b = 220;
                    } else if (bonus.type == "RANDOM_BONUS") {
                        r = 240; g = 200; b = 40;
                    } else if (bonus.type == "LEVEL_PASS") {
                        r = 255; g = 215; b = 0;
                    }
                    
                    std::uniform_real_distribution<float> vxDist(-15.0f, 15.0f);
                    std::uniform_real_distribution<float> vyDist(-35.0f, -10.0f);
                    std::uniform_real_distribution<float> sizeDist(3.0f, 6.0f);
                    std::uniform_real_distribution<float> durDist(0.4f, 0.7f);

                    gameplay::Vec2 startPos{
                        bonus.position.x + bonus.size.w * 0.5f,
                        bonus.position.y + bonus.size.h * 0.5f
                    };

                    int particleCount = (bonus.type == "LEVEL_PASS") ? 2 : ((bonus.type == "BONUS_SCORE_10000" || bonus.type == "RAINBOW_BOUNTY" || bonus.type == "BLOOD_TITHE") ? 3 : 1);
                    for (int p = 0; p < particleCount; ++p) {
                        unsigned char pr = r, pg = g, pb = b;
                        if (bonus.type == "LEVEL_PASS") {
                            if (std::uniform_int_distribution<int>(0, 1)(effectRng_) == 0) {
                                pr = 255; pg = 215; pb = 0; // Gold
                            } else {
                                pr = 0; pg = 255; pb = 255; // Cyan
                            }
                        } else if (bonus.type == "BONUS_SCORE_10000") {
                            static constexpr std::array<std::tuple<unsigned char, unsigned char, unsigned char>, 6> pastelRainbow{{
                                {255, 170, 170}, // Pastel Red/Pink
                                {255, 210, 170}, // Pastel Orange
                                {255, 255, 170}, // Pastel Yellow
                                {170, 255, 170}, // Pastel Green
                                {170, 220, 255}, // Pastel Blue
                                {220, 170, 255}  // Pastel Purple/Lavender
                            }};
                            std::uniform_int_distribution<std::size_t> colorDist(0, pastelRainbow.size() - 1);
                            auto [pr_c, pg_c, pb_c] = pastelRainbow[colorDist(effectRng_)];
                            pr = pr_c;
                            pg = pg_c;
                            pb = pb_c;
                        } else if (bonus.type == "RAINBOW_BOUNTY") {
                            static constexpr std::array<std::tuple<unsigned char, unsigned char, unsigned char>, 6> vibrantRainbow{{
                                {255, 30, 30},    // Red
                                {255, 130, 30},   // Orange
                                {255, 230, 30},   // Yellow
                                {30, 255, 30},    // Green
                                {30, 220, 255},   // Cyan
                                {180, 50, 255}    // Purple
                            }};
                            std::uniform_int_distribution<std::size_t> colorDist(0, vibrantRainbow.size() - 1);
                            auto [pr_c, pg_c, pb_c] = vibrantRainbow[colorDist(effectRng_)];
                            pr = pr_c;
                            pg = pg_c;
                            pb = pb_c;
                        } else if (bonus.type == "BLOOD_TITHE") {
                            static constexpr std::array<std::tuple<unsigned char, unsigned char, unsigned char>, 3> bloodColors{{
                                {128, 0, 32},    // Burgundy
                                {170, 0, 0},     // Dark Red
                                {80, 0, 20}      // Almost black red
                            }};
                            std::uniform_int_distribution<std::size_t> colorDist(0, bloodColors.size() - 1);
                            auto [pr_c, pg_c, pb_c] = bloodColors[colorDist(effectRng_)];
                            pr = pr_c;
                            pg = pg_c;
                            pb = pb_c;
                            if (std::uniform_real_distribution<float>(0.0f, 1.0f)(effectRng_) < 0.2f) {
                                pr = 255; pg = 30; pb = 30; // Scarlet/bright red
                            }
                        }
                        
                        float size = sizeDist(effectRng_);
                        float vx = vxDist(effectRng_) * ((bonus.type == "LEVEL_PASS") ? 1.5f : 1.0f);
                        float vy = vyDist(effectRng_) * ((bonus.type == "LEVEL_PASS") ? 1.5f : 1.0f);
                        float dur = durDist(effectRng_);

                        bonusParticles_.push_back(BonusParticle{
                            .position = startPos,
                            .velocity = gameplay::Vec2{vx, vy},
                            .size = size,
                            .age = 0.0,
                            .duration = static_cast<double>(dur),
                            .r = pr,
                            .g = pg,
                            .b = pb,
                            .isTrail = true
                        });
                    }
                }
            }
        }
    }
    if (!debugSpawnMenuOpen_) {
        updateBrickImpactEffects(fixedDeltaSeconds);
        updateBonusEffects(fixedDeltaSeconds);
        updateGameOverState(fixedDeltaSeconds);
        updateLevelCompleteState(fixedDeltaSeconds);
    }
    launchRequested_ = false;
    callBallRequested_ = false;
    shootPlasmaRequested_ = false;
    pauseRequested_ = false;
    physicsDebugToggleRequested_ = false;
}

void SdlRuntime::render() {
    updateSmokeCycle();
    imgui_->beginFrame();
    const bool gameOverCursor = world_ && world_->phase() == gameplay::GamePhase::GameOver;
    const bool levelCompleteCursor = levelCompleteActive_ && levelCompleteAge_ >= levelCompleteFadeOutDuration_;
    if (screen_ == RuntimeScreen::InGame && !gameOverCursor && !levelCompleteCursor && !debugSpawnMenuOpen_) {
        SDL_HideCursor();
    } else {
        SDL_ShowCursor();
    }

    if (screen_ == RuntimeScreen::MainMenu) {
        renderMainMenuScene();
    } else if (screen_ == RuntimeScreen::LevelIntro) {
        renderLevelIntro();
    } else {
        renderGameScene();
    }

    if (screen_ == RuntimeScreen::MainMenu) {
        // Ensure menu background texture is loaded.
        if (menuBackgroundTexture_ == nullptr) {
            menuBackgroundTexture_ = assets_->texture(assetRegistry_.menu().background.generic_string());
        }

        if (loadingScreenActive_ && loadingScreenView_) {
            // ── Loading screen overlay (blocks menu interaction) ──
            // Lazy-load textures for loading screen.
            if (gameLogoTexture_ == nullptr) {
                gameLogoTexture_ = assets_->texture("sprites/arcadeblocks2_logo.png");
            }
            if (studioLogoTexture_ == nullptr) {
                studioLogoTexture_ = assets_->texture("sprites/Leocallidus_games_logo.png");
            }

            const ui::LoadingScreenAssets loadAssets{
                .gameLogo = gameLogoTexture_,
                .studioLogo = studioLogoTexture_,
                .menuBackground = menuBackgroundTexture_,
            };
            const auto loadResult = loadingScreenView_->render(
                loadAssets, runtimeLocalization(config_), config_.settings.language);

            if (loadResult.dismissed) {
                // Loading screen is fully done — transition to main menu.
                loadingScreenActive_ = false;
                loadingScreenView_.reset();
                // Fade out welcome sound, then start menu music.
                if (audio_) {
                    audio_->fadeSfxOut(0.5);
                }
                playMenuMusic();
                core::Log::info("Loading screen finished, transitioning to main menu");
            }
        } else {
            // ── Normal main menu rendering ──
            const bool helpWasOpen = helpOpen_;
            const ui::MainMenuSceneAssets sceneAssets{
                .logo = nullptr,
                .background = menuBackgroundTexture_,
            };
            const auto action = mainMenuView_.render(sceneAssets, runtimeLocalization(config_), config_.settings.language, settingsOpen_, helpOpen_, exitConfirmOpen_);
            playUiSound(action.soundEffect);
            bool running = true;
            handleMainMenuAction(action, running);
            if (!running) {
                SDL_Event quit{};
                quit.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&quit);
            }
            if (settingsOpen_) {
                const auto settingsAction = settingsView_.render(runtimeLocalization(config_), config_.settings, ui::SettingsContext::MainMenu);
                playUiSound(settingsAction.soundEffect);
                handleSettingsAction(settingsAction);
            } else {
                settingsView_.resetSession();
            }
            if (helpOpen_) {
                if (gameplayAtlas_ == nullptr) {
                    gameplayAtlas_ = assets_->spriteAtlas();
                }
                if (gameplayAtlasTexture_ == nullptr) {
                    gameplayAtlasTexture_ = assets_->spriteAtlasTexture();
                }
                const ui::HelpViewAssets helpAssets{
                    .atlas = gameplayAtlas_,
                    .atlasTexture = gameplayAtlasTexture_,
                };
                const auto helpAction = helpView_.render(runtimeLocalization(config_), config_.settings.language, config_.settings, helpAssets, false, !helpWasOpen);
                playUiSound(helpAction.soundEffect);
                handleHelpAction(helpAction);
            }

            static bool debugMenuVisible = false;
            static bool debugBonusesMenuVisible = false;
            static bool levelsMenuVisible = false;

            if (debugBackgroundTexture_ == nullptr &&
                (debugMenuOpen_ || debugMenuVisible ||
                 debugBonusesMenuOpen_ || debugBonusesMenuVisible ||
                 levelsMenuOpen_ || levelsMenuVisible)) {
                debugBackgroundTexture_ = assets_->texture("sprites/debug.jpeg");
            }

            if (debugMenuOpen_ || debugMenuVisible) {
                const auto debugAction = debugMenuView_.render(
                    debugBackgroundTexture_,
                    runtimeLocalization(config_),
                    config_.settings.language,
                    debugMenuOpen_);
                playUiSound(debugAction.soundEffect);
                debugMenuVisible = debugAction.isVisible;
                if (debugAction.action == ui::DebugMenuAction::OpenLevels) {
                    debugMenuOpen_ = false;
                    levelsMenuOpen_ = true;
                } else if (debugAction.action == ui::DebugMenuAction::OpenBonuses) {
                    debugMenuOpen_ = false;
                    debugBonusesMenuOpen_ = true;
                }
            }

            if (debugBonusesMenuOpen_ || debugBonusesMenuVisible) {
                const auto debugBonusesAction = debugBonusesView_.render(
                    debugBackgroundTexture_,
                    runtimeLocalization(config_),
                    config_.settings.language,
                    debugBonusesMenuOpen_);
                playUiSound(debugBonusesAction.soundEffect);
                debugBonusesMenuVisible = debugBonusesAction.isVisible;
                if (debugBonusesAction.action == ui::DebugBonusesAction::Back && !debugBonusesMenuOpen_) {
                    debugMenuOpen_ = true;
                    playUiSound(ui::UiSoundEffect::DebugOpen);
                }
            }

            if (levelsMenuOpen_ || levelsMenuVisible) {
                const auto levelsAction = levelsMenuView_.render(
                    debugBackgroundTexture_,
                    assetRegistry_,
                    runtimeLocalization(config_),
                    config_.settings.language,
                    levelsMenuOpen_);
                playUiSound(levelsAction.soundEffect);
                levelsMenuVisible = levelsAction.isVisible;
                if (levelsAction.action == ui::LevelsMenuAction::StartLevel) {
                    startLevel(levelsAction.selectedLevel);
                } else if (levelsAction.action == ui::LevelsMenuAction::Close && !levelsMenuOpen_) {
                    debugMenuOpen_ = true;
                    playUiSound(ui::UiSoundEffect::DebugOpen);
                }
            }
        }
    } else if (screen_ == RuntimeScreen::LevelIntro) {
        // Do nothing during level intro
    } else {
        renderHud();
        renderGameOverOverlay();
        renderLevelCompleteOverlay();
        renderResumeCountdownOverlay();
        if (screen_ == RuntimeScreen::PauseMenu) {
            const bool helpWasOpen = helpOpen_;
            const bool pauseBlocked = settingsOpen_ || helpOpen_;
            const bool openedThisFrame = !pauseMenuWasOpen_;
            pauseMenuWasOpen_ = true;
            const auto action = pauseView_.render(runtimeLocalization(config_), config_.settings.language, pauseBlocked, openedThisFrame, pauseRequested_);
            pauseRequested_ = false;
            playUiSound(action.soundEffect);
            handlePauseAction(action);
            if (settingsOpen_) {
                const auto settingsAction = settingsView_.render(runtimeLocalization(config_), config_.settings, ui::SettingsContext::Pause);
                playUiSound(settingsAction.soundEffect);
                handleSettingsAction(settingsAction);
            } else {
                settingsView_.resetSession();
            }
            if (helpOpen_) {
                const ui::HelpViewAssets helpAssets{
                    .atlas = gameplayAtlas_,
                    .atlasTexture = gameplayAtlasTexture_,
                };
                const auto helpAction = helpView_.render(runtimeLocalization(config_), config_.settings.language, config_.settings, helpAssets, true, !helpWasOpen);
                playUiSound(helpAction.soundEffect);
                handleHelpAction(helpAction);
            }
        } else {
            pauseMenuWasOpen_ = false;
        }
    }

    bool wasSpawnOpen = debugSpawnMenuOpen_;
    if (debugSpawnMenuOpen_ && screen_ == RuntimeScreen::InGame && world_) {
        ImGui::SetNextWindowSize(ImVec2{380.0f, 600.0f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Debug Spawn Bonus (O)", &debugSpawnMenuOpen_)) {
            ImGui::Text("Click to spawn bonus at top center:");
            ImGui::Separator();
            
            struct SpawnInfo {
                const char* type;
                const char* nameEn;
                const char* nameRu;
            };

            static const std::array<SpawnInfo, 32> spawnList{{
                {"BONUS_SCORE", "Extra Score", "Дополнительные очки"},
                {"BONUS_SCORE_200", "Score +200", "+200 очков"},
                {"BONUS_SCORE_500", "Score +500", "+500 очков"},
                {"BONUS_SCORE_10000", "Score +10000", "+10000 очков"},
                {"ADD_FIVE_SECONDS", "Add 5 Seconds", "+5 секунд"},
                {"CALL_BALL", "Call Ball", "Притяжение мяча"},
                {"EXTRA_LIFE", "Extra Life", "Дополнительная жизнь"},
                {"INCREASE_PADDLE", "Increase Paddle", "Увеличение ракетки"},
                {"STICKY_PADDLE", "Sticky Paddle", "Липкая ракетка"},
                {"SLOW_BALLS", "Slow Balls", "Замедление мячей"},
                {"ENERGY_BALLS", "Energy Balls", "Энергетические мячи"},
                {"BONUS_WALL", "Safety Wall", "Защитный барьер"},
                {"BONUS_MAGNET", "Bonus Magnet", "Магнит бонусов"},
                {"BONUS_BALL", "Extra Ball", "Дополнительный мяч"},
                {"PLASMA_WEAPON", "Plasma Weapon", "Плазменная пушка"},
                {"EXPLOSION_BALLS", "Explosive Balls", "Взрывные мячи"},
                {"TRICKSTER", "Trickster", "Шулер"},
                {"SCORE_RAIN", "Score Rain", "Дождь очков"},
                {"LEVEL_PASS", "Level Pass", "Проход уровня"},
                {"RAINBOW_BOUNTY", "Rainbow Bounty", "Радужная щедрость"},
                {"BLOOD_TITHE", "Blood Tithe", "Кровавая дань"},
                {"CHAOTIC_BALLS", "Chaotic Trajectory", "Хаотичные мячи"},
                {"FROZEN_PADDLE", "Frozen Paddle", "Замороженная ракетка"},
                {"DECREASE_PADDLE", "Decrease Paddle", "Уменьшение ракетки"},
                {"FAST_BALLS", "Fast Balls", "Ускорение мячей"},
                {"PENALTIES_MAGNET", "Penalty Magnet", "Магнит штрафов"},
                {"WEAK_BALLS", "Weak Balls", "Слабые мячи"},
                {"INVISIBLE_PADDLE", "Invisible Paddle", "Призрачная ракетка"},
                {"DARKNESS", "Darkness", "Темнота"},
                {"BAD_LUCK", "Bad Luck", "Невезуха"},
                {"RESET", "Reset Bonuses", "Сброс бонусов"},
                {"RANDOM_BONUS", "Random Bonus", "Случайный бонус"}
            }};

            bool isRussian = (config_.settings.language == settings::Language::Russian);
            
            for (const auto& item : spawnList) {
                const char* label = isRussian ? item.nameRu : item.nameEn;
                if (ImGui::Button(label, ImVec2{-1.0f, 0.0f})) {
                    float spawnX = world_->paddle().position.x + world_->paddle().size.w * 0.5f;
                    float spawnY = 150.0f;
                    world_->spawnBonus(item.type, gameplay::Vec2{spawnX, spawnY});
                    playUiSound(ui::UiSoundEffect::Select);
                }
            }
        }
        ImGui::End();
    }
    if (wasSpawnOpen != debugSpawnMenuOpen_) {
        updateMouseCapture();
        if (!debugSpawnMenuOpen_) {
            playUiSound(ui::UiSoundEffect::Back);
        }
    }

    imgui_->endFrame();
    rendererApi_->endFrame();
    logAssetStatsOnce();

    if (frameStats_.frameCount % 30 == 0) {
        updateWindowTitle();
    }
}

bool SdlRuntime::loadSelectedLevel() {
    currentLevelAssets_ = assetRegistry_.level(config_.level);
    levelBackgroundTexture_ = nullptr;
    gameplayAtlasTexture_ = nullptr;
    if (const auto missing = assetRegistry_.firstMissingRequiredAsset(currentLevelAssets_)) {
        core::Log::error("Missing required MVP asset: " + assetRegistry_.resolve(*missing).string());
        return false;
    }

    gameplayAtlas_ = assets_->spriteAtlas();
    if (gameplayAtlas_ != nullptr) {
        for (const auto& [_, sprite] : currentLevelAssets_.brickSprites) {
            if (!gameplayAtlas_->contains(sprite)) {
                core::Log::error("Missing MVP brick sprite in atlas: " + sprite);
                return false;
            }
        }
        if (!gameplayAtlas_->contains("paddle.png") || !gameplayAtlas_->contains("ball.png")) {
            core::Log::error("Missing required MVP gameplay sprite in atlas: paddle.png or ball.png");
            return false;
        }
    } else {
        core::Log::error("Failed to load sprite atlas metadata for MVP asset validation");
        return false;
    }

    if (isIntroBossLevel(config_.level)) {
        world_ = std::move(*gameplay::GameWorld::createBossLevel(config_.level));
        world_->setPaddleSpeed(config_.settings.gameplay.paddleSpeed);
        world_->setTurboBallSpeed(config_.settings.gameplay.turboSpeed);

        auto result = levels::loadLevelDefinition(assetRegistry_.resolve(currentLevelAssets_.levelJson));
        if (result.level) {
            levelName_ = config_.settings.language == settings::Language::Russian 
                ? result.level->metadata.name_ru 
                : result.level->metadata.name_us;
            if (levelName_.empty()) levelName_ = result.level->metadata.name;
        } else {
            levelName_ = config_.level == 20 ? "\"Kaira\" (City Architect)" : "Firewall-7";
        }
        
        core::Log::info("Loaded BOSS level " + std::to_string(config_.level));
        return true;
    }

    auto result = levels::loadLevelDefinition(assetRegistry_.resolve(currentLevelAssets_.levelJson));
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.severity == levels::DiagnosticSeverity::Error) {
            core::Log::error(diagnostic.message);
        } else {
            core::Log::warn(diagnostic.message);
        }
    }

    if (!result.ok()) {
        core::Log::error("Failed to load classic level " + std::to_string(config_.level)
            + " from " + currentLevelAssets_.levelJson.generic_string());
        return false;
    }

    if (config_.level == 31) {
        levelName_ = "отладка";
    } else {
        if (config_.settings.language == settings::Language::Russian) {
            levelName_ = !result.level->metadata.name_ru.empty() ? result.level->metadata.name_ru : result.level->metadata.name;
        } else {
            levelName_ = !result.level->metadata.name_us.empty() ? result.level->metadata.name_us : result.level->metadata.name;
        }
    }
    world_ = gameplay::GameWorld::fromLevel(*result.level);
    world_->setPaddleSpeed(config_.settings.gameplay.paddleSpeed);
    world_->setTurboBallSpeed(config_.settings.gameplay.turboSpeed);
    core::Log::info(
        "Loaded classic level " + std::to_string(config_.level) + ": '" + levelName_
        + "', bricks=" + std::to_string(world_->bricks().size())
        + ", entities=" + std::to_string(world_->entities().size()));
    return true;
}

void SdlRuntime::startLevel(int level) {
    bool fromDebugMenu = levelsMenuOpen_ || debugMenuOpen_ || lastStartWasDebugMenu_;
    lastStartWasDebugMenu_ = fromDebugMenu;

    config_.level = level;
    if (!loadSelectedLevel()) {
        screen_ = RuntimeScreen::MainMenu;
        return;
    }

    leftPressed_ = false;
    rightPressed_ = false;
    spacePressed_ = false;
    mousePaddleActive_ = false;
    launchRequested_ = false;
    callBallRequested_ = false;
    shootPlasmaRequested_ = false;
    pauseRequested_ = false;
    physicsDebugToggleRequested_ = false;
    turboBallPressed_ = false;
    turboBallMouseButtonPressed_ = false;
    turboBallPulseSeconds_ = 0.0;
    settingsOpen_ = false;
    helpOpen_ = false;
    exitConfirmOpen_ = false;
    debugMenuOpen_ = false;
    levelsMenuOpen_ = false;
    debugBonusesMenuOpen_ = false;
    continuePurchaseCount_ = 0;
    bonusTimerHudVisibility_ = 0.0f;
    resetGameOverState();
    resetLevelCompleteState();
    accumulatorSeconds_ = 0.0;
    
    initLevelIntro(fromDebugMenu);
}

std::filesystem::path SdlRuntime::resolveLevelIntroSfx(int level) const {
    std::string nStr = std::to_string(level);
    std::vector<std::string> candidates = {
        "sounds/level_loadings/level_loading" + nStr + ".ogg",
        "sounds/level_loadings/level" + nStr + "_loading.ogg",
        "sounds/level_loadings/level_loading.ogg"
    };

    for (const auto& candidate : candidates) {
        std::filesystem::path fullPath = config_.assetsDirectory / candidate;
        std::error_code ec;
        if (std::filesystem::exists(fullPath, ec)) {
            return candidate;
        }
    }
    return "";
}

void SdlRuntime::initLevelIntro(bool fromDebugMenu) {
    levelIntroState_.levelNumber = config_.level;
    levelIntroState_.levelName = levelName_;
    levelIntroState_.chapterIdx = getIntroChapterIndex(config_.level);
    levelIntroState_.chapterAccent = getIntroChapterAccent(levelIntroState_.chapterIdx);
    levelIntroState_.chapterTitle = getIntroChapterTitle(levelIntroState_.chapterIdx, runtimeLocalization(config_), config_.settings.language);
    levelIntroState_.fromDebugMenu = fromDebugMenu;
    levelIntroState_.elapsedSeconds = 0.0;
    levelIntroState_.sfxStarted = false;
    levelIntroState_.fadeOutStarted = false;

    levelIntroState_.durationSeconds = 3.0;

    levelIntroState_.sfxPath = resolveLevelIntroSfx(config_.level);

    if (audio_) {
        audio_->stopMusic();
        audio_->stopAllSfx();
    }

    bool isBoss = isIntroBossLevel(config_.level);

    if (isBoss && audio_) {
        int bossNumber = config_.level / 10;
        std::string sfxName = "sounds/boss/boss_loading" + std::to_string(bossNumber) + ".ogg";
        if (assetRegistry_.exists(sfxName)) {
            audio_->playSfx(sfxName);
        } else {
            audio_->playSfx("sounds/boss/boss_loading1.ogg");
        }
        levelIntroState_.sfxStarted = true;
    } else if (!isBoss && !levelIntroState_.sfxPath.empty() && audio_) {
        double soundDuration = audio_->getSfxDuration(levelIntroState_.sfxPath);
        if (soundDuration > 0.0) {
            levelIntroState_.durationSeconds = soundDuration;
        }
    }

    screen_ = RuntimeScreen::LevelIntro;
    core::Log::info("Initialized level intro for level " + std::to_string(config_.level) + ", duration: " + std::to_string(levelIntroState_.durationSeconds) + "s, path: " + levelIntroState_.sfxPath.generic_string());
}

void SdlRuntime::restartLevel() {
    if (!loadSelectedLevel()) {
        screen_ = RuntimeScreen::MainMenu;
        return;
    }

    mousePaddleActive_ = false;
    callBallRequested_ = false;
    shootPlasmaRequested_ = false;
    turboBallPressed_ = false;
    turboBallMouseButtonPressed_ = false;
    turboBallPulseSeconds_ = 0.0;
    paddleControlLockRemaining_ = 0.0;
    continuePurchaseCount_ = 0;
    bonusTimerHudVisibility_ = 0.0f;
    resetGameOverState();
    resetLevelCompleteState();
    accumulatorSeconds_ = 0.0;
    
    initLevelIntro(lastStartWasDebugMenu_);
}

void SdlRuntime::pauseGame() {
    if (!world_ || screen_ != RuntimeScreen::InGame) {
        return;
    }
    if (world_->phase() == gameplay::GamePhase::GameOver) {
        return;
    }
    if (resumeCountdownActive_) {
        return;
    }

    world_->requestPause();
    if (audio_) {
        audio_->pauseAll(0.6);
    }
    screen_ = RuntimeScreen::PauseMenu;
    mousePaddleActive_ = false;
    callBallRequested_ = false;
    shootPlasmaRequested_ = false;
    turboBallPressed_ = false;
    turboBallMouseButtonPressed_ = false;
    turboBallPulseSeconds_ = 0.0;
    paddleControlLockRemaining_ = 0.0;
    updateMouseCapture();
    core::Log::info("Game paused");
}

void SdlRuntime::resumeGame() {
    if (!world_ || screen_ != RuntimeScreen::PauseMenu) {
        return;
    }
    if (resumeCountdownActive_) {
        return;
    }

    screen_ = RuntimeScreen::InGame;
    resumeCountdownActive_ = true;
    resumeCountdownRemaining_ = resumeCountdownDuration();
    resumeCountdownLastTick_ = -1;
    updateMouseCapture();
    core::Log::info("Resume countdown started");
}

double SdlRuntime::resumeCountdownDuration() const {
    return 5.0;
}

void SdlRuntime::updateResumeCountdown(double deltaSeconds) {
    if (!resumeCountdownActive_) {
        return;
    }

    resumeCountdownRemaining_ = std::max(0.0, resumeCountdownRemaining_ - deltaSeconds);

    const int total = static_cast<int>(std::ceil(resumeCountdownDuration()));
    const int elapsed = static_cast<int>(std::floor(resumeCountdownDuration() - resumeCountdownRemaining_));
    const int currentTick = std::clamp(elapsed, 0, total);
    if (currentTick != resumeCountdownLastTick_) {
        resumeCountdownLastTick_ = currentTick;
        if (audio_) {
            audio_->playSfx("sounds/menu/menu_select.ogg");
        }
    }

    if (resumeCountdownRemaining_ <= 0.0) {
        resumeCountdownActive_ = false;
        resumeCountdownLastTick_ = -1;
        leftPressed_ = false;
        rightPressed_ = false;
        mousePaddleActive_ = false;
        paddleControlLockRemaining_ = paddleControlLockAfterResumeSeconds;
        if (world_) {
            world_->resumeFromPause();
        }
        if (audio_) {
            audio_->resumeAll(0.4);
        }
        core::Log::info("Resume countdown finished");
    }
}

void SdlRuntime::exitToMenu() {
    if (world_ && world_->phase() == gameplay::GamePhase::Paused) {
        world_->resumeFromPause();
    }
    settingsOpen_ = false;
    helpOpen_ = false;
    exitConfirmOpen_ = false;
    resetGameOverState();
    resetLevelCompleteState();
    mousePaddleActive_ = false;
    screen_ = RuntimeScreen::MainMenu;
    updateMouseCapture();
    if (audio_) {
        audio_->stopAllSfx();
    }
    playMenuMusic();
    core::Log::info("Exited to main menu");
}

void SdlRuntime::playMenuMusic() {
    if (audio_) {
        audio_->playMusic(assetRegistry_.menu().music, true);
    }
}

void SdlRuntime::playLevelMusic() {
    if (!audio_) {
        return;
    }

    audio_->playMusic(currentLevelAssets_.music, true);
}

void SdlRuntime::playGameOverMusic() {
    if (audio_) {
        audio_->playMusic("music/gameplay/game_over.ogg", true);
    }
}

void SdlRuntime::preloadLevelSfx() {
    if (audio_) {
        audio_->preloadSfx(currentLevelAssets_.preloadSfx);
    }
}

void SdlRuntime::handleAudioEvents() {
    if (!world_) {
        return;
    }

    for (const auto& event : world_->consumeAudioEvents()) {
        spawnBrickImpactEffect(event);
        spawnLifeLostEffect(event);
        if (audio_) {
            playAudioEvent(event);
        }
    }
}

void SdlRuntime::playAudioEvent(const gameplay::AudioEvent& event) {
    const float pan = panFromLogicalX(event.position.x);
    const auto randomPath = [this]<std::size_t Count>(const std::array<const char*, Count>& paths) {
        std::uniform_int_distribution<std::size_t> dist(0, paths.size() - 1);
        return std::filesystem::path{paths[dist(effectRng_)]};
    };

    if (event.type == gameplay::AudioEventType::PlasmaShot) {
        audio_->playSfx("sounds/bonus_effects/plasma_shot.ogg", pan);
        return;
    }

    if (event.type == gameplay::AudioEventType::PlasmaBrickHit) {
        static constexpr std::array<const char*, 2> hitPaths{{
            "sounds/gameplay/plasma_brick_hit1.ogg",
            "sounds/gameplay/plasma_brick_hit2.ogg",
        }};
        std::uniform_real_distribution<float> pitchDist(0.85f, 1.25f);
        float pitch = pitchDist(effectRng_);
        audio_->playSfxWithPitch(randomPath(hitPaths), pan, pitch);
        return;
    }

    if (event.type == gameplay::AudioEventType::BrickBreak) {
        if (event.detail.rfind("silent", 0) == 0) {
            return;
        }
        if (world_ && world_->isBonusActive("ENERGY_BALLS")) {
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEnergyBallHitTime_).count() >= 60) {
                lastEnergyBallHitTime_ = now;
                std::uniform_real_distribution<float> pitchDist(0.85f, 1.25f);
                float pitch = pitchDist(effectRng_);
                audio_->playSfxWithPitch("sounds/gameplay/energy_ball_brick_hit.ogg", pan, pitch);
            }
            return;
        }
        static constexpr std::array<const char*, 4> paths{{
            "sounds/gameplay/brick_break.ogg",
            "sounds/gameplay/brick_break2.ogg",
            "sounds/gameplay/brick_break3.ogg",
            "sounds/gameplay/brick_break4.ogg",
        }};
        audio_->playSfx(randomPath(paths), pan);
        return;
    }

    if (event.type == gameplay::AudioEventType::Explosion) {
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastExplosionTime_).count() >= 80) {
            lastExplosionTime_ = now;
            if (world_ && world_->isBonusActive("EXPLOSION_BALLS")) {
                std::uniform_real_distribution<float> pitchDist(0.85f, 1.25f);
                float pitch = pitchDist(effectRng_);
                audio_->playSfxWithPitch("sounds/gameplay/explosion.ogg", pan, pitch);
            } else {
                audio_->playSfx("sounds/gameplay/explosion.ogg", pan);
            }
        }
        return;
    }

    if (event.type == gameplay::AudioEventType::BrickHit) {
        static constexpr std::array<const char*, 2> paths{{
            "sounds/gameplay/no_dest_brick1.ogg",
            "sounds/gameplay/no_dest_brick2.ogg",
        }};
        audio_->playSfx(randomPath(paths), pan);
        return;
    }

    if (event.type == gameplay::AudioEventType::LifeLost) {
        livesLostThisLevel_++;
    }

    if (event.type == gameplay::AudioEventType::LifeLost && world_ && world_->lives() <= 1) {
        audio_->playSfx("sounds/gameplay/small_count_lives.ogg", pan);
        return;
    }

    if (event.type == gameplay::AudioEventType::LifeLost) {
        static constexpr std::array<const char*, 4> paths{{
            "sounds/gameplay/life_lost.ogg",
            "sounds/gameplay/life_lost2.ogg",
            "sounds/gameplay/life_lost3.ogg",
            "sounds/gameplay/life_lost4.ogg",
        }};
        audio_->playSfx(randomPath(paths), pan);
        return;
    }

    if (event.type == gameplay::AudioEventType::GameOver) {
        audio_->playSfx("sounds/gameplay/game_over.ogg", pan);
        return;
    }

    if (event.type == gameplay::AudioEventType::LevelComplete) {
        return;
    }

    if (event.type == gameplay::AudioEventType::CallBallPaddle) {
        if (config_.settings.audio.callBallSound) {
            audio_->playSfx("sounds/bonus_effects/call_ball_paddle.ogg", pan);
        }
        return;
    }

    if (event.type == gameplay::AudioEventType::BonusPickup) {
        if (event.detail == "BONUS_SCORE" || event.detail == "BONUS_SCORE_200" || event.detail == "BONUS_SCORE_500" || event.detail == "BONUS_SCORE_10000") {
            audio_->playSfx("sounds/bonus_activation/extra_score.ogg", pan);
        } else if (event.detail == "EXTRA_LIFE") {
            audio_->playSfx("sounds/bonus_activation/extra_life.ogg", pan);
        } else if (event.detail == "BONUS_BALL") {
            audio_->playSfx("sounds/bonus_activation/extra_ball.ogg", pan);
        } else if (event.detail == "CALL_BALL") {
            audio_->playSfx("sounds/bonus_activation/call_ball_activation.ogg", pan);
        } else if (event.detail == "SLOW_BALLS") {
            audio_->playSfx("sounds/bonus_activation/slow_balls.ogg", pan);
        } else if (event.detail == "FAST_BALLS") {
            audio_->playSfx("sounds/bonus_activation/fast_balls.ogg", pan);
        } else if (event.detail == "SCORE_RAIN") {
            audio_->playSfx("sounds/bonus_activation/score_rain.ogg", pan);
        } else if (event.detail == "WEAK_BALLS") {
            audio_->playSfx("sounds/bonus_activation/weak_balls.ogg", pan);
        } else if (event.detail == "ENERGY_BALLS") {
            audio_->playSfx("sounds/bonus_activation/energy_balls.ogg", pan);
        } else if (event.detail == "EXPLOSION_BALLS") {
            audio_->playSfx("sounds/bonus_activation/explosion_ball.ogg", pan);
        } else if (event.detail == "INCREASE_PADDLE") {
            audio_->playSfx("sounds/bonus_activation/increase_paddle.ogg", pan);
        } else if (event.detail == "DECREASE_PADDLE") {
            audio_->playSfx("sounds/bonus_activation/decrease_paddle.ogg", pan);
        } else if (event.detail == "STICKY_PADDLE") {
            audio_->playSfx("sounds/bonus_activation/sticky_paddle.ogg", pan);
        } else if (event.detail == "PLASMA_WEAPON") {
            audio_->playSfx("sounds/bonus_activation/plasma_weapon.ogg", pan);
        } else if (event.detail == "PLASMA_RECHARGE") {
            audio_->playSfx("sounds/bonus_activation/recharge_plasma_weapon.ogg", pan);
        } else if (event.detail == "FROZEN_PADDLE") {
            audio_->playSfx("sounds/bonus_activation/frozen_paddle.ogg", pan);
        } else if (event.detail == "INVISIBLE_PADDLE") {
            audio_->playSfx("sounds/bonus_activation/ghost_paddle.ogg", pan);
        } else if (event.detail == "BONUS_WALL") {
            audio_->playSfx("sounds/bonus_activation/bonus_wall.ogg", pan);
        } else if (event.detail == "DARKNESS") {
            audio_->playSfx("sounds/bonus_activation/darkness.ogg", pan);
        } else if (event.detail == "CHAOTIC_BALLS") {
            audio_->playSfx("sounds/bonus_activation/chaotic_balls.ogg", pan);
        } else if (event.detail == "BONUS_MAGNET") {
            audio_->playSfx("sounds/bonus_activation/magnet_active.ogg", pan);
        } else if (event.detail == "PENALTIES_MAGNET") {
            audio_->playSfx("sounds/bonus_activation/penalty_magnet.ogg", pan);
        } else if (event.detail == "BAD_LUCK") {
            audio_->playSfx("sounds/bonus_activation/bad_luck.ogg", pan);
        } else if (event.detail == "TRICKSTER") {
            audio_->playSfx("sounds/bonus_activation/trickster.ogg", pan);
        } else if (event.detail == "ADD_FIVE_SECONDS") {
            audio_->playSfx("sounds/bonus_activation/add_five_seconds.ogg", pan);
        } else if (event.detail == "RESET") {
            audio_->playSfx("sounds/bonus_activation/reset.ogg", pan);
        } else if (event.detail == "RANDOM_BONUS") {
            audio_->playSfx("sounds/bonus_activation/random.ogg", pan);
        } else if (event.detail == "RAINBOW_BOUNTY") {
            audio_->playSfx("sounds/bonus_activation/rainbow_bounty.ogg", pan);
        } else if (event.detail == "BLOOD_TITHE") {
            audio_->playSfx("sounds/bonus_activation/blood_tithe.ogg", pan);
        } else {
            audio_->playSfx("sounds/bonus_activation/bonus_pickup.ogg", pan);
            audio_->playSfx("sounds/bonus_activation/extra_score.ogg", pan);
        }

        // Spawn pickup particle burst!
        unsigned char r = 100, g = 255, b = 100;
        if (event.detail == "BONUS_SCORE") {
            r = 255; g = 200; b = 50;
        } else if (event.detail == "BLOOD_TITHE") {
            r = 128; g = 0; b = 32;
        } else if (event.detail == "BONUS_SCORE_200") {
            r = 50; g = 200; b = 255;
        } else if (event.detail == "BONUS_SCORE_500") {
            r = 255; g = 80; b = 200;
        } else if (event.detail == "BONUS_SCORE_10000") {
            r = 255; g = 170; b = 170;
        } else if (event.detail == "EXTRA_LIFE") {
            r = 255; g = 120; b = 0;
        } else if (event.detail == "BONUS_BALL") {
            r = 170; g = 255; b = 0;
        } else if (event.detail == "CALL_BALL") {
            r = 90; g = 235; b = 255;
        } else if (event.detail == "SLOW_BALLS") {
            r = 0; g = 191; b = 255;
        } else if (event.detail == "FAST_BALLS") {
            r = 255; g = 50; b = 50;
        } else if (event.detail == "SCORE_RAIN") {
            r = 255; g = 215; b = 0;
        } else if (event.detail == "WEAK_BALLS") {
            r = 160; g = 160; b = 160;
        } else if (event.detail == "ENERGY_BALLS") {
            r = 180; g = 0; b = 255;
        } else if (event.detail == "EXPLOSION_BALLS") {
            r = 255; g = 96; b = 30;
        } else if (event.detail == "INCREASE_PADDLE") {
            r = 0; g = 255; b = 128;
        } else if (event.detail == "DECREASE_PADDLE") {
            r = 255; g = 50; b = 50;
        } else if (event.detail == "STICKY_PADDLE") {
            r = 50; g = 220; b = 50;
        } else if (event.detail == "PLASMA_WEAPON" || event.detail == "PLASMA_RECHARGE") {
            r = 50; g = 255; b = 50;
        } else if (event.detail == "FROZEN_PADDLE") {
            r = 160; g = 220; b = 255;
        } else if (event.detail == "INVISIBLE_PADDLE") {
            r = 200; g = 170; b = 255;
        } else if (event.detail == "BONUS_WALL") {
            r = 0; g = 180; b = 255;
        } else if (event.detail == "DARKNESS") {
            r = 60; g = 30; b = 100;
        } else if (event.detail == "CHAOTIC_BALLS") {
            r = 255; g = 60; b = 180;
        } else if (event.detail == "BONUS_MAGNET") {
            r = 0; g = 255; b = 200;
        } else if (event.detail == "PENALTIES_MAGNET") {
            r = 255; g = 100; b = 0;
        } else if (event.detail == "BAD_LUCK") {
            r = 220; g = 0; b = 30;
        } else if (event.detail == "TRICKSTER") {
            r = 255; g = 215; b = 0;
        } else if (event.detail == "ADD_FIVE_SECONDS") {
            r = 50; g = 255; b = 150;
        } else if (event.detail == "RESET") {
            r = 220; g = 220; b = 220;
        } else if (event.detail == "RANDOM_BONUS") {
            r = 240; g = 200; b = 40;
        } else if (event.detail == "LEVEL_PASS") {
            r = 255; g = 215; b = 0;
        } else if (event.detail == "RAINBOW_BOUNTY") {
            r = 255; g = 30; b = 30;
        }

        std::uniform_real_distribution<float> angleDist(-pi * 0.8f, -pi * 0.2f);
        std::uniform_real_distribution<float> speedDist(100.0f, 360.0f);
        std::uniform_real_distribution<float> sizeDist(6.0f, 12.0f);
        std::uniform_real_distribution<float> durDist(0.5, 0.9);
        std::uniform_real_distribution<float> xOffset(-event.size.w * 0.5f, event.size.w * 0.5f);

        int burstCount = (event.detail == "LEVEL_PASS" || event.detail == "TRICKSTER" || event.detail == "BONUS_SCORE_10000" || event.detail == "RAINBOW_BOUNTY" || event.detail == "BLOOD_TITHE") ? 60 : 20;
        for (int i = 0; i < burstCount; ++i) {
            float angle = angleDist(effectRng_);
            float speedMultiplier = (event.detail == "LEVEL_PASS" || event.detail == "TRICKSTER" || event.detail == "BONUS_SCORE_10000" || event.detail == "RAINBOW_BOUNTY" || event.detail == "BLOOD_TITHE") ? 1.8f : 1.0f;
            float speed = speedDist(effectRng_) * speedMultiplier;
            float size = sizeDist(effectRng_) * ((event.detail == "LEVEL_PASS" || event.detail == "TRICKSTER" || event.detail == "BONUS_SCORE_10000" || event.detail == "RAINBOW_BOUNTY" || event.detail == "BLOOD_TITHE") ? 1.4f : 1.0f);
            float dur = durDist(effectRng_) * ((event.detail == "LEVEL_PASS" || event.detail == "TRICKSTER" || event.detail == "BONUS_SCORE_10000" || event.detail == "RAINBOW_BOUNTY" || event.detail == "BLOOD_TITHE") ? 1.5f : 1.0f);
            float dx = xOffset(effectRng_);

            gameplay::Vec2 startPos{
                event.position.x + event.size.w * 0.5f + dx,
                event.position.y + event.size.h * 0.5f
            };

            unsigned char pr = r, pg = g, pb = b;
            if (event.detail == "LEVEL_PASS") {
                if (std::uniform_int_distribution<int>(0, 1)(effectRng_) == 0) {
                    pr = 255; pg = 215; pb = 0; // Gold
                } else {
                    pr = 0; pg = 255; pb = 255; // Cyan
                }
            } else if (event.detail == "BONUS_SCORE_10000") {
                static constexpr std::array<std::tuple<unsigned char, unsigned char, unsigned char>, 6> pastelRainbow{{
                    {255, 170, 170}, // Pastel Red/Pink
                    {255, 210, 170}, // Pastel Orange
                    {255, 255, 170}, // Pastel Yellow
                    {170, 255, 170}, // Pastel Green
                    {170, 220, 255}, // Pastel Blue
                    {220, 170, 255}  // Pastel Purple/Lavender
                }};
                std::uniform_int_distribution<std::size_t> colorDist(0, pastelRainbow.size() - 1);
                auto [pr_c, pg_c, pb_c] = pastelRainbow[colorDist(effectRng_)];
                pr = pr_c;
                pg = pg_c;
                pb = pb_c;
            } else if (event.detail == "RAINBOW_BOUNTY") {
                static constexpr std::array<std::tuple<unsigned char, unsigned char, unsigned char>, 6> vibrantRainbow{{
                    {255, 30, 30},    // Red
                    {255, 130, 30},   // Orange
                    {255, 230, 30},   // Yellow
                    {30, 255, 30},    // Green
                    {30, 220, 255},   // Cyan
                    {180, 50, 255}    // Purple
                }};
                std::uniform_int_distribution<std::size_t> colorDist(0, vibrantRainbow.size() - 1);
                auto [pr_c, pg_c, pb_c] = vibrantRainbow[colorDist(effectRng_)];
                pr = pr_c;
                pg = pg_c;
                pb = pb_c;
            } else if (event.detail == "BLOOD_TITHE") {
                static constexpr std::array<std::tuple<unsigned char, unsigned char, unsigned char>, 3> bloodColors{{
                    {128, 0, 32},    // Burgundy
                    {170, 0, 0},     // Dark Red
                    {80, 0, 20}      // Almost black red
                }};
                std::uniform_int_distribution<std::size_t> colorDist(0, bloodColors.size() - 1);
                auto [pr_c, pg_c, pb_c] = bloodColors[colorDist(effectRng_)];
                pr = pr_c;
                pg = pg_c;
                pb = pb_c;
                if (std::uniform_real_distribution<float>(0.0f, 1.0f)(effectRng_) < 0.2f) {
                    pr = 255; pg = 30; pb = 30; // Scarlet/bright red
                }
            }

            bonusParticles_.push_back(BonusParticle{
                .position = startPos,
                .velocity = gameplay::Vec2{std::cos(angle) * speed, std::sin(angle) * speed},
                .size = size,
                .age = 0.0,
                .duration = dur,
                .r = pr,
                .g = pg,
                .b = pb,
                .isTrail = false
            });
        }

        float maxRadius = 100.0f;
        if (event.detail == "LEVEL_PASS" || event.detail == "TRICKSTER") {
            maxRadius = 250.0f;
        }

        bonusPickupGlows_.push_back(BonusPickupGlow{
            .center = gameplay::Vec2{event.position.x + event.size.w * 0.5f, event.position.y + event.size.h * 0.5f},
            .age = 0.0,
            .duration = (event.detail == "LEVEL_PASS" || event.detail == "TRICKSTER") ? 0.7 : 0.4,
            .maxRadius = maxRadius,
            .r = r,
            .g = g,
            .b = b
        });

        return;
    }

    if (event.type == gameplay::AudioEventType::BonusSpawn) {
        // Spawn small pop of particles at spawn!
        unsigned char r = 100, g = 255, b = 100;
        if (event.detail == "BONUS_SCORE") {
            r = 255; g = 200; b = 50;
        } else if (event.detail == "BONUS_SCORE_200") {
            r = 50; g = 200; b = 255;
        } else if (event.detail == "BONUS_SCORE_500") {
            r = 255; g = 80; b = 200;
        } else if (event.detail == "EXTRA_LIFE") {
            r = 255; g = 120; b = 0;
        } else if (event.detail == "BONUS_BALL") {
            r = 170; g = 255; b = 0;
        } else if (event.detail == "CALL_BALL") {
            r = 90; g = 235; b = 255;
        } else if (event.detail == "SLOW_BALLS") {
            r = 0; g = 191; b = 255;
        } else if (event.detail == "FAST_BALLS") {
            r = 255; g = 50; b = 50;
        } else if (event.detail == "SCORE_RAIN") {
            r = 255; g = 215; b = 0;
        } else if (event.detail == "WEAK_BALLS") {
            r = 160; g = 160; b = 160;
        } else if (event.detail == "LEVEL_PASS") {
            r = 255; g = 215; b = 0;
        }

        std::uniform_real_distribution<float> angleDist(-pi, pi);
        std::uniform_real_distribution<float> speedDist(50.0f, 150.0f);
        std::uniform_real_distribution<float> sizeDist(4.0f, 8.0f);
        std::uniform_real_distribution<float> durDist(0.3, 0.6);

        int spawnCount = (event.detail == "LEVEL_PASS") ? 25 : 8;
        for (int i = 0; i < spawnCount; ++i) {
            float angle = angleDist(effectRng_);
            float speedMultiplier = (event.detail == "LEVEL_PASS") ? 2.0f : 1.0f;
            float speed = speedDist(effectRng_) * speedMultiplier;
            float size = sizeDist(effectRng_) * ((event.detail == "LEVEL_PASS") ? 1.3f : 1.0f);
            float dur = durDist(effectRng_) * ((event.detail == "LEVEL_PASS") ? 1.5f : 1.0f);

            gameplay::Vec2 startPos{
                event.position.x + event.size.w * 0.5f,
                event.position.y + event.size.h * 0.5f
            };

            unsigned char pr = r, pg = g, pb = b;
            if (event.detail == "LEVEL_PASS") {
                if (std::uniform_int_distribution<int>(0, 1)(effectRng_) == 0) {
                    pr = 255; pg = 215; pb = 0; // Gold
                } else {
                    pr = 0; pg = 255; pb = 255; // Cyan
                }
            }

            bonusParticles_.push_back(BonusParticle{
                .position = startPos,
                .velocity = gameplay::Vec2{std::cos(angle) * speed, std::sin(angle) * speed},
                .size = size,
                .age = 0.0,
                .duration = dur,
                .r = pr,
                .g = pg,
                .b = pb,
                .isTrail = false
            });
        }
        return;
    }

    const auto found = currentLevelAssets_.sfxByEvent.find(event.type);
    if (found == currentLevelAssets_.sfxByEvent.end() || found->second.empty()) {
        return;
    }

    if (event.type == gameplay::AudioEventType::BossSectionDestroyed && !event.detail.empty()) {
        try {
            int idx = std::stoi(event.detail) - 1;
            if (idx >= 0 && static_cast<size_t>(idx) < found->second.size()) {
                audio_->playSfx(found->second[idx], pan);
                return;
            }
        } catch (...) {}
    }

    std::uniform_int_distribution<std::size_t> dist(0, found->second.size() - 1);
    audio_->playSfx(found->second[dist(effectRng_)], pan);
}

void SdlRuntime::updateGameOverState(double fixedDeltaSeconds) {
    if (!world_ || screen_ != RuntimeScreen::InGame || world_->phase() != gameplay::GamePhase::GameOver) {
        return;
    }

    if (!gameOverActive_) {
        gameOverActive_ = true;
        gameOverAge_ = 0.0;
        gameOverSelectedIndex_ = 0;
        continueSelectedIndex_ = 0;
        continuePromptOpen_ = false;
        mousePaddleActive_ = false;
        core::Log::info("Game over");
    } else {
        gameOverAge_ += fixedDeltaSeconds;
    }

    if (!gameOverMusicStarted_ && gameOverAge_ >= 0.6) {
        playGameOverMusic();
        gameOverMusicStarted_ = true;
    }
}

void SdlRuntime::updateLevelCompleteState(double fixedDeltaSeconds) {
    if (!world_ || screen_ != RuntimeScreen::InGame || world_->phase() != gameplay::GamePhase::LevelComplete) {
        return;
    }

    if (!levelCompleteActive_) {
        levelCompleteActive_ = true;
        levelCompleteAge_ = 0.0;
        mousePaddleActive_ = false;
        core::Log::info("Level complete");
        
        double fadeDuration = 0.75;
        if (world_->hasBoss()) {
            fadeDuration = audio_ ? audio_->getSfxDuration("sounds/boss/boss_completed1.ogg") : 1.2;
            if (fadeDuration <= 0.0) {
                fadeDuration = 1.2;
            }
        }
        levelCompleteFadeOutDuration_ = fadeDuration;

        if (audio_) {
            if (world_->hasBoss()) {
                audio_->stopMusic();
            } else {
                audio_->pauseAll(0.75); // Fade music out over 0.75 seconds
            }
        }
    } else {
        double prevAge = levelCompleteAge_;
        levelCompleteAge_ += fixedDeltaSeconds;
        if (prevAge < levelCompleteFadeOutDuration_ && levelCompleteAge_ >= levelCompleteFadeOutDuration_) {
            levelCompleteMenuOpen_ = true;
            updateMouseCapture(); // Release grab and show cursor
        }
    }

    if (!levelCompleteSoundPlayed_ && levelCompleteAge_ >= levelCompleteFadeOutDuration_) {
        playUiSound(ui::UiSoundEffect::LevelCompleteRandom);
        levelCompleteSoundPlayed_ = true;
    }
}

void SdlRuntime::resetGameOverState() {
    gameOverAge_ = 0.0;
    gameOverSelectedIndex_ = 0;
    continueSelectedIndex_ = 0;
    gameOverActive_ = false;
    gameOverMusicStarted_ = false;
    continuePromptOpen_ = false;
}

void SdlRuntime::resetLevelCompleteState() {
    levelCompleteActive_ = false;
    levelCompleteAge_ = 0.0;
    levelCompleteFadeOutDuration_ = 0.75;
    levelPlayTime_ = 0.0;
    livesLostThisLevel_ = 0;
    levelCompleteSoundPlayed_ = false;
    levelCompleteMenuOpen_ = false;
    levelCompleteView_.reset();
    if (audio_) {
        audio_->stopMusic();
        audio_->resumeAll(0.0);
    }
}

int SdlRuntime::continueCost() const noexcept {
    constexpr int baseCost = 10000;
    constexpr int increment = 5000;
    return baseCost + continuePurchaseCount_ * increment;
}

float SdlRuntime::gameOverFadeProgress() const noexcept {
    if (!gameOverActive_) {
        return 0.0f;
    }
    return std::clamp(static_cast<float>(gameOverAge_ / 0.75), 0.0f, 1.0f);
}

float SdlRuntime::levelCompleteFadeProgress() const noexcept {
    if (!levelCompleteActive_) {
        return 0.0f;
    }
    return std::clamp(static_cast<float>(levelCompleteAge_ / levelCompleteFadeOutDuration_), 0.0f, 1.0f);
}

void SdlRuntime::spawnBrickImpactEffect(const gameplay::AudioEvent& event) {
    if (event.type != gameplay::AudioEventType::BrickBreak
        && event.type != gameplay::AudioEventType::BrickHit
        && event.type != gameplay::AudioEventType::Explosion
        && event.type != gameplay::AudioEventType::PlasmaBrickHit) {
        return;
    }
    if (event.size.w <= 0.0f || event.size.h <= 0.0f) {
        return;
    }

    const auto kind = event.type == gameplay::AudioEventType::Explosion
        ? BrickImpactEffectKind::Explosion
        : (event.type == gameplay::AudioEventType::BrickBreak
            ? BrickImpactEffectKind::Break
            : (event.type == gameplay::AudioEventType::PlasmaBrickHit
                ? BrickImpactEffectKind::Dissolve
                : BrickImpactEffectKind::Blocked));
            
    BrickImpactEffectKind finalKind = kind;
    if (kind == BrickImpactEffectKind::Break && world_) {
        if (world_->isBonusActive("ENERGY_BALLS")) {
            finalKind = BrickImpactEffectKind::Dissolve;
        } else if (world_->isBonusActive("EXPLOSION_BALLS")) {
            finalKind = BrickImpactEffectKind::Explosion;
        }
    }

    BrickImpactEffect effect{
        .kind = finalKind,
        .center = gameplay::Vec2{
            event.position.x + event.size.w * 0.5f,
            event.position.y + event.size.h * 0.5f,
        },
        .brickSize = event.size,
        .age = 0.0,
        .duration = finalKind == BrickImpactEffectKind::Explosion ? 0.72
            : (finalKind == BrickImpactEffectKind::Blocked ? brickBlockedEffectDuration
            : (finalKind == BrickImpactEffectKind::Dissolve ? 0.50 : brickBreakEffectDuration)),
        .particles = {},
    };

    std::uniform_real_distribution<float> angleDist(-pi, pi);
    std::uniform_real_distribution<float> speedDist(
        effect.kind == BrickImpactEffectKind::Explosion ? 360.0f : (effect.kind == BrickImpactEffectKind::Break ? 220.0f : (effect.kind == BrickImpactEffectKind::Dissolve ? 50.0f : 70.0f)),
        effect.kind == BrickImpactEffectKind::Explosion ? 980.0f : (effect.kind == BrickImpactEffectKind::Break ? 760.0f : (effect.kind == BrickImpactEffectKind::Dissolve ? 250.0f : 220.0f)));
    std::uniform_real_distribution<float> sizeDist(
        effect.kind == BrickImpactEffectKind::Explosion ? 8.0f : (effect.kind == BrickImpactEffectKind::Break ? 5.0f : (effect.kind == BrickImpactEffectKind::Dissolve ? 4.0f : 4.0f)),
        effect.kind == BrickImpactEffectKind::Explosion ? 28.0f : (effect.kind == BrickImpactEffectKind::Break ? 22.0f : (effect.kind == BrickImpactEffectKind::Dissolve ? 12.0f : 10.0f)));
    std::uniform_real_distribution<float> offsetDist(-0.45f, 0.45f);
    const int particleCount = effect.kind == BrickImpactEffectKind::Explosion ? 52 : (effect.kind == BrickImpactEffectKind::Break ? 40 : (effect.kind == BrickImpactEffectKind::Dissolve ? 32 : 14));
    effect.particles.reserve(static_cast<std::size_t>(particleCount));

    levels::BrickColor brickColor = levels::BrickColor::Blue;
    bool hasColor = false;
    std::string colorStr = event.detail;
    if (colorStr.rfind("silent,", 0) == 0) {
        colorStr = colorStr.substr(7);
    }
    if (auto parsed = levels::parseBrickColor(colorStr)) {
        brickColor = *parsed;
        hasColor = true;
    }

    auto getBrickParticleColor = [](levels::BrickColor col, float hot, int index) -> std::tuple<unsigned char, unsigned char, unsigned char> {
        switch (col) {
            case levels::BrickColor::Blue:
                return {
                    static_cast<unsigned char>(10 + hot * 40),
                    static_cast<unsigned char>(100 + hot * 110),
                    255
                };
            case levels::BrickColor::Cyan:
                return {
                    0,
                    static_cast<unsigned char>(200 + hot * 55),
                    static_cast<unsigned char>(220 + hot * 35)
                };
            case levels::BrickColor::DarkBlue:
                return {
                    0,
                    static_cast<unsigned char>(50 + hot * 70),
                    static_cast<unsigned char>(180 + hot * 75)
                };
            case levels::BrickColor::Green:
                return {
                    static_cast<unsigned char>(40 + hot * 100),
                    static_cast<unsigned char>(220 + hot * 35),
                    static_cast<unsigned char>(40 + hot * 60)
                };
            case levels::BrickColor::Pink:
                return {
                    255,
                    static_cast<unsigned char>(80 + hot * 100),
                    static_cast<unsigned char>(140 + hot * 90)
                };
            case levels::BrickColor::Purple:
                return {
                    static_cast<unsigned char>(140 + hot * 80),
                    static_cast<unsigned char>(30 + hot * 50),
                    255
                };
            case levels::BrickColor::Yellow:
                return {
                    255,
                    static_cast<unsigned char>(200 + hot * 55),
                    static_cast<unsigned char>(0 + hot * 80)
                };
            case levels::BrickColor::Orange:
                return {
                    255,
                    static_cast<unsigned char>(100 + hot * 90),
                    static_cast<unsigned char>(0 + hot * 40)
                };
            case levels::BrickColor::Red:
                return {
                    255,
                    static_cast<unsigned char>(20 + hot * 80),
                    static_cast<unsigned char>(20 + hot * 60)
                };
            case levels::BrickColor::Explosive:
                if (index % 3 == 0) return {255, 60, 20};
                if (index % 3 == 1) return {255, 140, 20};
                return {255, 220, 60};
            case levels::BrickColor::Indestructible:
                return {
                    static_cast<unsigned char>(180 + hot * 60),
                    static_cast<unsigned char>(180 + hot * 60),
                    static_cast<unsigned char>(190 + hot * 60)
                };
            case levels::BrickColor::LightGray:
                return {
                    static_cast<unsigned char>(150 + hot * 70),
                    static_cast<unsigned char>(155 + hot * 70),
                    static_cast<unsigned char>(160 + hot * 70)
                };
            case levels::BrickColor::Shielded:
                return {
                    static_cast<unsigned char>(100 + hot * 80),
                    static_cast<unsigned char>(160 + hot * 80),
                    static_cast<unsigned char>(210 + hot * 45)
                };
        }
        return {255, 255, 255};
    };

    for (int i = 0; i < particleCount; ++i) {
        const float angle = angleDist(effectRng_);
        const float speed = speedDist(effectRng_);
        const float hot = static_cast<float>(i % 3) / 2.0f;
        
        unsigned char pr = 255, pg = 255, pb = 255;
        if (hasColor) {
            if (i % 5 == 0 && (effect.kind == BrickImpactEffectKind::Break || effect.kind == BrickImpactEffectKind::Blocked)) {
                pr = 255;
                pg = 255;
                pb = 225;
            } else {
                std::tie(pr, pg, pb) = getBrickParticleColor(brickColor, hot, i);
            }
        } else {
            if (event.type == gameplay::AudioEventType::PlasmaBrickHit) {
                if (i % 2 == 0) {
                    pr = 50; pg = 255; pb = 50;
                } else {
                    pr = 0; pg = 255; pb = 255;
                }
            } else if (effect.kind == BrickImpactEffectKind::Explosion) {
                pr = 255;
                pg = static_cast<unsigned char>(95.0f + hot * 120.0f);
                pb = static_cast<unsigned char>(35.0f + hot * 45.0f);
            } else if (effect.kind == BrickImpactEffectKind::Break) {
                if (i % 5 == 0) {
                    pr = 255;
                    pg = 255;
                    pb = 225;
                } else {
                    pr = 255;
                    pg = static_cast<unsigned char>(155.0f + hot * 80.0f);
                    pb = static_cast<unsigned char>(55.0f + hot * 110.0f);
                }
            } else if (effect.kind == BrickImpactEffectKind::Dissolve) {
                if (i % 2 == 0) {
                    pr = 180; pg = 0; pb = 255;
                } else {
                    pr = 0; pg = 220; pb = 255;
                }
            } else {
                pr = 90; pg = 235; pb = 255;
            }
        }

        effect.particles.push_back(BrickImpactParticle{
            .position = gameplay::Vec2{
                effect.center.x + event.size.w * offsetDist(effectRng_),
                effect.center.y + event.size.h * offsetDist(effectRng_),
            },
            .velocity = gameplay::Vec2{std::cos(angle) * speed, std::sin(angle) * speed},
            .size = sizeDist(effectRng_),
            .spin = offsetDist(effectRng_) * 220.0f,
            .r = pr,
            .g = pg,
            .b = pb,
        });
    }

    brickImpactEffects_.push_back(std::move(effect));
}

void SdlRuntime::spawnLifeLostEffect(const gameplay::AudioEvent& event) {
    if (event.type != gameplay::AudioEventType::LifeLost) {
        return;
    }

    LifeLostEffect effect{
        .center = event.position,
        .age = 0.0,
        .duration = lifeLostEffectDuration,
        .particles = {},
    };

    std::uniform_real_distribution<float> angleDist(-pi, pi);
    std::uniform_real_distribution<float> speedDist(150.0f, 780.0f);
    std::uniform_real_distribution<float> sizeDist(5.0f, 18.0f);
    effect.particles.reserve(42);
    for (int i = 0; i < 42; ++i) {
        const float angle = angleDist(effectRng_);
        const float speed = speedDist(effectRng_);
        const bool cyan = (i % 4) == 0;
        effect.particles.push_back(BrickImpactParticle{
            .position = event.position,
            .velocity = gameplay::Vec2{std::cos(angle) * speed, std::sin(angle) * speed},
            .size = sizeDist(effectRng_),
            .spin = 0.0f,
            .r = static_cast<unsigned char>(cyan ? 120 : 255),
            .g = static_cast<unsigned char>(cyan ? 235 : 85 + (i % 3) * 45),
            .b = static_cast<unsigned char>(cyan ? 255 : 170 + (i % 2) * 55),
        });
    }

    lifeLostEffects_.push_back(std::move(effect));
}

void SdlRuntime::updateBrickImpactEffects(double fixedDeltaSeconds) {
    for (auto& effect : brickImpactEffects_) {
        effect.age += fixedDeltaSeconds;
    }
    for (auto& effect : lifeLostEffects_) {
        effect.age += fixedDeltaSeconds;
    }

    std::erase_if(brickImpactEffects_, [](const BrickImpactEffect& effect) {
        return effect.age >= effect.duration;
    });
    std::erase_if(lifeLostEffects_, [](const LifeLostEffect& effect) {
        return effect.age >= effect.duration;
    });
}

void SdlRuntime::renderBrickImpactEffects() {
    for (const auto& effect : brickImpactEffects_) {
        const float progress = static_cast<float>(effect.age / effect.duration);
        const float eased = easeOut(progress);
        if (effect.kind == BrickImpactEffectKind::Explosion) {
            const auto drawRing = [this](gameplay::Vec2 center, float radius, render::Color color, int segments = 64) {
                for (int i = 0; i < segments; ++i) {
                    const float a0 = (static_cast<float>(i) / static_cast<float>(segments)) * pi * 2.0f;
                    const float a1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * pi * 2.0f;
                    rendererApi_->drawLine(
                        center.x + std::cos(a0) * radius,
                        center.y + std::sin(a0) * radius,
                        center.x + std::cos(a1) * radius,
                        center.y + std::sin(a1) * radius,
                        color);
                }
            };
            rendererApi_->drawRect(
                render::Rect{effect.center.x - 130.0f * eased, effect.center.y - 130.0f * eased, 260.0f * eased, 260.0f * eased},
                render::Color{255, 96, 30, fadeAlpha(progress, 52.0f)});
            drawRing(effect.center, 40.0f + eased * 150.0f, render::Color{255, 180, 70, fadeAlpha(progress, 225.0f)});
            drawRing(effect.center, 18.0f + eased * 90.0f, render::Color{255, 72, 35, fadeAlpha(progress, 190.0f)}, 42);
            for (int ray = 0; ray < 14; ++ray) {
                const float angle = (static_cast<float>(ray) / 14.0f) * pi * 2.0f + progress * 0.45f;
                rendererApi_->drawLine(
                    effect.center.x + std::cos(angle) * 20.0f,
                    effect.center.y + std::sin(angle) * 20.0f,
                    effect.center.x + std::cos(angle) * (70.0f + eased * 150.0f),
                    effect.center.y + std::sin(angle) * (70.0f + eased * 150.0f),
                    render::Color{255, 125, 45, fadeAlpha(progress, 155.0f)});
            }
        } else if (effect.kind == BrickImpactEffectKind::Break) {
            const float glowScale = 1.0f + eased * 1.25f;
            rendererApi_->drawRect(
                render::Rect{
                    effect.center.x - effect.brickSize.w * glowScale * 0.5f,
                    effect.center.y - effect.brickSize.h * glowScale * 0.5f,
                    effect.brickSize.w * glowScale,
                    effect.brickSize.h * glowScale,
                },
                render::Color{255, 220, 120, fadeAlpha(progress, 95.0f)});
            const float burstScale = 1.0f + eased * 0.75f;
            const float w = effect.brickSize.w * burstScale;
            const float h = effect.brickSize.h * burstScale;
            const float x = effect.center.x - w * 0.5f;
            const float y = effect.center.y - h * 0.5f;
            const auto hotEdge = render::Color{255, 236, 170, fadeAlpha(progress, 210.0f)};
            rendererApi_->drawRect(render::Rect{x, y, w, 3.0f}, hotEdge);
            rendererApi_->drawRect(render::Rect{x, y + h - 3.0f, w, 3.0f}, hotEdge);
            rendererApi_->drawRect(render::Rect{x, y, 3.0f, h}, hotEdge);
            rendererApi_->drawRect(render::Rect{x + w - 3.0f, y, 3.0f, h}, hotEdge);
            for (int ray = 0; ray < 8; ++ray) {
                const float angle = (static_cast<float>(ray) / 8.0f) * pi * 2.0f + progress * 0.25f;
                rendererApi_->drawLine(
                    effect.center.x + std::cos(angle) * effect.brickSize.w * 0.24f,
                    effect.center.y + std::sin(angle) * effect.brickSize.h * 0.24f,
                    effect.center.x + std::cos(angle) * (effect.brickSize.w * 0.58f + eased * 54.0f),
                    effect.center.y + std::sin(angle) * (effect.brickSize.h * 0.58f + eased * 34.0f),
                    render::Color{255, 190, 70, fadeAlpha(progress, 150.0f)});
            }
        } else if (effect.kind == BrickImpactEffectKind::Dissolve) {
            const float shrinkScale = 1.0f - eased * 0.9f;
            const float w = effect.brickSize.w * shrinkScale;
            const float h = effect.brickSize.h * shrinkScale;
            const float x = effect.center.x - w * 0.5f;
            const float y = effect.center.y - h * 0.5f;
            
            const auto outlineColor = render::Color{180, 0, 255, fadeAlpha(progress, 180.0f)};
            rendererApi_->drawRect(render::Rect{x, y, w, 3.0f}, outlineColor);
            rendererApi_->drawRect(render::Rect{x, y + h - 3.0f, w, 3.0f}, outlineColor);
            rendererApi_->drawRect(render::Rect{x, y, 3.0f, h}, outlineColor);
            rendererApi_->drawRect(render::Rect{x + w - 3.0f, y, 3.0f, h}, outlineColor);

            rendererApi_->drawRect(
                render::Rect{x + 3.0f, y + 3.0f, std::max(0.0f, w - 6.0f), std::max(0.0f, h - 6.0f)},
                render::Color{0, 220, 255, fadeAlpha(progress, 90.0f)});
        } else {
            const float ringScale = 1.0f + eased * 0.55f;
            const float w = effect.brickSize.w * ringScale;
            const float h = effect.brickSize.h * ringScale;
            constexpr float thickness = 5.0f;
            const auto color = render::Color{100, 235, 255, fadeAlpha(progress, 185.0f)};
            const float x = effect.center.x - w * 0.5f;
            const float y = effect.center.y - h * 0.5f;
            rendererApi_->drawRect(render::Rect{x, y, w, thickness}, color);
            rendererApi_->drawRect(render::Rect{x, y + h - thickness, w, thickness}, color);
            rendererApi_->drawRect(render::Rect{x, y, thickness, h}, color);
            rendererApi_->drawRect(render::Rect{x + w - thickness, y, thickness, h}, color);
            rendererApi_->drawRect(
                render::Rect{x + w * 0.12f, y + h * 0.18f, w * 0.76f, h * 0.64f},
                render::Color{90, 225, 255, fadeAlpha(progress, 70.0f)});
        }

        for (const auto& particle : effect.particles) {
            const float drag = 1.0f - progress * 0.35f;
            const float gravity = effect.kind == BrickImpactEffectKind::Explosion ? 520.0f
                                : (effect.kind == BrickImpactEffectKind::Break ? 420.0f
                                : (effect.kind == BrickImpactEffectKind::Dissolve ? -150.0f : 130.0f));
            const float x = particle.position.x + particle.velocity.x * static_cast<float>(effect.age) * drag;
            const float y = particle.position.y
                + particle.velocity.y * static_cast<float>(effect.age) * drag
                + 0.5f * gravity * static_cast<float>(effect.age * effect.age);
            const float size = std::max(1.0f, particle.size * (1.0f - progress * 0.65f));
            rendererApi_->drawRect(
                render::Rect{x - size * 0.5f, y - size * 0.5f, size, size},
                render::Color{particle.r, particle.g, particle.b, fadeAlpha(progress, 220.0f)});
        }
    }
}

void SdlRuntime::updateBonusEffects(double fixedDeltaSeconds) {
    for (auto& p : bonusParticles_) {
        p.age += fixedDeltaSeconds;
        if (p.isTrail) {
            p.position.x += p.velocity.x * static_cast<float>(fixedDeltaSeconds);
            p.position.y += p.velocity.y * static_cast<float>(fixedDeltaSeconds);
        } else {
            const float progress = static_cast<float>(p.age / p.duration);
            const float drag = 1.0f - progress * 0.4f;
            p.position.x += p.velocity.x * drag * static_cast<float>(fixedDeltaSeconds);
            p.position.y += p.velocity.y * drag * static_cast<float>(fixedDeltaSeconds);
        }
    }
    std::erase_if(bonusParticles_, [](const BonusParticle& p) {
        return p.age >= p.duration;
    });

    for (auto& g : bonusPickupGlows_) {
        g.age += fixedDeltaSeconds;
    }
    std::erase_if(bonusPickupGlows_, [](const BonusPickupGlow& g) {
        return g.age >= g.duration;
    });

    if (world_) {
        bool frozenActive = world_->isBonusActive("FROZEN_PADDLE");
        bool plasmaActive = world_->isBonusActive("PLASMA_WEAPON");
        bool stickyActive = world_->isBonusActive("STICKY_PADDLE");
        bool invisibleActive = world_->isBonusActive("INVISIBLE_PADDLE");
        bool darknessActive = world_->isBonusActive("DARKNESS");

        auto updateBlend = [](float& blend, bool active, double dt) {
            if (active) {
                blend = std::min(1.0f, blend + static_cast<float>(dt / 0.5));
            } else {
                blend = std::max(0.0f, blend - static_cast<float>(dt / 0.75));
            }
        };

        updateBlend(paddleFrozenBlend_, frozenActive, fixedDeltaSeconds);
        updateBlend(paddlePlasmaBlend_, plasmaActive, fixedDeltaSeconds);
        updateBlend(paddleStickyBlend_, stickyActive, fixedDeltaSeconds);
        updateBlend(paddleInvisibleBlend_, invisibleActive, fixedDeltaSeconds);
        updateBlend(paddleDarknessBlend_, darknessActive, fixedDeltaSeconds);
    } else {
        paddleFrozenBlend_ = 0.0f;
        paddlePlasmaBlend_ = 0.0f;
        paddleStickyBlend_ = 0.0f;
        paddleInvisibleBlend_ = 0.0f;
        paddleDarknessBlend_ = 0.0f;
    }
}

void SdlRuntime::renderBonusEffects() {
    for (const auto& p : bonusParticles_) {
        const float progress = static_cast<float>(p.age / p.duration);
        float size = p.size;
        if (p.isTrail) {
            size = std::max(1.0f, p.size * (1.0f - progress));
        } else {
            size = std::max(1.0f, p.size * (1.0f - progress * progress));
        }

        const unsigned char alpha = fadeAlpha(progress, 200.0f);
        const unsigned char glowAlpha = fadeAlpha(progress, 80.0f);

        if (p.isTrail) {
            rendererApi_->drawRect(
                render::Rect{p.position.x - size * 0.5f, p.position.y - size * 0.5f, size, size},
                render::Color{p.r, p.g, p.b, alpha});
        } else {
            const float glowSize = size * 2.0f;
            rendererApi_->drawRect(
                render::Rect{p.position.x - glowSize * 0.5f, p.position.y - glowSize * 0.5f, glowSize, glowSize},
                render::Color{p.r, p.g, p.b, glowAlpha});
            rendererApi_->drawRect(
                render::Rect{p.position.x - size * 0.5f, p.position.y - size * 0.5f, size, size},
                render::Color{255, 255, 255, alpha});
        }
    }

    const auto drawRing = [this](gameplay::Vec2 center, float radius, render::Color color, int segments = 32) {
        for (int i = 0; i < segments; ++i) {
            const float a0 = (static_cast<float>(i) / static_cast<float>(segments)) * pi * 2.0f;
            const float a1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * pi * 2.0f;
            rendererApi_->drawLine(
                center.x + std::cos(a0) * radius,
                center.y + std::sin(a0) * radius,
                center.x + std::cos(a1) * radius,
                center.y + std::sin(a1) * radius,
                color);
        }
    };

    for (const auto& g : bonusPickupGlows_) {
        const float progress = static_cast<float>(g.age / g.duration);
        const float eased = easeOut(progress);
        const float radius = g.maxRadius * eased;

        drawRing(g.center, radius, render::Color{g.r, g.g, g.b, fadeAlpha(progress, 160.0f)}, 24);
        rendererApi_->drawRect(
            render::Rect{g.center.x - radius * 0.5f, g.center.y - radius * 0.5f, radius, radius},
            render::Color{g.r, g.g, g.b, fadeAlpha(progress, 40.0f)});
    }
}

void SdlRuntime::renderLifeLostEffects() {
    const auto drawRing = [this](gameplay::Vec2 center, float radius, render::Color color, int segments = 56) {
        for (int i = 0; i < segments; ++i) {
            const float a0 = (static_cast<float>(i) / static_cast<float>(segments)) * pi * 2.0f;
            const float a1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * pi * 2.0f;
            rendererApi_->drawLine(
                center.x + std::cos(a0) * radius,
                center.y + std::sin(a0) * radius,
                center.x + std::cos(a1) * radius,
                center.y + std::sin(a1) * radius,
                color);
        }
    };

    for (const auto& effect : lifeLostEffects_) {
        const float progress = static_cast<float>(effect.age / effect.duration);
        const float eased = easeOut(progress);
        rendererApi_->drawRect(render::Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, render::Color{25, 0, 36, fadeAlpha(progress, 80.0f)});
        drawRing(effect.center, 70.0f + eased * 540.0f, render::Color{255, 80, 210, fadeAlpha(progress, 210.0f)});
        drawRing(effect.center, 18.0f + eased * 260.0f, render::Color{120, 235, 255, fadeAlpha(progress, 165.0f)}, 44);
        drawRing(effect.center, 110.0f + eased * 760.0f, render::Color{255, 220, 120, fadeAlpha(progress, 95.0f)}, 72);

        for (int ray = 0; ray < 18; ++ray) {
            const float angle = (static_cast<float>(ray) / 18.0f) * pi * 2.0f + progress * 0.8f;
            const float inner = 30.0f + eased * 110.0f;
            const float outer = 120.0f + eased * 620.0f;
            rendererApi_->drawLine(
                effect.center.x + std::cos(angle) * inner,
                effect.center.y + std::sin(angle) * inner,
                effect.center.x + std::cos(angle) * outer,
                effect.center.y + std::sin(angle) * outer,
                render::Color{255, 120, 235, fadeAlpha(progress, 120.0f)});
        }

        for (const auto& particle : effect.particles) {
            const float t = static_cast<float>(effect.age);
            const float x = particle.position.x + particle.velocity.x * t * (1.0f - progress * 0.25f);
            const float y = particle.position.y + particle.velocity.y * t * (1.0f - progress * 0.25f) + 0.5f * 360.0f * t * t;
            const float size = std::max(1.0f, particle.size * (1.0f - progress * 0.75f));
            rendererApi_->drawRect(
                render::Rect{x - size * 0.5f, y - size * 0.5f, size, size},
                render::Color{particle.r, particle.g, particle.b, fadeAlpha(progress, 230.0f)});
        }
    }
}

void SdlRuntime::renderRespawnChargeEffect() {
    if (!world_ || world_->phase() != gameplay::GamePhase::LifeLost) {
        return;
    }

    const auto& ball = world_->ball();
    const auto& paddle = world_->paddle();
    const double remaining = world_->respawnLaunchDelayRemaining();
    const float charge = 1.0f - static_cast<float>(std::clamp(remaining / respawnLaunchDelaySeconds, 0.0, 1.0));
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(frameStats_.frameCount) * 0.18f);
    const gameplay::Vec2 ballCenter{ball.position.x, ball.position.y};
    const gameplay::Vec2 paddleCenter{paddle.position.x + paddle.size.w * 0.5f, paddle.position.y + paddle.size.h * 0.5f};

    const auto drawRing = [this](gameplay::Vec2 center, float radius, render::Color color, int segments = 54) {
        for (int i = 0; i < segments; ++i) {
            if ((i % 5) == 4) {
                continue;
            }
            const float a0 = (static_cast<float>(i) / static_cast<float>(segments)) * pi * 2.0f;
            const float a1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * pi * 2.0f;
            rendererApi_->drawLine(
                center.x + std::cos(a0) * radius,
                center.y + std::sin(a0) * radius,
                center.x + std::cos(a1) * radius,
                center.y + std::sin(a1) * radius,
                color);
        }
    };

    rendererApi_->drawLine(
        paddleCenter.x,
        paddleCenter.y - 8.0f,
        ballCenter.x,
        ballCenter.y,
        render::Color{120, 235, 255, static_cast<unsigned char>(90 + charge * 90)});
    rendererApi_->drawRect(
        render::Rect{paddle.position.x - 18.0f, paddle.position.y - 10.0f, paddle.size.w + 36.0f, paddle.size.h + 20.0f},
        render::Color{70, 220, 255, static_cast<unsigned char>(35 + pulse * 25.0f)});

    drawRing(ballCenter, ball.radius + 18.0f + pulse * 8.0f, render::Color{120, 235, 255, static_cast<unsigned char>(120 + charge * 80)});
    drawRing(ballCenter, ball.radius + 42.0f - charge * 16.0f, render::Color{255, 220, 120, static_cast<unsigned char>(80 + charge * 120)}, 42);

    const float barWidth = 260.0f;
    const float barHeight = 8.0f;
    const float barX = paddleCenter.x - barWidth * 0.5f;
    const float barY = paddle.position.y + paddle.size.h + 24.0f;
    rendererApi_->drawRect(render::Rect{barX, barY, barWidth, barHeight}, render::Color{30, 45, 70, 150});
    rendererApi_->drawRect(render::Rect{barX, barY, barWidth * charge, barHeight}, render::Color{120, 235, 255, 210});
    if (remaining <= 0.0) {
        const auto text = runtimeLocalization(config_).text(config_.settings.language, "gameplay.launch_ready");
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2{barX + 62.0f, barY + 14.0f},
            ImGui::GetColorU32(ImVec4{1.0f, 0.94f, 0.67f, 0.90f}),
            text.c_str());
    }
}

void SdlRuntime::renderResumeCountdownOverlay() {
    if (!resumeCountdownActive_) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return;
    }

    const ImVec2 viewportSize = viewport->Size;
    const ImVec2 viewportPos = viewport->Pos;

    ImDrawList* background = ImGui::GetBackgroundDrawList();
    background->AddRectFilled(
        viewportPos,
        ImVec2{viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y},
        ImGui::GetColorU32(ImVec4{0.01f, 0.015f, 0.045f, 0.55f}));

    const int total = static_cast<int>(std::ceil(resumeCountdownDuration()));
    const int elapsed = static_cast<int>(std::floor(resumeCountdownDuration() - resumeCountdownRemaining_));
    const int currentTick = std::clamp(elapsed, 0, total);
    const bool isFinalTick = currentTick >= total;
    const float progress = std::clamp(static_cast<float>(resumeCountdownRemaining_) / static_cast<float>(resumeCountdownDuration()), 0.0f, 1.0f);
    const float scalePulse = 1.0f + 0.18f * (1.0f - progress) - 0.10f * progress;

    const auto& localization = runtimeLocalization(config_);
    const std::string label = localization.text(config_.settings.language, isFinalTick ? "resume.countdown.go" : "resume.countdown.label");
    const std::string number = isFinalTick ? std::string{} : std::to_string(std::max(1, total - currentTick));

    ImDrawList* foreground = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();

    const float labelFontSize = 32.0f;
    const float numberFontSize = 168.0f * scalePulse;
    const float gap = isFinalTick ? 0.0f : 24.0f;

    const ImVec2 labelSize = font->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, label.c_str());
    const ImVec2 numberSize = number.empty() ? ImVec2{0.0f, 0.0f}
                                              : font->CalcTextSizeA(numberFontSize, FLT_MAX, 0.0f, number.c_str());

    const float totalHeight = labelSize.y + gap + numberSize.y;
    const float centerX = viewportPos.x + viewportSize.x * 0.5f;
    const float centerY = viewportPos.y + viewportSize.y * 0.5f;

    const ImVec2 labelPos{centerX - labelSize.x * 0.5f, centerY - totalHeight * 0.5f};
    foreground->AddText(
        font,
        labelFontSize,
        labelPos,
        ImGui::GetColorU32(ui::UiTheme::accent(ui::UiAccent::Cyan)),
        label.c_str());

    if (!number.empty()) {
        const ImVec2 numberPos{centerX - numberSize.x * 0.5f, labelPos.y + labelSize.y + gap};
        foreground->AddText(
            font,
            numberFontSize,
            numberPos,
            ImGui::GetColorU32(ui::UiTheme::accent(isFinalTick ? ui::UiAccent::Green : ui::UiAccent::Pink)),
            number.c_str());
    }
}

void SdlRuntime::renderGameOverOverlay() {
    if (!world_ || !gameOverActive_ || world_->phase() != gameplay::GamePhase::GameOver) {
        return;
    }

    const float appear = std::clamp(static_cast<float>((gameOverAge_ - 0.45) / 0.55), 0.0f, 1.0f);
    const float eased = easeOut(appear);
    const float overlayAlpha = std::clamp(0.20f + eased * 0.58f, 0.0f, 0.82f);
    const bool inputReady = appear >= 0.98f;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return;
    }

    ImDrawList* background = ImGui::GetBackgroundDrawList();
    background->AddRectFilled(
        viewport->Pos,
        ImVec2{viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y},
        ImGui::GetColorU32(ImVec4{0.01f, 0.015f, 0.045f, overlayAlpha}));

    if (continuePromptOpen_) {
        if (inputReady) {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                continueSelectedIndex_ = continueSelectedIndex_ == 0 ? 1 : 0;
                playUiSound(ui::UiSoundEffect::Hover);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                continuePromptOpen_ = false;
                playUiSound(ui::UiSoundEffect::Back);
            }
        }

        ui::UiLayout::setNextCenteredWindow(520.0f, 548.0f);
        ImGui::SetNextWindowBgAlpha(0.94f);
        ImGui::Begin(
            "GameOverContinue",
            nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);

        ui::UiTheme::renderCyberpunkPanelTitle(
            runtimeLocalization(config_).text(config_.settings.language, "gameover.continue.title").c_str(),
            ui::UiAccent::Cyan);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextColored(
            ui::UiTheme::textSecondary(),
            "%s",
            runtimeLocalization(config_).text(config_.settings.language, "gameover.continue.description").c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2{0.0f, 14.0f});

        const int cost = continueCost();
        const int points = world_->score();
        const int remaining = points - cost;
        const auto card = [](const char* label, const std::string& value, ui::UiAccent accent) {
            const ImVec2 size{320.0f, 88.0f};
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(pos, ImVec2{pos.x + size.x, pos.y + size.y}, ImGui::GetColorU32(ImVec4{0.07f, 0.09f, 0.18f, 0.88f}), 16.0f);
            drawList->AddRect(pos, ImVec2{pos.x + size.x, pos.y + size.y}, ImGui::GetColorU32(ui::UiTheme::accent(accent)), 16.0f, 0, 2.0f);
            ImGui::Dummy(ImVec2{0.0f, 10.0f});
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(label).x) * 0.5f);
            ImGui::TextColored(ui::UiTheme::accent(accent), "%s", label);
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(value.c_str()).x) * 0.5f);
            ImGui::Text("%s", value.c_str());
            ImGui::Dummy(ImVec2{0.0f, 10.0f});
        };

        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 320.0f) * 0.5f);
        card(runtimeLocalization(config_).text(config_.settings.language, "gameover.continue.cost").c_str(), formatNumber(cost), ui::UiAccent::Pink);
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 320.0f) * 0.5f);
        card(runtimeLocalization(config_).text(config_.settings.language, "gameover.continue.points").c_str(), formatNumber(points), ui::UiAccent::Cyan);
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 320.0f) * 0.5f);
        card(
            runtimeLocalization(config_).text(config_.settings.language, "gameover.continue.remaining").c_str(),
            remaining >= 0 ? formatNumber(remaining) : runtimeLocalization(config_).text(config_.settings.language, "gameover.continue.not_enough"),
            remaining >= 0 ? ui::UiAccent::Green : ui::UiAccent::Red);

        ImGui::Dummy(ImVec2{0.0f, 14.0f});
        const ImVec2 buttonSize{190.0f, ui::UiTheme::buttonHeight()};
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - buttonSize.x * 2.0f - 14.0f) * 0.5f);
        const auto confirm = ui::UiTheme::renderNeonButton(
            "gameover-confirm",
            (remaining >= 0
                 ? runtimeLocalization(config_).text(config_.settings.language, "gameover.continue.confirm")
                 : runtimeLocalization(config_).text(config_.settings.language, "gameover.continue.not_enough_button"))
                .c_str(),
            remaining >= 0 ? ui::UiAccent::Pink : ui::UiAccent::Red,
            buttonSize,
            continueSelectedIndex_ == 0);
        if (inputReady && confirm.hovered && continueSelectedIndex_ != 0) {
            continueSelectedIndex_ = 0;
            playUiSound(ui::UiSoundEffect::Hover);
        }
        ImGui::SameLine(0.0f, 14.0f);
        const auto back = ui::UiTheme::renderNeonButton(
            "gameover-back",
            runtimeLocalization(config_).text(config_.settings.language, "gameover.continue.back").c_str(),
            ui::UiAccent::Cyan,
            buttonSize,
            continueSelectedIndex_ == 1);
        if (inputReady && back.hovered && continueSelectedIndex_ != 1) {
            continueSelectedIndex_ = 1;
            playUiSound(ui::UiSoundEffect::Hover);
        }

        const bool activate = inputReady && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space));
        if (inputReady && remaining >= 0 && (confirm.pressed || (continueSelectedIndex_ == 0 && activate))) {
            if (world_->continueAfterGameOver(cost, 3)) {
                ++continuePurchaseCount_;
                resetGameOverState();
                playUiSound(ui::UiSoundEffect::Select);
                playLevelMusic();
            }
        } else if (inputReady && (back.pressed || (continueSelectedIndex_ == 1 && activate))) {
            continuePromptOpen_ = false;
            playUiSound(ui::UiSoundEffect::Back);
        }

        ImGui::End();
        return;
    }

    if (inputReady) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            gameOverSelectedIndex_ = gameOverSelectedIndex_ > 0 ? gameOverSelectedIndex_ - 1 : 2;
            playUiSound(ui::UiSoundEffect::Hover);
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            gameOverSelectedIndex_ = gameOverSelectedIndex_ < 2 ? gameOverSelectedIndex_ + 1 : 0;
            playUiSound(ui::UiSoundEffect::Hover);
        }
    }

    ui::UiLayout::setNextCenteredWindow(540.0f, 430.0f + (1.0f - eased) * 42.0f);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin(
        "Game Over###GameOverWindow",
        nullptr,
        ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoBackground);

    const auto titleText = runtimeLocalization(config_).text(config_.settings.language, "gameover.title");
    const char* title = titleText.c_str();
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - titleSize.x) * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 titlePos = ImGui::GetCursorScreenPos();
    for (int glow = 6; glow >= 1; --glow) {
        const float offset = static_cast<float>(glow) * 1.5f;
        drawList->AddText(
            ImVec2{titlePos.x - offset * 0.25f, titlePos.y - offset * 0.1f},
            ImGui::GetColorU32(ImVec4{1.0f, 0.22f, 0.72f, 0.06f * glow * eased}),
            title);
    }
    ImGui::TextColored(ImVec4{1.0f, 0.43f, 0.78f, eased}, "%s", title);

    ImGui::Dummy(ImVec2{0.0f, 30.0f});
    const auto subtitle = runtimeLocalization(config_).text(config_.settings.language, "gameover.subtitle");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(subtitle.c_str()).x) * 0.5f);
    ImGui::TextColored(ImVec4{1.0f, 1.0f, 1.0f, 0.92f * eased}, "%s", subtitle.c_str());

    const auto scoreLine = runtimeLocalization(config_).text(config_.settings.language, "gameover.score_prefix") + formatNumber(world_->score());
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(scoreLine.c_str()).x) * 0.5f);
    ImGui::TextColored(ImVec4{0.49f, 0.91f, 0.98f, 0.9f * eased}, "%s", scoreLine.c_str());
    ImGui::Dummy(ImVec2{0.0f, 22.0f});

    const ImVec2 buttonSize{240.0f, ui::UiTheme::buttonHeight()};
    const auto drawButton = [&](int index, const char* id, const char* label, ui::UiAccent accent) {
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - buttonSize.x) * 0.5f);
        const auto button = ui::UiTheme::renderNeonButton(id, label, accent, buttonSize, gameOverSelectedIndex_ == index);
        if (inputReady && button.hovered && gameOverSelectedIndex_ != index) {
            gameOverSelectedIndex_ = index;
            playUiSound(ui::UiSoundEffect::Hover);
        }
        ImGui::Dummy(ImVec2{0.0f, 10.0f});
        return button;
    };

    const auto continueButton = drawButton(
        0,
        "gameover-continue",
        runtimeLocalization(config_).text(config_.settings.language, "gameover.continue").c_str(),
        ui::UiAccent::Pink);
    const auto restartButton = drawButton(
        1,
        "gameover-restart",
        runtimeLocalization(config_).text(config_.settings.language, "gameover.restart").c_str(),
        ui::UiAccent::Purple);
    const auto menuButton = drawButton(
        2,
        "gameover-menu",
        runtimeLocalization(config_).text(config_.settings.language, "gameover.main_menu").c_str(),
        ui::UiAccent::Purple);
    const bool activate = inputReady && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space));

    if (inputReady && (continueButton.pressed || (gameOverSelectedIndex_ == 0 && activate))) {
        continuePromptOpen_ = true;
        continueSelectedIndex_ = 0;
        playUiSound(ui::UiSoundEffect::Select);
    } else if (inputReady && (restartButton.pressed || (gameOverSelectedIndex_ == 1 && activate))) {
        playUiSound(ui::UiSoundEffect::Select);
        restartLevel();
    } else if (inputReady && (menuButton.pressed || (gameOverSelectedIndex_ == 2 && activate))) {
        playUiSound(ui::UiSoundEffect::Select);
        exitToMenu();
    }

    ImGui::End();
}

void SdlRuntime::renderLevelCompleteOverlay() {
    if (!levelCompleteMenuOpen_) {
        return;
    }

    const render::Texture* background = levelBackgroundTexture_;
    if (background == nullptr) {
        if (debugBackgroundTexture_ == nullptr) {
            debugBackgroundTexture_ = assets_->texture("sprites/debug.jpeg");
        }
        background = debugBackgroundTexture_;
    }

    ui::LevelCompleteStats stats{
        .playerName = config_.settings.gameplay.playerName,
        .levelNumber = config_.level,
        .score = world_ ? world_->score() : 0,
        .timeSeconds = levelPlayTime_,
        .livesLost = livesLostThisLevel_,
        .positiveBonuses = 0,
        .negativeBonuses = 0
    };

    const auto result = levelCompleteView_.render(
        background,
        runtimeLocalization(config_),
        config_.settings.language,
        stats,
        levelCompleteAge_ - levelCompleteFadeOutDuration_,
        levelCompleteMenuOpen_
    );

    playUiSound(result.soundEffect);

    if (result.action == ui::LevelCompleteAction::Restart) {
        restartLevel();
    } else if (result.action == ui::LevelCompleteAction::Continue) {
        int nextLevel = config_.level + 1;
        auto nextMapping = assetRegistry_.level(nextLevel);
        if (assetRegistry_.exists(nextMapping.levelJson)) {
            startLevel(nextLevel);
        } else {
            std::string msg = runtimeLocalization(config_).text(config_.settings.language, "debug.level_complete.no_levels_left");
            if (msg.empty()) {
                msg = "You have completed all levels!";
            }
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Arcade Blocks II", msg.c_str(), window_);
            resetLevelCompleteState();
            screen_ = RuntimeScreen::MainMenu;
            playMenuMusic();
        }
    } else if (!levelCompleteMenuOpen_) {
        resetLevelCompleteState();
        screen_ = RuntimeScreen::MainMenu;
        playMenuMusic();
    }
}

void SdlRuntime::playUiSound(ui::UiSoundEffect effect) {
    if (!audio_ || effect == ui::UiSoundEffect::None) {
        return;
    }

    switch (effect) {
    case ui::UiSoundEffect::None:
        break;
    case ui::UiSoundEffect::Hover:
        audio_->playSfx("sounds/menu/menu_hover.ogg");
        break;
    case ui::UiSoundEffect::Select:
        audio_->playSfx("sounds/menu/menu_select.ogg");
        break;
    case ui::UiSoundEffect::Back:
        audio_->playSfx("sounds/menu/menu_back.ogg");
        break;
    case ui::UiSoundEffect::SettingsChange:
        audio_->playSfx("sounds/menu/settings_change.ogg");
        break;
    case ui::UiSoundEffect::DebugOpen:
        audio_->playSfx("sounds/menu/debug_open.ogg");
        break;
    case ui::UiSoundEffect::StarGlow:
        audio_->playSfx("sounds/gameplay/level_end_star_glow.ogg");
        break;
    case ui::UiSoundEffect::LevelCompleteRandom: {
        static constexpr std::array<const char*, 5> paths{{
            "sounds/gameplay/level_complete.ogg",
            "sounds/gameplay/level_complete2.ogg",
            "sounds/gameplay/level_complete3.ogg",
            "sounds/gameplay/level_complete4.ogg",
            "sounds/gameplay/level_complete5.ogg",
        }};
        std::uniform_int_distribution<std::size_t> dist(0, paths.size() - 1);
        audio_->playSfx(paths[dist(effectRng_)]);
        break;
    }
    }
}

void SdlRuntime::renderLevelIntro() {
    rendererApi_->beginFrame(render::Color{7, 10, 28, 255});

    if (showLevelBackground_ && levelBackgroundTexture_ == nullptr) {
        levelBackgroundTexture_ = assets_->texture(currentLevelAssets_.background.generic_string());
    }
    render::Texture* background = showLevelBackground_ ? levelBackgroundTexture_ : nullptr;

    if (background != nullptr) {
        rendererApi_->drawTexture(*background, render::Rect{0.0f, 0.0f, 1920.0f, 1080.0f});
    } else {
        rendererApi_->drawRect(render::Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, render::Color{7, 10, 28, 255});
    }

    // Add a dark overlay for readability
    rendererApi_->drawRect(render::Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, render::Color{0, 0, 0, 130});

    // Draw ImGui intro window overlay
    renderLevelIntroOverlay();
}

void SdlRuntime::renderLevelIntroOverlay() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});

    ImGui::Begin("LevelIntroFullscreen", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    double elapsed = levelIntroState_.elapsedSeconds;
    double duration = levelIntroState_.durationSeconds;
    const float scale = ui::UiLayout::viewportScale();

    // 2.1 Easing Slide Animations & Alpha Transitions
    float alpha = 1.0f;
    float offsetY = 0.0f;

    if (elapsed < 0.55) {
        float t = static_cast<float>(elapsed / 0.55);
        t = std::clamp(t, 0.0f, 1.0f);
        // ease-out cubic
        float ease = 1.0f - std::pow(1.0f - t, 3.0f);
        offsetY = - (1.0f - ease) * 80.0f * scale;
        alpha = t;
    } else if (duration - elapsed < 0.55) {
        float t = static_cast<float>((0.55 - (duration - elapsed)) / 0.55);
        t = std::clamp(t, 0.0f, 1.0f);
        // ease-in cubic
        float ease = std::pow(t, 3.0f);
        offsetY = ease * 80.0f * scale;
        alpha = 1.0f - t;
    }
    alpha = std::clamp(alpha, 0.0f, 1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    // 2.4 Boss Mode Settings
    bool isBoss = isIntroBossLevel(levelIntroState_.levelNumber);
    ui::UiAccent currentAccent = isBoss ? ui::UiAccent::Red : levelIntroState_.chapterAccent;
    ImVec4 accentColor = ui::UiTheme::accent(currentAccent);
    ImU32 accentU32 = ImGui::GetColorU32(accentColor);
    ImU32 accentGlow = ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 0.35f});

    // Screen shake for boss levels
    float shakeX = 0.0f;
    float shakeY = 0.0f;
    if (isBoss) {
        shakeX = 2.0f * std::sin(elapsed * 20.0f) * scale;
        shakeY = 2.0f * std::cos(elapsed * 20.0f) * scale;
    }
    float totalOffsetX = shakeX;
    float totalOffsetY = offsetY + shakeY;

    const float centerX = viewport->Size.x * 0.5f;
    const float centerY = viewport->Size.y * 0.5f;
    const float panelW = 750.0f * scale;
    const float panelH = 340.0f * scale;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 minPos = ImVec2{winPos.x + centerX - panelW * 0.5f, winPos.y + centerY - panelH * 0.5f};
    ImVec2 maxPos = ImVec2{minPos.x + panelW, minPos.y + panelH};

    ImVec2 pMin = ImVec2{minPos.x + totalOffsetX, minPos.y + totalOffsetY};
    ImVec2 pMax = ImVec2{maxPos.x + totalOffsetX, maxPos.y + totalOffsetY};
    ImVec2 panelCenter = ImVec2{centerX + totalOffsetX, centerY + totalOffsetY};

    // --- DECORATIVE LAYERS (drawn before text) ---

    // 1. Scanlines (CRT-effect) over the viewport
    float scanlineThickness = (isBoss ? 1.2f : 1.0f) * scale;
    ImU32 scanlineCol = ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, (18.0f / 255.0f) * alpha});
    for (float y = winPos.y; y < winPos.y + viewport->Size.y; y += 3.0f * scale) {
        drawList->AddLine(
            ImVec2{winPos.x, y},
            ImVec2{winPos.x + viewport->Size.x, y},
            scanlineCol,
            scanlineThickness
        );
    }

    // 5. Radial glow behind the panel
    float maxRadius = 480.0f * scale;
    for (int i = 24; i > 0; --i) {
        float r = (static_cast<float>(i) / 24.0f) * maxRadius;
        float norm = static_cast<float>(i) / 24.0f;
        float ringAlpha = (1.0f - norm) * (1.0f - norm) * 0.18f * alpha;
        drawList->AddCircleFilled(panelCenter, r, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, ringAlpha}), 36);
    }

    // Panel Fill
    drawList->AddRectFilled(pMin, pMax, ImGui::GetColorU32(ui::UiTheme::panelFill()), 8.0f * scale);

    // Panel Borders
    if (isBoss) {
        // Double border: inner 2px, outer 1px with 6px offset
        ImVec2 outerMin = ImVec2{pMin.x - 6.0f * scale, pMin.y - 6.0f * scale};
        ImVec2 outerMax = ImVec2{pMax.x + 6.0f * scale, pMax.y + 6.0f * scale};
        
        drawList->AddRect(pMin, pMax, accentU32, 8.0f * scale, 0, 2.0f * scale);
        drawList->AddRect(outerMin, outerMax, accentU32, (8.0f + 6.0f) * scale, 0, 1.0f * scale);
        drawList->AddRect(outerMin, outerMax, accentGlow, (8.0f + 6.0f) * scale, 0, 3.0f * scale);
    } else {
        drawList->AddRect(pMin, pMax, accentU32, 8.0f * scale, 0, 2.0f * scale);
        drawList->AddRect(
            ImVec2{pMin.x - 2.0f * scale, pMin.y - 2.0f * scale},
            ImVec2{pMax.x + 2.0f * scale, pMax.y + 2.0f * scale},
            accentGlow,
            8.0f * scale,
            0,
            3.0f * scale
        );
    }

    // 2. Corner brackets
    float bracketLen = 28.0f * scale;
    float bracketThick = 3.0f * scale;
    auto drawBracketLine = [&](ImVec2 pt1, ImVec2 pt2) {
        drawList->AddLine(pt1, pt2, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 0.25f * alpha}), bracketThick + 4.0f * scale);
        drawList->AddLine(pt1, pt2, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 1.0f * alpha}), bracketThick);
    };

    // Top-Left corner
    drawBracketLine(ImVec2{pMin.x, pMin.y}, ImVec2{pMin.x + bracketLen, pMin.y});
    drawBracketLine(ImVec2{pMin.x, pMin.y}, ImVec2{pMin.x, pMin.y + bracketLen});
    // Top-Right corner
    drawBracketLine(ImVec2{pMax.x, pMin.y}, ImVec2{pMax.x - bracketLen, pMin.y});
    drawBracketLine(ImVec2{pMax.x, pMin.y}, ImVec2{pMax.x, pMin.y + bracketLen});
    // Bottom-Left corner
    drawBracketLine(ImVec2{pMin.x, pMax.y}, ImVec2{pMin.x + bracketLen, pMax.y});
    drawBracketLine(ImVec2{pMin.x, pMax.y}, ImVec2{pMin.x, pMax.y - bracketLen});
    // Bottom-Right corner
    drawBracketLine(ImVec2{pMax.x, pMax.y}, ImVec2{pMax.x - bracketLen, pMax.y});
    drawBracketLine(ImVec2{pMax.x, pMax.y}, ImVec2{pMax.x, pMax.y - bracketLen});

    // 3. Running divider line
    float lineY = pMin.y + (isBoss ? 95.0f : 75.0f) * scale;
    float lineStartX = pMin.x + 30.0f * scale;
    float lineEndX = pMax.x - 30.0f * scale;
    float lineW = lineEndX - lineStartX;
    drawList->AddLine(ImVec2{lineStartX, lineY}, ImVec2{lineEndX, lineY}, accentGlow, 1.5f * scale);

    // Spark and trail
    float sparkOffset = std::fmod(static_cast<float>(elapsed) * 220.0f * scale, lineW);
    float sparkX = lineStartX + sparkOffset;
    for (int i = 5; i >= 1; --i) {
        float trailOffset = sparkOffset - i * 8.0f * scale;
        if (trailOffset < 0.0f) {
            trailOffset += lineW;
        }
        float trailX = lineStartX + trailOffset;
        float trailAlpha = (1.0f - i * 0.18f) * 0.7f * alpha;
        if (trailAlpha > 0.0f) {
            drawList->AddCircleFilled(ImVec2{trailX, lineY}, 2.0f * scale, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, trailAlpha}));
        }
    }
    drawList->AddCircleFilled(ImVec2{sparkX, lineY}, 6.0f * scale, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 0.6f * alpha}));
    drawList->AddCircleFilled(ImVec2{sparkX, lineY}, 3.0f * scale, ImGui::GetColorU32(ImVec4{1.0f, 1.0f, 1.0f, 1.0f * alpha}));

    // 4. Progress bar at bottom
    float barY = pMax.y - 12.0f * scale;
    float barStartX = pMin.x + 30.0f * scale;
    float barEndX = pMax.x - 30.0f * scale;
    float progress = std::clamp(static_cast<float>(elapsed / duration), 0.0f, 1.0f);
    float filledW = (barEndX - barStartX) * progress;
    float currentBarEndX = barStartX + filledW;

    drawList->AddLine(ImVec2{barStartX, barY}, ImVec2{barEndX, barY}, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 0.15f * alpha}), 3.0f * scale);
    drawList->AddLine(ImVec2{barStartX, barY}, ImVec2{currentBarEndX, barY}, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 1.0f * alpha}), 3.0f * scale);

    if (progress > 0.0f && progress < 1.0f) {
        drawList->AddCircleFilled(ImVec2{currentBarEndX, barY}, 5.0f * scale, ImGui::GetColorU32(ImVec4{accentColor.x, accentColor.y, accentColor.z, 0.5f * alpha}));
        drawList->AddCircleFilled(ImVec2{currentBarEndX, barY}, 2.0f * scale, ImGui::GetColorU32(ImVec4{1.0f, 1.0f, 1.0f, 1.0f * alpha}));
    }

    // --- TEXT LAYERS ---

    // 1. Chapter Title (with pulsing glow)
    std::string chapStr = levelIntroState_.chapterTitle;
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::SetWindowFontScale(1.3f * scale);
    ImVec2 chapSz = ImGui::CalcTextSize(chapStr.c_str());
    ImVec2 chapPos = ImVec2{centerX + totalOffsetX - chapSz.x * 0.5f, centerY + totalOffsetY - panelH * 0.5f + 25.0f * scale};

    ImGui::SetCursorPos(chapPos);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{accentColor.x, accentColor.y, accentColor.z, 1.0f * alpha});
    ImGui::TextUnformatted(chapStr.c_str());
    ImGui::PopStyleColor();

    float glowPulse = 0.5f + 0.5f * std::sin(static_cast<float>(elapsed) * 3.2f);
    ImGui::SetWindowFontScale(1.3f * 1.02f * scale);
    ImVec2 chapGlowSz = ImGui::CalcTextSize(chapStr.c_str());
    ImVec2 chapGlowPos = ImVec2{
        centerX + totalOffsetX - chapGlowSz.x * 0.5f,
        chapPos.y - (chapGlowSz.y - chapSz.y) * 0.5f
    };
    ImGui::SetCursorPos(chapGlowPos);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{accentColor.x, accentColor.y, accentColor.z, 0.4f * glowPulse * alpha});
    ImGui::TextUnformatted(chapStr.c_str());
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();

    // 2. Boss Battle Sub-header (only on boss levels)
    if (isBoss) {
        std::string bossText = runtimeLocalization(config_).text(config_.settings.language, "level_intro.boss_badge");
        if (bossText == "level_intro.boss_badge") {
            bossText = config_.settings.language == settings::Language::Russian ? "[ БОСС ]" : "[ BOSS BATTLE ]";
        }
        float pulse = 0.7f + 0.3f * std::abs(std::sin(static_cast<float>(elapsed) * 6.0f));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::SetWindowFontScale(1.2f * scale);
        ImVec2 bossSz = ImGui::CalcTextSize(bossText.c_str());
        ImGui::SetCursorPos(ImVec2{centerX + totalOffsetX - bossSz.x * 0.5f, centerY + totalOffsetY - panelH * 0.5f + 68.0f * scale});
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 0.2f, 0.2f, pulse * alpha});
        ImGui::TextUnformatted(bossText.c_str());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();

        // Per-level boss name (only level 30 currently overrides).
        if (levelIntroState_.levelNumber == 30) {
            std::string bossName = runtimeLocalization(config_).text(config_.settings.language, "level_intro.boss_name_helios");
            if (bossName == "level_intro.boss_name_helios") {
                bossName = config_.settings.language == settings::Language::Russian
                    ? "Гелиос — Ядро Самосознания"
                    : "Helios — Self-Awareness Core";
            }
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::SetWindowFontScale(1.0f * scale);
            ImVec2 bossNameSz = ImGui::CalcTextSize(bossName.c_str());
            ImGui::SetCursorPos(ImVec2{
                centerX + totalOffsetX - bossNameSz.x * 0.5f,
                centerY + totalOffsetY - panelH * 0.5f + 95.0f * scale
            });
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.85f, 0.95f, 1.0f, alpha});
            ImGui::TextUnformatted(bossName.c_str());
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
        }

        if (levelIntroState_.levelNumber == 40) {
            std::string bossName = runtimeLocalization(config_).text(config_.settings.language, "level_intro.boss_name_singularity");
            if (bossName == "level_intro.boss_name_singularity") {
                bossName = config_.settings.language == settings::Language::Russian
                    ? "Сингулярность — Точка Сингулярности"
                    : "Singularity — Singularity Point";
            }
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::SetWindowFontScale(1.0f * scale);
            ImVec2 bossNameSz = ImGui::CalcTextSize(bossName.c_str());
            ImGui::SetCursorPos(ImVec2{
                centerX + totalOffsetX - bossNameSz.x * 0.5f,
                centerY + totalOffsetY - panelH * 0.5f + 95.0f * scale
            });
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.85f, 0.75f, 1.0f, alpha});
            ImGui::TextUnformatted(bossName.c_str());
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
        }
    }

    // 3. Level Number (with overshoot pop-in and breathing)
    auto easeOutBack = [](float tVal) -> float {
        float c1 = 1.70158f;
        float c2 = c1 + 1.0f;
        float tMinus1 = tVal - 1.0f;
        return 1.0f + c2 * std::pow(tMinus1, 3.0f) + c1 * std::pow(tMinus1, 2.0f);
    };

    float popScale = 1.0f;
    if (elapsed < 0.15) {
        popScale = 0.6f;
    } else if (elapsed <= 0.55) {
        float t = static_cast<float>((elapsed - 0.15) / 0.40);
        popScale = 0.6f + 0.4f * easeOutBack(t);
    }
    float breathing = 1.0f + 0.06f * std::sin(static_cast<float>(elapsed) * 1.8f);
    float numScale = popScale * breathing;

    std::string prefix = runtimeLocalization(config_).text(config_.settings.language, "level_intro.level_prefix");
    if (prefix == "level_intro.level_prefix") {
        prefix = config_.settings.language == settings::Language::Russian ? "УРОВЕНЬ " : "LEVEL ";
    }
    std::string levelNumStr = prefix + std::to_string(levelIntroState_.levelNumber);

    ImGui::SetWindowFontScale(1.8f * scale);
    ImVec2 baseNumSz = ImGui::CalcTextSize(levelNumStr.c_str());
    ImGui::SetWindowFontScale(1.8f * numScale * scale);
    ImVec2 scaledNumSz = ImGui::CalcTextSize(levelNumStr.c_str());

    float numY = centerY + totalOffsetY - panelH * 0.5f + (isBoss ? 130.0f : 120.0f) * scale;
    ImGui::SetCursorPos(ImVec2{
        centerX + totalOffsetX - scaledNumSz.x * 0.5f,
        numY - (scaledNumSz.y - baseNumSz.y) * 0.5f
    });
    ImGui::TextUnformatted(levelNumStr.c_str());
    ImGui::SetWindowFontScale(1.0f);

    // 4. Level Name (Typewriter)
    std::string nameStr = "«" + levelIntroState_.levelName + "»";
    int totalLen = getUtf8Length(nameStr);
    int charsShown = std::clamp(static_cast<int>((elapsed - 0.45) / 0.04), 0, totalLen);
    std::string typedText = getUtf8Prefix(nameStr, charsShown);
    if (charsShown < totalLen && std::fmod(elapsed, 0.5) < 0.25) {
        typedText += "|";
    }

    ImGui::SetWindowFontScale(1.4f * scale);
    ImVec2 fullNameSz = ImGui::CalcTextSize(nameStr.c_str());
    float nameTextX = centerX + totalOffsetX - fullNameSz.x * 0.5f;
    float nameTextY = centerY + totalOffsetY - panelH * 0.5f + (isBoss ? 205.0f : 195.0f) * scale;

    ImGui::SetCursorPos(ImVec2{nameTextX, nameTextY});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.8f, 0.8f, 0.8f, 1.0f * alpha});
    ImGui::TextUnformatted(typedText.c_str());
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);

    // 5. Debug Badge (if from debug menu)
    if (levelIntroState_.fromDebugMenu) {
        std::string badge = runtimeLocalization(config_).text(config_.settings.language, "level_intro.debug_badge");
        if (badge == "level_intro.debug_badge") {
            badge = config_.settings.language == settings::Language::Russian ? "[ РЕЖИМ ОТЛАДКИ ]" : "[ DEBUG MODE ]";
        }
        float badgePulse = 0.8f + 0.2f * std::sin(static_cast<float>(elapsed) * 12.56637f);
        ImGui::SetWindowFontScale(1.1f * scale);
        ImVec2 badgeSz = ImGui::CalcTextSize(badge.c_str());
        float badgeY = centerY + totalOffsetY - panelH * 0.5f + (isBoss ? 280.0f : 275.0f) * scale;

        ImGui::SetCursorPos(ImVec2{centerX + totalOffsetX - badgeSz.x * 0.5f, badgeY});
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 0.1f, 0.1f, badgePulse * alpha});
        ImGui::TextUnformatted(badge.c_str());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
    }

    ImGui::PopStyleVar();
    ImGui::End();
}

void SdlRuntime::renderGameScene() {
    rendererApi_->beginFrame(render::Color{7, 10, 28, 255});

    if (showLevelBackground_ && levelBackgroundTexture_ == nullptr) {
        levelBackgroundTexture_ = assets_->texture(currentLevelAssets_.background.generic_string());
    }
    render::Texture* background = showLevelBackground_ ? levelBackgroundTexture_ : nullptr;

    if (background != nullptr) {
        rendererApi_->drawTexture(*background, render::Rect{0.0f, 0.0f, 1920.0f, 1080.0f});
    } else {
        rendererApi_->drawRect(render::Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, render::Color{7, 10, 28, 255});
    }

    rendererApi_->drawRect(render::Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, render::Color{0, 0, 0, 45});

    renderGameWorldBricks();
    renderBoss();
    renderBrickImpactEffects();

    if (paddleDarknessBlend_ > 0.0f) {
        unsigned char alpha = static_cast<unsigned char>(242.0f * std::clamp(paddleDarknessBlend_, 0.0f, 1.0f));
        rendererApi_->drawRect(render::Rect{0.0f, 0.0f, 1920.0f, 1080.0f}, render::Color{12, 6, 24, alpha});
    }

    if (world_) {
        const float gameOverFade = gameOverFadeProgress();
        const float levelCompleteFade = levelCompleteFadeProgress();
        const float fadeProgress = std::max(gameOverFade, levelCompleteFade);
        const auto gameplayAlpha = static_cast<unsigned char>(std::clamp(1.0f - fadeProgress, 0.0f, 1.0f) * 255.0f);

        // Render BONUS_WALL active barrier
        if (world_->isBonusActive("BONUS_WALL")) {
            float opacityFactor = 1.0f;
            for (const auto& timer : world_->activeBonusTimers()) {
                if (timer.type == "BONUS_WALL") {
                    if (timer.remainingSeconds > 0.0) {
                        if (timer.remainingSeconds <= 1.5) {
                            opacityFactor = static_cast<float>(timer.remainingSeconds / 1.5);
                        }
                    } else if (timer.fadeOutRemainingSeconds > 0.0) {
                        opacityFactor = static_cast<float>(timer.fadeOutRemainingSeconds / 0.75);
                    }
                    break;
                }
            }
            float pulse = 0.7f + 0.3f * std::sin(static_cast<float>(world_->paddleSpawnAge()) * 12.0f);
            unsigned char alpha = static_cast<unsigned char>(180 * opacityFactor * pulse * (gameplayAlpha / 255.0f));
            const auto& bounds = world_->bounds().bounds;
            float wallY = bounds.bottom - 6.0f;
            float wallH = 8.0f;
            float wallW = bounds.right - bounds.left;
            // Glowing light blue/cyan neon wall
            rendererApi_->drawRect(render::Rect{bounds.left, wallY - 2.0f, wallW, wallH + 4.0f}, render::Color{0, 150, 255, static_cast<unsigned char>(alpha * 0.4f)});
            rendererApi_->drawRect(render::Rect{bounds.left, wallY, wallW, wallH}, render::Color{130, 220, 255, alpha});
        }

        const auto& paddle = world_->paddle();
        
        float spawnScale = 1.0f;
        float spawnAlpha = 1.0f;
        if (world_->paddleSpawnAge() < 1.0) {
            const double t = world_->paddleSpawnAge();
            spawnScale = static_cast<float>(t * t * (3.0 - 2.0 * t)); // Smoothstep
            spawnAlpha = static_cast<float>(t);
        }
        
        float energyBallBlendFactor = 0.0f;
        float explosionBallBlendFactor = 0.0f;
        if (world_) {
            for (const auto& timer : world_->activeBonusTimers()) {
                if (timer.type == "ENERGY_BALLS") {
                    if (timer.remainingSeconds > 0.0) {
                        double age = timer.durationSeconds - timer.remainingSeconds;
                        energyBallBlendFactor = std::clamp(static_cast<float>(age / 0.5), 0.0f, 1.0f);
                    } else if (timer.fadeOutRemainingSeconds > 0.0) {
                        energyBallBlendFactor = std::clamp(static_cast<float>(timer.fadeOutRemainingSeconds / 0.75), 0.0f, 1.0f);
                    }
                } else if (timer.type == "EXPLOSION_BALLS") {
                    if (timer.remainingSeconds > 0.0) {
                        double age = timer.durationSeconds - timer.remainingSeconds;
                        explosionBallBlendFactor = std::clamp(static_cast<float>(age / 0.5), 0.0f, 1.0f);
                    } else if (timer.fadeOutRemainingSeconds > 0.0) {
                        explosionBallBlendFactor = std::clamp(static_cast<float>(timer.fadeOutRemainingSeconds / 0.75), 0.0f, 1.0f);
                    }
                }
            }
        }
        
        const float renderWidth = paddle.size.w * spawnScale;
        const float renderX = paddle.position.x + (paddle.size.w - renderWidth) * 0.5f;
        
        float invisibleFactor = 1.0f - std::clamp(paddleInvisibleBlend_, 0.0f, 1.0f);
        const auto baseAlpha = static_cast<unsigned char>(gameplayAlpha * spawnAlpha * invisibleFactor);
        
        // Multi-state paddle blending formulas ( Frozen -> Plasma+Sticky -> Plasma -> Sticky -> Normal )
        float W_frozen = std::clamp(paddleFrozenBlend_, 0.0f, 1.0f);
        float W_plasma_sticky = std::clamp((1.0f - W_frozen) * paddlePlasmaBlend_ * paddleStickyBlend_, 0.0f, 1.0f);
        float W_plasma = std::clamp((1.0f - W_frozen) * paddlePlasmaBlend_ * (1.0f - paddleStickyBlend_), 0.0f, 1.0f);
        float W_sticky = std::clamp((1.0f - W_frozen) * (1.0f - paddlePlasmaBlend_) * paddleStickyBlend_, 0.0f, 1.0f);
        float W_normal = std::clamp((1.0f - W_frozen) * (1.0f - paddlePlasmaBlend_) * (1.0f - paddleStickyBlend_), 0.0f, 1.0f);

        auto drawSpecialtyPaddle = [&](const char* spriteName, float ox_sprite, float oy_sprite, float w_sprite, float h_sprite, unsigned char alpha) {
            float scaleX = renderWidth / 847.0f;
            float scaleY = paddle.size.h / 258.0f;
            float left_offset = (ox_sprite - 88.0f) * scaleX;
            float top_offset = (oy_sprite - 385.0f) * scaleY;
            float w_draw = w_sprite * scaleX;
            float h_draw = h_sprite * scaleY;
            renderSpriteOrPlaceholder(
                spriteName,
                renderX + left_offset,
                paddle.position.y + top_offset,
                w_draw,
                h_draw,
                alpha
            );
        };

        if (W_normal > 0.0f) {
            drawSpecialtyPaddle("paddle.png", 88.0f, 385.0f, 847.0f, 258.0f, static_cast<unsigned char>(baseAlpha * W_normal));
        }
        if (W_sticky > 0.0f) {
            drawSpecialtyPaddle("slime_paddle.png", 73.0f, 257.0f, 879.0f, 461.0f, static_cast<unsigned char>(baseAlpha * W_sticky));
        }
        if (W_plasma > 0.0f) {
            drawSpecialtyPaddle("pw_paddle.png", 88.0f, 188.0f, 849.0f, 627.0f, static_cast<unsigned char>(baseAlpha * W_plasma));
        }
        if (W_plasma_sticky > 0.0f) {
            drawSpecialtyPaddle("slime_pw_paddle.png", 83.0f, 19.0f, 866.0f, 916.0f, static_cast<unsigned char>(baseAlpha * W_plasma_sticky));
        }
        if (W_frozen > 0.0f) {
            drawSpecialtyPaddle("freeze_paddle.png", 88.0f, 385.0f, 847.0f, 258.0f, static_cast<unsigned char>(baseAlpha * W_frozen));
        }

        // Render plasma bullets
        for (const auto& bullet : world_->plasmaBullets()) {
            if (bullet.alive) {
                renderSpriteOrPlaceholder(
                    "plasma_shot.png",
                    bullet.position.x, bullet.position.y,
                    bullet.size.w, bullet.size.h,
                    gameplayAlpha
                );
            }
        }


        auto drawBallWithGlow = [&](float x, float y, float radius, unsigned char alpha) {
            float normalAlphaFactor = 1.0f - energyBallBlendFactor - explosionBallBlendFactor;
            normalAlphaFactor = std::clamp(normalAlphaFactor, 0.0f, 1.0f);
            
            if (normalAlphaFactor > 0.0f) {
                renderSpriteOrPlaceholder(
                    "ball.png",
                    x, y, radius * 2.0f, radius * 2.0f,
                    static_cast<unsigned char>(alpha * normalAlphaFactor));
            }
            if (energyBallBlendFactor > 0.0f) {
                renderSpriteOrPlaceholder(
                    "energy_ball.png",
                    x, y, radius * 2.0f, radius * 2.0f,
                    static_cast<unsigned char>(alpha * energyBallBlendFactor));
            }
            if (explosionBallBlendFactor > 0.0f) {
                renderSpriteOrPlaceholder(
                    "explosion_ball.png",
                    x, y, radius * 2.0f, radius * 2.0f,
                    static_cast<unsigned char>(alpha * explosionBallBlendFactor));
            }
        };

        const auto& ball = world_->ball();

        // Render launch trajectories for attached balls
        if (config_.settings.gameplay.showLaunchTrajectory) {
            if (ball.state == gameplay::BallState::AttachedToPaddle) {
                renderLaunchTrajectory(ball, 220.0f, gameplayAlpha);
            }
            float spread = -150.0f;
            for (const auto& eb : world_->extraBalls()) {
                if (eb.state == gameplay::BallState::AttachedToPaddle) {
                    renderLaunchTrajectory(eb, spread, gameplayAlpha);
                    spread += 100.0f;
                }
            }
        }

        drawBallWithGlow(ball.position.x - ball.radius, ball.position.y - ball.radius, ball.radius, gameplayAlpha);

        for (const auto& eb : world_->extraBalls()) {
            drawBallWithGlow(eb.position.x - eb.radius, eb.position.y - eb.radius, eb.radius, gameplayAlpha);
        }

        for (const auto& bonus : world_->fallingBonuses()) {
            if (bonus.alive) {
                std::string spriteName;
                if (bonus.type == "BONUS_SCORE") {
                    spriteName = "bonus_score.png";
                } else if (bonus.type == "BONUS_SCORE_200") {
                    spriteName = "bonus_score200.png";
                } else if (bonus.type == "BONUS_SCORE_500") {
                    spriteName = "bonus_score500.png";
                } else if (bonus.type == "BONUS_SCORE_10000") {
                    spriteName = "10k_score.png";
                } else if (bonus.type == "EXTRA_LIFE") {
                    spriteName = "extra_life.png";
                } else if (bonus.type == "BONUS_BALL") {
                    spriteName = "bonus_ball.png";
                } else if (bonus.type == "CALL_BALL") {
                    spriteName = "call_ball_to_paddle_bonus.png";
                } else if (bonus.type == "SLOW_BALLS") {
                    spriteName = "slow_balls.png";
                } else if (bonus.type == "FAST_BALLS") {
                    spriteName = "fast_balls.png";
                } else if (bonus.type == "SCORE_RAIN") {
                    spriteName = "score_rain.png";
                } else if (bonus.type == "WEAK_BALLS") {
                    spriteName = "weak_balls.png";
                } else if (bonus.type == "ENERGY_BALLS") {
                    spriteName = "energy_balls.png";
                } else if (bonus.type == "EXPLOSION_BALLS") {
                    spriteName = "explosion_balls.png";
                } else if (bonus.type == "INCREASE_PADDLE") {
                    spriteName = "increase_paddle.png";
                } else if (bonus.type == "DECREASE_PADDLE") {
                    spriteName = "decrease_paddle.png";
                } else if (bonus.type == "STICKY_PADDLE") {
                    spriteName = "sticky_paddle.png";
                } else if (bonus.type == "PLASMA_WEAPON") {
                    spriteName = "plasma_weapon.png";
                } else if (bonus.type == "FROZEN_PADDLE") {
                    spriteName = "freeze_paddle.png";
                } else if (bonus.type == "INVISIBLE_PADDLE") {
                    spriteName = "invisible_paddle.png";
                } else if (bonus.type == "BONUS_WALL") {
                    spriteName = "bonus_wall.png";
                } else if (bonus.type == "DARKNESS") {
                    spriteName = "darkness.png";
                } else if (bonus.type == "CHAOTIC_BALLS") {
                    spriteName = "chaotic_balls.png";
                } else if (bonus.type == "BONUS_MAGNET") {
                    spriteName = "bonus_magnet.png";
                } else if (bonus.type == "PENALTIES_MAGNET") {
                    spriteName = "penalties_magnet.png";
                } else if (bonus.type == "BAD_LUCK") {
                    spriteName = "bad_luck.png";
                } else if (bonus.type == "TRICKSTER") {
                    spriteName = "trickster.png";
                } else if (bonus.type == "ADD_FIVE_SECONDS") {
                    spriteName = "bonus_add_five_second.png";
                } else if (bonus.type == "RESET") {
                    spriteName = "reset.png";
                } else if (bonus.type == "RANDOM_BONUS") {
                    spriteName = "random.png";
                } else if (bonus.type == "LEVEL_PASS") {
                    spriteName = "level_complete_bonus.png";
                } else if (bonus.type == "RAINBOW_BOUNTY") {
                    spriteName = "rainbow_bounty.png";
                } else if (bonus.type == "BLOOD_TITHE") {
                    spriteName = "blood_tithe.png";
                }
                if (!spriteName.empty()) {
                    float scale = 1.0f;
                    if (bonus.age < 0.3) {
                        scale = static_cast<float>(bonus.age / 0.3);
                        scale = scale * scale * (3.0f - 2.0f * scale); // Smoothstep
                    }
                    if (bonus.type == "LEVEL_PASS" || bonus.type == "TRICKSTER") {
                        // Pulsing animation
                        scale *= 1.0f + 0.15f * std::sin(static_cast<float>(bonus.age) * 8.0f);
                    }
                    float w = bonus.size.w * scale;
                    float h = bonus.size.h * scale;
                    if (bonus.type == "BONUS_SCORE_10000" ||
                        bonus.type == "LEVEL_PASS" ||
                        bonus.type == "FROZEN_PADDLE" ||
                        bonus.type == "DARKNESS" ||
                        bonus.type == "BAD_LUCK" ||
                        bonus.type == "EXPLOSION_BALLS" ||
                        bonus.type == "TRICKSTER") {
                        
                        float aspect = 1.0f;
                        if (gameplayAtlas_ != nullptr) {
                            if (const auto frame = gameplayAtlas_->find(spriteName)) {
                                if (frame->h > 0) {
                                    aspect = static_cast<float>(frame->w) / static_cast<float>(frame->h);
                                }
                            }
                        }
                        
                        float targetSize = 48.0f;
                        if (bonus.type == "LEVEL_PASS" || bonus.type == "TRICKSTER" || bonus.type == "BAD_LUCK") {
                            targetSize = 52.0f;
                        }
                        
                        if (aspect >= 1.0f) {
                            w = targetSize * aspect * scale;
                            h = targetSize * scale;
                        } else {
                            w = targetSize * scale;
                            h = (targetSize / aspect) * scale;
                        }
                    }
                    float x = bonus.position.x + (bonus.size.w - w) * 0.5f;
                    float y = bonus.position.y + (bonus.size.h - h) * 0.5f;

                    float alphaMult = 1.0f;
                    if (bonus.fadeOutRemainingSeconds > 0.0) {
                        alphaMult = static_cast<float>(bonus.fadeOutRemainingSeconds / 0.75);
                    }
                    if (bonus.type == "RAINBOW_BOUNTY") {
                        int trailCount = 6;
                        for (int i = 0; i < trailCount; ++i) {
                            float offset = 18.0f * (i + 1);
                            float trailX = x;
                            float trailY = y - offset;
                            float trailW = w * (1.0f - i * 0.10f);
                            float trailH = h * (1.0f - i * 0.10f);
                            float offsetX = (w - trailW) * 0.5f;
                            float offsetY = (h - trailH) * 0.5f;
                            
                            float alphaFactor = 1.0f - static_cast<float>(i) / static_cast<float>(trailCount);
                            unsigned char trailAlpha = static_cast<unsigned char>(180 * alphaFactor * (gameplayAlpha * alphaMult / 255.0f));
                            
                            renderSpriteOrPlaceholder(
                                "rainbow_bounty.png",
                                trailX + offsetX,
                                trailY + offsetY,
                                trailW,
                                trailH,
                                trailAlpha
                            );
                        }
                    } else if (bonus.type == "BLOOD_TITHE") {
                        int trailCount = 6;
                        for (int i = 0; i < trailCount; ++i) {
                            float offset = 18.0f * (i + 1);
                            float trailX = x;
                            float trailY = y - offset;
                            float trailW = w * (1.0f - i * 0.10f);
                            float trailH = h * (1.0f - i * 0.10f);
                            float offsetX = (w - trailW) * 0.5f;
                            float offsetY = (h - trailH) * 0.5f;
                            
                            float alphaFactor = 1.0f - static_cast<float>(i) / static_cast<float>(trailCount);
                            unsigned char trailAlpha = static_cast<unsigned char>(180 * alphaFactor * (gameplayAlpha * alphaMult / 255.0f));
                            
                            renderSpriteOrPlaceholder(
                                "blood_tithe.png",
                                trailX + offsetX,
                                trailY + offsetY,
                                trailW,
                                trailH,
                                trailAlpha
                            );
                        }
                    }
                    renderSpriteOrPlaceholder(
                        spriteName,
                        x,
                        y,
                        w,
                        h,
                        gameplayAlpha * alphaMult);
                }
            }
        }
    }

    renderBonusEffects();
    renderLifeLostEffects();
    renderRespawnChargeEffect();

    renderPhysicsDebugOverlay();

    if (debugOverlayVisible_) {
        const auto statsLine = formatString(
            "fps %.1f  frame %.3f ms  updates/frame %llu  alpha %.2f",
            frameStats_.framesPerSecond,
            frameStats_.frameSeconds * 1000.0,
            static_cast<unsigned long long>(frameStats_.updatesThisFrame),
            frameStats_.interpolationAlpha);
        rendererApi_->drawText(24.0f, 168.0f, statsLine);
    }
}

void SdlRuntime::renderMainMenuScene() {
    // Background is now drawn by ImGui MainMenuView with random selection.
    rendererApi_->beginFrame(render::Color{5, 9, 24, 255});
}

void SdlRuntime::renderHud() {
    if (!world_) {
        return;
    }

    std::vector<ui::BonusTimerModel> bonusTimers;
    bonusTimers.reserve(world_->activeBonusTimers().size());
    const auto& localization = runtimeLocalization(config_);
    for (const auto& timer : world_->activeBonusTimers()) {
        std::string name;
        if (timer.type == "CALL_BALL") {
            name = localization.text(config_.settings.language, "bonus.call_ball.name");
            if (name.empty()) {
                name = localization.text(config_.settings.language, "help.bonuses.call_ball.title");
            }
        } else if (timer.type == "SLOW_BALLS") {
            name = localization.text(config_.settings.language, "bonus.slow_balls.name");
        } else if (timer.type == "FAST_BALLS") {
            name = localization.text(config_.settings.language, "bonus.fast_balls.name");
        } else if (timer.type == "SCORE_RAIN") {
            name = localization.text(config_.settings.language, "bonus.score_rain.name");
        } else if (timer.type == "WEAK_BALLS") {
            name = localization.text(config_.settings.language, "bonus.weak_balls.name");
        } else if (timer.type == "ENERGY_BALLS") {
            name = localization.text(config_.settings.language, "bonus.energy_balls.name");
        } else if (timer.type == "EXPLOSION_BALLS") {
            name = localization.text(config_.settings.language, "bonus.explosion_balls.name");
        } else if (timer.type == "INCREASE_PADDLE") {
            name = localization.text(config_.settings.language, "bonus.increase_paddle.name");
            if (timer.stacks > 1) {
                name += " x" + std::to_string(timer.stacks);
            }
        } else if (timer.type == "DECREASE_PADDLE") {
            name = localization.text(config_.settings.language, "bonus.decrease_paddle.name");
        } else if (timer.type == "STICKY_PADDLE") {
            name = localization.text(config_.settings.language, "bonus.sticky_paddle.name");
        } else if (timer.type == "PLASMA_WEAPON") {
            name = localization.text(config_.settings.language, "bonus.plasma_weapon.name");
        } else if (timer.type == "FROZEN_PADDLE") {
            name = localization.text(config_.settings.language, "bonus.frozen_paddle.name");
        } else if (timer.type == "INVISIBLE_PADDLE") {
            name = localization.text(config_.settings.language, "bonus.invisible_paddle.name");
        } else if (timer.type == "BONUS_WALL") {
            name = localization.text(config_.settings.language, "bonus.bonus_wall.name");
        } else if (timer.type == "DARKNESS") {
            name = localization.text(config_.settings.language, "bonus.darkness.name");
        } else if (timer.type == "CHAOTIC_BALLS") {
            name = localization.text(config_.settings.language, "bonus.chaotic_balls.name");
        } else if (timer.type == "BONUS_MAGNET") {
            name = localization.text(config_.settings.language, "bonus.bonus_magnet.name");
        } else if (timer.type == "PENALTIES_MAGNET") {
            name = localization.text(config_.settings.language, "bonus.penalties_magnet.name");
        } else if (timer.type == "BAD_LUCK") {
            name = localization.text(config_.settings.language, "bonus.bad_luck.name");
        } else if (timer.type == "TRICKSTER") {
            name = localization.text(config_.settings.language, "bonus.trickster.name");
        } else if (timer.type == "RAINBOW_BOUNTY") {
            name = localization.text(config_.settings.language, "bonus.rainbow_bounty.name");
        } else if (timer.type == "BLOOD_TITHE") {
            name = localization.text(config_.settings.language, "bonus.blood_tithe.name");
        }
        if (name.empty()) {
            name = timer.type;
        }
        void* iconTexture = nullptr;
        float uv0x = 0.0f;
        float uv0y = 0.0f;
        float uv1x = 1.0f;
        float uv1y = 1.0f;
        
        std::string frameName;
        if (timer.type == "CALL_BALL") {
            frameName = "call_ball_to_paddle_bonus.png";
        } else if (timer.type == "SLOW_BALLS") {
            frameName = "slow_balls.png";
        } else if (timer.type == "FAST_BALLS") {
            frameName = "fast_balls.png";
        } else if (timer.type == "SCORE_RAIN") {
            frameName = "score_rain.png";
        } else if (timer.type == "WEAK_BALLS") {
            frameName = "weak_balls.png";
        } else if (timer.type == "ENERGY_BALLS") {
            frameName = "energy_balls.png";
        } else if (timer.type == "EXPLOSION_BALLS") {
            frameName = "explosion_balls.png";
        } else if (timer.type == "INCREASE_PADDLE") {
            frameName = "increase_paddle.png";
        } else if (timer.type == "DECREASE_PADDLE") {
            frameName = "decrease_paddle.png";
        } else if (timer.type == "STICKY_PADDLE") {
            frameName = "sticky_paddle.png";
        } else if (timer.type == "PLASMA_WEAPON") {
            frameName = "plasma_weapon.png";
        } else if (timer.type == "FROZEN_PADDLE") {
            frameName = "freeze_paddle.png";
        } else if (timer.type == "INVISIBLE_PADDLE") {
            frameName = "invisible_paddle.png";
        } else if (timer.type == "BONUS_WALL") {
            frameName = "bonus_wall.png";
        } else if (timer.type == "DARKNESS") {
            frameName = "darkness.png";
        } else if (timer.type == "CHAOTIC_BALLS") {
            frameName = "chaotic_balls.png";
        } else if (timer.type == "BONUS_MAGNET") {
            frameName = "bonus_magnet.png";
        } else if (timer.type == "PENALTIES_MAGNET") {
            frameName = "penalties_magnet.png";
        } else if (timer.type == "BAD_LUCK") {
            frameName = "bad_luck.png";
        } else if (timer.type == "TRICKSTER") {
            frameName = "trickster.png";
        } else if (timer.type == "RAINBOW_BOUNTY") {
            frameName = "rainbow_bounty.png";
        } else if (timer.type == "BLOOD_TITHE") {
            frameName = "blood_tithe.png";
        }

        if (!frameName.empty() && gameplayAtlas_ != nullptr && gameplayAtlasTexture_ != nullptr) {
            if (const auto frame = gameplayAtlas_->find(frameName);
                frame && !frame->rotated && gameplayAtlasTexture_->width() > 0 && gameplayAtlasTexture_->height() > 0) {
                iconTexture = reinterpret_cast<void*>(gameplayAtlasTexture_->native());
                uv0x = static_cast<float>(frame->x) / static_cast<float>(gameplayAtlasTexture_->width());
                uv0y = static_cast<float>(frame->y) / static_cast<float>(gameplayAtlasTexture_->height());
                uv1x = static_cast<float>(frame->x + frame->w) / static_cast<float>(gameplayAtlasTexture_->width());
                uv1y = static_cast<float>(frame->y + frame->h) / static_cast<float>(gameplayAtlasTexture_->height());
            }
        }

        bonusTimers.push_back(ui::BonusTimerModel{
            .type = timer.type,
            .name = std::move(name),
            .durationSeconds = timer.durationSeconds,
            .remainingSeconds = timer.remainingSeconds,
            .fadeOutRemainingSeconds = timer.fadeOutRemainingSeconds,
            .iconTexture = iconTexture,
            .iconUv0x = uv0x,
            .iconUv0y = uv0y,
            .iconUv1x = uv1x,
            .iconUv1y = uv1y,
        });
    }

    hudView_.render(ui::HudModel{
        .level = config_.level,
        .score = world_->score(),
        .lives = world_->lives(),
        .activeBricks = world_->activeBrickCount(),
        .entityCount = static_cast<int>(world_->entities().size()),
        .hasBoss = world_->hasBoss(),
        .bossHealthNormalized = world_->hasBoss() ? world_->bossHealthNormalized() : 0.0f,
        .bossSectionHealthsNormalized = [&]() {
            std::vector<float> r;
            if (world_->hasBoss() && world_->boss().sectionCount > 1) {
                for (int i = 0; i < world_->boss().sectionCount; ++i) {
                    float val = world_->boss().sectionMaxHealth[i] > 0 
                        ? static_cast<float>(world_->boss().sectionHealth[i]) / world_->boss().sectionMaxHealth[i] 
                        : 0.0f;
                    r.push_back(val);
                }
            }
            return r;
        }(),
        .bossHpLabel = [&]() {
            if (world_->hasBoss() && world_->boss().sectionCount > 1) {
                std::string text = runtimeLocalization(config_).text(config_.settings.language, "gameplay.boss_section_hp_short");
                return text == "gameplay.boss_section_hp_short" ? (config_.settings.language == settings::Language::Russian ? "СЕКЦ. :" : "SECT. :") : text;
            }
            return std::string("BOSS HP:");
        }(),
        .levelName = levelName_,
        .phase = gameplay::toString(world_->phase()),
        .assetStats = textureStatsLine(assets_->stats()),
        .audioStats = audio_ ? [this] {
            const auto stats = audio_->stats();
            std::ostringstream output;
            output << "audio " << (stats.initialized ? "on" : (stats.enabled ? "unavailable" : "off"))
                   << "  sfx " << stats.sfxLoaded
                   << "  approx " << (static_cast<double>(stats.approximateSfxBytes) / (1024.0 * 1024.0)) << " MiB";
            if (!stats.currentMusic.empty()) {
                output << "  music " << stats.currentMusic;
            }
            return output.str();
        }() : std::string{"audio unavailable"},
        .framesPerSecond = frameStats_.framesPerSecond,
        .frameMilliseconds = frameStats_.frameSeconds * 1000.0,
        .updatesThisFrame = frameStats_.updatesThisFrame,
        .debug = debugOverlayVisible_,
        .assetStatsVisible = assetStatsVisible_,
        .physicsDebug = world_->physicsDebugDrawEnabled(),
        .bonusTimerVisibility = bonusTimerHudVisibility_,
        .bonusTimers = std::move(bonusTimers),
    });
}

void SdlRuntime::handleMainMenuAction(const ui::MainMenuRenderResult& result, bool& running) {
    switch (result.action) {
    case ui::MainMenuAction::None:
        break;
    case ui::MainMenuAction::StartLevel1:
        startLevel(1);
        break;
    case ui::MainMenuAction::Exit:
        running = false;
        break;
    }
}

void SdlRuntime::handlePauseAction(const ui::PauseRenderResult& result) {
    switch (result.action) {
    case ui::PauseAction::None:
        break;
    case ui::PauseAction::Resume:
        resumeGame();
        break;
    case ui::PauseAction::Settings:
        settingsOpen_ = true;
        break;
    case ui::PauseAction::Help:
        helpOpen_ = true;
        break;
    case ui::PauseAction::Restart:
        restartLevel();
        break;
    case ui::PauseAction::ExitToMenu:
        exitToMenu();
        break;
    }
}

void SdlRuntime::handleHelpAction(const ui::HelpRenderResult& result) {
    if (result.action == ui::HelpAction::Close) {
        helpOpen_ = false;
    }
}

void SdlRuntime::handleSettingsAction(const ui::SettingsRenderResult& result) {
    if (result.settings) {
        config_.settings.language = result.settings->language;
    }

    if (result.audioPreviewChanged && result.settings) {
        config_.settings.audio = result.settings->audio;
        applyAudioSettings();
    }

    switch (result.action) {
    case ui::SettingsAction::None:
        break;
    case ui::SettingsAction::Apply:
        if (result.settings) {
            applyRuntimeSettings(*result.settings);
            static_cast<void>(saveRuntimeSettings());
        }
        break;
    case ui::SettingsAction::Close:
        if (result.settings) {
            applyRuntimeSettings(*result.settings);
            static_cast<void>(saveRuntimeSettings());
        }
        settingsOpen_ = false;
        break;
    case ui::SettingsAction::Discard:
        if (result.settings) {
            applyRuntimeSettings(*result.settings);
        }
        settingsOpen_ = false;
        break;
    case ui::SettingsAction::TestSound:
        if (audio_) {
            audio_->playSfx("sounds/menu/settings_change.ogg");
        }
        break;
    }
}

void SdlRuntime::applyRuntimeSettings(const settings::GameSettings& settings) {
    config_.settings = settings;
    inputBindings_ = InputBindings{
        .moveLeft = keycodeOrDefault(config_.settings.controls.moveLeft.keyName, SDLK_LEFT),
        .moveRight = keycodeOrDefault(config_.settings.controls.moveRight.keyName, SDLK_RIGHT),
        .launch = keycodeOrDefault(config_.settings.controls.launch.keyName, SDLK_SPACE),
        .pause = keycodeOrDefault(config_.settings.controls.pause.keyName, SDLK_ESCAPE),
        .callBall = keycodeOrDefault(config_.settings.controls.callBall.keyName, SDLK_B),
        .turbo = keycodeOrDefault(config_.settings.controls.turbo.keyName, SDLK_X),
        .turboBall = keycodeOrDefault(config_.settings.controls.turboBall.keyName, SDLK_V),
        .plasma = keycodeOrDefault(config_.settings.controls.plasma.keyName, SDLK_Z),
    };
    if (world_) {
        world_->setPaddleSpeed(config_.settings.gameplay.paddleSpeed);
        world_->setTurboBallSpeed(config_.settings.gameplay.turboSpeed);
    }
    showLevelBackground_ = config_.settings.video.showLevelBackground;
    vsyncEnabled_ = config_.settings.video.vsync;

    if (const auto parsed = settings::parseResolutionString(config_.settings.video.resolution)) {
        config_.windowWidth = parsed->first;
        config_.windowHeight = parsed->second;
    }

    const bool targetFullscreen = config_.settings.video.windowMode == settings::VideoWindowMode::Fullscreen;
    if (targetFullscreen != fullscreen_) {
        fullscreen_ = targetFullscreen;
        if (!SDL_SetWindowFullscreen(window_, fullscreen_)) {
            core::Log::warn(sdlError("SDL_SetWindowFullscreen"));
            fullscreen_ = !targetFullscreen;
            config_.settings.video.windowMode = fullscreen_
                ? settings::VideoWindowMode::Fullscreen
                : settings::VideoWindowMode::Windowed;
        }
    }

    applyVideoSettings();
    applyAudioSettings();

    uiScale_ = std::clamp(config_.settings.video.uiScale, 0.75f, 2.0f);
    if (imgui_) {
        imgui_->setScale(uiScale_);
    }

    updateMouseCapture();
    updateWindowTitle();
}

bool SdlRuntime::saveRuntimeSettings() {
    const auto& localization = runtimeLocalization(config_);
    if (config_.settingsFilePath.empty()) {
        settingsView_.setStatus(localization.text(config_.settings.language, "settings.status.no_settings_path"));
        core::Log::warn("Settings apply skipped persistence: empty settings path");
        return false;
    }

    settings::SettingsStore store{config_.settingsFilePath};
    const auto saveResult = store.save(config_.settings);
    if (!saveResult.ok) {
        settingsView_.setStatus(localization.text(config_.settings.language, "settings.status.save_failed_prefix") + saveResult.error);
        core::Log::error(saveResult.error);
        return false;
    }

    settingsView_.setStatus(localization.text(config_.settings.language, "settings.status.saved"));
    core::Log::info("Settings saved: " + config_.settingsFilePath.string());
    return true;
}

void SdlRuntime::applySmokeScenario() {
    switch (config_.smokeScenario) {
    case core::SmokeScenario::None:
        return;
    case core::SmokeScenario::MainMenu:
        screen_ = RuntimeScreen::MainMenu;
        break;
    case core::SmokeScenario::Settings:
        screen_ = RuntimeScreen::MainMenu;
        settingsOpen_ = true;
        break;
    case core::SmokeScenario::SettingsSave:
        screen_ = RuntimeScreen::MainMenu;
        settingsOpen_ = true;
        config_.settings.audio.masterVolume =
            config_.settings.audio.masterVolume == 0.37 ? 0.63 : 0.37;
        applyRuntimeSettings(config_.settings);
        if (!saveRuntimeSettings()) {
            smokeScenarioFailed_ = true;
            break;
        }
        {
            settings::SettingsStore store{config_.settingsFilePath};
            const auto result = store.load();
            if (!result.ok || result.settings.audio.masterVolume != config_.settings.audio.masterVolume) {
                smokeScenarioFailed_ = true;
                core::Log::error("Settings-save smoke verification failed");
            }
        }
        break;
    case core::SmokeScenario::SettingsCycle:
        screen_ = RuntimeScreen::MainMenu;
        settingsOpen_ = true;
        smokeCycleOpen_ = true;
        break;
    case core::SmokeScenario::Help:
        screen_ = RuntimeScreen::MainMenu;
        helpOpen_ = true;
        break;
    case core::SmokeScenario::HelpCycle:
        screen_ = RuntimeScreen::MainMenu;
        helpOpen_ = true;
        smokeCycleOpen_ = true;
        break;
    case core::SmokeScenario::Pause:
    case core::SmokeScenario::PauseSettings:
    case core::SmokeScenario::PauseHelp:
        screen_ = RuntimeScreen::InGame;
        pauseGame();
        settingsOpen_ = config_.smokeScenario == core::SmokeScenario::PauseSettings;
        helpOpen_ = config_.smokeScenario == core::SmokeScenario::PauseHelp;
        break;
    }

    if (config_.smokeScenario != core::SmokeScenario::None) {
        core::Log::info("Smoke scenario ready: " + std::string{core::toString(config_.smokeScenario)});
    }
}

void SdlRuntime::updateSmokeCycle() {
    const bool settingsCycle = config_.smokeScenario == core::SmokeScenario::SettingsCycle;
    const bool helpCycle = config_.smokeScenario == core::SmokeScenario::HelpCycle;
    if (!settingsCycle && !helpCycle) {
        return;
    }

    constexpr std::uint64_t framesPerState = 15;
    const bool shouldOpen = ((frameStats_.frameCount / framesPerState) % 2U) == 0U;
    if (shouldOpen != smokeCycleOpen_) {
        smokeCycleOpen_ = shouldOpen;
        ++smokeCycleTransitions_;
    }
    settingsOpen_ = settingsCycle && shouldOpen;
    helpOpen_ = helpCycle && shouldOpen;
}

bool SdlRuntime::smokeScenarioNeedsGameplay() const noexcept {
    return config_.smokeScenario == core::SmokeScenario::Pause
        || config_.smokeScenario == core::SmokeScenario::PauseSettings
        || config_.smokeScenario == core::SmokeScenario::PauseHelp;
}

void SdlRuntime::renderGameWorldBricks() {
    if (!world_) {
        return;
    }

    unsigned char alpha = 255;
    if (paddleDarknessBlend_ > 0.0f) {
        alpha = static_cast<unsigned char>(255.0f * (1.0f - std::clamp(paddleDarknessBlend_, 0.0f, 1.0f)));
    }

    for (const auto& brick : world_->bricks()) {
        if (!brick.alive) {
            continue;
        }
        const auto sprite = currentLevelAssets_.brickSprites.find(brick.color);
        const auto spriteName = sprite != currentLevelAssets_.brickSprites.end() ? sprite->second : std::string{"blue_brick.png"};
        
        float drawX = brick.position.x;
        float drawY = brick.position.y;
        float drawW = brick.size.w;
        float drawH = brick.size.h;

        if (brick.color == levels::BrickColor::Explosive) {
            float aspect = 1.24f;
            if (gameplayAtlas_ != nullptr) {
                if (const auto frame = gameplayAtlas_->find(spriteName)) {
                    if (frame->h > 0) {
                        aspect = static_cast<float>(frame->w) / static_cast<float>(frame->h);
                    }
                }
            }
            // Standard brick size is typically 80x40.
            // Scale drawH up to 48.0f to make it look prominent, and scale drawW accordingly.
            drawH = 48.0f;
            drawW = drawH * aspect;
            // Center the explosive brick around the standard slot position.
            drawX = brick.position.x + (brick.size.w - drawW) * 0.5f;
            drawY = brick.position.y + (brick.size.h - drawH) * 0.5f;
        }

        renderSpriteOrPlaceholder(spriteName, drawX, drawY, drawW, drawH, alpha);

        if (brick.maxHealth > 1) {
            const float damageProgress = std::clamp(
                static_cast<float>(brick.maxHealth - std::max(0, brick.health)) / static_cast<float>(brick.maxHealth - 1),
                0.0f,
                1.0f);
            if (damageProgress > 0.0f) {
                const float alphaScale = static_cast<float>(alpha) / 255.0f;
                const auto darkAlpha = static_cast<unsigned char>((45.0f + damageProgress * 135.0f) * alphaScale);
                rendererApi_->drawRect(
                    render::Rect{drawX, drawY, drawW, drawH},
                    render::Color{0, 0, 0, darkAlpha});

                const auto sootAlpha = static_cast<unsigned char>((50.0f + damageProgress * 95.0f) * alphaScale);
                rendererApi_->drawLine(
                    drawX + drawW * 0.20f,
                    drawY + drawH * (0.25f + damageProgress * 0.12f),
                    drawX + drawW * 0.55f,
                    drawY + drawH * (0.50f + damageProgress * 0.15f),
                    render::Color{18, 14, 22, sootAlpha});
                rendererApi_->drawLine(
                    drawX + drawW * 0.48f,
                    drawY + drawH * 0.18f,
                    drawX + drawW * 0.78f,
                    drawY + drawH * (0.35f + damageProgress * 0.25f),
                    render::Color{12, 10, 16, sootAlpha});
                if (damageProgress > 0.55f) {
                    rendererApi_->drawLine(
                        drawX + drawW * 0.35f,
                        drawY + drawH * 0.78f,
                        drawX + drawW * 0.70f,
                        drawY + drawH * 0.62f,
                        render::Color{8, 7, 12, sootAlpha});
                }
            }
        }
        if (brick.color == levels::BrickColor::Explosive) {
            const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(frameStats_.frameCount) * 0.18f + brick.position.x * 0.01f);
            rendererApi_->drawRect(
                render::Rect{drawX + 4.0f, drawY + 4.0f, drawW - 8.0f, 4.0f},
                render::Color{255, 82, 26, static_cast<unsigned char>((150 + pulse * 65.0f) * (alpha / 255.0f))});
            rendererApi_->drawRect(
                render::Rect{drawX + 4.0f, drawY + drawH - 8.0f, drawW - 8.0f, 4.0f},
                render::Color{255, 180, 45, static_cast<unsigned char>((130 + pulse * 55.0f) * (alpha / 255.0f))});
            rendererApi_->drawLine(
                drawX + drawW * 0.24f,
                drawY + drawH * 0.72f,
                drawX + drawW * 0.76f,
                drawY + drawH * 0.28f,
                render::Color{255, 54, 18, static_cast<unsigned char>(220 * (alpha / 255.0f))});
            rendererApi_->drawLine(
                drawX + drawW * 0.28f,
                drawY + drawH * 0.26f,
                drawX + drawW * 0.72f,
                drawY + drawH * 0.74f,
                render::Color{255, 226, 92, static_cast<unsigned char>(210 * (alpha / 255.0f))});
        }
    }
}

void SdlRuntime::renderPhysicsDebugOverlay() {
    if (!world_ || !world_->physicsDebugDrawEnabled()) {
        return;
    }

    const auto& paddle = world_->paddle();
    drawDebugRect(paddle.position.x, paddle.position.y, paddle.size.w, paddle.size.h);

    const auto& ball = world_->ball();
    drawDebugRect(ball.position.x - ball.radius, ball.position.y - ball.radius, ball.radius * 2.0f, ball.radius * 2.0f);

    for (const auto& brick : world_->bricks()) {
        if (brick.alive) {
            drawDebugRect(brick.position.x, brick.position.y, brick.size.w, brick.size.h);
        }
    }

    const auto bounds = world_->bounds().bounds;
    drawDebugRect(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top);
}

void SdlRuntime::drawDebugRect(float x, float y, float w, float h) {
    constexpr float thickness = 3.0f;
    const render::Color color{0, 255, 255, 180};
    rendererApi_->drawRect(render::Rect{x, y, w, thickness}, color);
    rendererApi_->drawRect(render::Rect{x, y + h - thickness, w, thickness}, color);
    rendererApi_->drawRect(render::Rect{x, y, thickness, h}, color);
    rendererApi_->drawRect(render::Rect{x + w - thickness, y, thickness, h}, color);
}

void SdlRuntime::renderSpriteOrPlaceholder(std::string_view spriteName, float x, float y, float w, float h, unsigned char alpha) {
    if (gameplayAtlasTexture_ == nullptr) {
        gameplayAtlasTexture_ = assets_->spriteAtlasTexture();
    }
    if (gameplayAtlas_ != nullptr && gameplayAtlasTexture_ != nullptr) {
        if (const auto frame = gameplayAtlas_->find(spriteName)) {
            rendererApi_->drawSprite(*gameplayAtlasTexture_, *frame, render::Rect{x, y, w, h}, alpha);
            return;
        }
    }

    if (spriteName.ends_with(".png")) {
        std::string path = "sprites/" + std::string(spriteName);
        if (auto* tex = assets_->texture(path)) {
            render::SpriteFrame frame{
                .x = 0,
                .y = 0,
                .w = tex->width(),
                .h = tex->height(),
                .rotated = false
            };
            rendererApi_->drawSprite(*tex, frame, render::Rect{x, y, w, h}, alpha);
            return;
        }
    }

    rendererApi_->drawRect(render::Rect{x, y, w, h}, render::Color{255, 0, 180, alpha});
    rendererApi_->drawText(x + 8.0f, y + 8.0f, spriteName, render::Color{255, 255, 255, alpha});
}

void SdlRuntime::renderBoss() {
    if (!world_ || !world_->hasBoss()) {
        return;
    }

    const float gameOverFade = gameOverFadeProgress();
    const float levelCompleteFade = levelCompleteFadeProgress();
    const float fadeProgress = std::max(gameOverFade, levelCompleteFade);
    const auto alpha = static_cast<unsigned char>(std::clamp(1.0f - fadeProgress, 0.0f, 1.0f) * 255.0f);

    float t = static_cast<float>(accumulatorSeconds_ + frameStats_.frameCount * 0.016f);
    float shake = 0.0f;
    if (world_->bossRemainingHealth() <= 5) {
        shake = std::sin(t * 30.0f) * 4.0f;
    }

    const auto& boss = world_->boss();
    
    // 4. Projectiles (drawn below boss body)
    for (const auto& p : boss.projectiles) {
        float pulse = 0.5f + 0.5f * std::sin(p.age * 20.0f);
        
        // Render a glowing trail
        int trailCount = 6;
        for (int i = trailCount; i >= 1; --i) {
            float historyTime = static_cast<float>(i) * 0.02f;
            if (historyTime > p.age) continue;
            
            float trailX = p.position.x;
            float trailY = p.position.y - p.velocity.y * historyTime;
            float trailW = p.size.w * (1.0f - i * 0.12f);
            float trailH = p.size.h * (1.0f - i * 0.12f);
            float offsetX = (p.size.w - trailW) * 0.5f;
            float offsetY = (p.size.h - trailH) * 0.5f;
            
            unsigned char trailAlpha = static_cast<unsigned char>((255 / (i + 1)) * (alpha / 255.0f));
            renderSpriteOrPlaceholder("plasma_shot.png", trailX + offsetX, trailY + offsetY, trailW, trailH, trailAlpha);
        }

        // Draw pulsating energy halo around the projectile
        unsigned char haloAlpha = static_cast<unsigned char>((0.4f + 0.6f * pulse) * 150.0f * (alpha / 255.0f));
        rendererApi_->drawRect(render::Rect{p.position.x - 3.0f, p.position.y - 3.0f, p.size.w + 6.0f, p.size.h + 6.0f}, render::Color{0, 255, 255, haloAlpha});

        // Main projectile sprite
        renderSpriteOrPlaceholder("plasma_shot.png", p.position.x, p.position.y, p.size.w, p.size.h, alpha);
        
        // Inner bright core
        unsigned char coreAlpha = static_cast<unsigned char>(200.0f * (alpha / 255.0f));
        rendererApi_->drawRect(render::Rect{p.position.x + p.size.w * 0.3f, p.position.y + p.size.h * 0.3f, p.size.w * 0.4f, p.size.h * 0.4f}, render::Color{255, 255, 255, coreAlpha});
    }
    
    float x = boss.position.x + shake;
    float y = boss.position.y;

    float flashRatio = 0.0f;
    if (boss.hitFlashRemainingSeconds > 0.0) {
        flashRatio = static_cast<float>(boss.hitFlashRemainingSeconds / 0.15);
        x += (std::rand() % 11 - 5) * flashRatio;
        y += (std::rand() % 11 - 5) * flashRatio;
    }

    if (boss.levelNumber == 30) {
        renderBossHelios(boss, x, y, alpha, flashRatio, t);
    } else if (boss.levelNumber == 40) {
        renderBossSingularity(boss, x, y, alpha, flashRatio, t);
    } else if (boss.levelNumber == 50) {
        renderBossChronarch(boss, x, y, alpha, flashRatio, t);
    } else if (boss.sectionCount > 1) {
        // Boss 2
        for (int i = 0; i < boss.sectionCount; ++i) {
            const auto& sec = boss.sections[i];
            float secX = x + sec.localBounds.left;
            float secY = y + sec.localBounds.top;
            float secW = sec.localBounds.right - sec.localBounds.left;
            float secH = sec.localBounds.bottom - sec.localBounds.top;

            if (sec.alive) {
                std::string spriteName = "boss2_sections" + std::to_string(i + 1) + ".png";
                renderSpriteOrPlaceholder(spriteName.c_str(), secX, secY, secW, secH, alpha);
                if (flashRatio > 0.0f) {
                    unsigned char flashAlpha = static_cast<unsigned char>(flashRatio * 180.0f * (alpha / 255.0f));
                    rendererApi_->drawRect(render::Rect{secX, secY, secW, secH}, render::Color{255, 255, 255, flashAlpha});
                }
                if (boss.sectionHealth[i] <= 10) {
                    float damageAlpha = 0.5f + 0.5f * std::sin(t * 10.0f);
                    unsigned char overlayAlpha = static_cast<unsigned char>(damageAlpha * 80.0f * (alpha / 255.0f));
                    rendererApi_->drawRect(render::Rect{secX, secY, secW, secH}, render::Color{255, 0, 0, overlayAlpha});
                }
            } else {
                std::string spriteName = "boss2_sections" + std::to_string(i + 1) + ".png";
                renderSpriteOrPlaceholder(spriteName.c_str(), secX, secY, secW, secH, static_cast<unsigned char>(alpha * 0.15f));
            }
        }

        if (boss.shieldActive || boss.shieldGlowAlpha > 0.0f) {
            unsigned char shieldAlpha = static_cast<unsigned char>(boss.shieldGlowAlpha * 255.0f * (alpha / 255.0f));
            rendererApi_->drawRect(render::Rect{x - 10.0f, y - 10.0f, boss.size.w + 20.0f, boss.size.h + 20.0f}, render::Color{0, 255, 255, shieldAlpha});
        }
    } else {
        // Boss 1
        float w = boss.size.w;
        float h = boss.size.h;

        renderSpriteOrPlaceholder("firewall7_sprite.png", x, y, w, h, alpha);

        if (flashRatio > 0.0f) {
            unsigned char flashAlpha = static_cast<unsigned char>(flashRatio * 180.0f * (alpha / 255.0f));
            rendererApi_->drawRect(render::Rect{x, y, w, h}, render::Color{255, 255, 255, flashAlpha});
        }

        if (world_->bossRemainingHealth() <= 10) {
            float damageAlpha = 0.5f + 0.5f * std::sin(t * 10.0f);
            unsigned char overlayAlpha = static_cast<unsigned char>(damageAlpha * 80.0f * (alpha / 255.0f));
            rendererApi_->drawRect(render::Rect{x, y, w, h}, render::Color{255, 0, 0, overlayAlpha});
        }
    }
}

void SdlRuntime::renderBossHelios(const gameplay::Boss& boss, float x, float y,
                                  unsigned char alpha, float flashRatio, float t) {
    using render::Rect;
    using render::Color;
    const float w = boss.size.w;
    const float h = boss.size.h;
    const bool inPhase2 = boss.phase == gameplay::Boss::Phase::Two;
    const bool invuln = boss.invulnTimeRemaining > 0.0;
    const float invulnAlpha = invuln ? 0.55f : 1.0f;
    const unsigned char bossAlpha = static_cast<unsigned char>(alpha * invulnAlpha);

    // 1. Telegraph telegraph of the laser (drawn behind body).
    if (boss.laserAlpha > 0.0f &&
        (boss.laserState == gameplay::Boss::LaserState::Charging ||
         boss.laserState == gameplay::Boss::LaserState::Firing ||
         boss.laserState == gameplay::Boss::LaserState::Cooldown)) {
        const float fromY = y + h;
        const float beamH = 1080.0f - fromY;
        const bool isWide = boss.laserWidth >= boss.laserWidthPhaseTwo - 0.1f;
        const char* beamSprite = (boss.laserState == gameplay::Boss::LaserState::Firing)
            ? (isWide ? "boss3_wide_laser_beam.png" : "boss3_laser_beam.png")
            : (isWide ? "boss3_wide_laser_warning.png" : "boss3_laser_warning.png");
        const unsigned char beamAlpha = static_cast<unsigned char>(
            std::clamp(boss.laserAlpha, 0.0f, 1.0f) * bossAlpha);
        renderSpriteOrPlaceholder(beamSprite,
                                  boss.laserTargetX - boss.laserWidth * 0.5f,
                                  fromY,
                                  boss.laserWidth, beamH, beamAlpha);
    }

    // 2. Three orbiting crystals, drawn UNDER the body so the core overlays them.
    // The Helios boss body is portrait (220x300) so we arrange the crystals
    // to skim its lower edge, instead of stretching across a wide base.
    constexpr int crystalCount = 3;
    const float orbitXs[crystalCount] = { -40.0f, 110.0f, 180.0f };
    for (int i = 0; i < crystalCount; ++i) {
        const float baseX = x + orbitXs[i];
        const float baseY = y + w * 0.5f - 30.0f;
        const float sway = std::sin(t * 4.0f + i * 1.7f) * 8.0f;
        const float phaseContrast = inPhase2 ? 1.0f : (0.7f + 0.3f * std::sin(t * 4.0f + i));
        const float crystalAlpha = static_cast<unsigned char>(
            bossAlpha * (0.55f + 0.45f * std::sin(t * 5.0f + i * 0.9f)) * phaseContrast);

        renderSpriteOrPlaceholder("boss3_crystal.png", baseX + sway, baseY, 70.0f, 70.0f, crystalAlpha);
    }

    // 3. Core (slightly faded during teleport invuln).
    if (world_->bossRemainingHealth() <= 0 && boss.defeated) {
        // Defeated frame: show the destroyed sprite for 1.2 seconds.
        renderSpriteOrPlaceholder("boss3_destroyed.png", x, y, w, h, bossAlpha);
    } else {
        renderSpriteOrPlaceholder("boss3_core.png", x, y, w, h, bossAlpha);
    }

    // 4. Hit flash overlay (whitening).
    if (flashRatio > 0.0f) {
        unsigned char flashAlpha = static_cast<unsigned char>(flashRatio * 180.0f * (alpha / 255.0f));
        rendererApi_->drawRect(Rect{x, y, w, h}, Color{255, 255, 255, flashAlpha});
    }

    // 5. Crystal glow on hit (samples the alpha bouncing toward 0 post hit).
    if (boss.crystalFlashAlpha > 0.0f) {
        const unsigned char cryAlpha = static_cast<unsigned char>(
            boss.crystalFlashAlpha * 220.0f * (alpha / 255.0f));
        rendererApi_->drawRect(Rect{x - 6.0f, y - 6.0f, w + 12.0f, h + 12.0f},
                               Color{180, 220, 255, cryAlpha});
    }

    // 6. Phase 2 aura - faint red glow rectangle around the body.
    if (inPhase2) {
        const float pulse = 0.5f + 0.5f * std::sin(t * 6.0f);
        const unsigned char auraAlpha = static_cast<unsigned char>(
            pulse * 120.0f * (alpha / 255.0f));
        rendererApi_->drawRect(Rect{x - 14.0f, y - 14.0f, w + 28.0f, h + 28.0f},
                               Color{255, 60, 60, auraAlpha});
    }

    // 7. Teleport flash - quick expanding ring centered on boss.
    if (invuln) {
        const float progress = static_cast<float>(
            (boss.invulnOnTeleportSeconds - boss.invulnTimeRemaining) /
            std::max(0.001, boss.invulnOnTeleportSeconds));
        const float flashSize = 200.0f * (0.6f + 0.7f * progress);
        const float cx = x + w * 0.5f - flashSize * 0.5f;
        const float cy = y + h * 0.5f - flashSize * 0.5f;
        const unsigned char flashAlpha = static_cast<unsigned char>(
            (1.0f - progress) * 200.0f * (alpha / 255.0f));
        renderSpriteOrPlaceholder("boss3_teleport_flash.png",
                                  cx, cy, flashSize, flashSize, flashAlpha);
    }

    // 8. HP bar with phase markers (70%, 40%, 10%) — drawn last so it's on top.
    const float hpY = y - 18.0f;
    rendererApi_->drawRect(Rect{x, hpY, w, 6.0f}, Color{40, 40, 50, 200});
    const float fillW = w * std::clamp(static_cast<float>(boss.currentHealth) /
                                       static_cast<float>(boss.maxHealth), 0.0f, 1.0f);
    Color fillColor = (boss.phase == gameplay::Boss::Phase::Two)
        ? Color{255, 70, 70, 230}
        : Color{80, 130, 220, 230};
    rendererApi_->drawRect(Rect{x, hpY, fillW, 6.0f}, fillColor);

    // Tick marks for thresholds
    auto tickAt = [&](float frac) {
        const float tx = x + w * frac;
        rendererApi_->drawRect(Rect{tx - 1.0f, hpY - 2.0f, 2.0f, 10.0f},
                               Color{240, 240, 240, 220});
    };
    tickAt(0.70f);
    tickAt(0.40f);
    tickAt(0.10f);

    // 9. Drones (only in phase 2).
    for (const auto& d : world_->bossDrones()) {
        if (!d.alive) continue;
        const float dx = d.position.x;
        const float dy = d.position.y;
        const float ds = d.size.w;
        renderSpriteOrPlaceholder("boss3_drone.png", dx, dy, ds, ds, alpha);
        if (d.hitFlashRemainingSeconds > 0.0) {
            unsigned char dxFlash = static_cast<unsigned char>(
                (d.hitFlashRemainingSeconds / 0.15f) * 200.0f * (alpha / 255.0f));
            rendererApi_->drawRect(Rect{dx, dy, ds, ds}, Color{255, 255, 255, dxFlash});
        }
    }
}

void SdlRuntime::renderBossSingularity(const gameplay::Boss& boss, float x, float y,
                                       unsigned char alpha, float flashRatio, float t) {
    using render::Rect;
    using render::Color;
    using Phase = gameplay::Boss::Phase;
    const float w = boss.size.w;
    const float h = boss.size.h;
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const bool inPhase4 = boss.phase == Phase::Four;
    const bool inPhase3or4 = boss.phase == Phase::Three || inPhase4;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});

    ImGui::Begin("BossSingularityOverlay", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground);

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Phase-based palette: core (centre), hot-spot (rim accents), halo (outer aura).
    auto palette = [](Phase phase) {
        switch (phase) {
            case Phase::One:   return std::tuple<Color, Color, Color>{
                Color{200, 180, 255, 230},
                Color{180, 220, 255, 220},
                Color{ 90,  80, 200, 160}};
            case Phase::Two:   return std::tuple<Color, Color, Color>{
                Color{180, 130, 220, 230},
                Color{140, 180, 255, 220},
                Color{110,  60, 180, 160}};
            case Phase::Three: return std::tuple<Color, Color, Color>{
                Color{255, 150, 100, 230},
                Color{255, 100,  80, 220},
                Color{200,  80,  40, 160}};
            case Phase::Four:  return std::tuple<Color, Color, Color>{
                Color{255,  80,  60, 240},
                Color{255,  30,  30, 230},
                Color{255, 140,  60, 170}};
        }
        return std::tuple<Color, Color, Color>{
            Color{200, 180, 255, 230},
            Color{180, 220, 255, 220},
            Color{ 90,  80, 200, 160}};
    };
    auto [coreColor, hotColor, haloColor] = palette(boss.phase);

    float overrideAlpha = alpha / 255.0f;
    const float pulse = 0.5f + 0.5f * std::sin(t * 6.0);
    const float phaseFactor = inPhase4 ? 1.6f : 1.0f;

    // 1. Accretion ring (outer dark disk with halo).
    const float ringOuter = std::max(w, h) * 0.55f;
    const float ringInner = ringOuter - 28.0f;
    // Draw outer ring as 4 concentric ellipses around the centre to fake a
    // filled annulus with alpha that pulses with the phase factor.
    const std::array<float, 4> bands = {1.00f, 0.85f, 0.70f, 0.55f};
    for (std::size_t k = 0; k < bands.size(); ++k) {
        const float r = ringOuter * bands[k];
        const float inner = ringInner * bands[k];
        const unsigned char bandAlpha = static_cast<unsigned char>(
            (haloColor.a * 0.6f) * (0.7f + 0.3f * (k + 1) / bands.size()) *
            overrideAlpha * phaseFactor);
        rendererApi_->drawRect(
            Rect{cx - r, cy - r, r * 2.0f, r * 1.6f},
            Color{haloColor.r, haloColor.g, haloColor.b, bandAlpha});
        // Hole in the middle of the ring fills with full background colour.
        if (inner > 6.0f) {
            const unsigned char coreFiller = static_cast<unsigned char>(
                240.0f * overrideAlpha);
            rendererApi_->drawRect(
                Rect{cx - inner, cy - inner, inner * 2.0f, inner * 1.6f},
                Color{20, 8, 30, coreFiller});
        }
    }

    // 2. Hot-spots: 4 rotating accents on the rim.
    const float ringR = ringInner + 6.0f;
    const float hotspotRot = t * 0.55f;  // turn rate per second
    for (int i = 0; i < 4; ++i) {
        const float angle = hotspotRot + i * (3.14159265f * 0.5f);
        const float sx = cx + std::cos(angle) * ringR;
        const float sy = cy + std::sin(angle) * ringR * 0.6f;
        const float radius = 14.0f + pulse * 4.0f;
        const unsigned char hsAlpha = static_cast<unsigned char>(
            (hotColor.a + pulse * 40.0f) * overrideAlpha);
        const float haloR = 26.0f;
        rendererApi_->drawRect(
            Rect{sx - haloR, sy - haloR, haloR * 2, haloR * 2},
            Color{hotColor.r, hotColor.g, hotColor.b, static_cast<unsigned char>(hsAlpha * 0.6f)});
        renderSpriteOrPlaceholder("boss4_hotspot.png", sx - radius, sy - radius,
                                 radius * 2.0f, radius * 2.0f, hsAlpha);
    }

    // 3. Core.
    const float corePulse = 0.92f + 0.08f * std::sin(t * 8.0f);
    const float coreR = 22.0f * phaseFactor;
    renderSpriteOrPlaceholder("boss4_core.png",
                              cx - coreR, cy - coreR,
                              coreR * 2.0f, coreR * 2.0f,
                              static_cast<unsigned char>(coreColor.a * corePulse * overrideAlpha));
    // bright inner spark
    rendererApi_->drawRect(
        Rect{cx - 6.0f, cy - 6.0f, 12.0f, 12.0f},
        Color{255, 255, 255, static_cast<unsigned char>(220.0f * overrideAlpha)});
    if (flashRatio > 0.0f) {
        const float flash = std::clamp(flashRatio, 0.0f, 1.0f);
        const float flashR = ringOuter * (0.55f + 0.45f * flash);
        rendererApi_->drawRect(
            Rect{cx - flashR, cy - flashR, flashR * 2.0f, flashR * 1.6f},
            Color{255, 245, 210, static_cast<unsigned char>(120.0f * flash * overrideAlpha)});
    }

    // 4. HP bar with 4 marker ticks at 66%, 33%, 10%.
    const float hpY = y - 22.0f;
    rendererApi_->drawRect(Rect{x, hpY, w, 8.0f}, Color{30, 25, 45, 200});
    const float fillW = w * std::clamp(static_cast<float>(boss.currentHealth) /
                                       static_cast<float>(boss.maxHealth), 0.0f, 1.0f);
    Color fillColor = inPhase4
        ? Color{255, 60, 60, 240}
        : (boss.phase == Phase::Three
            ? Color{255, 130, 90, 230}
            : Color{160, 130, 220, 230});
    rendererApi_->drawRect(Rect{x, hpY, fillW, 8.0f}, fillColor);
    auto tick = [&](float frac) {
        const float tx = x + w * frac;
        rendererApi_->drawRect(Rect{tx - 1.0f, hpY - 2.0f, 2.0f, 12.0f},
                               Color{250, 250, 250, 220});
    };
    tick(0.66f);
    tick(0.33f);
    tick(0.10f);

    // 5. Gravity field halo (phase 3+).
    if (inPhase3or4) {
        const float glowR = boss.gravityFieldRadius *
            (0.85f + 0.15f * std::sin(t * 4.0f));
        // Wide alpha-fading ambient ring (drawn as a thick outline).
        rendererApi_->drawRect(
            Rect{cx - glowR, cy - glowR, glowR * 2.0f, glowR * 1.6f},
            Color{180, 100, 255, static_cast<unsigned char>(50.0f * overrideAlpha)});
        // Secondary tighter ring (the actual fall-off radius).
        rendererApi_->drawRect(
            Rect{cx - glowR * 0.6f, cy - glowR * 0.6f,
                 glowR * 1.2f, glowR * 0.9f},
            Color{220, 160, 255, static_cast<unsigned char>(90.0f * overrideAlpha)});

        // Phase 3+ overlay text: "GRAVITY FIELD ACTIVE".
        std::string txt = runtimeLocalization(config_).text(
            config_.settings.language, "gameplay.boss_gravity_active");
        if (txt == "gameplay.boss_gravity_active") {
            txt = config_.settings.language == settings::Language::Russian
                ? "[ ГРАВИТАЦИОННОЕ ПОЛЕ ]"
                : "[ GRAVITY FIELD ACTIVE ]";
        }
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::SetWindowFontScale(1.0f);
        ImVec2 sz = ImGui::CalcTextSize(txt.c_str());
        ImGui::SetCursorPos(ImVec2{
            x + w * 0.5f - sz.x * 0.5f,
            hpY + 16.0f});
        const float glowP = 0.7f + 0.3f * std::sin(static_cast<float>(t) * 4.0);
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4{0.85f, 0.65f, 1.0f, glowP * overrideAlpha});
        ImGui::TextUnformatted(txt.c_str());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();
    }

    // 6. Singularity pulse (expanding ring, only when active).
    const auto& pulseObj = world_->bossSingularityPulse();
    if (pulseObj.active) {
        const float r = pulseObj.currentRadius;
        const float alphaFalloff = 0.55f * (1.0f - (r / boss.singularityPulseRadius));
        const unsigned char pulseAlpha = static_cast<unsigned char>(
            200.0f * alphaFalloff * overrideAlpha);
        rendererApi_->drawRect(
            Rect{cx - r, cy - r, r * 2.0f, r * 1.6f},
            Color{200, 100, 255, pulseAlpha});
    }

    // 7. Gravity mines (phase 4 only).
    if (boss.phase == Phase::Four) {
        for (const auto& m : world_->bossGravityMines()) {
            const float lifeFrac = std::clamp(
                1.0f - (m.age / boss.gravityMineLifetimeSeconds), 0.0f, 1.0f);
            const float ringR = boss.gravityMineVisualRadius +
                10.0f * std::sin(t * 8.0f + m.age * 5.0f);
            const unsigned char mineAlpha = static_cast<unsigned char>(
                220.0f * lifeFrac * overrideAlpha);
            // Dark core.
            rendererApi_->drawRect(
                Rect{m.x - ringR * 0.6f, m.y - ringR * 0.6f,
                     ringR * 1.2f, ringR * 1.2f},
                Color{20, 0, 0, mineAlpha});
            // Red pulsing ring.
            rendererApi_->drawRect(
                Rect{m.x - ringR, m.y - ringR, ringR * 2.0f, ringR * 2.0f},
                Color{255, 60, 60, static_cast<unsigned char>(mineAlpha * 0.6f)});
            renderSpriteOrPlaceholder("boss4_mine.png",
                                     m.x - ringR * 0.7f,
                                     m.y - ringR * 0.7f,
                                     ringR * 1.4f, ringR * 1.4f,
                                     static_cast<unsigned char>(mineAlpha * 0.9f));
        }
    }

    // 8. Defeated frame.
    if (boss.defeated) {
        renderSpriteOrPlaceholder("boss4_destroyed.png",
                                  x, y, w, h,
                                  static_cast<unsigned char>(alpha * 0.4f));
    }

    // 9. Drones (use the same boss3_drone sprite).
    for (const auto& d : world_->bossDrones()) {
        if (!d.alive) continue;
        renderSpriteOrPlaceholder("boss3_drone.png",
                                  d.position.x, d.position.y,
                                  d.size.w, d.size.h, alpha);
        if (d.hitFlashRemainingSeconds > 0.0) {
            unsigned char fa = static_cast<unsigned char>(
                (d.hitFlashRemainingSeconds / 0.15f) * 200.0f * (alpha / 255.0f));
            rendererApi_->drawRect(
                Rect{d.position.x, d.position.y, d.size.w, d.size.h},
                Color{255, 255, 255, fa});
        }
    }

    // 10. Localized phase label under the boss.
    {
        std::string phaseKey;
        switch (boss.phase) {
            case Phase::One:   phaseKey = "gameplay.boss_singularity_phase_collapse"; break;
            case Phase::Two:   phaseKey = "gameplay.boss_singularity_phase_accretion"; break;
            case Phase::Three: phaseKey = "gameplay.boss_singularity_phase_singularity"; break;
            case Phase::Four:  phaseKey = "gameplay.boss_singularity_phase_collapse_in"; break;
        }
        std::string phaseTxt = runtimeLocalization(config_).text(config_.settings.language, phaseKey);
        if (phaseTxt == phaseKey) {
            switch (boss.phase) {
                case Phase::One:   phaseTxt = (config_.settings.language == settings::Language::Russian) ? "Фаза 1: Коллапс" : "Phase 1: Collapse"; break;
                case Phase::Two:   phaseTxt = (config_.settings.language == settings::Language::Russian) ? "Фаза 2: Аккреция" : "Phase 2: Accretion"; break;
                case Phase::Three: phaseTxt = (config_.settings.language == settings::Language::Russian) ? "Фаза 3: Сингулярность" : "Phase 3: Singularity"; break;
                case Phase::Four:  phaseTxt = (config_.settings.language == settings::Language::Russian) ? "Фаза 4: Схлопывание" : "Phase 4: Collapse"; break;
            }
        }

        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::SetWindowFontScale(1.0f);
        ImVec2 sz = ImGui::CalcTextSize(phaseTxt.c_str());
        ImGui::SetCursorPos(ImVec2{
            x + w * 0.5f - sz.x * 0.5f,
            y + h + 15.0f
        });
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImVec4{static_cast<float>(coreColor.r) / 255.0f,
                   static_cast<float>(coreColor.g) / 255.0f,
                   static_cast<float>(coreColor.b) / 255.0f,
                   overrideAlpha});
        ImGui::TextUnformatted(phaseTxt.c_str());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();
    }

    ImGui::End();
}

void SdlRuntime::renderBossChronarch(const gameplay::Boss& boss, float x, float y,
                                     unsigned char alpha, float flashRatio, float t) {
    using render::Rect;
    using render::Color;
    using Phase = gameplay::Boss::Phase;
    const float w = boss.size.w;
    const float h = boss.size.h;
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;
    const ImVec2 viewportPos = viewport->Pos;
    const ImVec2 viewportSize = viewport->Size;

    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});

    ImGui::Begin("BossChronarchOverlay", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground);

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 0. Zero Hour Full-Screen Overlay
    float overrideAlpha = alpha / 255.0f;
    if (boss.zeroHourRemainingSeconds > 0.0) {
        drawList->AddRectFilled(
            viewportPos,
            ImVec2{viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y},
            IM_COL32(0, 80, 160, static_cast<int>(35.0f * overrideAlpha))
        );
        for (float sy = 0; sy < viewportSize.y; sy += 6.0f) {
            drawList->AddLine(
                ImVec2{viewportPos.x, viewportPos.y + sy},
                ImVec2{viewportPos.x + viewportSize.x, viewportPos.y + sy},
                IM_COL32(0, 255, 255, static_cast<int>(12.0f * overrideAlpha)),
                1.0f
            );
        }
        float screenCx = viewportPos.x + viewportSize.x * 0.5f;
        float screenCy = viewportPos.y + viewportSize.y * 0.5f;
        float clockR = 250.0f + std::sin(t * 12.0f) * 15.0f;
        drawList->AddCircle(ImVec2{screenCx, screenCy}, clockR, IM_COL32(0, 255, 255, static_cast<int>(25.0f * overrideAlpha)), 4.0f);
        for (int k = 0; k < 12; ++k) {
            float theta = k * (6.2831853f / 12.0f) + t * 0.2f;
            drawList->AddLine(
                ImVec2{screenCx + std::cos(theta) * (clockR - 25.0f), screenCy + std::sin(theta) * (clockR - 25.0f)},
                ImVec2{screenCx + std::cos(theta) * (clockR - 5.0f), screenCy + std::sin(theta) * (clockR - 5.0f)},
                IM_COL32(0, 255, 255, static_cast<int>(20.0f * overrideAlpha)),
                2.0f
            );
        }
    }

    // Colors mapping based on Phase
    ImU32 coreColor, bodyColor, accentColor;
    switch (boss.phase) {
        case Phase::One:
            coreColor = IM_COL32(255, 226, 154, alpha);
            bodyColor = IM_COL32(139, 90, 43, alpha);
            accentColor = IM_COL32(0, 191, 255, alpha);
            break;
        case Phase::Two:
            coreColor = IM_COL32(255, 180, 94, alpha);
            bodyColor = IM_COL32(184, 134, 11, alpha);
            accentColor = IM_COL32(255, 127, 0, alpha);
            break;
        case Phase::Three:
            coreColor = IM_COL32(215, 124, 255, alpha);
            bodyColor = IM_COL32(75, 0, 130, alpha);
            accentColor = IM_COL32(186, 85, 211, alpha);
            break;
        case Phase::Four:
        default:
            coreColor = IM_COL32(239, 255, 255, alpha);
            bodyColor = IM_COL32(47, 79, 79, alpha);
            accentColor = IM_COL32(0, 255, 255, alpha);
            break;
    }

    float scx = cx + viewportPos.x;
    float scy = cy + viewportPos.y;

    // 1. Time Rifts (drawn behind boss body)
    for (const auto& rift : boss.timeRifts) {
        float rcx = rift.center.x + viewportPos.x;
        float rcy = rift.center.y + viewportPos.y;
        float r = rift.radius;
        if (rift.kind == gameplay::TimeRiftKind::Slow) {
            drawList->AddCircle(ImVec2{rcx, rcy}, r, IM_COL32(0, 191, 255, static_cast<int>(180 * overrideAlpha)), 3.0f);
            drawList->AddCircleFilled(ImVec2{rcx, rcy}, r - 2.0f, IM_COL32(0, 191, 255, static_cast<int>(25 * overrideAlpha)));
            for (int k = 0; k < 12; ++k) {
                float theta = k * (6.2831853f / 12.0f);
                drawList->AddLine(
                    ImVec2{rcx + std::cos(theta) * (r - 10.0f), rcy + std::sin(theta) * (r - 10.0f)},
                    ImVec2{rcx + std::cos(theta) * (r - 3.0f), rcy + std::sin(theta) * (r - 3.0f)},
                    IM_COL32(0, 191, 255, static_cast<int>(120 * overrideAlpha)),
                    1.5f
                );
            }
        } else if (rift.kind == gameplay::TimeRiftKind::Haste) {
            drawList->AddCircle(ImVec2{rcx, rcy}, r, IM_COL32(255, 69, 0, static_cast<int>(180 * overrideAlpha)), 3.0f);
            drawList->AddCircleFilled(ImVec2{rcx, rcy}, r - 2.0f, IM_COL32(255, 69, 0, static_cast<int>(25 * overrideAlpha)));
            drawList->PathClear();
            for (int k = 0; k < 36; ++k) {
                float theta = k * 0.25f + t * 5.0f;
                float currR = (r - 10.0f) * (k / 36.0f);
                drawList->PathLineTo(ImVec2{rcx + std::cos(theta) * currR, rcy + std::sin(theta) * currR});
            }
            drawList->PathStroke(IM_COL32(255, 140, 0, static_cast<int>(120 * overrideAlpha)), false, 2.0f);
        } else if (rift.kind == gameplay::TimeRiftKind::Rewind) {
            drawList->AddCircle(ImVec2{rcx, rcy}, r, IM_COL32(186, 85, 211, static_cast<int>(200 * overrideAlpha)), 3.5f);
            drawList->AddCircleFilled(ImVec2{rcx, rcy}, r - 2.0f, IM_COL32(186, 85, 211, static_cast<int>(30 * overrideAlpha)));
            float handAngle = -t * 4.0f;
            drawList->AddLine(
                ImVec2{rcx, rcy},
                ImVec2{rcx + std::cos(handAngle) * (r - 15.0f), rcy + std::sin(handAngle) * (r - 15.0f)},
                IM_COL32(218, 112, 214, alpha),
                2.5f
            );
        }
    }

    // 2. Main Boss Body (Central bronze/gold disk 320x220)
    if (boss.defeated) {
        renderSpriteOrPlaceholder("boss5_destroyed.png", x, y, w, h, alpha);
    } else {
        float rx = w * 0.5f;
        float ry = h * 0.5f;
        const int numSegments = 64;
        drawList->PathClear();
        for (int i = 0; i < numSegments; ++i) {
            float angle = i * (6.2831853f / numSegments);
            drawList->PathLineTo(ImVec2{scx + std::cos(angle) * rx, scy + std::sin(angle) * ry});
        }
        drawList->PathFillConvex(bodyColor);

        drawList->PathClear();
        for (int i = 0; i < numSegments; ++i) {
            float angle = i * (6.2831853f / numSegments);
            drawList->PathLineTo(ImVec2{scx + std::cos(angle) * rx, scy + std::sin(angle) * ry});
        }
        drawList->PathStroke(accentColor, true, 3.0f);

        for (int hIndex = 0; hIndex < 12; ++hIndex) {
            float angle = hIndex * (6.2831853f / 12.0f);
            drawList->AddLine(
                ImVec2{scx + std::cos(angle) * (rx - 15.0f), scy + std::sin(angle) * (ry - 10.0f)},
                ImVec2{scx + std::cos(angle) * (rx - 4.0f), scy + std::sin(angle) * (ry - 3.0f)},
                accentColor,
                2.0f
            );
        }

        float pulse = 0.5f + 0.5f * std::sin(t * 5.0f);
        float coreR = 30.0f;
        drawList->AddCircleFilled(ImVec2{scx, scy}, coreR, coreColor);
        drawList->AddCircle(ImVec2{scx, scy}, coreR + 4.0f + pulse * 6.0f, accentColor, 2.0f);

        if (flashRatio > 0.0f) {
            ImU32 bodyFlashCol = IM_COL32(255, 255, 255, static_cast<int>(flashRatio * 180.0f * overrideAlpha));
            drawList->PathClear();
            for (int i = 0; i < numSegments; ++i) {
                float angle = i * (6.2831853f / numSegments);
                drawList->PathLineTo(ImVec2{scx + std::cos(angle) * rx, scy + std::sin(angle) * ry});
            }
            drawList->PathFillConvex(bodyFlashCol);
        }
    }

    // 3. Paradox Shards (orbiting)
    for (size_t i = 0; i < boss.paradoxShards.size(); ++i) {
        const auto& shard = boss.paradoxShards[i];
        gameplay::Vec2 shardPos = { boss.position.x + w * 0.5f + shard.orbitOffset.x, boss.position.y + h * 0.5f + shard.orbitOffset.y };
        float shcx = shardPos.x + viewportPos.x;
        float shcy = shardPos.y + viewportPos.y;

        ImVec2 pts[4] = {
            ImVec2{shcx, shcy - shard.size.h * 0.5f},
            ImVec2{shcx + shard.size.w * 0.5f, shcy},
            ImVec2{shcx, shcy + shard.size.h * 0.5f},
            ImVec2{shcx - shard.size.w * 0.5f, shcy}
        };

        if (shard.alive) {
            ImU32 shardCol = accentColor;
            drawList->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], shardCol);
            drawList->AddQuad(pts[0], pts[1], pts[2], pts[3], IM_COL32(255, 255, 255, alpha), 2.0f);

            if (shard.hitFlashRemainingSeconds > 0.0) {
                float fRatio = static_cast<float>(shard.hitFlashRemainingSeconds / 0.15);
                ImU32 shFlash = IM_COL32(255, 255, 255, static_cast<int>(fRatio * 200.0f * overrideAlpha));
                drawList->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], shFlash);
            }
        } else {
            drawList->AddQuad(pts[0], pts[1], pts[2], pts[3], IM_COL32(150, 150, 150, static_cast<int>(60.0f * overrideAlpha)), 1.5f);
        }
    }

    // 4. Clock Hands (Laser Beams)
    for (const auto& hand : boss.clockHands) {
        float angle = hand.angleRadians;
        ImVec2 handStart = ImVec2{scx, scy};
        ImVec2 handEnd = ImVec2{scx + std::cos(angle) * hand.length, scy + std::sin(angle) * hand.length};

        if (hand.telegraphing) {
            drawList->AddLine(handStart, handEnd, IM_COL32(255, 165, 0, static_cast<int>(100 * overrideAlpha)), 2.0f);
            float dotPos = std::fmod(t * 300.0f, hand.length);
            ImVec2 dotVec = ImVec2{scx + std::cos(angle) * dotPos, scy + std::sin(angle) * dotPos};
            drawList->AddCircleFilled(dotVec, 4.0f, IM_COL32(255, 220, 100, static_cast<int>(200 * overrideAlpha)));
        } else if (hand.active) {
            drawList->AddLine(handStart, handEnd, IM_COL32(255, 69, 0, static_cast<int>(120 * overrideAlpha)), hand.width + 8.0f);
            drawList->AddLine(handStart, handEnd, IM_COL32(255, 140, 0, static_cast<int>(180 * overrideAlpha)), hand.width + 3.0f);
            drawList->AddLine(handStart, handEnd, IM_COL32(255, 250, 205, alpha), hand.width * 0.4f);
        }
    }

    // 5. HP Bar with 3 marker ticks (75%, 50%, 25%)
    const float hpY = y - 22.0f;
    rendererApi_->drawRect(Rect{x, hpY, w, 8.0f}, Color{30, 25, 45, 200});
    const float fillW = w * std::clamp(static_cast<float>(boss.currentHealth) /
                                       static_cast<float>(boss.maxHealth), 0.0f, 1.0f);
    Color fillColor = (boss.phase == Phase::Four)
        ? Color{239, 255, 255, 230}
        : (boss.phase == Phase::Three
            ? Color{215, 124, 255, 230}
            : (boss.phase == Phase::Two
                ? Color{255, 180, 94, 230}
                : Color{255, 226, 154, 230}));
    rendererApi_->drawRect(Rect{x, hpY, fillW, 8.0f}, fillColor);

    auto tick = [&](float frac) {
        const float tx = x + w * frac;
        rendererApi_->drawRect(Rect{tx - 1.0f, hpY - 2.0f, 2.0f, 12.0f}, Color{250, 250, 250, 220});
    };
    tick(0.75f);
    tick(0.50f);
    tick(0.25f);

    // 6. Phase Name & Zero Hour HUD
    {
        std::string phaseKey;
        switch (boss.phase) {
            case Phase::One:   phaseKey = "gameplay.boss_chronarch_phase_1"; break;
            case Phase::Two:   phaseKey = "gameplay.boss_chronarch_phase_2"; break;
            case Phase::Three: phaseKey = "gameplay.boss_chronarch_phase_3"; break;
            case Phase::Four:  phaseKey = "gameplay.boss_chronarch_phase_4"; break;
        }
        std::string phaseTxt = runtimeLocalization(config_).text(config_.settings.language, phaseKey);
        if (phaseTxt == phaseKey) {
            switch (boss.phase) {
                case Phase::One:   phaseTxt = (config_.settings.language == settings::Language::Russian) ? "Фаза 1: Завод часов" : "Phase 1: Clockwork"; break;
                case Phase::Two:   phaseTxt = (config_.settings.language == settings::Language::Russian) ? "Фаза 2: Ускоренный отсчет" : "Phase 2: Accelerated Countdown"; break;
                case Phase::Three: phaseTxt = (config_.settings.language == settings::Language::Russian) ? "Фаза 3: Парадокс" : "Phase 3: Paradox"; break;
                case Phase::Four:  phaseTxt = (config_.settings.language == settings::Language::Russian) ? "Фаза 4: Нулевой час" : "Phase 4: Zero Hour"; break;
            }
        }

        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::SetWindowFontScale(1.0f);
        ImVec2 sz = ImGui::CalcTextSize(phaseTxt.c_str());
        ImGui::SetCursorPos(ImVec2{
            x + w * 0.5f - sz.x * 0.5f,
            y + h + 15.0f
        });
        ImVec4 textColVec;
        if (boss.phase == Phase::Four)      textColVec = ImVec4{0.93f, 1.0f, 1.0f, overrideAlpha};
        else if (boss.phase == Phase::Three) textColVec = ImVec4{0.84f, 0.49f, 1.0f, overrideAlpha};
        else if (boss.phase == Phase::Two)   textColVec = ImVec4{1.0f, 0.70f, 0.37f, overrideAlpha};
        else                                textColVec = ImVec4{1.0f, 0.88f, 0.60f, overrideAlpha};

        ImGui::PushStyleColor(ImGuiCol_Text, textColVec);
        ImGui::TextUnformatted(phaseTxt.c_str());
        ImGui::PopStyleColor();

        if (boss.phase == Phase::Four && boss.zeroHourRemainingSeconds > 0.0) {
            std::string zhKey = "gameplay.boss_chronarch_zero_hour";
            std::string zhTxt = runtimeLocalization(config_).text(config_.settings.language, zhKey);
            if (zhTxt == zhKey) {
                zhTxt = (config_.settings.language == settings::Language::Russian) ? "НУЛЕВОЙ ЧАС" : "ZERO HOUR";
            }
            ImVec2 zhSz = ImGui::CalcTextSize(zhTxt.c_str());
            ImGui::SetCursorPos(ImVec2{
                x + w * 0.5f - zhSz.x * 0.5f,
                hpY - 32.0f
            });
            float zhPulse = 0.5f + 0.5f * std::sin(t * 15.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 0.2f, 0.2f, (0.5f + 0.5f * zhPulse) * overrideAlpha});
            ImGui::TextUnformatted(zhTxt.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();
    }

    ImGui::End();
}

void SdlRuntime::renderLaunchTrajectory(const gameplay::Ball& ball, float initialVx, unsigned char gameplayAlpha) {
    gameplay::Vec2 velocity{initialVx, -620.0f};
    float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    gameplay::Vec2 dir{velocity.x / speed, velocity.y / speed};

    gameplay::Vec2 currentPos = ball.position;
    const float radius = ball.radius;
    const auto& bounds = world_->bounds().bounds;

    float patternOffset = 0.0f;
    float totalDistanceDrawn = 0.0f;

    int segments = 3;
    for (int seg = 0; seg < segments; ++seg) {
        Ray ray{currentPos, dir};
        float closestT = 1e9f;
        gameplay::Vec2 closestNormal{};
        bool hitSomething = false;

        // 1. Intersect with left wall limit
        if (dir.x < 0.0f) {
            float t = safe_div((bounds.left + radius) - currentPos.x, dir.x);
            if (t > 0.05f && t < closestT) {
                closestT = t;
                closestNormal = gameplay::Vec2{1.0f, 0.0f};
                hitSomething = true;
            }
        }
        // 2. Intersect with right wall limit
        if (dir.x > 0.0f) {
            float t = safe_div((bounds.right - radius) - currentPos.x, dir.x);
            if (t > 0.05f && t < closestT) {
                closestT = t;
                closestNormal = gameplay::Vec2{-1.0f, 0.0f};
                hitSomething = true;
            }
        }
        // 3. Intersect with top wall limit
        if (dir.y < 0.0f) {
            float t = safe_div((bounds.top + radius) - currentPos.y, dir.y);
            if (t > 0.05f && t < closestT) {
                closestT = t;
                closestNormal = gameplay::Vec2{0.0f, 1.0f};
                hitSomething = true;
            }
        }

        // 4. Intersect with bricks
        for (const auto& brick : world_->bricks()) {
            if (!brick.alive) continue;
            gameplay::Vec2 normal{};
            float t = intersectAABB(ray,
                                   brick.position.x - radius,
                                   brick.position.x + brick.size.w + radius,
                                   brick.position.y - radius,
                                   brick.position.y + brick.size.h + radius,
                                   normal);
            if (t > 0.05f && t < closestT) {
                closestT = t;
                closestNormal = normal;
                hitSomething = true;
            }
        }

        if (!hitSomething || closestT > 3000.0f) {
            gameplay::Vec2 endPos{currentPos.x + dir.x * 1000.0f, currentPos.y + dir.y * 1000.0f};
            drawBeautifulDottedLine(currentPos, endPos, gameplayAlpha, patternOffset, totalDistanceDrawn);
            break;
        }

        gameplay::Vec2 hitPos{currentPos.x + dir.x * closestT, currentPos.y + dir.y * closestT};
        drawBeautifulDottedLine(currentPos, hitPos, gameplayAlpha, patternOffset, totalDistanceDrawn);

        if (std::abs(closestNormal.y) > 0.5f) {
            drawLandingGlow(hitPos, radius, gameplayAlpha);
            break;
        }

        dir.x = -dir.x;
        currentPos = hitPos;
    }
}

void SdlRuntime::drawBeautifulDottedLine(gameplay::Vec2 start, gameplay::Vec2 end, unsigned char baseAlpha, float& patternOffset, float& totalDistanceDrawn) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float length = std::sqrt(dx * dx + dy * dy);

    if (length < 1.0f) return;

    float ux = dx / length;
    float uy = dy / length;

    float step = 24.0f;
    float d = patternOffset;

    while (d < length) {
        float x = start.x + ux * d;
        float y = start.y + uy * d;
        float dist = totalDistanceDrawn + d;

        float maxDist = 800.0f;
        if (dist > maxDist) {
            break;
        }

        float fade = 1.0f - (dist / maxDist);
        fade = std::max(0.0f, fade);
        fade = fade * fade * (3.0f - 2.0f * fade);

        unsigned char dotAlpha = static_cast<unsigned char>(baseAlpha * fade * 0.75f);
        if (dotAlpha > 2) {
            float dotSize = 7.0f * (0.4f + 0.6f * fade);
            renderSpriteOrPlaceholder("ball.png", x - dotSize * 0.5f, y - dotSize * 0.5f, dotSize, dotSize, dotAlpha);
        }

        d += step;
    }

    patternOffset = d - length;
    totalDistanceDrawn += length;
}

void SdlRuntime::drawLandingGlow(gameplay::Vec2 pos, float radius, unsigned char baseAlpha) {
    unsigned char glowAlpha = static_cast<unsigned char>(baseAlpha * 0.8f);
    if (glowAlpha > 0) {
        float flareWidth = radius * 2.0f;
        float flareHeight = 4.0f;
        rendererApi_->drawRect(render::Rect{pos.x - flareWidth * 0.5f, pos.y - flareHeight * 0.5f, flareWidth, flareHeight}, render::Color{255, 255, 255, glowAlpha});
        rendererApi_->drawRect(render::Rect{pos.x - flareHeight * 0.5f, pos.y - flareWidth * 0.5f, flareHeight, flareWidth}, render::Color{255, 255, 255, glowAlpha});
    }
}

void SdlRuntime::logAssetStatsOnce() {
    if (assetStatsLogged_) {
        return;
    }

    const auto stats = assets_->stats();
    if (stats.loadedTextures == 0) {
        return;
    }

    core::Log::info("Asset stats: " + textureStatsLine(stats));
    if (stats.approximateTextureBytes > 32ULL * 1024ULL * 1024ULL) {
        core::Log::warn("Texture memory exceeds Phase 1 PRD budget of 32 MiB for sprite textures");
    }
    assets_->releaseUnused();
    assetStatsLogged_ = true;
}

void SdlRuntime::logPerformanceSummary(double totalRuntimeMilliseconds) const {
    const auto textureStats = assets_ ? assets_->stats() : render::AssetStats{};
    const auto audioStats = audio_ ? audio_->stats() : audio::AudioStats{};
    const double averageFrameMilliseconds = measuredFrames_ > 0
        ? frameMillisecondsSum_ / static_cast<double>(measuredFrames_)
        : 0.0;
    const char* mode = config_.startInLevel ? "level" : "menu";

    std::ostringstream output;
    output.setf(std::ios::fixed);
    output.precision(3);
    output << "PERF_SUMMARY"
           << " mode=" << mode
           << " level=" << config_.level
           << " startup_ms=" << startupMilliseconds_
           << " runtime_ms=" << totalRuntimeMilliseconds
           << " frames=" << frameStats_.frameCount
           << " updates=" << frameStats_.updateCount
           << " avg_frame_ms=" << averageFrameMilliseconds
           << " min_frame_ms=" << frameMillisecondsMin_
           << " max_frame_ms=" << frameMillisecondsMax_
           << " textures=" << textureStats.loadedTextures
           << " texture_mib=" << (static_cast<double>(textureStats.approximateTextureBytes) / (1024.0 * 1024.0))
           << " texture_requests=" << textureStats.textureRequests
           << " cache_hits=" << textureStats.cacheHits
           << " load_attempts=" << textureStats.loadAttempts
           << " failed_loads=" << textureStats.failedLoads
           << " ui_transitions=" << smokeCycleTransitions_
           << " sfx_loaded=" << audioStats.sfxLoaded
           << " audio_mib=" << (static_cast<double>(audioStats.approximateSfxBytes) / (1024.0 * 1024.0))
           << " audio_initialized=" << (audioStats.initialized ? "true" : "false");
    if (!audioStats.currentMusic.empty()) {
        output << " music=" << audioStats.currentMusic;
    }
    core::Log::info(output.str());
}

void SdlRuntime::updateWindowTitle() {
    if (window_ == nullptr) {
        return;
    }

    char title[256]{};
    std::snprintf(
        title,
        sizeof(title),
        "Arcade Blocks II | %.1f FPS | %.2f ms | updates %llu | %s",
        frameStats_.framesPerSecond,
        frameStats_.frameSeconds * 1000.0,
        static_cast<unsigned long long>(frameStats_.updatesThisFrame),
        fullscreen_ ? "fullscreen" : "windowed");
    SDL_SetWindowTitle(window_, title);
}

void SdlRuntime::toggleFullscreen() {
    fullscreen_ = !fullscreen_;
    if (!SDL_SetWindowFullscreen(window_, fullscreen_)) {
        core::Log::warn(sdlError("SDL_SetWindowFullscreen"));
        fullscreen_ = !fullscreen_;
        return;
    }

    core::Log::info(std::string("Fullscreen toggled: ") + (fullscreen_ ? "on" : "off"));
    updateMouseCapture();
    updateWindowTitle();
}

void SdlRuntime::updateMouseCapture() {
    if (window_ == nullptr) {
        return;
    }

    const bool levelCompleteOverlayOpen = levelCompleteActive_ && levelCompleteAge_ >= levelCompleteFadeOutDuration_;
    const bool shouldCapture = screen_ == RuntimeScreen::InGame && focused_ && !fullscreen_ && !levelCompleteOverlayOpen && !debugSpawnMenuOpen_;
    if (!shouldCapture && mouseCaptureEnabled_ == shouldCapture) {
        return;
    }

    if (shouldCapture) {
        if (!SDL_HideCursor()) {
            core::Log::warn(sdlError("SDL_HideCursor"));
        }
        int width = 0;
        int height = 0;
        if (SDL_GetWindowSize(window_, &width, &height) && width > 0 && height > 0) {
            const SDL_Rect rect{0, 0, width, height};
            if (!SDL_SetWindowMouseRect(window_, &rect)) {
                core::Log::warn(sdlError("SDL_SetWindowMouseRect"));
            }
        }
        if (!SDL_SetWindowMouseGrab(window_, true)) {
            core::Log::warn(sdlError("SDL_SetWindowMouseGrab"));
        }
    } else {
        if (!SDL_ShowCursor()) {
            core::Log::warn(sdlError("SDL_ShowCursor"));
        }
        if (!SDL_SetWindowMouseGrab(window_, false)) {
            core::Log::warn(sdlError("SDL_SetWindowMouseGrab"));
        }
        if (!SDL_SetWindowMouseRect(window_, nullptr)) {
            core::Log::warn(sdlError("SDL_SetWindowMouseRect"));
        }
    }

    mouseCaptureEnabled_ = shouldCapture;
}

void SdlRuntime::updateMousePaddleFromWindowX(float windowX) {
    if (renderer_ == nullptr) {
        mousePaddleCenterX_ = std::clamp(windowX, 0.0f, static_cast<float>(config_.logicalWidth));
        mousePaddleActive_ = true;
        return;
    }

    float logicalX = windowX;
    float logicalY = 0.0f;
    if (!SDL_RenderCoordinatesFromWindow(renderer_, windowX, 0.0f, &logicalX, &logicalY)) {
        core::Log::warn(sdlError("SDL_RenderCoordinatesFromWindow"));
    }

    mousePaddleCenterX_ = std::clamp(logicalX, 0.0f, static_cast<float>(config_.logicalWidth));
    mousePaddleActive_ = true;
}

bool SdlRuntime::matchesKey(SDL_Keycode eventKey, SDL_Keycode binding, SDL_Keycode alternate) const noexcept {
    return eventKey == binding || (alternate != SDLK_UNKNOWN && eventKey == alternate);
}

void SdlRuntime::applyVideoSettings() {
    if (renderer_ == nullptr || window_ == nullptr) {
        return;
    }

    if (!SDL_SetRenderVSync(renderer_, vsyncEnabled_ ? 1 : 0)) {
        core::Log::warn(sdlError("SDL_SetRenderVSync"));
    }

    if (!fullscreen_) {
        if (!SDL_SetWindowSize(window_, config_.windowWidth, config_.windowHeight)) {
            core::Log::warn(sdlError("SDL_SetWindowSize"));
        }
    }

    // Compute frame pacing interval (0 = unlimited).
    const int fps = config_.settings.video.fpsLimit;
    frameLimitUs_ = (fps > 0 && !vsyncEnabled_)
        ? static_cast<std::uint64_t>(1'000'000ull / static_cast<std::uint64_t>(fps))
        : 0ull;
    core::Log::debug(
        "FPS limit: " + (fps > 0 ? std::to_string(fps) + " fps (" + std::to_string(frameLimitUs_) + " µs/frame)"
                                  : "unlimited"));
}

void SdlRuntime::applyAudioSettings() {
    if (!audio_) {
        return;
    }

    audio_->setMasterVolume(static_cast<float>(config_.settings.audio.masterVolume));
    audio_->setMusicVolume(static_cast<float>(config_.settings.audio.musicVolume));
    audio_->setSfxVolume(static_cast<float>(config_.settings.audio.sfxVolume));
}

std::string SdlRuntime::sdlError(const char* context) const {
    return std::string{context} + " failed: " + SDL_GetError();
}

} // namespace arcadeblocks::platform
