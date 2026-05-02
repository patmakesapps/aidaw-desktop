#pragma once
#include <JuceHeader.h>

// Runtime palette. Values are mutated by ThemeManager when the user
// switches themes. All consumers continue reading Theme::colX and call
// repaint() in response to ThemeManager change broadcasts.
struct Theme
{
    // ---- Layout (compile-time constants) ----
    static constexpr int rulerH     = 24;
    static constexpr int keyWidth   = 96;
    static constexpr int rowHeight  = 26;
    static constexpr int velocityH  = 70;

    // ---- Backgrounds ----
    inline static juce::uint32 colBgMain   = 0xFF0C1016;
    inline static juce::uint32 colBgPanel  = 0xFF0E141C;
    inline static juce::uint32 colBgRuler  = 0xFF0A0F15;

    // ---- Grid lines ----
    inline static juce::uint32 colHeaderDiv = 0x22344A62;
    inline static juce::uint32 colGridBar   = 0x5538E0FF;
    inline static juce::uint32 colGridBeat  = 0x1A38E0FF;
    inline static juce::uint32 colGridSub   = 0x0D38E0FF;
    inline static juce::uint32 colOctave    = 0x2038E0FF;

    // ---- Ruler ----
    inline static juce::uint32 colBarTick  = 0x66FFFFFF;
    inline static juce::uint32 colBarLabel = 0xD0EAF2FF;

    // ---- MIDI rows ----
    inline static juce::uint32 colRowEven  = 0xFF0F151D;
    inline static juce::uint32 colRowOdd   = 0xFF0B1118;

    // ---- Keys ----
    inline static juce::uint32 colKeyWhite = 0xFF121925;
    inline static juce::uint32 colKeyBlack = 0xFF0A0F15;
    inline static juce::uint32 colKeySep   = 0x29324352;

    // ---- Text ----
    inline static juce::uint32 colText     = 0xE6FFFFFF;
    inline static juce::uint32 colTextDim  = 0x88FFFFFF;

    // ---- Accents ----
    inline static juce::uint32 colAccent   = 0xFF3CE0FF;
    inline static juce::uint32 colAccent2  = 0xFF59F3C3;
    inline static juce::uint32 colPlayhead = 0xFF3CE0FF;
    inline static juce::uint32 colLoop     = 0xFF3CE0FF;
    inline static juce::uint32 colLoopFill = 0x143CE0FF;
    inline static juce::uint32 colSelect   = 0x3329FFC1;
    inline static juce::uint32 colSelectBd = 0x8829FFC1;

    // ---- Notes ----
    inline static juce::uint32 colNoteA       = 0xFF59F3C3;
    inline static juce::uint32 colNoteB       = 0xFF7AA7FF;
    inline static float        noteBorderGain = 0.40f;

    // ---- Velocity lane ----
    inline static juce::uint32 colVelLane  = 0xFF0B1118;

    // ---- Buttons ----
    inline static juce::uint32 colBtnIdle   = 0xFF111923;
    inline static juce::uint32 colBtnHover  = 0xFF182331;
    inline static juce::uint32 colBtnActive = 0xFF0D131B;
    inline static juce::uint32 colBtnText   = 0xE6FFFFFF;
    inline static juce::uint32 colBtnStroke = 0x22FFFFFF;

    // ---- TopBar window chrome ----
    inline static juce::uint32 colChromeTop  = 0xFF0E0E0E;
    inline static juce::uint32 colChromeBot  = 0xFF151515;
    inline static juce::uint32 colPillBg     = 0xFF1C1C1C;
    inline static juce::uint32 colDanger     = 0xFFEF4444;

    // ---- Helpers ----
    static inline int    xFromBeats(double beats, double ppb, int x0) { return x0 + (int)std::round(beats * ppb); }
    static inline double beatsFromX(int x, double ppb, int x0)        { return juce::jmax(0, x - x0) / ppb; }

    static inline int subDivisions(double ppb)
    {
        if (ppb >= 800.0) return 16;  // 1/16 at extreme zoom
        if (ppb >= 360.0) return 8;   // 1/8
        if (ppb >= 250.0) return 4;   // 1/4
        if (ppb >= 160.0) return 2;   // 1/2
        return 0;
    }
    static inline double snapQuantumBeats(double ppb, bool snap)
    {
        if (!snap) return 0.0;
        if (ppb >= 800.0) return 1.0 / 16.0;
        if (ppb >= 360.0) return 1.0 / 8.0;
        if (ppb >= 250.0) return 1.0 / 4.0;
        if (ppb >= 160.0) return 1.0 / 2.0;
        return 1.0; // 1 beat
    }
};
