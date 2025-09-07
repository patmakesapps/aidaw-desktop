#pragma once
#include <JuceHeader.h>
#include "Models.h"

class TrackLaneComponent : public juce::Component
{
public:
    TrackLaneComponent(TrackModel& m,
                       std::function<void(TrackModel&)> onSelect,
                       std::function<void(size_t)> onDuplicate,
                       std::function<void(TrackLaneComponent&, int yInContent)> onDragLaneCb,
                       std::function<void(TrackLaneComponent&)> onDragStartCb,
                       std::function<void(TrackLaneComponent&)> onDragEndCb,
                       size_t myIndex);

    void setSelected(bool on);
    bool isSelected() const;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    static constexpr int headerWidth = 180;
    TrackModel&      model;

private:
    juce::Label      title;
    std::function<void(TrackModel&)>                         onSelected;
    std::function<void(size_t)>                              onDuplicateLane;
    std::function<void(TrackLaneComponent&, int yInContent)> onDragLane;
    std::function<void(TrackLaneComponent&)>                 onDragStart;
    std::function<void(TrackLaneComponent&)>                 onDragEnd;

    size_t index { 0 };
    bool selected { false };
    bool draggingHeader { false };
};
