cmake_minimum_required(VERSION 3.22)

project(BigVoix VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<?$<?CONFIG:Debug>:Debug>")

if (NOT TARGET juce::juce_audio_utils)
    add_subdirectory(JUCE EXCLUDE_FROM_ALL)
endif()

set(BIGVOIX_FORMATS VST3 AU Standalone)

if (DEFINED JUCE_AAX_SDK_PATH AND EXISTS "${JUCE_AAX_SDK_PATH}")
    juce_set_aax_sdk_path("${JUCE_AAX_SDK_PATH}")
    list(APPEND BIGVOIX_FORMATS AAX)
endif()

juce_add_plugin(BigVoix
    COMPANY_NAME "BigAudio"
    COMPANY_COPYRIGHT "Copyright (c) 2026 BigAudio"
    PLUGIN_MANUFACTURER_CODE BgAu
    PLUGIN_CODE BgVx
    FORMATS ${BIGVOIX_FORMATS}
    PRODUCT_NAME "BigVoix"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Fx" "Dynamics" "EQ" "Reverb" "Delay"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    AAX_CATEGORY "AAX_ePlugInCategory_Dynamics"
)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/clap-juce-extensions/CMakeLists.txt")
    add_subdirectory(clap-juce-extensions EXCLUDE_FROM_ALL)
    clap_juce_extensions_plugin(
        TARGET BigVoix
        CLAP_ID "com.bigaudio.bigvoix"
        CLAP_FEATURES "audio-effect" "vocal" "compressor" "equalizer" "reverb" "delay"
    )
endif()

target_sources(BigVoix
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
        Source/DSP/BigVoixCoreDSP.h
        Source/DSP/BigVoixSpaceDSP.h
        Source/GUI/BigVoixLookAndFeel.h
)

target_include_directories(BigVoix
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(BigVoix
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if (MSVC)
    target_compile_options(BigVoix PRIVATE
        $<$<CONFIG:Release>:/O2 /Oi /Ot /arch:AVX2 /fp:fast>
        /W4 /permissive-
    )
else()
    target_compile_options(BigVoix PRIVATE
        $<$<CONFIG:Release>:-O3 -ffast-math -funroll-loops>
        -Wall -Wextra -Wpedantic
    )
endif()

target_link_libraries(BigVoix
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

