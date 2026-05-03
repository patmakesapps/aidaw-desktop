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

    // Adaptive grid density. Frame-all can push beats only a few pixels apart;
    // at that scale draw bars instead of turning the playlist into a comb.
    const double pixPerBar = ppb * 4.0;
    int barLineStride = 1;
    if (pixPerBar < 12.0)
    {
        const int raw = (int) std::ceil(12.0 / juce::jmax(1.0, pixPerBar));
        while (barLineStride < raw) barLineStride <<= 1;
    }

    int barLabelStride = barLineStride;
    if (pixPerBar * barLabelStride < 58.0)
    {
        const int raw = (int) std::ceil(58.0 / juce::jmax(1.0, pixPerBar));
        while (barLabelStride < raw) barLabelStride <<= 1;
    }

    const bool drawBeatLines = ppb >= 10.0;
    if (drawBeatLines)
    {
        g.setColour(juce::Colour(Theme::colGridBeat));
        for (int beat = 0; beat <= totalBeats; ++beat)
        {
            if ((beat % 4) == 0)
                continue;

            const int x = gridX0 + (int) std::round(beat * ppb);
            g.fillRect(x, rulerH, 1, H - rulerH);
        }
    }

    const int totalBars = (int) std::ceil(totalBeats / 4.0);
    for (int bar = 0; bar <= totalBars; bar += barLineStride)
    {
        const int beat = bar * 4;
        const int x = gridX0 + (int) std::round(beat * ppb);

        g.setColour(juce::Colour(Theme::colGridBar));
        g.fillRect(x, rulerH, 1, H - rulerH);

        g.setColour(juce::Colour(Theme::colBarTick));
        g.fillRect(x, 0, 1, rulerH);

        if ((bar % barLabelStride) == 0)
        {
            g.setColour(juce::Colour(Theme::colBarLabel));
            g.setFont(11.0f);
            g.drawFittedText(juce::String(bar + 1),
                             x + 4, 2, 40, rulerH - 4,
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

    if (trackReorderIndicatorY)
    {
        const int y = juce::jlimit(rulerH, H - 1, *trackReorderIndicatorY);
        const int x = TrackLaneComponent::headerWidth;
        const int w = juce::jmax(0, W - x);

        g.setColour(juce::Colour(Theme::colAccent).withAlpha(0.20f));
        g.fillRect(x, y - 4, w, 8);
        g.setColour(juce::Colour(Theme::colAccent));
        g.fillRect(x, y - 1, w, 2);
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
