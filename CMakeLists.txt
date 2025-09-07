cmake_minimum_required(VERSION 3.22)
project(AIDAW VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE submodule
add_subdirectory(third_party/juce)

# Create a minimal GUI app target
juce_add_gui_app(AIDAW
    PRODUCT_NAME "AIDAW"
)

target_sources(AIDAW PRIVATE
    apps/AIDAW/Source/Main.cpp
)

target_compile_definitions(AIDAW PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(AIDAW PRIVATE
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_utils
)

juce_generate_juce_header(AIDAW)
