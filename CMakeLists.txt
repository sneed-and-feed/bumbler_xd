cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Bumbler XD — Classic Wasp XT Homage Synthesizer
# Modern C++20 / JUCE 8 VST3 & Standalone Audio Plugin
# ==============================================================================

# Configure Apple deployment target before project()
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
    endif()
endif()

project(BUMBLER_XD_PROJECT VERSION 1.0.5 LANGUAGES C CXX)

# Enforce ISO C++20 standard without compiler extensions
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ==============================================================================
# Compiler Flags & Optimizations
# ==============================================================================
if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
        NOMINMAX
    )
    # /utf-8: Enforce UTF-8 source and execution charset
    # /W4: High warning level for clean, robust C++ code
    # /O2: Maximum speed optimization
    # /fp:precise: Strict IEEE-754 adherence (preserves NaN/Inf checks & prevents FMA drift)
    add_compile_options(/utf-8 /W4 /O2 /fp:precise)
    
    # AVX2 SIMD instructions for modern x64 audio processing
    if(CMAKE_SIZEOF_VOID_P EQUAL 8 OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        add_compile_options(/arch:AVX2)
    endif()
else()
    # Clang / GCC
    # -ffp-contract=off prevents FMA contraction across expressions to match MSVC /fp:precise
    # and preserve strict IEEE-754 semantics across architectures.
    add_compile_options(-Wall -Wextra -O3 -ffp-contract=off)
    # Note: -ffast-math is omitted to maintain strict IEEE-754 semantics
    # and preserve NaN/Inf assertions in audio DSP tests.
endif()

# ==============================================================================
# JUCE 8 Dependency Provisioning (Offline Local Cache Fallback + Remote Git)
# ==============================================================================
include(FetchContent)

if(NOT TARGET juce::juce_audio_processors)
    # Search paths for local cached JUCE 8 source
    set(LOCAL_JUCE_SEARCH_PATHS
        "${JUCE_LOCAL_PATH}"
        "$ENV{JUCE_LOCAL_PATH}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../braun_as-42/build/_deps/juce-src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../braun_rb-26/build/_deps/juce-src"
    )

    set(FOUND_LOCAL_JUCE "")
    foreach(path IN LISTS LOCAL_JUCE_SEARCH_PATHS)
        if(path AND EXISTS "${path}/CMakeLists.txt")
            set(FOUND_LOCAL_JUCE "${path}")
            break()
        endif()
    endforeach()

    if(FOUND_LOCAL_JUCE)
        message(STATUS "JUCE 8: Using local cached repository from: ${FOUND_LOCAL_JUCE}")
        FetchContent_Declare(
            JUCE
            SOURCE_DIR "${FOUND_LOCAL_JUCE}"
        )
    else()
        message(STATUS "JUCE 8: Local cache not found, fetching from GitHub (v8.0.6)...")
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.6
            GIT_SHALLOW    TRUE
        )
    endif()

    FetchContent_MakeAvailable(JUCE)
endif()

# Global JUCE definitions
# Pure native UI: disable web browser and curl to eliminate third-party dependencies
add_compile_definitions(
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ==============================================================================
# 1. bumbler_dsp_core Static Library (Headless, Pure Real-Time C++20 DSP Engine)
# ==============================================================================
add_library(bumbler_dsp_core STATIC)

# M1-M4 Core DSP Sources
target_sources(bumbler_dsp_core PRIVATE
    source/dsp/BumblerCommon.h
    source/dsp/ParameterSnapshot.h
    source/dsp/BumblerOscillator.h
    source/dsp/BumblerOscillator.cpp
    source/dsp/WaspFilter.h
    source/dsp/WaspFilter.cpp
    source/dsp/BumblerLFO.h
    source/dsp/BumblerLFO.cpp
    source/dsp/BumblerEnvelopes.h
    source/dsp/BumblerEnvelopes.cpp
    source/dsp/BumblerVoice.h
    source/dsp/BumblerVoice.cpp
    source/dsp/BumblerVoiceManager.h
    source/dsp/BumblerVoiceManager.cpp
    source/dsp/BumblerEngine.h
    source/dsp/BumblerEngine.cpp
    source/dsp/CharacterCircuits.h
    source/dsp/CharacterCircuits.cpp
)

target_include_directories(bumbler_dsp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp
)

target_link_libraries(bumbler_dsp_core PUBLIC
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ==============================================================================
# 2. BumblerXD Audio Plugin Target (VST3 + Standalone)
# ==============================================================================
set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(BumblerXD
    COMPANY_NAME "Sneed's Feed & Seed Ltd."
    BUNDLE_ID "com.sneedandfeed.bumbler_xd"
    VERSION "1.0.5"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Sned
    PLUGIN_CODE BmbX
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Bumbler XD"
)

# Plugin processor and editor sources
target_sources(BumblerXD PRIVATE
    source/plugin/Parameters.h
    source/plugin/parameters/ParameterHelpers.h
    source/plugin/parameters/OscillatorParameters.h
    source/plugin/parameters/FilterParameters.h
    source/plugin/parameters/EnvelopeParameters.h
    source/plugin/parameters/LfoParameters.h
    source/plugin/parameters/CharacterParameters.h
    source/plugin/parameters/PresetParameters.h
    source/plugin/PluginProcessor.h
    source/plugin/PluginProcessor.cpp
    source/plugin/PluginEditor.h
    source/plugin/PluginEditor.cpp
)

# UI Component sources (M1 stubs / M5 full UI)
foreach(ui_source
    source/ui/BumblerLookAndFeel.h
    source/ui/BumblerLookAndFeel.cpp
    source/ui/LCDGraphDisplay.h
    source/ui/LCDGraphDisplay.cpp
    source/ui/HorizontalFader.h
    source/ui/HorizontalFader.cpp
)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ui_source}")
        target_sources(BumblerXD PRIVATE "${ui_source}")
    endif()
endforeach()

target_include_directories(BumblerXD PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/source/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp
)

target_link_libraries(BumblerXD PRIVATE
    bumbler_dsp_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ==============================================================================
# 3. Headless CLI Example & Minimal Integration Example
# ==============================================================================
add_executable(bumbler_headless_example examples/headless_synth.cpp)
target_include_directories(bumbler_headless_example PRIVATE source source/dsp source/plugin)
target_link_libraries(bumbler_headless_example PRIVATE bumbler_dsp_core)
if(MSVC)
    target_compile_definitions(bumbler_headless_example PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bumbler_headless_example PRIVATE /utf-8 /W4 /O2 /fp:precise)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8 OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(bumbler_headless_example PRIVATE /arch:AVX2)
    endif()
else()
    target_compile_options(bumbler_headless_example PRIVATE -Wall -Wextra -O3)
endif()

add_executable(bumbler_minimal_example examples/minimal_integration.cpp)
target_include_directories(bumbler_minimal_example PRIVATE source source/dsp source/plugin)
target_link_libraries(bumbler_minimal_example PRIVATE bumbler_dsp_core)
if(MSVC)
    target_compile_definitions(bumbler_minimal_example PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bumbler_minimal_example PRIVATE /utf-8 /W4 /O2 /fp:precise)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8 OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(bumbler_minimal_example PRIVATE /arch:AVX2)
    endif()
else()
    target_compile_options(bumbler_minimal_example PRIVATE -Wall -Wextra -O3)
endif()

# ==============================================================================
# 4. Headless Verification Tests & CTest Integration
# ==============================================================================
option(BUMBLER_BUILD_TESTS "Build Bumbler XD test executables" ON)

if(BUMBLER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

