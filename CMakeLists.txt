cmake_minimum_required(VERSION 3.22)

project(HEAT VERSION 2.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(HEAT_BUILD_TESTS "Build HEAT unit tests and measurement tools" ON)
option(HEAT_BUILD_TOOLS "Build the UI snapshot renderer and the GPU meter check" ON)
option(HEAT_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(HEAT_SANITIZE_THREAD "Build with ThreadSanitizer" OFF)
set(HEAT_JUCE_DIR "" CACHE PATH "Path to a local JUCE 8 checkout (fetched automatically when empty)")

if(HEAT_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    add_link_options(-fsanitize=address,undefined)
endif()
if(HEAT_SANITIZE_THREAD AND NOT MSVC)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# ----------------------------------------------------------------------------
# JUCE
# ----------------------------------------------------------------------------
if(HEAT_JUCE_DIR)
    add_subdirectory(${HEAT_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ----------------------------------------------------------------------------
# heat_dsp: the complete audio engine. Pure C++20, no JUCE dependency, so it
# can be unit-tested, measured and sanitised in isolation.
# ----------------------------------------------------------------------------
add_library(heat_dsp STATIC
    Source/DSP/GainComputer.cpp
    Source/DSP/Ballistics.cpp
    Source/DSP/SidechainFilter.cpp
    Source/DSP/CompressMacro.cpp
    Source/DSP/CompressorEngine.cpp
    Source/DSP/HeatEngine.cpp
    Source/DSP/Multiband.cpp
    Source/DSP/TruePeakLimiter.cpp
    Source/Modes/ModeProfiles.cpp
    Source/Nonlinear/TubeStage.cpp
    Source/Nonlinear/IronStage.cpp
    Source/Quality/Oversampler.cpp)
target_include_directories(heat_dsp PUBLIC Source)
target_compile_features(heat_dsp PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(heat_dsp PRIVATE /W4)
else()
    target_compile_options(heat_dsp PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion)
endif()

# ----------------------------------------------------------------------------
# Plugin
# ----------------------------------------------------------------------------
set(HEAT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND HEAT_FORMATS AU)
endif()

juce_add_binary_data(HeatBinaryData
    NAMESPACE HeatBinary
    SOURCES
        Resources/Fonts/Inter-Light.ttf
        Resources/Fonts/Inter-Regular.ttf
        Resources/Fonts/Inter-Medium.ttf
        Resources/Fonts/Inter-SemiBold.ttf)

set(HEAT_PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Core/Parameters.cpp
    Source/Core/PluginState.cpp
    Source/Core/PresetManager.cpp
    Source/Core/FactoryPresets.cpp
    Source/UI/HeatTheme.cpp
    Source/UI/HeatLookAndFeel.cpp
    Source/UI/HeatKnob.cpp
    Source/UI/GainReductionDisplay.cpp
    Source/UI/SegmentedSelector.cpp
    Source/UI/Header.cpp
    Source/UI/AdvancedPanel.cpp
    Source/UI/AdvancedSlider.cpp
    Source/UI/GpuMeter.cpp
    Source/UI/Icons.cpp
    Source/Graphics/SilverSurface.cpp)

set(HEAT_JUCE_MODULES
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_opengl)

set(HEAT_JUCE_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

# Common configuration for every target that compiles the plugin sources
# (the plugin itself, the plugin-level tests and the snapshot renderer).
function(heat_configure_juce_target target)
    target_sources(${target} PRIVATE ${HEAT_PLUGIN_SOURCES})
    target_include_directories(${target} PRIVATE Source)
    target_compile_definitions(${target} PUBLIC ${HEAT_JUCE_DEFINITIONS} HEAT_VERSION_STRING="${PROJECT_VERSION}")
    target_link_libraries(${target}
        PRIVATE
            heat_dsp
            HeatBinaryData
            ${HEAT_JUCE_MODULES}
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

juce_add_plugin(HEAT
    PRODUCT_NAME "HEAT"
    COMPANY_NAME "Nova Audio"
    COMPANY_WEBSITE "https://novaaudio.example"
    BUNDLE_ID com.novaaudio.heat
    PLUGIN_MANUFACTURER_CODE Nova
    PLUGIN_CODE Het2
    DESCRIPTION "HEAT Analog Dynamics Processor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS ${HEAT_FORMATS})

heat_configure_juce_target(HEAT)

# ----------------------------------------------------------------------------
# Tests, measurement and tooling
# ----------------------------------------------------------------------------
if(HEAT_BUILD_TESTS)
    enable_testing()

    # Pure-DSP tests: fast, JUCE-free, sanitizer friendly.
    add_executable(heat_dsp_tests
        Tests/TestMain.cpp
        Tests/TestRunner.cpp
        Tests/TestSignals.cpp
        Tests/GainComputerTests.cpp
        Tests/CompressMacroTests.cpp
        Tests/DetectorTests.cpp
        Tests/BallisticsTests.cpp
        Tests/StereoTests.cpp
        Tests/SidechainTests.cpp
        Tests/MixAlignmentTests.cpp
        Tests/EngineTests.cpp
        Tests/StageHarness.cpp
        Tests/NonlinearTests.cpp
        Tests/AliasingTests.cpp
        Tests/LookaheadTests.cpp
        Tests/MidSideTests.cpp
        Tests/SidechainEqTests.cpp
        Tests/LimiterTests.cpp
        Tests/MultibandTests.cpp
        Tests/HysteresisTests.cpp
        Tests/FeatureMatrixTests.cpp)
    target_link_libraries(heat_dsp_tests PRIVATE heat_dsp)
    add_test(NAME heat_dsp_tests COMMAND heat_dsp_tests)

    # Measurement generator: CSV + WAV output for the quality gates, CPU profile.
    add_executable(heat_measure Tools/HeatMeasure.cpp Tests/TestSignals.cpp Tests/StageHarness.cpp)
    target_include_directories(heat_measure PRIVATE Tests)
    target_link_libraries(heat_measure PRIVATE heat_dsp)

    # Plugin-level tests: parameters, state, presets, A/B, automation fuzz.
    juce_add_console_app(heat_plugin_tests PRODUCT_NAME "HEAT Plugin Tests")
    target_sources(heat_plugin_tests PRIVATE
        Tests/PluginTestMain.cpp
        Tests/TestRunner.cpp
        Tests/TestSignals.cpp
        Tests/StateTests.cpp
        Tests/AutomationTests.cpp
        Tests/RealtimeTests.cpp)
    target_include_directories(heat_plugin_tests PRIVATE Tests)
    heat_configure_juce_target(heat_plugin_tests)
    add_test(NAME heat_plugin_tests COMMAND heat_plugin_tests)

endif()

if(HEAT_BUILD_TOOLS)
    juce_add_console_app(heat_snapshot PRODUCT_NAME "HEAT Snapshot")
    target_sources(heat_snapshot PRIVATE Tools/HeatSnapshot.cpp)
    heat_configure_juce_target(heat_snapshot)

    # GPU meter verification: renders the OpenGL meter in a real window and
    # compares its read-back pixels with the CPU meter.
    juce_add_gui_app(heat_gpucheck PRODUCT_NAME "heat_gpucheck")
    target_sources(heat_gpucheck PRIVATE Tools/HeatGpuCheck.cpp)
    heat_configure_juce_target(heat_gpucheck)
endif()
