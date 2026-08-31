cmake_minimum_required(VERSION 3.25)

project(
    ArcadeBlocksII
    VERSION 0.1.0
    DESCRIPTION "Native C++ foundation for Arcade Blocks II"
    LANGUAGES CXX
)

include(CTest)
include(FetchContent)
include(cmake/ArcadeBlocksCompilerOptions.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARCADEBLOCKS_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(ARCADEBLOCKS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ARCADEBLOCKS_USE_SYSTEM_DEPS "Prefer system packages over FetchContent for third-party dependencies" OFF)

if(ARCADEBLOCKS_USE_SYSTEM_DEPS)
    find_package(SDL3 3.4.10 CONFIG REQUIRED)
    find_package(SDL3_image 3.4.4 CONFIG REQUIRED)
    find_package(SDL3_mixer 3.2.4 CONFIG REQUIRED)
    find_package(SDL3_ttf 3.2.2 CONFIG REQUIRED)
    find_package(nlohmann_json 3.12.0 CONFIG REQUIRED)
    find_package(box2d 3.1.1 CONFIG REQUIRED)
    find_package(Catch2 3.9.1 CONFIG REQUIRED)
else()
    find_package(SDL3 3.4.10 CONFIG QUIET)
    if(NOT SDL3_FOUND)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.4.10
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(SDL3)
    endif()

    find_package(SDL3_image 3.4.4 CONFIG QUIET)
    if(NOT SDL3_image_FOUND)
        set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL3_image
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
            GIT_TAG release-3.4.4
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(SDL3_image)
    endif()

    find_package(SDL3_mixer 3.2.4 CONFIG QUIET)
    if(NOT SDL3_mixer_FOUND)
        set(SDLMIXER_INSTALL OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_FLAC OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_MP3 OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
        set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL3_mixer
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
            GIT_TAG release-3.2.4
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(SDL3_mixer)
    endif()

    find_package(SDL3_ttf 3.2.2 CONFIG QUIET)
    if(NOT SDL3_ttf_FOUND)
        set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
        set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
        set(SDLTTF_TESTS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL3_ttf
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
            GIT_TAG release-3.2.2
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(SDL3_ttf)
    endif()

    find_package(nlohmann_json 3.12.0 CONFIG QUIET)
    if(NOT nlohmann_json_FOUND)
        set(JSON_Install OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.12.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(nlohmann_json)
    endif()

    find_package(box2d 3.1.1 CONFIG QUIET)
    if(NOT box2d_FOUND)
        set(BOX2D_INSTALL OFF CACHE BOOL "" FORCE)
        set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
        set(BOX2D_DOCS OFF CACHE BOOL "" FORCE)
        set(BOX2D_PROFILE OFF CACHE BOOL "" FORCE)
        set(BOX2D_VALIDATE OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            box2d
            GIT_REPOSITORY https://github.com/erincatto/box2d.git
            GIT_TAG v3.1.1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(box2d)
    endif()

    find_package(Catch2 3.9.1 CONFIG QUIET)
    if(NOT Catch2_FOUND)
        set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
        set(CATCH_INSTALL_HELPERS OFF CACHE BOOL "" FORCE)
        set(CATCH_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.9.1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
endif()

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(arcadeblocks2_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(arcadeblocks2_imgui
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(arcadeblocks2_imgui
    PUBLIC
        SDL3::SDL3
)

arcadeblocks_apply_project_options(arcadeblocks2_imgui)

add_library(arcadeblocks2_foundation STATIC
    src/assets/AssetRegistry.cpp
    src/audio/AudioSystem.cpp
    src/audio/SoundBank.cpp
    src/core/Application.cpp
    src/core/Clock.cpp
    src/core/CommandLine.cpp
    src/core/Log.cpp
    src/core/Version.cpp
    src/gameplay/GameWorld.cpp
    src/levels/LevelLoader.cpp
    src/levels/LevelRepository.cpp
    src/levels/LevelTypes.cpp
    src/localization/Localization.cpp
    src/platform/Paths.cpp
    src/platform/SdlRuntime.cpp
    src/physics/PhysicsWorld.cpp
    src/render/AssetManager.cpp
    src/render/Renderer.cpp
    src/render/SpriteAtlas.cpp
    src/settings/KeyBinding.cpp
    src/settings/Settings.cpp
    src/settings/SettingsStore.cpp
    src/render/Texture.cpp
    src/ui/DebugMenuView.cpp
    src/ui/DebugBonusesView.cpp
    src/ui/HelpView.cpp
    src/ui/HudView.cpp
    src/ui/ImGuiLayer.cpp
    src/ui/LevelCompleteView.cpp
    src/ui/LevelsMenuView.cpp
    src/ui/LoadingScreenView.cpp
    src/ui/MainMenuView.cpp
    src/ui/PauseView.cpp
    src/ui/SettingsView.cpp
    src/ui/UiLayout.cpp
    src/ui/UiTheme.cpp
)

add_library(ArcadeBlocksII::foundation ALIAS arcadeblocks2_foundation)

target_include_directories(arcadeblocks2_foundation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

arcadeblocks_apply_project_options(arcadeblocks2_foundation)

target_link_libraries(arcadeblocks2_foundation
    PUBLIC
        SDL3::SDL3
    PRIVATE
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        arcadeblocks2_imgui
        box2d::box2d
        nlohmann_json::nlohmann_json
)

target_compile_definitions(arcadeblocks2_foundation
    PUBLIC
        ARCADEBLOCKS_VERSION="${PROJECT_VERSION}"
        ARCADEBLOCKS_SDL_VERSION="3.4.10"
)

add_executable(ArcadeBlocksII
    src/main.cpp
)

target_link_libraries(ArcadeBlocksII
    PRIVATE
        ArcadeBlocksII::foundation
)

arcadeblocks_apply_project_options(ArcadeBlocksII)

if(BUILD_TESTING)
    add_executable(arcadeblocks2_foundation_tests
        tests/FoundationSmokeTest.cpp
        tests/KeyBindingTest.cpp
        tests/LocalizationTest.cpp
        tests/SettingsStoreTest.cpp
        tests/BossLevelTests.cpp
        tests/BossLevel30Tests.cpp
        tests/BossLevel40Tests.cpp
        tests/BossLevel50Tests.cpp
    )

    target_link_libraries(arcadeblocks2_foundation_tests
        PRIVATE
            ArcadeBlocksII::foundation
            Catch2::Catch2WithMain
    )

    target_compile_definitions(arcadeblocks2_foundation_tests
        PRIVATE
            ARCADEBLOCKS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    arcadeblocks_apply_project_options(arcadeblocks2_foundation_tests)

    add_test(
        NAME foundation.version
        COMMAND arcadeblocks2_foundation_tests
    )

    add_test(
        NAME app.version
        COMMAND ArcadeBlocksII --version
    )

    add_test(
        NAME app.stage9-level-no-audio-smoke
        COMMAND ArcadeBlocksII --level 1 --windowed --no-audio --smoke-frames 3
    )
    set_tests_properties(app.stage9-level-no-audio-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
    )

    add_test(
        NAME app.stage9-menu-no-audio-smoke
        COMMAND ArcadeBlocksII --windowed --no-audio --smoke-frames 3
    )
    set_tests_properties(app.stage9-menu-no-audio-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
    )

    add_test(
        NAME app.stage9-audio-smoke
        COMMAND ArcadeBlocksII --windowed --smoke-frames 3 --debug
    )
    set_tests_properties(app.stage9-audio-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
    )

    add_test(
        NAME app.stage10-level1-vertical-slice
        COMMAND ArcadeBlocksII --level 1 --windowed --smoke-frames 6 --debug
    )
    set_tests_properties(app.stage10-level1-vertical-slice
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
    )

    add_test(
        NAME app.stage10-boss-level-smoke
        COMMAND ArcadeBlocksII --level 10 --windowed --smoke-frames 6 --debug
    )
    set_tests_properties(app.stage10-boss-level-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
    )

    function(add_stage15_smoke_test test_name scenario)
        set(test_data_dir "${CMAKE_CURRENT_BINARY_DIR}/test-data/${test_name}")
        add_test(
            NAME "${test_name}"
            COMMAND ArcadeBlocksII
                --windowed
                --no-audio
                --smoke-frames 3
                --smoke-scenario "${scenario}"
                --settings-file "${test_data_dir}/settings.json"
        )
        set_tests_properties("${test_name}"
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy;XDG_DATA_HOME=${test_data_dir}/xdg"
                LABELS "stage10;smoke"
        )
    endfunction()

    add_stage15_smoke_test(app.stage15-main-menu-smoke main-menu)
    add_stage15_smoke_test(app.stage15-settings-smoke settings)
    add_stage15_smoke_test(app.stage15-settings-save-smoke settings-save)
    add_stage15_smoke_test(app.stage15-help-smoke help)
    add_stage15_smoke_test(app.stage15-pause-smoke pause)
    add_stage15_smoke_test(app.stage15-pause-settings-smoke pause-settings)
    add_stage15_smoke_test(app.stage15-pause-help-smoke pause-help)

    add_test(
        NAME app.stage11-settings-cycle-smoke
        COMMAND ArcadeBlocksII
            --windowed
            --no-audio
            --smoke-frames 60
            --smoke-scenario settings-cycle
            --perf-summary
            --settings-file "${CMAKE_CURRENT_BINARY_DIR}/test-data/app.stage11-settings-cycle-smoke/settings.json"
    )
    set_tests_properties(app.stage11-settings-cycle-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/app.stage11-settings-cycle-smoke/xdg"
            PASS_REGULAR_EXPRESSION "load_attempts=1 .*ui_transitions=4"
            LABELS "stage11;performance;smoke"
    )

    add_test(
        NAME app.stage11-help-cycle-smoke
        COMMAND ArcadeBlocksII
            --windowed
            --no-audio
            --smoke-frames 60
            --smoke-scenario help-cycle
            --perf-summary
            --settings-file "${CMAKE_CURRENT_BINARY_DIR}/test-data/app.stage11-help-cycle-smoke/settings.json"
    )
    set_tests_properties(app.stage11-help-cycle-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/app.stage11-help-cycle-smoke/xdg"
            PASS_REGULAR_EXPRESSION "textures=2 .*load_attempts=2 .*ui_transitions=4"
            LABELS "stage11;performance;smoke"
    )

    add_test(
        NAME app.invalid-assets-dir
        COMMAND ArcadeBlocksII --assets-dir ${CMAKE_CURRENT_BINARY_DIR}/missing-assets
    )
    set_tests_properties(app.invalid-assets-dir
        PROPERTIES
            WILL_FAIL TRUE
    )
endif()

include(GNUInstallDirs)

install(TARGETS ArcadeBlocksII
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

install(DIRECTORY assets/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/arcadeblocks2/assets
)

if(UNIX AND NOT APPLE)
    install(FILES tools/packaging/linux/arcadeblocks2.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES assets/sprites/favicon-192.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/192x192/apps
        RENAME arcadeblocks2.png
    )
    install(FILES assets/sprites/favicon-32.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps
        RENAME arcadeblocks2.png
    )
    install(FILES assets/sprites/favicon-192.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
        RENAME arcadeblocks2.png
    )
endif()

set(CPACK_PACKAGE_NAME "arcadeblocks2")
set(CPACK_PACKAGE_VENDOR "leocallidus")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native C++ foundation for Arcade Blocks II")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT "leoalekseev27@gmail.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/leocallidus/arcadeblocks2")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "leocallidus <leoalekseev27@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME "arcadeblocks2_${PROJECT_VERSION}_amd64.deb")

set(CPACK_RPM_PACKAGE_LICENSE "Proprietary")
set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
set(CPACK_RPM_FILE_NAME "arcadeblocks2-${PROJECT_VERSION}-1.x86_64.rpm")

include(CPack)

