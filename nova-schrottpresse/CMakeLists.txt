cmake_minimum_required(VERSION 3.22)

project(SCHROTTPRESSE VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimale macOS-Version")

option(SP_COPY_AFTER_BUILD "Plugins nach dem Bauen in ~/Library/Audio/Plug-Ins kopieren" OFF)
option(SP_BUILD_TESTS "Selbsttest bauen" ON)
option(SP_LINUX_HEADLESS "Nur für Testbuilds auf Linux-Servern ohne Audio- und X11-Pakete" OFF)

# JUCE wird beim ersten Konfigurieren automatisch von GitHub geladen
set(SP_JUCE_TAG "8.0.16-security" CACHE STRING "JUCE-Version (Git-Tag)")
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${SP_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(SP_FORMATS AU VST3 Standalone)
else()
    set(SP_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Schrottpresse
    PRODUCT_NAME              "Schrottpresse"
    COMPANY_NAME              "NOVA"
    BUNDLE_ID                 "com.nova.schrottpresse"
    PLUGIN_MANUFACTURER_CODE  Nova
    PLUGIN_CODE               Schr
    FORMATS                   ${SP_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   ${SP_COPY_AFTER_BUILD}
    AU_MAIN_TYPE              kAudioUnitType_Effect
    VST3_CATEGORIES           Fx Distortion
    VERSION                   ${PROJECT_VERSION})

# Schriften (SIL Open Font License, siehe fonts/)
juce_add_binary_data(SchrottFonts
    HEADER_NAME BinaryData.h
    NAMESPACE   BinaryData
    SOURCES     fonts/Stencil.ttf fonts/PlexCondSemi.ttf fonts/PlexCondMed.ttf fonts/PlexMono.ttf)
set_target_properties(SchrottFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SP_SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Engine.cpp)
set(SP_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
set(SP_HEADLESS_DEFS JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0)

target_sources(Schrottpresse PRIVATE ${SP_SOURCES})
target_include_directories(Schrottpresse PRIVATE Source)
target_compile_definitions(Schrottpresse PUBLIC ${SP_DEFS})
if(SP_LINUX_HEADLESS)
    target_compile_definitions(Schrottpresse PUBLIC ${SP_HEADLESS_DEFS})
endif()
target_link_libraries(Schrottpresse
    PRIVATE
        SchrottFonts
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Selbsttest: Null-Test, alle Presets, Rechenzeit, Speichern/Laden
if(SP_BUILD_TESTS)
    juce_add_console_app(SchrottTest PRODUCT_NAME "SchrottTest")
    target_sources(SchrottTest PRIVATE tests/schrott_test.cpp ${SP_SOURCES})
    target_include_directories(SchrottTest PRIVATE Source)
    target_compile_definitions(SchrottTest PRIVATE ${SP_DEFS})
    if(SP_LINUX_HEADLESS)
        target_compile_definitions(SchrottTest PRIVATE ${SP_HEADLESS_DEFS})
    endif()
    target_link_libraries(SchrottTest PRIVATE SchrottFonts juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    enable_testing()
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/render)
    add_test(NAME selbsttest COMMAND SchrottTest ${CMAKE_BINARY_DIR}/render)
endif()
