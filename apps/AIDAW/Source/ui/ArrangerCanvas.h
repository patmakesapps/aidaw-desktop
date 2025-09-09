#pragma once
#include <JuceHeader.h>
#include "Models.h"
#include <functional>

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
    void resized() override;

    int    xFromBeats(double beats) const;
    double beatsFromX(int x) const;

private:
    void ensureDefaultTracks(int n);
    void addTrack();
    int  computeContentHeight() const;

    std::vector<TrackModel>& tracks;
    double& bpmRef;
    bool&   snapToGridRef;
    double& pixelsPerBeatRef;
    double& playheadBeatsRef;
    std::function<void()> notifyChanged;

    juce::TextButton addTrackButton { "+" };

    static constexpr int rulerH     = 20;
    static constexpr int laneHeight = 110;
    static constexpr int laneGap    = 8;
    static constexpr int bottomPad  = 40;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangerCanvas)
};
