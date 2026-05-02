#include "MidiEditor.h"

#include <utility>

// ==== NoteComponent =========================================================
MidiEditor::Content::NoteComponent::NoteComponent(
    uint32 uid,
    std::function<void(uint32, juce::Point<int>, const juce::ModifierKeys&, bool&, int&)> onDown,
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

void MidiEditor::Content::NoteComponent::setLabel(const juce::String& text)
{
    label = text;
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

    auto handle = [&] (bool left, bool active)
    {
        const float w = juce::jlimit(3.0f, 7.0f, r.getWidth() * 0.18f);
        auto hr = left ? r.removeFromLeft(w) : getLocalBounds().toFloat().removeFromRight(w);
        g.setColour(juce::Colours::white.withAlpha(active ? 0.42f : 0.18f));
        g.fillRoundedRectangle(hr.reduced(1.0f), radius * 0.55f);
    };
    handle(true,  hitEdge == 1 || hoverEdge == 1);
    handle(false, hitEdge == 2 || hoverEdge == 2);

    if (label.isNotEmpty() && getWidth() >= 34)
    {
        g.setColour(juce::Colours::black.withAlpha(0.48f));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawFittedText(label, getLocalBounds().reduced(7, 1), juce::Justification::centredLeft, 1);
        g.setColour(juce::Colours::white.withAlpha(0.82f));
        g.drawFittedText(label, getLocalBounds().reduced(6, 1), juce::Justification::centredLeft, 1);
    }
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
        onMouseDown(noteId, e.getEventRelativeTo(getParentComponent()).getPosition(), e.mods, didSelect, edge);

    hitEdge = edge;
    repaint();
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

    hitEdge = 0;
    repaint();
}

void MidiEditor::Content::NoteComponent::mouseMove(const juce::MouseEvent& e)
{
    hoverEdge = edgeAtLocalPoint(e.getPosition());
    setMouseCursor(hoverEdge == 0 ? juce::MouseCursor::NormalCursor
                                  : juce::MouseCursor::LeftRightResizeCursor);
    repaint();
}

void MidiEditor::Content::NoteComponent::mouseExit(const juce::MouseEvent&)
{
    hoverEdge = 0;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

int MidiEditor::Content::NoteComponent::edgeAtLocalPoint(juce::Point<int> p) const
{
    const int handleW = juce::jlimit(5, 10, getWidth() / 5);
    if (p.x <= handleW) return 1;
    if (p.x >= getWidth() - handleW) return 2;
    return 0;
}
