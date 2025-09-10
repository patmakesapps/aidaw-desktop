#pragma once
#include <JuceHeader.h>
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

private:
    std::vector<TrackModel>& tracks;
    double& bpmRef;
    bool&   snapToGridRef;
    double& pixelsPerBeatRef;
    double& playheadBeatsRef;
    std::function<void()> notifyChanged;

    static constexpr int rulerH = 20;
};
