#include "ClipComponent.h"
#include "../loops/LoopsRegistry.h"

ClipComponent::ClipComponent(ClipModel& m,
                             juce::AudioFormatManager& fmIn,
                             juce::AudioThumbnailCache& cacheIn,
                             double& bpmRefIn,
                             std::function<void()> changedCB)
    : model(m),
      onChanged(std::move(changedCB)),
      fm(fmIn),
      bpmRef(bpmRefIn),
      thumb(256, fmIn, cacheIn)
{
    setInterceptsMouseClicks(true, true);
    // Intentionally NOT setBufferedToImage: when zoomed in, a clip's
    // component can be hundreds of thousands of pixels wide. Buffering
    // would allocate an image of that full size and stall drag/zoom on
    // long files. Repainting via drawChannels is clipped to the visible
    // dirty rect by JUCE, which is fast enough.

    if (model.kind == ClipModel::Kind::Audio && model.file.existsAsFile())
    {
        if (model.showImportSpinner)
            spinnerVisibleUntilMs = juce::Time::getMillisecondCounter() + 650;

        (void) ThumbPersistence::load(thumb, model.file);
        thumb.setSource(new juce::FileInputSource(model.file)); // takes ownership

        if (model.showImportSpinner)
            startTimerHz(30);
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

void ClipComponent::setDragPreview(bool on)
{
    if (dragPreview == on)
        return;

    dragPreview = on;
    repaint();
}

juce::Rectangle<int> ClipComponent::leftHandle() const
{
    return getLocalBounds().withWidth(6);
}

juce::Rectangle<int> ClipComponent::rightHandle() const
{
    return getLocalBounds().withX(getWidth() - 6).withWidth(6);
}

void ClipComponent::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Body
    g.setColour(dragPreview ? juce::Colour(0xFF234C5F)
                             : (selected ? juce::Colour(0xFF2E3A52)
                                         : juce::Colour(0xFF373737)));
    g.fillRoundedRectangle(r, 8.0f);

    // Waveform / MIDI preview area
    auto inner = r.reduced(8.0f, 6.0f).toNearestInt();

    if (model.kind == ClipModel::Kind::MidiLoop)
    {
        const auto* loop = LoopsRegistry::instance().get(model.loopId);
        const juce::String name = loop != nullptr ? loop->name : model.label;

        g.setColour(juce::Colour(0xFF101820));
        g.fillRoundedRectangle(inner.toFloat(), 5.0f);

        g.setColour(juce::Colour(0x223CE0FF));
        for (int i = 1; i < 4; ++i)
            g.fillRect(inner.getX() + (i * inner.getWidth()) / 4, inner.getY(), 1, inner.getHeight());

        if (loop != nullptr)
        {
            const int pitchMin = 36;
            const int pitchMax = 84;
            const double loopLen = juce::jmax(1.0, loop->lengthBeats);
            for (const auto& n : loop->notes)
            {
                const double x0 = juce::jlimit(0.0, loopLen, n.startBeats);
                const double x1 = juce::jlimit(0.0, loopLen, n.startBeats + n.lengthBeats);
                const int nx = inner.getX() + (int) std::round((x0 / loopLen) * inner.getWidth());
                const int nw = juce::jmax(4, (int) std::round(((x1 - x0) / loopLen) * inner.getWidth()));
                const float t = (float) juce::jlimit(0.0, 1.0, (n.pitch - pitchMin) / (double) (pitchMax - pitchMin));
                const int ny = inner.getBottom() - 7 - (int) std::round(t * (inner.getHeight() - 12));

                g.setColour(juce::Colour(0xFF3CE0FF).withAlpha(0.88f));
                g.fillRoundedRectangle((float) nx, (float) ny, (float) nw, 6.0f, 3.0f);
            }
        }

        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.setFont(juce::Font(12.0f).boldened());
        g.drawText(name.isNotEmpty() ? name : "MIDI Loop", getLocalBounds().reduced(10, 6),
                   juce::Justification::topLeft, true);
    }
    else if (model.kind == ClipModel::Kind::Audio && model.file.existsAsFile()
             && thumb.getTotalLength() > 0.0)
    {
        g.setColour(juce::Colour(0xFFB4B4B4));

        // Map beats -> seconds using current BPM
        const double secPerBeat = 60.0 / juce::jmax(1.0, bpmRef);

        const double startSecInFile = juce::jmax(0.0, model.offsetBeats * secPerBeat);
        const double endSecInFile   = startSecInFile + (model.lengthBeats * secPerBeat);

        const double fileLenSec     = thumb.getTotalLength();
        const double s = juce::jlimit(0.0, fileLenSec, startSecInFile);
        const double e = juce::jlimit(s,    fileLenSec, endSecInFile);

        // Only render the columns that actually fall inside the current
        // graphics clip (i.e. visible on screen). For huge zoomed clips
        // this avoids iterating hundreds of thousands of off-screen
        // pixel columns inside JUCE's drawChannels loop.
        const auto clipBounds = g.getClipBounds();
        const auto visible    = inner.getIntersection(clipBounds);
        if (! visible.isEmpty() && inner.getWidth() > 0 && e > s)
        {
            const double secsPerPx = (e - s) / (double) inner.getWidth();
            const double visStart  = s + (visible.getX()     - inner.getX()) * secsPerPx;
            const double visEnd    = s + (visible.getRight() - inner.getX()) * secsPerPx;
            thumb.drawChannels(g, visible,
                               juce::jlimit(0.0, fileLenSec, visStart),
                               juce::jlimit(0.0, fileLenSec, visEnd),
                               1.0f);
        }

        const bool showSpinner = model.showImportSpinner
                              && juce::Time::getMillisecondCounter() < spinnerVisibleUntilMs;
        if (showSpinner)
        {
            g.setColour(juce::Colour(Theme::colBgPanel).withAlpha(0.42f));
            g.fillRoundedRectangle(inner.toFloat(), 5.0f);

            const float cx = r.getCentreX();
            const float cy = r.getCentreY();
            const float rad = juce::jmin(r.getWidth() * 0.12f, r.getHeight() * 0.28f, 12.0f);

            juce::Path arc;
            arc.addArc(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f,
                       spinnerAngle,
                       spinnerAngle + juce::MathConstants<float>::pi * 1.3f,
                       true);
            g.setColour(juce::Colour(Theme::colAccent).withAlpha(0.95f));
            g.strokePath(arc, juce::PathStrokeType(2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));
        }
    }
    else if (model.kind == ClipModel::Kind::Audio && model.file.existsAsFile())
    {
        if (model.showImportSpinner
            && juce::Time::getMillisecondCounter() < spinnerVisibleUntilMs)
        {
            const float cx = r.getCentreX();
            const float cy = r.getCentreY();
            const float rad = juce::jmin(r.getWidth() * 0.12f, r.getHeight() * 0.28f, 12.0f);

            juce::Path arc;
            arc.addArc(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f,
                       spinnerAngle,
                       spinnerAngle + juce::MathConstants<float>::pi * 1.3f,
                       true);
            g.setColour(juce::Colour(Theme::colAccent).withAlpha(0.85f));
            g.strokePath(arc, juce::PathStrokeType(2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));
        }

        g.setColour(juce::Colour(Theme::colTextDim));
        g.setFont(11.0f);
        g.drawText(model.file.getFileNameWithoutExtension(),
                   getLocalBounds().reduced(8, 6),
                   juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour(juce::Colour(Theme::colTextDim));
        g.setFont(juce::Font(12.0f).boldened());
        g.drawText("Clip", getLocalBounds().reduced(8, 6),
                   juce::Justification::centredLeft, true);
    }

    // Border
    g.setColour(dragPreview ? juce::Colour(Theme::colAccent)
                             : (selected ? juce::Colour(0xFF3B82F6)
                                         : juce::Colour(0x66FFFFFF)));
    g.drawRoundedRectangle(r, 8.0f, (selected || dragPreview) ? 2.0f : 1.0f);
}

void ClipComponent::mouseMove(const juce::MouseEvent& e)
{
    lastMousePos = e.getPosition();
    updateCursorForPoint(lastMousePos);
}

void ClipComponent::timerCallback()
{
    if (!model.showImportSpinner
        || juce::Time::getMillisecondCounter() >= spinnerVisibleUntilMs)
    {
        model.showImportSpinner = false;
        stopTimer();
        repaint();
        return;
    }

    spinnerAngle += 0.18f;
    if (spinnerAngle > juce::MathConstants<float>::twoPi)
        spinnerAngle -= juce::MathConstants<float>::twoPi;
    repaint();
}

void ClipComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (thumb.isFullyLoaded())
    {
        if (model.file.existsAsFile() && !savedOnce)
        {
            ThumbPersistence::save(thumb, model.file);
            savedOnce = true;
        }
        if (onChanged) onChanged();
    }
    repaint();
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
