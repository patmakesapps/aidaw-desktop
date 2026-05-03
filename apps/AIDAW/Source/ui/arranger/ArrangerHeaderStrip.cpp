#include "ArrangerHeaderStrip.h"
#include "../shared/Theme.h"

/* ====================== TrackHeaderRow ====================== */

TrackHeaderRow::TrackHeaderRow(TrackModel& m,
                               std::function<void(TrackModel&)> onSelect,
                               std::function<void(size_t)> onDuplicate,
                               std::function<void(TrackHeaderRow&, int)> onDragLaneCb,
                               std::function<void(TrackHeaderRow&)> onDragStartCb,
                               std::function<void(TrackHeaderRow&)> onDragEndCb,
                               size_t myIndex)
    : model(m),
      index(myIndex),
      onSelected(std::move(onSelect)),
      onDuplicateLane(std::move(onDuplicate)),
      onDragLane(std::move(onDragLaneCb)),
      onDragStart(std::move(onDragStartCb)),
      onDragEnd(std::move(onDragEndCb))
{
    title.setText(model.name, juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    title.setEditable(true);
    title.setMinimumHorizontalScale(0.8f);
    title.onTextChange = [this]() { model.name = title.getText(); repaint(); };
    addAndMakeVisible(title);
    title.setTooltip("Rename track. Double-click header to duplicate. Drag header to reorder.");
}

void TrackHeaderRow::setSelected(bool on)
{
    if (selected == on) return;
    selected = on;
    repaint();
}

void TrackHeaderRow::paint(juce::Graphics& g)
{
    auto r = getLocalBounds();

    g.setColour(juce::Colour(selected ? 0xFF1D2434 : 0xFF141414));
    g.fillRect(r);

    if (selected)
    {
        g.setColour(juce::Colour(Theme::colAccent).withAlpha(0.55f));
        g.fillRect(0, 0, 3, r.getHeight());
    }

    g.setColour(juce::Colour(0x22FFFFFF));
    g.drawRect(r);
}

void TrackHeaderRow::resized()
{
    title.setBounds(12, 6, getWidth() - 18, getHeight() - 12);
}

void TrackHeaderRow::mouseDown(const juce::MouseEvent& e)
{
    draggingHeader = true;

    if (onSelected)
        onSelected(model);

    if (onDragStart)
        onDragStart(*this);

    if (e.getNumberOfClicks() >= 2 && onDuplicateLane)
        onDuplicateLane(index);
}

void TrackHeaderRow::mouseDrag(const juce::MouseEvent& e)
{
    if (! draggingHeader || onDragLane == nullptr) return;
    if (auto* parent = getParentComponent())
        onDragLane(*this, e.getEventRelativeTo(parent).getPosition().y);
}

void TrackHeaderRow::mouseUp(const juce::MouseEvent&)
{
    if (draggingHeader && onDragEnd) onDragEnd(*this);
    draggingHeader = false;
}

/* ====================== HeaderStrip ====================== */

HeaderStrip::HeaderStrip()
{
    setOpaque(true);
    plusButton.setTooltip("Add track");
    plusButton.setWantsKeyboardFocus(false);
    plusButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(Theme::colBtnIdle));
    plusButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::colTextDim));
    addAndMakeVisible(plusButton);
}

void HeaderStrip::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0D0D0D));

    // Right edge divider so the strip reads as a distinct pinned panel.
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.fillRect(getWidth() - 1, 0, 1, getHeight());
}

void HeaderStrip::setRows(std::vector<std::unique_ptr<TrackHeaderRow>> newRows)
{
    for (auto& r : rows)
        if (r) removeChildComponent(r.get());

    rows = std::move(newRows);

    for (auto& r : rows)
    {
        if (r == nullptr) continue;
        addAndMakeVisible(r.get());
    }

    plusButton.toFront(true);
    layoutChildren();
}

void HeaderStrip::layoutChildren()
{
    const int w = getWidth();
    int y = laneTop - vscroll;

    for (auto& r : rows)
    {
        if (r == nullptr) continue;
        r->setBounds(0, y, w, laneH);
        y += laneH;
    }

    const int btnW = 28, btnH = 28;
    const int xBtn = juce::jmax(4, (w - btnW) / 2);
    const int yBtn = (laneTop + (int) rows.size() * laneH + 6) - vscroll;
    plusButton.setBounds(xBtn, yBtn, btnW, btnH);
}
