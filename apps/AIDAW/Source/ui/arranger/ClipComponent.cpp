#include "ClipComponent.h"

ClipComponent::ClipComponent(ClipModel& m,
                             juce::AudioFormatManager& fmIn,
                             juce::AudioThumbnailCache& cacheIn,
                             double& bpmRefIn,
                             std::function<void()> changedCB)
    : model(m),
      onChanged(std::move(changedCB)),
      fm(fmIn),
      bpmRef(bpmRefIn),
      thumb(512, fmIn, cacheIn)
{
    setInterceptsMouseClicks(true, true);
    setBufferedToImage(true);

    if (model.file.existsAsFile())
    {
        (void) ThumbPersistence::load(thumb, model.file);
        thumb.setSource(new juce::FileInputSource(model.file)); // takes ownership
    }

    thumb.addChangeListener(this);
}

ClipComponent::~ClipComponent()
{
    thumb.removeChangeListener(this);
}

void ClipComponent::setActiveTool(ArrangerTool t)
{
    activeTool = t;
    updateCursorForPoint(lastMousePos);
}

void ClipComponent::setSelected(bool on)
{
    selected = on;
    repaint();
}

juce::Rectangle<int> ClipComponent::leftHandle() const
{
    return getLocalBounds().withWidth(6);
}

juce::Rectangle<int> ClipComponent::rightHandle() const
{
    return getLocalBounds().withX(getRight() - 6).withWidth(6);
}

void ClipComponent::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Body
    g.setColour(selected ? juce::Colour(0xFF2E3A52) : juce::Colour(0xFF373737));
    g.fillRoundedRectangle(r, 8.0f);

    // Waveform area
    auto inner = r.reduced(8.0f, 6.0f).toNearestInt();

    if (thumb.getTotalLength() > 0.0)
    {
        g.setColour(juce::Colour(0xFFB4B4B4));

        // Map beats -> seconds using current BPM
        const double secPerBeat = 60.0 / juce::jmax(1.0, bpmRef);

        const double startSecInFile = juce::jmax(0.0, model.offsetBeats * secPerBeat);
        const double endSecInFile   = startSecInFile + (model.lengthBeats * secPerBeat);

        const double fileLenSec     = thumb.getTotalLength();
        const double s = juce::jlimit(0.0, fileLenSec, startSecInFile);
        const double e = juce::jlimit(s,    fileLenSec, endSecInFile);

        // Draw ONLY the subrange represented by this clip
        thumb.drawChannels(g, inner, s, e, 1.0f);
    }
    else
    {
        // Fallback: show file name
        g.setColour(juce::Colours::white);
        auto font = juce::Font(12.0f).boldened();
        g.setFont(font);
        auto base = model.file.existsAsFile()
                  ? model.file.getFileNameWithoutExtension()
                  : juce::String("Clip");
        g.drawText(base, getLocalBounds().reduced(8, 6),
                   juce::Justification::centredLeft, true);
    }

    // Border
    g.setColour(selected ? juce::Colour(0xFF3B82F6) : juce::Colour(0x66FFFFFF));
    g.drawRoundedRectangle(r, 8.0f, selected ? 2.0f : 1.0f);
}

void ClipComponent::mouseMove(const juce::MouseEvent& e)
{
    lastMousePos = e.getPosition();
    updateCursorForPoint(lastMousePos);
}

void ClipComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (model.file.existsAsFile() && thumb.getTotalLength() > 0.0 && !savedOnce)
    {
        ThumbPersistence::save(thumb, model.file);
        savedOnce = true;
    }
    repaint();
    if (onChanged) onChanged();
}

void ClipComponent::updateCursorForPoint(juce::Point<int> p)
{
    using MC = juce::MouseCursor;

    if (activeTool == ArrangerTool::Resize)
    {
        setMouseCursor((leftHandle().contains(p) || rightHandle().contains(p))
                       ? MC::LeftRightResizeCursor
                       : MC::NormalCursor);
        return;
    }

    switch (activeTool)
    {
        case ArrangerTool::Pointer: setMouseCursor(MC::DraggingHandCursor);  break;
        case ArrangerTool::Slice:   setMouseCursor(MC::CrosshairCursor);     break;
        case ArrangerTool::Zoom:    setMouseCursor(MC::PointingHandCursor);  break;
        case ArrangerTool::Resize:  break; // handled above
    }
}
