cmake_minimum_required(VERSION 3.22)
project(AIDAW VERSION 0.1.0 LANGUAGES CXX)

if (MSVC)
  add_compile_options(/utf-8)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party/juce)

juce_add_gui_app(AIDAW
    PRODUCT_NAME "AIDAW"
)

# ---------- Sources ----------
target_sources(AIDAW PRIVATE
    # App entry
    apps/AIDAW/Source/Main.cpp

    # ---- Arranger ----
    apps/AIDAW/Source/ui/TopBar.cpp
    apps/AIDAW/Source/ui/Models.h
    apps/AIDAW/Source/ui/ThumbCache.h
    apps/AIDAW/Source/ui/ClipComponent.h
    apps/AIDAW/Source/ui/ClipComponent.cpp
    apps/AIDAW/Source/ui/TrackLaneComponent.h
    apps/AIDAW/Source/ui/TrackLaneComponent.cpp
    apps/AIDAW/Source/ui/ArrangerCanvas.h
    apps/AIDAW/Source/ui/ArrangerCanvas.cpp
    apps/AIDAW/Source/ui/Arranger.h
    apps/AIDAW/Source/ui/Arranger.cpp

    # ---- MIDI Editor ----
    apps/AIDAW/Source/ui/MidiEditor.h
    apps/AIDAW/Source/ui/MidiEditor.cpp
    apps/AIDAW/Source/ui/Theme.h
)

target_compile_definitions(AIDAW PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
)

target_link_libraries(AIDAW PRIVATE
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_audio_formats
)

juce_generate_juce_header(AIDAW)
