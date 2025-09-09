#include "ArrangerCanvas.h"
#include "TrackLaneComponent.h" // <-- REQUIRED for TrackLaneComponent::headerWidth

ArrangerCanvas::ArrangerCanvas(std::vector<TrackModel>& t,
                               double& bpm,
                               bool& snap,
                               double& pxPerBeat,
                               double& playheadBeatsRefIn,
                               std::function<void()> notifyChangedIn)
    : tracks(t),
      bpmRef(bpm),
      snapToGridRef(snap),
      pixelsPerBeatRef(pxPerBeat),
      playheadBeatsRef(playheadBeatsRefIn),
      notifyChanged(std::move(notifyChangedIn))
{
    setInterceptsMouseClicks(true, true);

    addAndMakeVisible(addTrackButton);
    addTrackButton.setTooltip("Add track");
    addTrackButton.setWantsKeyboardFocus(false);
    addTrackButton.onClick = [this] { addTrack(); };
    addTrackButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1C1F26));
    addTrackButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.92f));

    ensureDefaultTracks(8);        // default 8 tracks on empty sessions
    setSize(getWidth(), computeContentHeight());
}

void ArrangerCanvas::ensureDefaultTracks(int n)
{
    if (!tracks.empty()) return;
    for (int i = 0; i < n; ++i) addTrack();
}

void ArrangerCanvas::addTrack()
{
    TrackModel m;
    m.name = juce::String("Track ") + juce::String((int)tracks.size() + 1);
    tracks.emplace_back(std::move(m));

    if (notifyChanged) notifyChanged();

    setSize(getWidth(), computeContentHeight());
    resized();
}

int ArrangerCanvas::computeContentHeight() const
{
    const int lanesH = (int)tracks.size() * (laneHeight + laneGap);
    const int btnH   = 28;
    return rulerH + lanesH + btnH + bottomPad;
}

void ArrangerCanvas::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0B0B0B));

    // Divider between lane headers and grid
    g.setColour(juce::Colour(0x22FFFFFF));
    g.fillRect(TrackLaneComponent::headerWidth, 0, 1, getHeight());

    const int gridX0 = TrackLaneComponent::headerWidth;
    const double ppb = pixelsPerBeatRef;
    const int totalBeats = (int) std::ceil((getWidth() - gridX0) / ppb);

    // Bars & beats
    for (int beat = 0; beat <= totalBeats; ++beat)
    {
        const int x = gridX0 + (int) std::round(beat * ppb);
        const bool isBar = (beat % 4) == 0;

        g.setColour(isBar ? juce::Colour(0x33FFFFFF) : juce::Colour(0x12FFFFFF));
        g.fillRect(x, rulerH, 1, getHeight() - rulerH);

        if (isBar)
        {
            g.setColour(juce::Colour(0x44FFFFFF));
            g.fillRect(x, 0, 1, rulerH);
            g.setColour(juce::Colour(0x66FFFFFF));
            g.setFont(11.0f);
            const int barIdx = (beat / 4) + 1;
            g.drawFittedText(juce::String(barIdx), x + 4, 2, 30, rulerH - 4,
                             juce::Justification::centredLeft, 1);
        }
    }

    // Sub-beat grid
    int subDivs = 0;
    if      (ppb >= 320.0) subDivs = 16;
    else if (ppb >= 200.0) subDivs = 8;
    else if (ppb >= 120.0) subDivs = 4;
    else if (ppb >= 80.0)  subDivs = 2;

    if (subDivs > 0)
    {
        g.setColour(juce::Colour(0x0EFFFFFF));
        for (int beat = 0; beat <= totalBeats; ++beat)
            for (int s = 1; s < subDivs; ++s)
            {
                const double frac = (double)s / (double)subDivs;
                const int x = gridX0 + (int) std::round((beat + frac) * ppb);
                g.fillRect(x, rulerH, 1, getHeight() - rulerH);
            }
    }

    // Playhead
    const int xPH = xFromBeats(playheadBeatsRef);
    g.setColour(juce::Colour(0xFF3B82F6));
    g.fillRect(xPH - 1, 0, 2, getHeight());
}

void ArrangerCanvas::resized()
{
    const int btnW = 36, btnH = 28;
    const int lanesH = (int)tracks.size() * (laneHeight + laneGap);
    const int y = rulerH + lanesH + 6;

    addTrackButton.setBounds(juce::jmax(8, getWidth() / 2 - btnW / 2), y, btnW, btnH);

    const int desiredH = computeContentHeight();
    if (desiredH != getHeight())
        setSize(getWidth(), desiredH);
}

int ArrangerCanvas::xFromBeats(double beats) const
{
    return TrackLaneComponent::headerWidth
           + (int) std::round(beats * pixelsPerBeatRef);
}

double ArrangerCanvas::beatsFromX(int x) const
{
    return juce::jmax(0, x - TrackLaneComponent::headerWidth) / pixelsPerBeatRef;
}
