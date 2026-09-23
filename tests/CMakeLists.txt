cmake_minimum_required(VERSION 3.22)

# ============================================================================
# Bumbler XD Headless CTest Suites
# ============================================================================
enable_testing()

set(TEST_COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../source
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/dsp
)

function(bumbler_add_test_target TARGET_NAME SOURCE_FILE TEST_NAME)
    add_executable(${TARGET_NAME} ${SOURCE_FILE})
    target_include_directories(${TARGET_NAME} PRIVATE ${TEST_COMMON_INCLUDES})
    
    if(MSVC)
        target_compile_definitions(${TARGET_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${TARGET_NAME} PRIVATE /utf-8 /W4 /O2 /fp:precise)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
            target_compile_options(${TARGET_NAME} PRIVATE /arch:AVX2)
        endif()
    else()
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -O3)
    endif()

    if(TARGET bumbler_dsp_core)
        target_link_libraries(${TARGET_NAME} PRIVATE bumbler_dsp_core)
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TARGET_NAME})
endfunction()

# 1. Core M1 DSP Unit Tests (Voice Allocation, PolyBLEP Oscillators, FM, RingMod, PWM, Zero Alloc)
bumbler_add_test_target(bumbler_dsp_unit_tests dsp_unit_tests.cpp DspUnitTests)

# 2. 6-Mode Wasp Filter Accuracy & Slopes
bumbler_add_test_target(bumbler_filter_tests filter_accuracy_tests.cpp FilterAccuracyTests)

# 3. Inter-Oscillator Modulation & Routing Matrix
bumbler_add_test_target(bumbler_modulation_tests modulation_tests.cpp ModulationTests)

# 4. Vintage Character Circuits (Dual, Drift, Distortion, Noise)
bumbler_add_test_target(bumbler_character_tests character_circuit_tests.cpp CharacterCircuitTests)

# 5. Real-Time Memory Safety, Denormal Protection & Invariants
bumbler_add_test_target(bumbler_realtime_safety_tests realtime_safety_tests.cpp RealtimeSafetyTests)

# 6. Master Headless Acceptance Runner (Tiers 1-4)
bumbler_add_test_target(bumbler_e2e_suite e2e_headless_suite.cpp E2EHeadlessSuite)

# 7. Milestone 1 Adversarial Stress & Numerical Stability Harness
bumbler_add_test_target(bumbler_m1_adversarial_tests m1_adversarial_stress_tests.cpp M1AdversarialStressTests)

# 8. Milestone 1 Adversarial Spectral Fidelity & Real-Time Safety Suite
bumbler_add_test_target(bumbler_challenger_m1_tests challenger_m1_tests.cpp ChallengerM1SpectralSafetyTests)

# 9. Milestone 2 Adversarial Filter Stress Suite (Challenger M2-1)
bumbler_add_test_target(bumbler_challenger_m2_tests challenger_m2_tests.cpp ChallengerM2FilterAdversarialTests)

# 10. Milestone 2 Filter Empirical Frequency Response Suite (Challenger M2-2)
bumbler_add_test_target(bumbler_challenger_m2_2_tests challenger_m2_2_tests.cpp ChallengerM2FrequencyResponseTests)

# 11. Milestone 2 Adversarial Stress & Limit Probing Suite (Challenger M2-1 Dedicated)
bumbler_add_test_target(bumbler_challenger_m2_1_tests challenger_m2_1_tests.cpp ChallengerM2AdversarialStressTests)

# 12. Milestone 3 Adversarial Modulation Stress Suite (Challenger M3-1)
bumbler_add_test_target(bumbler_challenger_m3_tests challenger_m3_tests.cpp ChallengerM3ModulationStressTests)

# 13. Milestone 3 Modulation Timing, Curvature & Routing Matrix Suite (Challenger M3-2)
bumbler_add_test_target(bumbler_challenger_m3_2_tests challenger_m3_2_tests.cpp ChallengerM3TimingRoutingTests)

# 14. Milestone 4 Character Circuits Adversarial Stress & Stability Suite (Challenger M4-1)
bumbler_add_test_target(bumbler_challenger_m4_tests challenger_m4_tests.cpp ChallengerM4CharacterStressTests)


# 15. Milestone 4 Character Circuits Empirical Acoustic & Statistical Suite (Challenger M4-2)
bumbler_add_test_target(bumbler_challenger_m4_2_tests challenger_m4_2_tests.cpp ChallengerM4EmpiricalAcoustics)

