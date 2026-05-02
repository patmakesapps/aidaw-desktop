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
    apps/AIDAW/Source/app/MainComponent.h
    apps/AIDAW/Source/app/MainComponent.cpp
    apps/AIDAW/Source/app/MainWindow.h
    apps/AIDAW/Source/app/MainWindow.cpp

    # ---- Audio Engine ----
    apps/AIDAW/Source/audio/MetronomeSource.h
    apps/AIDAW/Source/audio/MetronomeSource.cpp
    apps/AIDAW/Source/audio/TimelineAudioSource.h
    apps/AIDAW/Source/audio/TimelineAudioSource.cpp
    apps/AIDAW/Source/audio/EddieSynth.h
    apps/AIDAW/Source/audio/EddieSynth.cpp

    # ---- UI Shell ----
    apps/AIDAW/Source/ui/shell/TopBar.h
    apps/AIDAW/Source/ui/shell/TopBar.cpp

    # ---- Shared UI Models/Theme ----
    apps/AIDAW/Source/ui/shared/Models.h
    apps/AIDAW/Source/ui/shared/Theme.h
    apps/AIDAW/Source/ui/shared/ThemeManager.h
    apps/AIDAW/Source/ui/shared/AIDAWLook.h
    apps/AIDAW/Source/ui/shared/Icons.h
    apps/AIDAW/Source/ui/shared/ThumbCache.h

    # ---- Arranger ----
    apps/AIDAW/Source/ui/arranger/ClipComponent.h
    apps/AIDAW/Source/ui/arranger/ClipComponent.cpp
    apps/AIDAW/Source/ui/arranger/ArrangerFileUtils.h
    apps/AIDAW/Source/ui/arranger/ArrangerFileUtils.cpp
    apps/AIDAW/Source/ui/arranger/TrackLaneComponent.h
    apps/AIDAW/Source/ui/arranger/TrackLaneComponent.cpp
    apps/AIDAW/Source/ui/arranger/ArrangerCanvas.h
    apps/AIDAW/Source/ui/arranger/ArrangerCanvas.cpp
    apps/AIDAW/Source/ui/arranger/Arranger.h
    apps/AIDAW/Source/ui/arranger/Arranger.cpp

    # ---- MIDI Editor ----
    apps/AIDAW/Source/ui/midi/MidiNote.h
    apps/AIDAW/Source/ui/midi/MidiEditor.h
    apps/AIDAW/Source/ui/midi/MidiEditor.cpp
    apps/AIDAW/Source/ui/midi/MidiEditorNoteComponent.cpp

    # ---- Synth UI ----
    apps/AIDAW/Source/ui/synth/EddieSynthPanel.h
    apps/AIDAW/Source/ui/synth/EddieSynthPanel.cpp

    # ---- Loops ----
    apps/AIDAW/Source/ui/loops/LoopsRegistry.h
    apps/AIDAW/Source/ui/loops/LoopsModal.h

    # ---- Mixer ----
    apps/AIDAW/Source/ui/mixer/MixerComponent.h
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
