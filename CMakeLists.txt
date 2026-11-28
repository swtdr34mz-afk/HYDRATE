# HYDRATE — the plugin (VST3) and the standalone app, with JUCE 8.
#   cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
# JUCE comes from GitHub (or -DHYDRATE_JUCE_DIR=/path/to/JUCE).
cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(HYDRATE VERSION 1.3.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HYDRATE_JUCE_DIR "" CACHE PATH "a JUCE checkout (empty: from GitHub)")
if(HYDRATE_JUCE_DIR)
    add_subdirectory(${HYDRATE_JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

set(HYDRATE_DSP dsp/Engine.cpp dsp/Params.cpp dsp/Presets.cpp dsp/Waves.cpp dsp/Fx.cpp dsp/Phrases.cpp)

option(HYDRATE_PLUGIN "build the plugin" ON)
if(HYDRATE_PLUGIN)
    set(HYDRATE_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND HYDRATE_FORMATS AU)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND HYDRATE_FORMATS LV2)
    endif()
    juce_add_plugin(Hydrate
        PRODUCT_NAME "HYDRATE"
        COMPANY_NAME "swtdr34mz"
        BUNDLE_ID "com.swtdr34mz.hydrate"
        LV2URI "https://github.com/swtdr34mz-afk/HYDRATE"
        PLUGIN_MANUFACTURER_CODE Swtd
        PLUGIN_CODE Hydr
        FORMATS ${HYDRATE_FORMATS}
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        MICROPHONE_PERMISSION_ENABLED FALSE)
    target_sources(Hydrate PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp src/ui/Look.cpp ${HYDRATE_DSP})
    target_include_directories(Hydrate PRIVATE dsp src)
    target_compile_definitions(Hydrate PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(Hydrate PRIVATE
        HydrateData
        juce::juce_audio_utils
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()

juce_add_binary_data(HydrateData SOURCES
    fonts/FiraSans-SemiBold.ttf
    fonts/FiraSans-Bold.ttf
    fonts/FiraSans-ExtraBold.ttf
    fonts/AnonymousPro-Bold.ttf
    res/swtdr34mz-tag.png)

# a picture of the editor, without a window (tools/snapshot.cpp): -DHYDRATE_SNAPSHOT=ON
option(HYDRATE_SNAPSHOT "build HydrateShot, the editor's picture" OFF)
if(HYDRATE_SNAPSHOT)
    juce_add_console_app(HydrateShot PRODUCT_NAME "HydrateShot")
    target_sources(HydrateShot PRIVATE tools/snapshot.cpp src/PluginProcessor.cpp src/PluginEditor.cpp src/ui/Look.cpp ${HYDRATE_DSP})
    target_include_directories(HydrateShot PRIVATE dsp src)
    target_compile_definitions(HydrateShot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1
        JucePlugin_Build_Standalone=0 JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XSHM=0
        JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0)
    target_link_libraries(HydrateShot PRIVATE HydrateData juce::juce_audio_utils juce::juce_recommended_config_flags)
    # the MPC skin's renderer (mpc/skin/render_skin.cpp): each MPC page with the VST3's art, every control at every value
    juce_add_console_app(HydrateSkin PRODUCT_NAME "HydrateSkin")
    target_sources(HydrateSkin PRIVATE mpc/skin/render_skin.cpp src/PluginProcessor.cpp src/PluginEditor.cpp src/ui/Look.cpp ${HYDRATE_DSP})
    target_include_directories(HydrateSkin PRIVATE dsp src)
    target_compile_definitions(HydrateSkin PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1
        JucePlugin_Build_Standalone=0 JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XSHM=0
        JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0)
    target_link_libraries(HydrateSkin PRIVATE HydrateData juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