# 16. Milestone 5 APVTS Parameter & State Recall Test Suite
add_executable(bumbler_apvts_state_tests
    apvts_state_tests.cpp
    ../source/plugin/PluginProcessor.cpp
    ../source/plugin/PluginEditor.cpp
    ../source/ui/BumblerLookAndFeel.cpp
    ../source/ui/LCDGraphDisplay.cpp
    ../source/ui/HorizontalFader.cpp
)
target_include_directories(bumbler_apvts_state_tests PRIVATE
    ${TEST_COMMON_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/ui
)
if(MSVC)
    target_compile_definitions(bumbler_apvts_state_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bumbler_apvts_state_tests PRIVATE /utf-8 /W4 /O2 /fp:precise)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(bumbler_apvts_state_tests PRIVATE /arch:AVX2)
    endif()
else()
    target_compile_options(bumbler_apvts_state_tests PRIVATE -Wall -Wextra -O3)
endif()

target_link_libraries(bumbler_apvts_state_tests PRIVATE
    bumbler_dsp_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
)
add_test(NAME ApvtsStateTests COMMAND bumbler_apvts_state_tests)

# 17. Milestone 5 Adversarial Real-Time Latency & Audio Stability Suite (Challenger M5-2)
add_executable(bumbler_challenger_m5_2_tests
    challenger_m5_2_tests.cpp
    ../source/plugin/PluginProcessor.cpp
    ../source/plugin/PluginEditor.cpp
    ../source/ui/BumblerLookAndFeel.cpp
    ../source/ui/LCDGraphDisplay.cpp
    ../source/ui/HorizontalFader.cpp
)
target_include_directories(bumbler_challenger_m5_2_tests PRIVATE
    ${TEST_COMMON_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/ui
)
if(MSVC)
    target_compile_definitions(bumbler_challenger_m5_2_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bumbler_challenger_m5_2_tests PRIVATE /utf-8 /W4 /O2 /fp:precise)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(bumbler_challenger_m5_2_tests PRIVATE /arch:AVX2)
    endif()
else()
    target_compile_options(bumbler_challenger_m5_2_tests PRIVATE -Wall -Wextra -O3)
endif()

target_link_libraries(bumbler_challenger_m5_2_tests PRIVATE
    bumbler_dsp_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
)
add_test(NAME ChallengerM5RealtimeLatencyAndPresets COMMAND bumbler_challenger_m5_2_tests)

# 18. Milestone 5 Adversarial State Recall & Robustness Suite (Challenger M5-1)
add_executable(bumbler_challenger_m5_1_tests
    challenger_m5_1_tests.cpp
    ../source/plugin/PluginProcessor.cpp
    ../source/plugin/PluginEditor.cpp
    ../source/ui/BumblerLookAndFeel.cpp
    ../source/ui/LCDGraphDisplay.cpp
    ../source/ui/HorizontalFader.cpp
)
target_include_directories(bumbler_challenger_m5_1_tests PRIVATE
    ${TEST_COMMON_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/ui
)
if(MSVC)
    target_compile_definitions(bumbler_challenger_m5_1_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bumbler_challenger_m5_1_tests PRIVATE /utf-8 /W4 /O2 /fp:precise)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(bumbler_challenger_m5_1_tests PRIVATE /arch:AVX2)
    endif()
else()
    target_compile_options(bumbler_challenger_m5_1_tests PRIVATE -Wall -Wextra -O3)
endif()

target_link_libraries(bumbler_challenger_m5_1_tests PRIVATE
    bumbler_dsp_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
)
add_test(NAME ChallengerM5StateRecallAndRobustness COMMAND bumbler_challenger_m5_1_tests)

# 19. Milestone 5 Remediation GUI Lifecycle & SafePointer Stress Suite
add_executable(bumbler_challenger_m5_remediation_tests
    challenger_m5_remediation_tests.cpp
    ../source/plugin/PluginProcessor.cpp
    ../source/plugin/PluginEditor.cpp
    ../source/ui/BumblerLookAndFeel.cpp
    ../source/ui/LCDGraphDisplay.cpp
    ../source/ui/HorizontalFader.cpp
)
target_include_directories(bumbler_challenger_m5_remediation_tests PRIVATE
    ${TEST_COMMON_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/ui
)
if(MSVC)
    target_compile_definitions(bumbler_challenger_m5_remediation_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bumbler_challenger_m5_remediation_tests PRIVATE /utf-8 /W4 /O2 /fp:precise)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(bumbler_challenger_m5_remediation_tests PRIVATE /arch:AVX2)
    endif()
else()
    target_compile_options(bumbler_challenger_m5_remediation_tests PRIVATE -Wall -Wextra -O3)
endif()

target_link_libraries(bumbler_challenger_m5_remediation_tests PRIVATE
    bumbler_dsp_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
)
add_test(NAME ChallengerM5RemediationGuiStress COMMAND bumbler_challenger_m5_remediation_tests)

# 20. Milestone 6 Tier 5 Hardening & Adversarial Stress Suite (Challenger M6-1)
bumbler_add_test_target(bumbler_challenger_m6_tests challenger_m6_adversarial_tests.cpp ChallengerM6AdversarialStressTests)

# 21. Milestone 6 Tier 5 Hardening APVTS, Presets & UI Stress Suite (Challenger M6-2)
add_executable(bumbler_challenger_m6_2_tests
    challenger_m6_2_tests.cpp
    ../source/plugin/PluginProcessor.cpp
    ../source/plugin/PluginEditor.cpp
    ../source/ui/BumblerLookAndFeel.cpp
    ../source/ui/LCDGraphDisplay.cpp
    ../source/ui/HorizontalFader.cpp
)
target_include_directories(bumbler_challenger_m6_2_tests PRIVATE
    ${TEST_COMMON_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/ui
)
if(MSVC)
    target_compile_definitions(bumbler_challenger_m6_2_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(bumbler_challenger_m6_2_tests PRIVATE /utf-8 /W4 /O2 /fp:precise)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(bumbler_challenger_m6_2_tests PRIVATE /arch:AVX2)
    endif()
else()
    target_compile_options(bumbler_challenger_m6_2_tests PRIVATE -Wall -Wextra -O3)
endif()

target_link_libraries(bumbler_challenger_m6_2_tests PRIVATE
    bumbler_dsp_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
)
add_test(NAME ChallengerM62AdversarialStatePresetsGui COMMAND bumbler_challenger_m6_2_tests)

# 22. Golden Vector Tests & Regression Suite (Deterministic Invariants & Preset Vectors)
bumbler_add_test_target(bumbler_golden_vector_tests golden_vector_tests.cpp GoldenVectorTests)
target_include_directories(bumbler_golden_vector_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../source/plugin)

