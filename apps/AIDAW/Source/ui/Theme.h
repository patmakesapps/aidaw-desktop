#pragma once
#include <JuceHeader.h>

// Unified theme for Arranger + MIDI Editor
struct Theme
{
    // ---- Layout ----
    static constexpr int rulerH     = 24;
    static constexpr int keyWidth   = 96;
    static constexpr int rowHeight  = 26;
    static constexpr int velocityH  = 70;

    // ---- Backgrounds ----
    static constexpr uint32 colBgMain    = 0xFF1E2430; // deep slate
    static constexpr uint32 colBgPanel   = 0xFF232B38;
    static constexpr uint32 colBgRuler   = 0xFF202734;

    // ---- Lines / grid
    static constexpr uint32 colHeaderDiv = 0x224A586E; // key-edge divider
    static constexpr uint32 colGridBar   = 0x66FFFFFF; // bar lines
    static constexpr uint32 colGridBeat  = 0x22FFFFFF; // beats
    static constexpr uint32 colGridSub   = 0x10FFFFFF; // subdivisions
    static constexpr uint32 colOctave    = 0x2CFFFFFF; // octave accent (thin)
    static constexpr uint32 colBarTick   = 0x55FFFFFF; // top tick marks in ruler

    // ---- MIDI row zebra (matches arranger darkness) ----
    static constexpr uint32 colRowEven   = 0xFF2A3343;
    static constexpr uint32 colRowOdd    = 0xFF262F3E;

    // ---- Piano keys (dark) ----
    // white-keys become dark grey; black-keys become near black
    static constexpr uint32 colKeyWhite  = 0xFF2B3341; // was light; now dark
    static constexpr uint32 colKeyBlack  = 0xFF1D242E; // deeper for black keys
    static constexpr uint32 colKeySep    = 0x29314252;

    // ---- Text / labels ----
    static constexpr uint32 colText      = 0xE6FFFFFF;
    static constexpr uint32 colBarLabel  = 0xCCFFFFFF;

    // ---- Accents ----
    static constexpr uint32 colPlayhead  = 0xFF4CB8FF;
    static constexpr uint32 colLoop      = 0xFF4CB8FF;
    static constexpr uint32 colLoopFill  = 0x144CB8FF;
    static constexpr uint32 colSelect    = 0x334CB8FF;  // marquee fill
    static constexpr uint32 colSelectBd  = 0x884CB8FF;  // marquee border

    // ---- Notes ----
    static constexpr uint32 colNoteA     = 0xFF6E86FF; // indigo
    static constexpr uint32 colNoteB     = 0xFFB47CFF; // violet
    static constexpr float  noteBorderGain = 0.40f;

    // ---- Velocity lane ----
    static constexpr uint32 colVelLane   = 0xFF242E3B;

    // ---- Buttons (parity with Arranger) ----
    static constexpr uint32 colBtnIdle   = 0xFF232B38;
    static constexpr uint32 colBtnActive = 0xFF1C1F26;
    static constexpr uint32 colBtnText   = 0xE6FFFFFF;

    // ---- Helpers ----
    static inline int    xFromBeats(double beats, double ppb, int x0) { return x0 + (int)std::round(beats * ppb); }
    static inline double beatsFromX(int x, double ppb, int x0)        { return juce::jmax(0, x - x0) / ppb; }

    // Show fewer subs unless zoomed in
    static inline int subDivisions(double ppb)
    {
        if (ppb >= 360.0) return 8;   // 1/8
        if (ppb >= 250.0) return 4;   // 1/4
        if (ppb >= 160.0) return 2;   // 1/2
        return 0;
    }

    static inline double snapQuantumBeats(double ppb, bool snap)
    {
        if (!snap) return 0.0;
        if (ppb >= 360.0) return 1.0 / 8.0;
        if (ppb >= 250.0) return 1.0 / 4.0;
        if (ppb >= 160.0) return 1.0 / 2.0;
        return 1.0; // 1 beat
    }
};
