#include "TrackLaneComponent.h"

TrackLaneComponent::TrackLaneComponent(TrackModel& m,
                                       std::function<void(TrackModel&)> onSelect,
                                       std::function<void(size_t)> onDuplicate,
                                       std::function<void(TrackLaneComponent&, int)> onDragLaneCb,
                                       std::function<void(TrackLaneComponent&)> onDragStartCb,
                                       std::function<void(TrackLaneComponent&)> onDragEndCb,
                                       size_t myIndex)
    : model(m),
      onSelected(std::move(onSelect)),
      onDuplicateLane(std::move(onDuplicate)),
      onDragLane(std::move(onDragLaneCb)),
      onDragStart(std::move(onDragStartCb)),
      onDragEnd(std::move(onDragEndCb)),
      index(myIndex)
{
    // Let empty lane area pass clicks through to clips (no row selection)
    setInterceptsMouseClicks(false, true);

    title.setText(model.name, juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    title.setEditable(true);
    title.setMinimumHorizontalScale(0.8f);
    title.onTextChange = [this]() { model.name = title.getText(); repaint(); };
    addAndMakeVisible(title);
    title.setTooltip("Rename track. Double-click header to duplicate. Drag header to reorder.");
}

void TrackLaneComponent::setSelected(bool on) { selected = on; repaint(); }
bool TrackLaneComponent::isSelected() const { return selected; }

void TrackLaneComponent::paint(juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour(juce::Colour(0xFF0F0F0F)); g.fillRect(r);

    auto header = r.removeFromLeft(headerWidth);
    g.setColour(juce::Colour(0xFF141414)); g.fillRect(header);

    g.setColour(juce::Colour(0x22FFFFFF)); g.drawRect(header);

    g.setColour(juce::Colour(0x11FFFFFF));
    g.fillRect(0, getHeight()-1, getWidth(), 1);
}

void TrackLaneComponent::resized()
{
    title.setBounds(8, 6, headerWidth - 16, getHeight() - 12);
}

void TrackLaneComponent::mouseDown(const juce::MouseEvent& e)
{
    const bool inHeader = e.position.x <= (float) headerWidth;
    draggingHeader = inHeader;

    // No lane selection call; we’re disabling selectable rows.

    if (draggingHeader && onDragStart) onDragStart(*this);

    if (e.getNumberOfClicks() >= 2 && inHeader && onDuplicateLane)
        onDuplicateLane(index);
}

void TrackLaneComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!draggingHeader || onDragLane == nullptr) return;
    if (auto* parent = getParentComponent())
        onDragLane(*this, e.getEventRelativeTo(parent).getPosition().y);
}

void TrackLaneComponent::mouseUp(const juce::MouseEvent&)
{
    if (draggingHeader && onDragEnd) onDragEnd(*this);
    draggingHeader = false;
}
