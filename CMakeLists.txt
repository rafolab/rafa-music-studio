# RAFA MUSIC STUDIO
# Vlastna nahravacia aplikacia pre Windows postavena na Tracktion Engine + JUCE.
# Licencia: GPLv3 (pozri LICENSE.md)

cmake_minimum_required (VERSION 3.22)

project (RafaMusicStudio VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Na Windows linkujeme runtime staticky, aby .exe fungovalo bez dalsich instalacii
if (MSVC)
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options (/utf-8 /MP /bigobj)
endif()

include (FetchContent)
set (FETCHCONTENT_QUIET OFF)

# ------------------------------------------------------------------------------
# Zavislosti su pevne zafixovane na konkretne verzie (aby sa nic nerozbilo samo).
# JUCE berieme zvlast, pretoze Tracktion Engine ho ma ako SSH submodul.

FetchContent_Declare (juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7ff644f65d7daa1428c34b9738ebb5c449dc9172
    GIT_SHALLOW    FALSE)

FetchContent_Declare (tracktion_engine
    GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
    GIT_TAG        a5e44996f5f08dfa6af5f5d02d900fb3cfa37422
    GIT_SUBMODULES ""
    GIT_SHALLOW    FALSE
    SOURCE_SUBDIR  modules)   # pridame len moduly motora, nie priklady

FetchContent_MakeAvailable (juce tracktion_engine)

# ------------------------------------------------------------------------------
set (TARGET_NAME RafaMusicStudio)

juce_add_gui_app (${TARGET_NAME}
    PRODUCT_NAME "RAFA MUSIC STUDIO"
    COMPANY_NAME "rafolab"
    BUNDLE_ID "com.rafolab.rafamusicstudio"
    VERSION ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Nahravanie gitary"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE)    # obrazovka v HTML cez WebView2 (Windows 11 ho ma vstavany)

juce_generate_juce_header (${TARGET_NAME})

# Obrazovka v HTML (priecinok ui/) sa zabali priamo do .exe
file (GLOB_RECURSE RAFA_UI_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/*")
list (FILTER RAFA_UI_FILES EXCLUDE REGEX "\\.md$")
juce_add_binary_data (RafaUIData NAMESPACE RafaUI HEADER_NAME RafaUIData.h SOURCES ${RAFA_UI_FILES})

target_sources (${TARGET_NAME} PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/TrackListComponent.cpp
    Source/TrackListComponent.h
    Source/Diagnostics.cpp
    Source/Diagnostics.h
    Source/WebBridge.cpp
    Source/TempoLoop.cpp
    Source/Mixer.cpp
    Source/Console.cpp
    Source/RafaLookAndFeel.h
    Source/Texts.h)

target_compile_definitions (${TARGET_NAME} PRIVATE
    JUCE_ASIO=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TARGET_NAME},JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TARGET_NAME},JUCE_VERSION>"
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)

target_link_libraries (${TARGET_NAME} PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
    RafaUIData
    juce::juce_recommended_config_flags)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux sa pouziva len na kontrolu kompilacie, nie na distribuciu
    target_link_libraries (${TARGET_NAME} PRIVATE "-latomic")
    target_compile_definitions (${TARGET_NAME} PRIVATE JUCE_JACK=1)
endif()
