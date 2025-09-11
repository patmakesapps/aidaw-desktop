#pragma once
#include <JuceHeader.h>
#include <optional>
#include "Models.h"
#include "TrackLaneComponent.h"

class ArrangerCanvas : public juce::Component
{
public:
    ArrangerCanvas(std::vector<TrackModel>& tracks,
                   double& bpm,
                   bool& snap,
                   double& pxPerBeat,
                   double& playheadBeatsRefIn,
                   std::function<void()> notifyChanged);

    void paint(juce::Graphics&) override;

    int    xFromBeats(double beats) const;
    double beatsFromX(int x) const;

    // Allow Arranger to show/hide the marquee (content coords)
    void setSelectionRect(std::optional<juce::Rectangle<int>> r)
    {
        selectionRect = std::move(r);
        repaint();
    }

private:
    std::vector<TrackModel>& tracks;
    double& bpmRef;
    bool&   snapToGridRef;
    double& pixelsPerBeatRef;
    double& playheadBeatsRef;
    std::function<void()> notifyChanged;

    std::optional<juce::Rectangle<int>> selectionRect;

    static constexpr int rulerH = 20;
};
