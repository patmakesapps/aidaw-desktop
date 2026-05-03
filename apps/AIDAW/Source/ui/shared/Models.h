#pragma once
#include <JuceHeader.h>
#include <vector>

/*** UTF-8 helper for button glyphs (works on Win/macOS) ***/
static inline juce::String U8(const char* s) {
    return juce::String(juce::CharPointer_UTF8(s));
}
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811
static inline juce::String U8(const char8_t* s) {
    return juce::String(juce::CharPointer_UTF8(reinterpret_cast<const char*>(s)));
}
#endif

struct ClipModel {
    enum class Kind { Audio, MidiLoop };

    juce::String id;
    Kind kind { Kind::Audio };
    double startBeats  = 0.0;   // timeline position
    double lengthBeats = 4.0;   // duration on timeline
    double offsetBeats = 0.0;   // starting offset inside source
    juce::File file;
    uint32 loopId { 0 };
    juce::String label;
    bool showImportSpinner { false }; // transient UI state, not serialized
};

struct TrackModel {
    juce::String id;
    juce::String name = "Audio";
    std::vector<ClipModel> clips;
};

enum class ArrangerTool { Pointer, Slice, Resize, Zoom };
