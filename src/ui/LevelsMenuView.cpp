#include "ui/LevelsMenuView.hpp"

#include "levels/LevelLoader.hpp"
#include "ui/UiLayout.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace arcadeblocks::ui {
namespace {

float clamp01(double value) {
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

bool activationPressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space);
}

int getChapterIndex(int level) {
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
    return 13; // Extra/Other levels
}

UiAccent getChapterAccent(int chapterIdx) {
    switch (chapterIdx) {
        case 1: return UiAccent::Pink;
        case 2: return UiAccent::Cyan;
        case 3: return UiAccent::Purple;
        case 4: return UiAccent::Lime;
        case 5: return UiAccent::Fuchsia;
        case 6: return UiAccent::Cyan;
        case 7: return UiAccent::Brown;
        case 8: return UiAccent::Cyan;
        case 9: return UiAccent::White;
        case 10: return UiAccent::Purple;
        case 11: return UiAccent::White;
        case 12: return UiAccent::Purple;
        default: return UiAccent::White;
    }
}

std::string getChapterTitle(int chapterIdx, const localization::Localization& localization, settings::Language language) {
    if (chapterIdx >= 1 && chapterIdx <= 12) {
        std::string key = "chapter.title." + std::to_string(chapterIdx);
        std::string localized = localization.text(language, key);
        if (!localized.empty()) {
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

struct LevelLayoutInfo {
    int index;
    ImVec2 pos;
};

} // namespace

void LevelsMenuView::discoverLevels(const assets::AssetRegistry& assetRegistry) {
    if (levelsDiscovered_) {
        return;
    }
    
    for (int i = 1; i <= 200; ++i) {
        auto mapping = assetRegistry.level(i);
        if (assetRegistry.exists(mapping.levelJson)) {
            auto result = levels::loadLevelDefinition(assetRegistry.resolve(mapping.levelJson));
            std::string name = "Level " + std::to_string(i);
            if (result.ok()) {
                name = i == 31 ? "отладка" : result.level->metadata.name;
            }
            availableLevels_.push_back({i, name});
        }
    }
    levelsDiscovered_ = true;
}

LevelsMenuRenderResult LevelsMenuView::render(
    const render::Texture* backgroundTexture,
    const assets::AssetRegistry& assetRegistry,
    const localization::Localization& localization,
    settings::Language language,
    bool& open) {
    
    LevelsMenuRenderResult result;
    const double now = ImGui::GetTime();
    const float scale = UiLayout::viewportScale();
    const int columns = 4;

    if (open != wasOpen_) {
        // State changed
        animationStartedAt_ = now;
        wasOpen_ = open;
    }

    if (open && !levelsDiscovered_) {
        discoverLevels(assetRegistry);
    }

    double elapsed = now - animationStartedAt_;
    float fadeAlpha = 0.0f;
    if (open) {
        fadeAlpha = clamp01(elapsed / 0.25);
    } else {
        fadeAlpha = 1.0f - clamp01(elapsed / 0.25);
    }

    if (fadeAlpha <= 0.0f && !open) {
        result.isVisible = false;
        return result;
    }
    result.isVisible = true;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});
    
    ImGui::Begin(
        "LevelsMenuFullscreen",
        nullptr,
        ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings);
            
    // Keep levels menu focused every frame while open, ensuring it renders
    // on top of other ImGui windows.
    if (open) {
        ImGui::SetWindowFocus();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    bool scrolledThisFrame = false;
    std::vector<LevelLayoutInfo> layoutInfos;
    if (!availableLevels_.empty()) {
        layoutInfos.reserve(availableLevels_.size());

        const float buttonWidth = 220.0f * scale;
        const float buttonHeight = 60.0f * scale;
        const float spacingX = 20.0f * scale;
        const float spacingY = 20.0f * scale;

        float currentY = 10.0f * scale;
        int currentChapter = -1;
        int colInChapter = 0;
        int rowInChapter = 0;

        for (int i = 0; i < static_cast<int>(availableLevels_.size()); ++i) {
            int level = availableLevels_[i].index;
            int chap = getChapterIndex(level);
            
            if (chap != currentChapter) {
                currentChapter = chap;
                colInChapter = 0;
                rowInChapter = 0;
                
                currentY += 10.0f * scale; // spacing before header
                currentY += 35.0f * scale; // header height + gap
            }
            
            float localX = 10.0f * scale + colInChapter * (buttonWidth + spacingX);
            float localY = currentY + rowInChapter * (buttonHeight + spacingY);
            
            layoutInfos.push_back({i, ImVec2{localX, localY}});
            
            colInChapter++;
            if (colInChapter >= columns) {
                colInChapter = 0;
                rowInChapter++;
            }
            
            bool nextIsDifferent = (i + 1 == static_cast<int>(availableLevels_.size())) || 
                                   (getChapterIndex(availableLevels_[i + 1].index) != currentChapter);
            if (nextIsDifferent) {
                int totalRowsInChapter = rowInChapter + (colInChapter > 0 ? 1 : 0);
                currentY += totalRowsInChapter * (buttonHeight + spacingY);
            }
        }
    }

    if (open && !availableLevels_.empty()) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            int bestIdx = -1;
            float bestDist = 1e9f;
            ImVec2 currentPos = layoutInfos[selectedIndex_].pos;

            for (int j = 0; j < static_cast<int>(layoutInfos.size()); ++j) {
                if (layoutInfos[j].pos.y < currentPos.y - 1.0f * scale) {
                    float dx = layoutInfos[j].pos.x - currentPos.x;
                    float dy = layoutInfos[j].pos.y - currentPos.y;
                    float dist = dx * dx * 3.0f + dy * dy;
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestIdx = j;
                    }
                }
            }
            if (bestIdx != -1) {
                selectedIndex_ = bestIdx;
                result.soundEffect = UiSoundEffect::Hover;
                scrolledThisFrame = true;
            }
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            int bestIdx = -1;
            float bestDist = 1e9f;
            ImVec2 currentPos = layoutInfos[selectedIndex_].pos;

            for (int j = 0; j < static_cast<int>(layoutInfos.size()); ++j) {
                if (layoutInfos[j].pos.y > currentPos.y + 1.0f * scale) {
                    float dx = layoutInfos[j].pos.x - currentPos.x;
                    float dy = layoutInfos[j].pos.y - currentPos.y;
                    float dist = dx * dx * 3.0f + dy * dy;
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestIdx = j;
                    }
                }
            }
            if (bestIdx != -1) {
                selectedIndex_ = bestIdx;
                result.soundEffect = UiSoundEffect::Hover;
                scrolledThisFrame = true;
            }
        } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            if (selectedIndex_ > 0) {
                selectedIndex_--;
                result.soundEffect = UiSoundEffect::Hover;
                scrolledThisFrame = true;
            }
        } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            if (selectedIndex_ < static_cast<int>(availableLevels_.size()) - 1) {
                selectedIndex_++;
                result.soundEffect = UiSoundEffect::Hover;
                scrolledThisFrame = true;
            }
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            open = false;
            result.soundEffect = UiSoundEffect::Back;
            result.action = LevelsMenuAction::Close;
        }
    }

    const float centerX = viewport->Size.x * 0.5f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();

    // 1. Draw background image
    if (backgroundTexture != nullptr && (*backgroundTexture) && backgroundTexture->native() != nullptr) {
        const float bgWidth = static_cast<float>(backgroundTexture->width());
        const float bgHeight = static_cast<float>(backgroundTexture->height());
        const float bgScaleX = viewport->Size.x / bgWidth;
        const float bgScaleY = viewport->Size.y / bgHeight;
        const float bgScaleVal = std::max(bgScaleX, bgScaleY);
        const float drawW = bgWidth * bgScaleVal;
        const float drawH = bgHeight * bgScaleVal;
        const float offsetX = (viewport->Size.x - drawW) * 0.5f;
        const float offsetY = (viewport->Size.y - drawH) * 0.5f;

        drawList->AddImage(
            reinterpret_cast<ImTextureID>(backgroundTexture->native()),
            ImVec2{origin.x + offsetX, origin.y + offsetY},
            ImVec2{origin.x + offsetX + drawW, origin.y + offsetY + drawH},
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::GetColorU32(ImVec4{1.0f, 1.0f, 1.0f, fadeAlpha})
        );
    } else {
        // Fallback dark background
        drawList->AddRectFilled(
            origin,
            ImVec2{origin.x + viewport->Size.x, origin.y + viewport->Size.y},
            ImGui::GetColorU32(ImVec4{0.01f, 0.02f, 0.08f, fadeAlpha}));
    }

    // Add a dark overlay for readability
    drawList->AddRectFilled(
        origin,
        ImVec2{origin.x + viewport->Size.x, origin.y + viewport->Size.y},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.5f * fadeAlpha}));

    const float fontSize = 64.0f * scale;
    const float titleY = 80.0f * scale;

    const std::string titleText = localization.text(language, "debug.levels.title");
    const std::string actualTitle = titleText.empty() ? "SELECT LEVEL" : titleText;
    ImVec2 titleSize = ImGui::CalcTextSize(actualTitle.c_str());

    // Draw title shadow
    drawList->AddText(nullptr, fontSize, ImVec2{origin.x + centerX - titleSize.x * 0.5f + 4.0f, origin.y + titleY + 4.0f},
        ImGui::GetColorU32(ImVec4{0.0f, 0.0f, 0.0f, 0.8f * fadeAlpha}), actualTitle.c_str());
    // Draw title
    drawList->AddText(nullptr, fontSize, ImVec2{origin.x + centerX - titleSize.x * 0.5f, origin.y + titleY},
        ImGui::GetColorU32(ImVec4{0.46f, 0.86f, 0.98f, fadeAlpha}), actualTitle.c_str());

    if (availableLevels_.empty()) {
        const std::string noLevelsText = localization.text(language, "debug.levels.no_levels");
        const char* noLevels = noLevelsText.empty() ? "No levels found." : noLevelsText.c_str();
        ImVec2 textSz = ImGui::CalcTextSize(noLevels);
        drawList->AddText(nullptr, 32.0f * scale, ImVec2{origin.x + centerX - textSz.x * 0.5f, origin.y + viewport->Size.y * 0.5f},
            ImGui::GetColorU32(ImVec4{0.7f, 0.7f, 0.7f, fadeAlpha}), noLevels);
    } else {
        const float buttonWidth = 220.0f * scale;
        const float buttonHeight = 60.0f * scale;
        const float spacingX = 20.0f * scale;
        const float gridWidth = columns * buttonWidth + (columns - 1) * spacingX;
        
        const float panelWidth = gridWidth + 40.0f * scale; // Extra space for scrollbar
        const float panelHeight = viewport->Size.y - titleY - 240.0f * scale;
        
        ImGui::SetCursorPos(ImVec2{centerX - panelWidth * 0.5f, titleY + 110.0f * scale});
        UiTheme::beginScrollPanel("levels_scroll_panel", ImVec2{panelWidth, panelHeight});

        int currentChapter = -1;
        for (int i = 0; i < static_cast<int>(availableLevels_.size()); ++i) {
            int level = availableLevels_[static_cast<std::size_t>(i)].index;
            std::string levelName = availableLevels_[static_cast<std::size_t>(i)].name;
            
            std::string displayName = std::to_string(level);

            int chap = getChapterIndex(level);
            if (chap != currentChapter) {
                currentChapter = chap;
                float headerY = layoutInfos[i].pos.y - 35.0f * scale;

                ImGui::SetCursorPos(ImVec2{10.0f * scale, headerY});
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeAlpha);

                ImVec2 screenPos = ImGui::GetCursorScreenPos();
                UiAccent chapAccent = getChapterAccent(chap);
                ImVec4 accentColorVec = UiTheme::accent(chapAccent);
                ImU32 accentColorU32 = ImGui::GetColorU32(accentColorVec);

                // Draw vertical neon accent bar
                drawList->AddRectFilled(
                    screenPos,
                    ImVec2{screenPos.x + 4.0f * scale, screenPos.y + 24.0f * scale},
                    accentColorU32,
                    2.0f * scale
                );

                // Render text next to the bar
                ImGui::SetCursorPos(ImVec2{20.0f * scale, headerY});
                
                std::string chapterLabel;
                std::string title = getChapterTitle(chap, localization, language);
                if (language == settings::Language::Russian) {
                    chapterLabel = (chap <= 12 ? "Глава " + std::to_string(chap) + ": " : "") + title;
                } else {
                    chapterLabel = (chap <= 12 ? "Chapter " + std::to_string(chap) + ": " : "") + title;
                }

                // Render glowing text
                ImGui::PushStyleColor(ImGuiCol_Text, accentColorVec);
                ImGui::TextUnformatted(chapterLabel.c_str());
                ImGui::PopStyleColor();

                // Draw a thin horizontal accent line under the header
                drawList->AddLine(
                    ImVec2{screenPos.x, screenPos.y + 28.0f * scale},
                    ImVec2{screenPos.x + panelWidth - 40.0f * scale, screenPos.y + 28.0f * scale},
                    ImGui::GetColorU32(ImVec4{accentColorVec.x, accentColorVec.y, accentColorVec.z, 0.2f * fadeAlpha}),
                    1.0f
                );

                ImGui::PopStyleVar();
            }

            float localX = layoutInfos[i].pos.x;
            float localY = layoutInfos[i].pos.y;

            ImGui::SetCursorPos(ImVec2{localX, localY});
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeAlpha);

            std::string label = displayName + "##" + std::to_string(level);
            std::string id = "level_btn_" + std::to_string(level);

            UiAccent btnAccent = getChapterAccent(chap);

            const auto button = UiTheme::renderNeonButton(
                id.c_str(), 
                label.c_str(), 
                btnAccent, 
                ImVec2{buttonWidth, buttonHeight}, 
                open && selectedIndex_ == i);
                
            ImGui::PopStyleVar();

            if (open && selectedIndex_ == i && scrolledThisFrame) {
                ImGui::SetScrollHereY();
            }

            if (open && button.hovered && selectedIndex_ != i) {
                selectedIndex_ = i;
                result.soundEffect = UiSoundEffect::Hover;
            }

            if (!open) continue;

            const bool activate = button.pressed || (selectedIndex_ == i && activationPressed());
            if (activate) {
                result.soundEffect = UiSoundEffect::Select;
                result.action = LevelsMenuAction::StartLevel;
                result.selectedLevel = level;
                open = false;
            }
        }

        UiTheme::endScrollPanel();
    }

    const float backBtnWidth = 200.0f * scale;
    const float backBtnHeight = 60.0f * scale;
    ImGui::SetCursorPos(ImVec2{centerX - backBtnWidth * 0.5f, viewport->Size.y - 100.0f * scale});
    
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeAlpha);
    const std::string backText = localization.text(language, "common.back");
    const char* backLabel = backText.empty() ? "Back" : backText.c_str();
    const auto backBtn = UiTheme::renderNeonButton(
        "levels_back_btn", 
        backLabel, 
        UiAccent::Pink, 
        ImVec2{backBtnWidth, backBtnHeight}, 
        false);
    ImGui::PopStyleVar();
    
    if (open && backBtn.pressed) {
        result.soundEffect = UiSoundEffect::Back;
        result.action = LevelsMenuAction::Close;
        open = false;
    }

    ImGui::End();

    return result;
}

} // namespace arcadeblocks::ui
