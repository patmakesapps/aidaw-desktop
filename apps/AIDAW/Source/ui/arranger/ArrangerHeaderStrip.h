#pragma once
#include <JuceHeader.h>
#include <functional>
#include "../shared/Models.h"
#include "TrackLaneComponent.h"

// Single pinned-track-header row. Rendered inside HeaderStrip, which is a
// fixed-position overlay anchored to the left of the arranger viewport so
// track names stay visible when the user scrolls horizontally.
class TrackHeaderRow : public juce::Component
{
public:
    TrackHeaderRow(TrackModel& m,
                   std::function<void(TrackModel&)> onSelect,
                   std::function<void(size_t)> onDuplicate,
                   std::function<void(TrackHeaderRow&, int yInStrip)> onDragLaneCb,
                   std::function<void(TrackHeaderRow&)> onDragStartCb,
                   std::function<void(TrackHeaderRow&)> onDragEndCb,
                   size_t myIndex);

    void setSelected(bool on);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    TrackModel& model;

private:
    juce::Label title;
    bool   selected { false };
    bool   draggingHeader { false };
    size_t index { 0 };

    std::function<void(TrackModel&)>                       onSelected;
    std::function<void(size_t)>                            onDuplicateLane;
    std::function<void(TrackHeaderRow&, int yInStrip)>     onDragLane;
    std::function<void(TrackHeaderRow&)>                   onDragStart;
    std::function<void(TrackHeaderRow&)>                   onDragEnd;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeaderRow)
};

// Fixed left strip that hosts pinned track-header rows. Sits on top of the
// arranger's main viewport so headers never scroll away horizontally; rows
// are translated vertically in lockstep with the viewport's Y scroll.
class HeaderStrip : public juce::Component
{
public:
    HeaderStrip();

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;

    // Replace all rows. The strip takes ownership of the passed-in rows.
    void setRows(std::vector<std::unique_ptr<TrackHeaderRow>> newRows);

    // Where the first row begins (y = laneTop). Matches the arranger's
    // ruler height so rows align with timeline lanes.
    void setLaneTop(int top)        { laneTop = top; layoutChildren(); }
    void setLaneHeight(int h)       { laneH = juce::jmax(8, h); layoutChildren(); }
    void setVerticalScroll(int yScroll) { vscroll = yScroll; layoutChildren(); }
    void setReorderPreview(int sourceIndex, int insertIndex, int cursorY);
    void clearReorderPreview();

    // The "+" add-track button is a child of the strip so it stays pinned.
    juce::TextButton plusButton { "+" };

    int rowCount() const { return (int) rows.size(); }
    TrackHeaderRow* row(int i) { return (i >= 0 && i < (int)rows.size()) ? rows[(size_t)i].get() : nullptr; }

private:
    void layoutChildren();

    std::vector<std::unique_ptr<TrackHeaderRow>> rows;

    int laneTop { 20 };
    int laneH   { 76 };
    int vscroll { 0 };
    int reorderSourceIndex { -1 };
    int reorderInsertIndex { -1 };
    int reorderCursorY { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderStrip)
};
