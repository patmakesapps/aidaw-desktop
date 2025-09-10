#pragma once
#include <JuceHeader.h>
#include <functional>
#include "Models.h"

class TrackLaneComponent : public juce::Component
{
public:
    // Column width for the lane header (track name, controls)
    static constexpr int headerWidth = 180;

    // Canonical layout sizes used by ArrangerCanvas for stacking lanes
    static constexpr int rowHeight   = 140; // tweak to match your theme
    static constexpr int rowGap      = 8;

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

    TrackModel& model;

private:
    juce::Label title;

    // Callbacks (onSelect is currently unused if rows are non-selectable,
    // but kept for API compatibility)
    std::function<void(TrackModel&)>                         onSelected;
    std::function<void(size_t)>                              onDuplicateLane;
    std::function<void(TrackLaneComponent&, int yInContent)> onDragLane;
    std::function<void(TrackLaneComponent&)>                 onDragStart;
    std::function<void(TrackLaneComponent&)>                 onDragEnd;

    size_t index { 0 };
    bool   selected { false };
    bool   draggingHeader { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackLaneComponent)
};
