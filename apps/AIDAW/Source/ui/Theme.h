#pragma once
#include <JuceHeader.h>

/** Centralized theme tokens so Arranger, MIDI, Mixer can match.
    Tune once and all editors update. */
namespace Theme
{
    // Surfaces
    static constexpr auto bg0 = 0xFFF3F5F8;  // page
    static constexpr auto bg1 = 0xFFE9EDF2;  // panel
    static constexpr auto bg2 = 0xFFDCE3EB;  // inset lanes

    // Strokes / grid
    static constexpr auto gridBar    = 0xFF2F6DF6;  // strong bars
    static constexpr auto gridBeat   = 0x332F6DF6;  // beats
    static constexpr auto gridMinor  = 0x1A2B3340;  // 1/2..1/32

    // Text
    static constexpr auto textPrimary   = 0xFF1D232D;
    static constexpr auto textSubtle    = 0xAA1D232D;

    // Note blocks
    static constexpr auto noteA   = 0xFF5B7CFF;
    static constexpr auto noteB   = 0xFF9B5CFF;
    static constexpr float noteRadius = 6.0f;

    // Helpers
    inline juce::Colour C(uint32_t argb) { return juce::Colour(argb); }
}
