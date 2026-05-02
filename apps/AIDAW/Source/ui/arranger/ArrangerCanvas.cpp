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
    setInterceptsMouseClicks(false, false);
}

void ArrangerCanvas::paint(juce::Graphics& g)
{
    // Restore original transparent background so Composer visuals aren't changed
    g.setColour(juce::Colours::transparentBlack);
    g.fillAll();

    const int H      = getHeight();
    const int W      = getWidth();
    const int gridX0 = TrackLaneComponent::headerWidth;
    const double ppb = pixelsPerBeatRef;
    const int totalBeats = (int) std::ceil((W - gridX0) / ppb);

    // Left header divider (unchanged)
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.fillRect(gridX0, 0, 1, H);

    // --- Bars & beats (no ruler background; no zebra rows) ---
    for (int beat = 0; beat <= totalBeats; ++beat)
    {
        const int x = gridX0 + (int) std::round(beat * ppb);
        const bool isBar = (beat % 4) == 0;

        g.setColour(juce::Colour(isBar ? Theme::colGridBar : Theme::colGridBeat));
        g.fillRect(x, rulerH, 1, H - rulerH);

        if (isBar)
        {
            g.setColour(juce::Colour(Theme::colBarTick));
            g.fillRect(x, 0, 1, rulerH);

            g.setColour(juce::Colour(Theme::colBarLabel));
            g.setFont(11.0f);
            g.drawFittedText(juce::String((beat / 4) + 1),
                             x + 4, 2, 30, rulerH - 4,
                             juce::Justification::centredLeft, 1);
        }
    }

    // Subdivisions (unchanged)
    const int subDivs = Theme::subDivisions(ppb);
    if (subDivs > 0)
    {
        g.setColour(juce::Colour(Theme::colGridSub));
        for (int beat = 0; beat <= totalBeats; ++beat)
            for (int s = 1; s < subDivs; ++s)
            {
                const double frac = (double) s / (double) subDivs;
                const int xs = gridX0 + (int) std::round((beat + frac) * ppb);
                g.fillRect(xs, rulerH, 1, H - rulerH);
            }
    }

    // Playhead (unchanged)
    const int xPH = xFromBeats(playheadBeatsRef);
    g.setColour(juce::Colour(Theme::colPlayhead));
    g.fillRect(xPH - 1, 0, 2, H);

    // Marquee uses themed colours (keeps the nice blue)
    if (selectionRect)
    {
        auto r = selectionRect.value();
        g.setColour(juce::Colour(Theme::colSelect));   g.fillRect(r);
        g.setColour(juce::Colour(Theme::colSelectBd)); g.drawRect(r, 1);
    }
}
