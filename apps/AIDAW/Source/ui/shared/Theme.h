#pragma once
#include <JuceHeader.h>

// ===== NEO-NOIR AURORA THEME =====
struct Theme
{
    // ---- Layout ----
    static constexpr int rulerH     = 24;
    static constexpr int keyWidth   = 96;
    static constexpr int rowHeight  = 26;
    static constexpr int velocityH  = 70;

    // ---- Backgrounds ----
    static constexpr uint32 colBgMain   = 0xFF0C1016;
    static constexpr uint32 colBgPanel  = 0xFF0E141C;
    static constexpr uint32 colBgRuler  = 0xFF0A0F15;

    // ---- Grid lines ----
    static constexpr uint32 colHeaderDiv = 0x22344A62;
    static constexpr uint32 colGridBar   = 0x5538E0FF;
    static constexpr uint32 colGridBeat  = 0x1A38E0FF;
    static constexpr uint32 colGridSub   = 0x0D38E0FF;
    static constexpr uint32 colOctave    = 0x2038E0FF;

    // ---- Ruler ----
    static constexpr uint32 colBarTick  = 0x66FFFFFF;
    static constexpr uint32 colBarLabel = 0xD0EAF2FF;

    // ---- MIDI rows ----
    static constexpr uint32 colRowEven  = 0xFF0F151D;
    static constexpr uint32 colRowOdd   = 0xFF0B1118;

    // ---- Keys ----
    static constexpr uint32 colKeyWhite = 0xFF121925;
    static constexpr uint32 colKeyBlack = 0xFF0A0F15;
    static constexpr uint32 colKeySep   = 0x29324352;

    // ---- Text ----
    static constexpr uint32 colText     = 0xE6FFFFFF;

    // ---- Accents ----
    static constexpr uint32 colPlayhead = 0xFF3CE0FF;
    static constexpr uint32 colLoop     = 0xFF3CE0FF;
    static constexpr uint32 colLoopFill = 0x143CE0FF;
    static constexpr uint32 colSelect   = 0x3329FFC1;
    static constexpr uint32 colSelectBd = 0x8829FFC1;

    // ---- Notes ----
    static constexpr uint32 colNoteA       = 0xFF59F3C3;
    static constexpr uint32 colNoteB       = 0xFF7AA7FF;
    static constexpr float  noteBorderGain = 0.40f;

    // ---- Velocity lane ----
    static constexpr uint32 colVelLane  = 0xFF0B1118;

    // ---- Buttons ----
    static constexpr uint32 colBtnIdle  = 0xFF111923;
    static constexpr uint32 colBtnActive= 0xFF0D131B;
    static constexpr uint32 colBtnText  = 0xE6FFFFFF;

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
