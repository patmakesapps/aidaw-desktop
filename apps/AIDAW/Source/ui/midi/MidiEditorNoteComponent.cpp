#include "MidiEditor.h"

#include <utility>

// ==== NoteComponent =========================================================
MidiEditor::Content::NoteComponent::NoteComponent(
    uint32 uid,
    std::function<void(uint32, juce::Point<int>, bool&, int&)> onDown,
    std::function<void(uint32, juce::Point<int>)> onDrag,
    std::function<void(uint32)> onUp,
    std::function<void(uint32)> onRightErase)
    : noteId(uid),
      onMouseDown(std::move(onDown)),
      onMouseDragCB(std::move(onDrag)),
      onMouseUpCB(std::move(onUp)),
      onRightEraseCB(std::move(onRightErase))
{
    setInterceptsMouseClicks(true, false);
    setRepaintsOnMouseActivity(true);
}

void MidiEditor::Content::NoteComponent::setSelected(bool s)
{
    selected = s;
    repaint();
}

void MidiEditor::Content::NoteComponent::setColors(juce::Colour body, juce::Colour rim)
{
    bodyCol = body;
    rimCol = rim;
    repaint();
}

void MidiEditor::Content::NoteComponent::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float radius = juce::jmin(r.getHeight() * 0.42f, 8.0f);

    juce::Colour top = bodyCol.brighter(0.18f);
    juce::Colour base = bodyCol.darker(0.18f);
    g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), base, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);

    g.setColour(rimCol.withAlpha(selected ? 1.0f : 0.7f));
    g.drawRoundedRectangle(r, radius, selected ? 2.0f : 1.2f);

    g.setColour(juce::Colours::white.withAlpha(0.20f));
    g.fillRect(r.reduced(2.0f).withWidth(2.0f));
}

void MidiEditor::Content::NoteComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        if (onRightEraseCB)
            onRightEraseCB(noteId);

        return;
    }

    bool didSelect = false;
    int edge = 0;
    if (onMouseDown)
        onMouseDown(noteId, e.getEventRelativeTo(getParentComponent()).getPosition(), didSelect, edge);

    hitEdge = edge;
}

void MidiEditor::Content::NoteComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
        return;

    if (onMouseDragCB)
        onMouseDragCB(noteId, e.getEventRelativeTo(getParentComponent()).getPosition());
}

void MidiEditor::Content::NoteComponent::mouseUp(const juce::MouseEvent&)
{
    if (onMouseUpCB)
        onMouseUpCB(noteId);
}
