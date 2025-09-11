#include "ArrangerCanvas.h"

ArrangerCanvas::ArrangerCanvas(std::vector<TrackModel>& t,
                               double& bpm,
                               bool& snap,
                               double& pxPerBeat,
                               double& playheadBeatsRefIn,
                               std::function<void()> notifyChangedIn)
    : tracks(t), bpmRef(bpm), snapToGridRef(snap),
      pixelsPerBeatRef(pxPerBeat), playheadBeatsRef(playheadBeatsRefIn),
      notifyChanged(std::move(notifyChangedIn))
{
    // Overlay should not block clicks to lanes/clips beneath it
    setInterceptsMouseClicks(false, false);
}

void ArrangerCanvas::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::transparentBlack);
    g.fillAll();

    // Header divider
    g.setColour(juce::Colour(0x22FFFFFF));
    g.fillRect(TrackLaneComponent::headerWidth, 0, 1, getHeight());

    const int gridX0 = TrackLaneComponent::headerWidth;
    const double ppb = pixelsPerBeatRef;
    const int totalBeats = (int)std::ceil((getWidth() - gridX0) / ppb);

    // Bars & beats
    for (int beat = 0; beat <= totalBeats; ++beat)
    {
        const int x = gridX0 + (int)std::round(beat * ppb);
        const bool isBar = (beat % 4) == 0;

        g.setColour(isBar ? juce::Colour(0x44FFFFFF) : juce::Colour(0x18FFFFFF));
        g.fillRect(x, rulerH, 1, getHeight() - rulerH);

        if (isBar)
        {
            g.setColour(juce::Colour(0x66FFFFFF));
            g.fillRect(x, 0, 1, rulerH);
            g.setColour(juce::Colour(0x88FFFFFF));
            g.setFont(11.0f);
            g.drawFittedText(juce::String((beat / 4) + 1), x + 4, 2, 30, rulerH-4,
                             juce::Justification::centredLeft, 1);
        }
    }

    // Subdivisions (denser when zoomed)
    int subDivs = 0;
    if      (ppb >= 320.0) subDivs = 16;
    else if (ppb >= 200.0) subDivs = 8;
    else if (ppb >= 120.0) subDivs = 4;
    else if (ppb >= 80.0)  subDivs = 2;

    if (subDivs > 0)
    {
        g.setColour(juce::Colour(0x11FFFFFF));
        for (int beat = 0; beat <= totalBeats; ++beat)
            for (int s = 1; s < subDivs; ++s)
            {
                const double frac = (double)s / (double)subDivs;
                const int x = gridX0 + (int)std::round((beat + frac) * ppb);
                g.fillRect(x, rulerH, 1, getHeight() - rulerH);
            }
    }

    // Playhead
    const int xPH = xFromBeats(playheadBeatsRef);
    g.setColour(juce::Colour(0xFF3B82F6));
    g.fillRect(xPH - 1, 0, 2, getHeight());

    // Marquee
    if (selectionRect)
    {
        auto r = selectionRect.value();
        juce::Colour fill   = juce::Colour(0x3348A3FF);
        juce::Colour border = juce::Colour(0xFF48A3FF);
        g.setColour(fill);
        g.fillRect(r);
        g.setColour(border);
        g.drawRect(r, 1);
    }
}

int ArrangerCanvas::xFromBeats(double beats) const
{
    return TrackLaneComponent::headerWidth + (int)std::round(beats * pixelsPerBeatRef);
}

double ArrangerCanvas::beatsFromX(int x) const
{
    return juce::jmax(0, x - TrackLaneComponent::headerWidth) / pixelsPerBeatRef;
}
