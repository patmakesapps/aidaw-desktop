#pragma once
#include <JuceHeader.h>
#include "../shared/Models.h"
#include "TrackLaneComponent.h"
#include "../shared/Theme.h"

class ArrangerCanvas : public juce::Component
{
public:
    ArrangerCanvas(std::vector<TrackModel>&,
                   double& bpm,
                   bool& snap,
                   double& pxPerBeat,
                   double& playheadBeatsRefIn,
                   std::function<void()> notifyChangedIn);

    void paint(juce::Graphics&) override;

    // helpers shared with Arranger
    int    xFromBeats(double beats) const { return Theme::xFromBeats(beats, pixelsPerBeatRef, TrackLaneComponent::headerWidth); }
    double beatsFromX(int x) const        { return Theme::beatsFromX(x, pixelsPerBeatRef, TrackLaneComponent::headerWidth);   }

    void setSelectionRect(std::optional<juce::Rectangle<int>> r) { selectionRect = std::move(r); repaint(); }
    static constexpr int rulerH = Theme::rulerH;

    std::vector<TrackModel>& tracks;
    double& bpmRef;
    bool&   snapToGridRef;
    double& pixelsPerBeatRef;
    double& playheadBeatsRef;

    std::function<void()> notifyChanged;

private:
    std::optional<juce::Rectangle<int>> selectionRect;
};
