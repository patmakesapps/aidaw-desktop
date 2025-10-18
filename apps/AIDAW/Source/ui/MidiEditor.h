#pragma once
#include <JuceHeader.h>
#include "Models.h"

// A very lightweight piano-roll style view (grid + a few demo notes).
// Keeps scope small: draw-only with zoom; no editing yet.
// You can expand later to full note editing/velocity lanes.

struct MidiNote
{
    int    pitch = 60;           // MIDI note number (C4 = 60)
    double startBeats = 0.0;     // where it starts
    double lengthBeats = 1.0;    // duration in beats
    int    velocity = 100;       // 1..127 (for future use)
};

class MidiEditor : public juce::Component
{
public:
    MidiEditor()
    {
        setOpaque(true);
        setWantsKeyboardFocus(true);

        // simple demo phrase
        demo.reserve(16);
        int seq[16] = {60, 62, 64, 65, 67, 69, 71, 72, 71, 69, 67, 65, 64, 62, 60, 55};
        for (int i = 0; i < 16; ++i)
        {
            MidiNote n;
            n.pitch = seq[i];
            n.startBeats = (double)i * 0.5; // eighths
            n.lengthBeats = 0.45;
            demo.push_back(n);
        }
    }

    // UI API
    void setBPM(double bpm) { bpmValue = juce::jlimit(40.0, 300.0, bpm); repaint(); }
    void setZoomBeatsPerScreen(double beats) // optional external hook
    {
        beatsPerScreen = juce::jlimit(4.0, 64.0, beats);
        repaint();
    }
    // Ctrl + mouse wheel handling (mirrors Arranger helper naming)
    void zoomDeltaFromWheel(double delta, int /*screenXUnused*/)
    {
        const double step = (delta > 0 ? -1.0 : +1.0);
        beatsPerScreen = juce::jlimit(4.0, 64.0, beatsPerScreen + step);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF0B0E12));

        const int leftKeyWidth = 56;

        // FIX: this must be mutable because removeFromLeft mutates the rect
        auto area = getLocalBounds();                // content area
        auto keys = area.removeFromLeft(leftKeyWidth); // carve out keys

        // Keys area
        drawPianoKeys(g, keys);

        // Grid area
        drawGrid(g, area, leftKeyWidth);

        // Notes
        drawNotes(g, area, leftKeyWidth);
    }

    void resized() override {}

private:
    // ----- helpers -----
    double bpmValue { 120.0 };
    double beatsPerScreen { 16.0 }; // horizontal zoom
    int    rows { 28 };             // number of visible semitone rows

    std::vector<MidiNote> demo;

    int pitchToRow(int pitch) const
    {
        // Center around C4..G6
        const int topPitch = 84;   // C6
        const int bottomPitch = topPitch - rows + 1;
        return juce::jlimit(0, rows - 1, topPitch - juce::jlimit(bottomPitch, topPitch, pitch));
    }

    double pxPerBeat(const juce::Rectangle<int>& grid) const
    {
        return (double) grid.getWidth() / beatsPerScreen;
    }

    void drawPianoKeys(juce::Graphics& g, juce::Rectangle<int> r)
    {
        const int keyH = juce::jmax(12, r.getHeight() / rows);
        const int topPitch = 84;
        auto keyRect = juce::Rectangle<int>(r.getX(), r.getY(), r.getWidth(), keyH);

        for (int i = 0; i < rows; ++i)
        {
            const int pitch = topPitch - i;
            const int pc = pitch % 12;
            const bool isBlack = (pc==1||pc==3||pc==6||pc==8||pc==10);

            g.setColour(isBlack ? juce::Colour(0xFF121418) : juce::Colour(0xFF171B21));
            g.fillRect(keyRect);

            // key labels for Cs
            if (pc == 0)
            {
                g.setColour(juce::Colour(0x66FFFFFF));
                g.setFont(11.0f);
                const int octave = (pitch / 12) - 1;
                g.drawText("C" + juce::String(octave), keyRect.reduced(6, 0), juce::Justification::centredLeft);
            }

            g.setColour(juce::Colour(0x222B3340));
            g.fillRect(keyRect.removeFromBottom(1));

            keyRect.translate(0, keyH);
        }

        // right border
        g.setColour(juce::Colour(0x223B82F6));
        g.fillRect(r.removeFromRight(1));
    }

    void drawGrid(juce::Graphics& g, juce::Rectangle<int> grid, int /*leftKeyWidth*/)
    {
        // background
        g.setColour(juce::Colour(0xFF0E1218));
        g.fillRect(grid);

        const double ppb = pxPerBeat(grid);
        const int totalBeats = juce::jmax(1, (int) std::ceil((double)grid.getWidth() / ppb));

        // horizontal rows
        const int rowH = juce::jmax(12, grid.getHeight() / rows);
        auto rowRect = juce::Rectangle<int>(grid.getX(), grid.getY(), grid.getWidth(), rowH);
        for (int r = 0; r < rows; ++r)
        {
            const bool cStripe = ((r / 12) % 2) == 0;
            g.setColour(cStripe ? juce::Colour(0xFF0E131B) : juce::Colour(0xFF10151D));
            g.fillRect(rowRect);
            rowRect.translate(0, rowH);
        }

        // vertical beats/bars
        for (int beat = 0; beat <= totalBeats; ++beat)
        {
            const int x = grid.getX() + (int) std::round(beat * ppb);
            const bool isBar = (beat % 4) == 0;

            g.setColour(isBar ? juce::Colour(0x334A7FFF) : juce::Colour(0x183C495A));
            g.fillRect(x, grid.getY(), 1, getHeight());

            if (isBar)
            {
                g.setColour(juce::Colour(0x4A7FFF));
                g.setFont(11.0f);
                g.drawText(juce::String((beat / 4) + 1), x + 4, grid.getY() + 2, 28, 14, juce::Justification::centredLeft);
            }
        }
    }

    void drawNotes(juce::Graphics& g, juce::Rectangle<int> grid, int /*leftKeyWidth*/)
    {
        const double ppb = pxPerBeat(grid);
        const int rowH = juce::jmax(12, grid.getHeight() / rows);

        for (size_t i = 0; i < demo.size(); ++i)
        {
            const auto& n = demo[i];
            const int row  = pitchToRow(n.pitch);
            const int x    = grid.getX() + (int) std::round(n.startBeats * ppb);
            const int y    = grid.getY() + row * rowH + 3;
            const int w    = juce::jmax(6, (int) std::round(n.lengthBeats * ppb));
            const int h    = rowH - 6;

            const bool alt = (i % 2) == 0;
            const auto body = juce::Colour(alt ? 0xFF2C72FA : 0xFF7A3BFF);
            const auto bodyDark = body.darker(0.35f);

            juce::Rectangle<float> r((float)x, (float)y, (float)w, (float)h);
            g.setColour(bodyDark); g.fillRoundedRectangle(r, 6.0f);
            g.setColour(body);     g.drawRoundedRectangle(r, 6.0f, 2.0f);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditor)
};
