#include "Arranger.h"
#include "ArrangerFileUtils.h"
#include "../loops/LoopsRegistry.h"
#include "../shared/ThemeManager.h"
#include <algorithm>
#include <cmath>

Arranger::Arranger()
    : canvas(tracks, bpmValue, snapToGrid, pixelsPerBeat, playheadBeats,
             [this]{ if (onProjectChanged) onProjectChanged(); })
{
    setWantsKeyboardFocus(true);
    fm.registerBasicFormats();

    for (auto* b : { &btnPointer, &btnSlice, &btnResize, &btnZoomTool })
        b->setClickingTogglesState(true);
    btnSnap.setClickingTogglesState(true);
    btnSnap.setToggleState(true, juce::dontSendNotification);

    for (auto* b : { &btnPointer, &btnSlice, &btnResize, &btnZoomTool,
                     &btnFrameAll, &btnSnap, &btnZoomIn, &btnZoomOut, &btnLoopIcon })
    {
        b->addListener(this);
        b->setIconScale(0.5f);
        addAndMakeVisible(b);
    }

    btnLoops.addListener(this); btnLoops.setWantsKeyboardFocus(false); addAndMakeVisible(btnLoops);
    btnEddie.addListener(this); btnEddie.setWantsKeyboardFocus(false); addAndMakeVisible(btnEddie);
    btnLoops.setTooltip("Open Loops (create/select)");
    btnEddie.setTooltip("Open Eddie synth");

    btnLoopIcon.setAccentTint(true);
    btnFrameAll.setAccentTint(true);

    ThemeManager::get().addChangeListener(this);

    setTool(ArrangerTool::Pointer);

    // Viewport + content
    addAndMakeVisible(view);
    view.setViewedComponent(&content, false);
    content.addAndMakeVisible(canvas);       // overlay (draws on top, non-intercepting)
    content.addAndMakeVisible(plusButton);   // add-track button
    canvas.setInterceptsMouseClicks(false, false);

    // Route background (empty grid) clicks to Arranger, too
    content.addMouseListener(this, true);

    // "+" button wiring
    plusButton.setTooltip("Add track");
    plusButton.setWantsKeyboardFocus(false);
    plusButton.onClick = [this]
    {
        addTrack("Audio " + juce::String((int)tracks.size() + 1));
        // auto-scroll to the new lane
        view.setViewPosition(view.getViewPositionX(),
                             juce::jmax(0, contentHeight() - view.getHeight()));
    };
    plusButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::colBtnIdle));
    plusButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::colTextDim));

    setBPM(120.0);
    setSnap(true);

    setZoomLimits(1.0, 4096.0);

    // Seed default lanes exactly once
    ensureDefaultTracks(kDefaultTrackCount);

    refreshAll();
    applyCursorForToolEverywhere();
}

Arranger::~Arranger()
{
    stopTimer();
    ThemeManager::get().removeChangeListener(this);
    for (auto* b : { &btnPointer, &btnSlice, &btnResize, &btnZoomTool,
                     &btnFrameAll, &btnSnap, &btnZoomIn, &btnZoomOut, &btnLoopIcon })
        b->removeListener(this);
    btnLoops.removeListener(this);
    btnEddie.removeListener(this);
}

void Arranger::changeListenerCallback (juce::ChangeBroadcaster*)
{
    plusButton.setColour(juce::TextButton::buttonColourId,    juce::Colour(Theme::colBtnIdle));
    plusButton.setColour(juce::TextButton::textColourOffId,   juce::Colour(Theme::colTextDim));
    canvas.repaint();
    repaint();
}

void Arranger::ensureDefaultTracks(int n)
{
    if (!tracks.empty()) return;
    for (int i = 0; i < n; ++i)
    {
        TrackModel t;
        t.id   = juce::Uuid().toString();
        t.name = "Track " + juce::String(i + 1);
        tracks.emplace_back(std::move(t));
    }
}

void Arranger::setBPM(double bpm)
{
    const double old = bpmValue;
    bpmValue = juce::jlimit(40.0, 300.0, bpm);
    pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;
    pixelsPerBeat = juce::jlimit(minPixelsPerBeat, maxPixelsPerBeat, pixelsPerBeat);
    zoomScale = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
    if (std::abs(old - bpmValue) > 1e-6) { pushUndo(); recomputeClipBeatLengthsForTempo(); }
    layoutClips(); repaint();
}

void Arranger::setPlayheadBeats(double beats) { playheadBeats = juce::jmax(0.0, beats); canvas.repaint(); }
double Arranger::getPlayheadBeats() const     { return playheadBeats; }

void Arranger::setSnap(bool on)
{
    snapToGrid = on; btnSnap.setToggleState(on, juce::dontSendNotification); repaint();
}

void Arranger::setTool(ArrangerTool t)
{
    tool = t;
    btnPointer .setToggleState(t == ArrangerTool::Pointer, juce::dontSendNotification);
    btnSlice   .setToggleState(t == ArrangerTool::Slice,   juce::dontSendNotification);
    btnResize  .setToggleState(t == ArrangerTool::Resize,  juce::dontSendNotification);
    btnZoomTool.setToggleState(t == ArrangerTool::Zoom,    juce::dontSendNotification);

    // Clear marquee and restore cursor/selection state
    selectionActive = false;
    canvas.setSelectionRect(std::nullopt);
    if (t != ArrangerTool::Zoom) { preZoomView.reset(); clickZoomStage = 0; }
    applyCursorForToolEverywhere(); repaint();
}

bool Arranger::isInterestedInFileDrag(const juce::StringArray&) { return true; }

void Arranger::filesDropped(const juce::StringArray& files, int x, int y)
{
    if (files.isEmpty()) return;
    pushUndo();

    const int contentX = x - view.getX() + view.getViewPositionX();
    const double dropBeats = canvas.beatsFromX(contentX);
    const double startBeats = snapToGrid ? std::round(dropBeats) : dropBeats;

    const int yContent = y - view.getY() + view.getViewPositionY();
    int laneIdx = laneIndexFromYAllowNew(yContent);

    for (int i=0;i<files.size();++i)
    {
        const juce::File f(files[i]);
        if (!f.existsAsFile()) continue;

        while (laneIdx >= (int)tracks.size())
            addTrack("Audio " + juce::String((int)tracks.size()+1), /*push=*/false);

        ClipModel clip;
        clip.id = juce::Uuid().toString();
        clip.kind = ClipModel::Kind::Audio;
        clip.file = f;
        clip.startBeats  = startBeats;
        clip.lengthBeats = ArrangerFileUtils::estimateBeatsFromFile(fm, f, bpmValue);
        clip.offsetBeats = 0.0;

        auto& lane = tracks[(size_t)laneIdx];
        auto name = ArrangerFileUtils::extractTrackNameFromAudio(fm, f);
        lane.name = name.isNotEmpty() ? name : f.getFileNameWithoutExtension();
        lane.clips.push_back(std::move(clip));
        ++laneIdx;
    }

    refreshAll();
    if (onProjectChanged) onProjectChanged();
}

bool Arranger::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    return dragSourceDetails.description.toString().startsWith("aidaw-loop:");
}

void Arranger::itemDropped(const SourceDetails& dragSourceDetails)
{
    const auto desc = dragSourceDetails.description.toString();
    if (! desc.startsWith("aidaw-loop:"))
        return;

    const uint32 loopId = (uint32) desc.fromFirstOccurrenceOf(":", false, false).getLargeIntValue();
    if (loopId == 0)
        return;

    const auto local = dragSourceDetails.localPosition;
    const int contentX = local.x - view.getX() + view.getViewPositionX();
    const int contentY = local.y - view.getY() + view.getViewPositionY();
    double beat = canvas.beatsFromX(contentX);
    if (snapToGrid)
        beat = std::round(beat);

    addMidiLoopClip(loopId, juce::jmax(0.0, beat), laneIndexFromYAllowNew(contentY));
}

void Arranger::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::colBgMain));
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.drawRect(getLocalBounds());
}

void Arranger::resized()
{
    auto r = getLocalBounds().reduced(8, 6);

    auto tools = r.removeFromTop(34);
    const int ib = 36;
    btnPointer .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnSlice   .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnResize  .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnZoomTool.setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(12);
    btnFrameAll.setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnSnap    .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(12);
    btnZoomOut .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnZoomIn  .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(12);
    btnLoopIcon.setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnLoops   .setBounds(tools.removeFromLeft(72)); tools.removeFromLeft(6);
    btnEddie   .setBounds(tools.removeFromLeft(74));

    view.setBounds(r);

    // ensure initial content size
    if (content.getWidth() <= 0)
        content.setSize(juce::jmax(1400, view.getWidth()), contentHeight());

    layoutLanes();
    layoutClips();
}

bool Arranger::keyPressed(const juce::KeyPress& key)
{
    const bool ctrl = key.getModifiers().isCommandDown();
    if (key.getKeyCode() == juce::KeyPress::spaceKey) { if (onPlayPressed) onPlayPressed(); return true; }

    if (ctrl && (key.getTextCharacter() == 'z' || key.getTextCharacter() == 'Z'))
    { if (key.getModifiers().isShiftDown()) redo(); else undo(); return true; }

    if (key.getTextCharacter() == '1') { setTool(ArrangerTool::Pointer); return true; }
    if (key.getTextCharacter() == '2') { setTool(ArrangerTool::Slice);   return true; }
    if (key.getTextCharacter() == '3') { setTool(ArrangerTool::Resize);  return true; }
    if (key.getTextCharacter() == '4') { setTool(ArrangerTool::Zoom);    return true; }

    if (key.getTextCharacter() == 'g' || key.getTextCharacter() == 'G') { setSnap(!snapToGrid); return true; }
    if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F') { frameAll(); return true; }

    if (key.getTextCharacter() == '+') { zoomAtViewportCenter(+1); return true; }
    if (key.getTextCharacter() == '-') { zoomAtViewportCenter(-1); return true; }

    // Delete priority: selected clip > selected lane
    if (key.getKeyCode() == juce::KeyPress::deleteKey)
    {
        if (selectedClip != nullptr) { removeClip(selectedClip); selectedClip = nullptr; return true; }
        if (selectedLaneIndex >= 0 && selectedLaneIndex < (int)tracks.size())
        {
            auto id = tracks[(size_t)selectedLaneIndex].id;
            removeTrackById(id);
            setSelectedLane(-1);
            return true;
        }
    }
    return false;
}

/*** helpers ***/
void Arranger::addTrack(const juce::String& name, bool push)
{
    if (push) pushUndo();
    TrackModel t; t.id = juce::Uuid().toString(); t.name = name;
    tracks.emplace_back(std::move(t));
    refreshAll();
}

void Arranger::addMidiLoopClip(uint32 loopId, double startBeats, int laneIndex)
{
    const auto* loop = LoopsRegistry::instance().get(loopId);
    if (loop == nullptr)
        return;

    pushUndo();

    while (laneIndex >= (int) tracks.size())
        addTrack("MIDI " + juce::String((int) tracks.size() + 1), false);

    if (tracks.empty())
        addTrack("MIDI 1", false);

    laneIndex = juce::jlimit(0, (int) tracks.size() - 1, laneIndex);
    auto& lane = tracks[(size_t) laneIndex];
    if (lane.name.startsWith("Track ") || lane.name.startsWith("Audio"))
        lane.name = "MIDI";

    ClipModel clip;
    clip.id = juce::Uuid().toString();
    clip.kind = ClipModel::Kind::MidiLoop;
    clip.loopId = loopId;
    clip.label = loop->name;
    clip.startBeats = juce::jmax(0.0, startBeats);
    clip.lengthBeats = juce::jmax(1.0, loop->lengthBeats);
    clip.offsetBeats = 0.0;

    lane.clips.push_back(std::move(clip));
    selectedClip = &lane.clips.back();
    setSelectedLane(laneIndex);
    refreshAll();
    if (onProjectChanged) onProjectChanged();
}

void Arranger::removeClipsForLoop(uint32 loopId)
{
    bool changed = false;
    pushUndo();
    for (auto& track : tracks)
    {
        const auto before = track.clips.size();
        track.clips.erase(std::remove_if(track.clips.begin(), track.clips.end(),
                                         [loopId](const ClipModel& clip)
                                         {
                                             return clip.kind == ClipModel::Kind::MidiLoop
                                                 && clip.loopId == loopId;
                                         }),
                          track.clips.end());
        changed = changed || before != track.clips.size();
    }

    if (! changed)
    {
        if (!undoStack.empty())
            undoStack.pop_back();
        return;
    }

    selectedClip = nullptr;
    refreshAll();
    if (onProjectChanged) onProjectChanged();
}

void Arranger::duplicateTrack(size_t idx)
{
    if (idx >= tracks.size()) return;
    pushUndo();
    TrackModel copy = tracks[idx];
    copy.id = juce::Uuid().toString();
    tracks.insert(tracks.begin() + (ptrdiff_t)idx + 1, std::move(copy));
    refreshAll();
    setSelectedLane((int)idx + 1);
}

void Arranger::removeTrackById(const juce::String& id)
{
    pushUndo();
    const auto it = std::find_if(tracks.begin(), tracks.end(),
        [&](const TrackModel& t){ return t.id == id; });
    if (it != tracks.end()) tracks.erase(it);
    refreshAll();
    if (onProjectChanged) onProjectChanged();
}

void Arranger::removeClip(ClipModel* clip)
{
    if (!clip) return;
    pushUndo();
    for (auto& t : tracks)
    {
        auto it = std::find_if(t.clips.begin(), t.clips.end(), [&](const ClipModel& c){ return &c == clip; });
        if (it != t.clips.end()) { t.clips.erase(it); break; }
    }
    refreshAll();
    if (onProjectChanged) onProjectChanged();
}

/*** ZOOM (marquee + clamps + restore) ***/
void Arranger::setZoomLimits(double minPPB, double maxPPB)
{
    minPixelsPerBeat = juce::jmax(1.0, minPPB);
    maxPixelsPerBeat = juce::jmax(minPixelsPerBeat + 1.0, maxPPB);
    pixelsPerBeat = juce::jlimit(minPixelsPerBeat, maxPixelsPerBeat, pixelsPerBeat);
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
}

void Arranger::zoomDelta(double steps) { zoomAtViewportCenter(steps); }

void Arranger::zoomDeltaFromWheel(double wheelDelta, int screenX)
{
    // Convert anchor X from screen to content coordinates
    const auto anchor = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
    int anchorX = view.getViewPositionX() + (int)std::round(anchor.x - getScreenX());
    if (std::abs((int)anchor.x - screenX) > 2) // if a screenX was provided, prefer it
        anchorX = view.getViewPositionX() + (screenX - getScreenX());

    applyZoomAtContentX(wheelDelta, anchorX);
}

void Arranger::zoomAtViewportCenter(double steps)
{
    const int anchorXContent = view.getViewPositionX() + view.getWidth() / 2;
    applyZoomAtContentX(steps, anchorXContent);
}

void Arranger::applyZoomAtContentX(double steps, int anchorXContent)
{
    const int contentW = juce::jmax(content.getWidth(), view.getWidth());
    const int ax = juce::jlimit(TrackLaneComponent::headerWidth, contentW - 1, anchorXContent);

    const double oldPPB   = pixelsPerBeat;
    const double beat     = juce::jmax(0.0, (ax - TrackLaneComponent::headerWidth) / oldPPB);
    const int    xBefore  = TrackLaneComponent::headerWidth + (int)std::round(beat * oldPPB);

    // Update scale
    zoomScale     = juce::jlimit(kMinZoom, kMaxZoom, zoomScale * std::pow(kZoomBase, steps));
    pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;

    // Clamp to safe limits
    pixelsPerBeat = juce::jlimit(minPixelsPerBeat, maxPixelsPerBeat, pixelsPerBeat);
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));

    // Keep anchor in place (pan to compensate)
    const int xAfter = TrackLaneComponent::headerWidth + (int)std::round(beat * pixelsPerBeat);
    const int dx     = xAfter - xBefore;

    layoutClips();
    extendContentToMaxClip();

    auto p   = view.getViewPosition();
    const int maxX = juce::jmax(0, content.getWidth() - view.getWidth());
    view.setViewPosition(juce::jlimit(0, maxX, p.getX() + dx), p.getY());

    canvas.repaint();
    repaint();
}

void Arranger::zoomToSelectionAndCenter(const juce::Rectangle<int>& rectContent)
{
    if (rectContent.isEmpty()) return;

    // Target: fit selection width to viewport width (minus header/margins)
    const int viewportW = juce::jmax(1, view.getWidth() - TrackLaneComponent::headerWidth - 32);
    const int selW      = juce::jmax(1, rectContent.getWidth());
    const double targetPPB = juce::jlimit(minPixelsPerBeat, maxPixelsPerBeat,
        (double)viewportW / (double)selW * pixelsPerBeat);

    pixelsPerBeat = targetPPB;
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
    layoutClips();
    extendContentToMaxClip();

    // Center selection (both axes)
    const int selCenterX = rectContent.getCentreX();
    const int desiredContentCenter = juce::jlimit(TrackLaneComponent::headerWidth,
                                                  content.getWidth() - 1, selCenterX);
    const int newViewX = juce::jlimit(0,
        juce::jmax(0, content.getWidth() - view.getWidth()),
        desiredContentCenter - view.getWidth() / 2);

    const int selCenterY = rectContent.getCentreY();
    const int newViewY = juce::jlimit(0,
        juce::jmax(0, content.getHeight() - view.getHeight()),
        selCenterY - view.getHeight() / 2);

    view.setViewPosition(newViewX, newViewY);

    canvas.repaint();
    repaint();
}

void Arranger::restorePreZoomView()
{
    if (!preZoomView) return;

    pixelsPerBeat = juce::jlimit(minPixelsPerBeat, maxPixelsPerBeat, preZoomView->ppb);
    zoomScale     = preZoomView->scale;
    layoutClips();
    extendContentToMaxClip();
    view.setViewPosition(preZoomView->vx, preZoomView->vy);

    // Clear selection and state
    selectionActive = false;
    canvas.setSelectionRect(std::nullopt);
    clickZoomStage = 0;
    canvas.repaint();
    repaint();
}

void Arranger::autoScrollDuringDrag()
{
    if (!dragAutoScrollActive || activeClip == nullptr || dragging == DragMode::None)
        return;

    const int edge = 44;
    const int step = 16;
    auto p = view.getViewPosition();
    const int maxX = juce::jmax(0, content.getWidth() - view.getWidth());
    const int maxY = juce::jmax(0, content.getHeight() - view.getHeight());

    int dx = 0;
    int dy = 0;
    if (lastDragViewportPos.x < edge) dx = -step;
    else if (lastDragViewportPos.x > view.getWidth() - edge) dx = step;

    if (lastDragViewportPos.y < edge) dy = -step;
    else if (lastDragViewportPos.y > view.getHeight() - edge) dy = step;

    if (dx == 0 && dy == 0)
        return;

    const auto old = p;
    p.setX(juce::jlimit(0, maxX, p.x + dx));
    p.setY(juce::jlimit(0, maxY, p.y + dy));
    if (p == old)
        return;

    view.setViewPosition(p);
    updateActiveClipDragAt({ p.x + lastDragViewportPos.x, p.y + lastDragViewportPos.y });
}

void Arranger::updateActiveClipDragAt(juce::Point<int> pos)
{
    if (!activeClip)
        return;

    const int dxPx = pos.x - dragStartPos.x;
    const double dxBeats = dxPx / pixelsPerBeat;
    auto& cm = activeClip->model;

    if (dragging == DragMode::Move)
    {
        double preview = clipStartBeatsAtDown + dxBeats;
        preview = juce::jlimit(0.0, 100000.0, preview);

        pendingMoveStartBeats = preview;
        pendingMoveValid = true;

        int hoverLane = laneIndexFromYAllowNew(pos.y);
        targetLaneIndex = juce::jlimit(0, (int)tracks.size(), hoverLane);

        const int laneH = laneHeight();
        const int y0 = laneTop();
        const int previewLaneY = y0 + juce::jmin(targetLaneIndex, (int)tracks.size()-1) * laneH + 10;
        const int w = (int)std::round(cm.lengthBeats * pixelsPerBeat);
        const int x = canvas.xFromBeats(preview);

        activeClip->setBounds(x, previewLaneY, juce::jmax(24, w), laneH - 20);
        return;
    }

    if (dragging == DragMode::Left)
    {
        double newStart = clipStartBeatsAtDown + dxBeats;
        double newLen = clipLenBeatsAtDown - dxBeats;
        if (snapToGrid) { newStart = std::round(newStart); newLen = std::round(newLen); }
        newStart = juce::jmax(0.0, newStart);
        newLen = juce::jmax(1.0, newLen);

        const double delta = newStart - clipStartBeatsAtDown;
        cm.startBeats = newStart;
        cm.lengthBeats = newLen;
        cm.offsetBeats = juce::jmax(0.0, clipOffsetBeatsAtDown + delta);
    }
    else if (dragging == DragMode::Right)
    {
        double newLen = clipLenBeatsAtDown + dxBeats;
        if (snapToGrid) newLen = std::round(newLen);
        cm.lengthBeats = juce::jmax(1.0, newLen);
    }

    extendContentToMaxClip();
    if (onProjectChanged) onProjectChanged();
    layoutClips();
}

/*** viewport/frame ***/
void Arranger::frameAll()
{
    double maxEndBeats = 64.0;
    for (auto& t : tracks)
        for (auto& c : t.clips)
            maxEndBeats = std::max(maxEndBeats, c.startBeats + c.lengthBeats);

    const int viewportW = juce::jmax(1, view.getWidth() - TrackLaneComponent::headerWidth - 40);
    const double targetBeatsVisible = maxEndBeats + 8.0;
    const double ppbNow = juce::jlimit(minPixelsPerBeat, maxPixelsPerBeat,
                                       (double)viewportW / targetBeatsVisible);

    ppbAt120      = ppbNow * (bpmValue / 120.0);
    zoomScale     = 1.0;
    pixelsPerBeat = ppbNow;

    const int neededW = TrackLaneComponent::headerWidth
                      + (int) std::ceil(maxEndBeats * pixelsPerBeat) + 220;
    content.setSize(juce::jmax(view.getWidth(), neededW), content.getHeight());
    canvas.setBounds(0, 0, content.getWidth(), content.getHeight());
    layoutClips();
    view.setViewPosition(0, view.getViewPositionY());
    repaint();
}

void Arranger::timerCallback()
{
    autoScrollDuringDrag();
}

void Arranger::refreshFromModel()
{
    selectedClip = nullptr;
    selectedLaneIndex = -1;
    refreshAll();
}

/*** internals ***/
void Arranger::pushUndo(){ undoStack.push_back(tracks); redoStack.clear(); }
void Arranger::undo()    { if (undoStack.empty()) return; redoStack.push_back(tracks); tracks = std::move(undoStack.back()); undoStack.pop_back(); refreshAll(); }
void Arranger::redo()    { if (redoStack.empty()) return; undoStack.push_back(tracks); tracks = std::move(redoStack.back()); redoStack.pop_back(); refreshAll(); }

void Arranger::applyCursorForToolEverywhere()
{
    using MC = juce::MouseCursor;
    MC c = MC::NormalCursor;
    switch (tool)
    {
        case ArrangerTool::Pointer: c = MC::DraggingHandCursor;    break;
        case ArrangerTool::Slice:   c = MC::CrosshairCursor;       break;
        case ArrangerTool::Resize:  c = MC::LeftRightResizeCursor; break;
        case ArrangerTool::Zoom:    c = MC::PointingHandCursor;    break;
    }
    setMouseCursor(c); view.setMouseCursor(c); canvas.setMouseCursor(c);
    for (auto& up : clipComps) up->setActiveTool(tool);
}

void Arranger::refreshAll()
{
    // rebuild lanes
    for (auto& lc : laneComps) content.removeChildComponent(lc.get());
    laneComps.clear();

    for (size_t i=0;i<tracks.size();++i)
    {
        auto& tm = tracks[i];
        auto lane = std::make_unique<TrackLaneComponent>(
            tm,
            [this, i](TrackModel&) { setSelectedLane((int)i); selectedClip = nullptr; updateClipSelectionVisuals(); },
            [this](size_t idx){ duplicateTrack(idx); },
            [this](TrackLaneComponent&, int yInContent)
            {
                const int idx = laneIndexFromY(yInContent);
                if (draggingLaneIndex >= 0 && idx != draggingLaneIndex)
                {
                    std::swap(tracks[(size_t)draggingLaneIndex], tracks[(size_t)idx]);
                    draggingLaneIndex = idx;
                    refreshAll();
                }
            },
            [this, i](TrackLaneComponent&){ draggingLaneIndex = (int)i; },
            [this](TrackLaneComponent&){ draggingLaneIndex = -1; refreshAll(); },
            i
        );
        lane->setSelected((int)i == selectedLaneIndex);
        content.addAndMakeVisible(lane.get());
        laneComps.emplace_back(std::move(lane));
    }

    // rebuild clips
    for (auto& cc : clipComps) content.removeChildComponent(cc.get());
    clipComps.clear();

    for (auto& t : tracks)
        for (auto& c : t.clips)
        {
            auto* cc = new ClipComponent(c, fm, cache, bpmValue,
                [this]{ extendContentToMaxClip(); if (onProjectChanged) onProjectChanged(); });

            cc->addMouseListener(this, true);
            cc->setSelected(selectedClip == &c);
            content.addAndMakeVisible(cc);
            clipComps.emplace_back(cc);
            clipComps.back()->setActiveTool(tool);
        }

    // keep overlay & plus button on top
    content.addAndMakeVisible(canvas);
    content.addAndMakeVisible(plusButton);
    canvas.toFront(false);      // grid above lanes/clips
    plusButton.toFront(true);   // button above grid

    layoutLanes();
    layoutClips();
    extendContentToMaxClip();   // width only
    canvas.repaint();
    repaint();
}

void Arranger::updateClipSelectionVisuals()
{
    for (auto& up : clipComps)
        up->setSelected(selectedClip && (&up->model == selectedClip));
}

void Arranger::setSelectedLane(int idx)
{
    selectedLaneIndex = (idx >= 0 && idx < (int)tracks.size()) ? idx : -1;
    for (int i=0;i<(int)laneComps.size();++i)
        laneComps[(size_t)i]->setSelected(i == selectedLaneIndex);
    grabKeyboardFocus(); repaint();
}

int Arranger::laneIndexFromY(int yInContent) const
{
    const int rulerH = laneTop();
    int y = yInContent - rulerH;
    int laneH = laneHeight();
    if (y < 0) return 0;
    int idx = (y) / laneH;
    if (tracks.empty()) return 0;
    return juce::jlimit(0, (int)tracks.size()-1, idx);
}

int Arranger::laneIndexFromYAllowNew(int yInContent) const
{
    const int rulerH = laneTop();
    int y = yInContent - rulerH;
    int laneH = laneHeight();
    if (y < 0) return 0;
    if (tracks.empty()) return 0;
    const int idx = (y) / laneH;
    return juce::jlimit(0, (int)tracks.size(), idx);
}

void Arranger::layoutLanes()
{
    const int w = juce::jmax(content.getWidth(), view.getWidth()); // don't reduce width here
    int y = laneTop();

    for (auto& lc : laneComps)
    {
        lc->setBounds(0, y, w, laneHeight());
        y += laneHeight();
    }

    // "+" 6px under the last lane, centered in header column
    const int btnW = 28, btnH = 28;
    const int xBtn = juce::jmax(4, (TrackLaneComponent::headerWidth - btnW) / 2);
    const int yBtn = laneTop() + (int)tracks.size() * laneHeight() + 6;
    plusButton.setBounds(xBtn, yBtn, btnW, btnH);

    // height only; width handled elsewhere
    content.setSize(w, contentHeight());

    // overlay covers everything and draws on top
    canvas.setBounds(0, 0, content.getWidth(), content.getHeight());
    canvas.toFront(false);
    plusButton.toFront(true);
}

void Arranger::layoutClips()
{
    const int laneH = laneHeight();
    const int y0 = laneTop();

    auto yForTrack = [&](TrackModel* tPtr)
    {
        int laneIndex = 0;
        for (size_t i = 0; i < tracks.size(); ++i)
            if (&tracks[i] == tPtr) { laneIndex = (int)i; break; }
        return y0 + laneIndex * laneH;
    };

    for (auto& ccUP : clipComps)
    {
        auto& cc = *ccUP;

        // Resolve the owning track and the exact ClipModel *by address*
        TrackModel* track = nullptr;
        ClipModel*  cm    = nullptr;
        for (auto& t : tracks)
        {
            for (auto& c : t.clips)
            {
                if (&c == &cc.model) { track = &t; cm = &c; break; }
            }
            if (cm) break;
        }
        if (!cm || !track) continue;

        // while moving a clip, don't stomp the live preview bounds
        if (activeClip && (&(*ccUP) == activeClip) && dragging == DragMode::Move && pendingMoveValid)
            continue;

        const int x = canvas.xFromBeats(cm->startBeats);
        const int w = (int) std::round(cm->lengthBeats * pixelsPerBeat);
        const int y = yForTrack(track);

        cc.setBounds(x, y + 10, juce::jmax(24, w), laneH - 20);
    }
}

void Arranger::extendContentToMaxClip()
{
    double maxEndBeats = 64.0;
    for (auto& t : tracks)
        for (auto& c : t.clips)
            maxEndBeats = std::max(maxEndBeats, c.startBeats + c.lengthBeats);

    const int neededW = TrackLaneComponent::headerWidth
                      + (int)std::ceil(maxEndBeats * pixelsPerBeat) + 400;

    content.setSize(juce::jmax(neededW, content.getWidth()),
                    content.getHeight());

    canvas.setBounds(0, 0, content.getWidth(), content.getHeight());
    canvas.toFront(false);
    plusButton.toFront(true);
    repaint();
}

/*** mouse routing ***/
void Arranger::mouseDown(const juce::MouseEvent& e)
{
    const bool isMiddle = e.mods.isMiddleButtonDown();
    const bool isRight  = e.mods.isRightButtonDown();
    const bool isLeft   = e.mods.isLeftButtonDown();

    if (isMiddle)
    {
        panActive = true;
        panStartMouse = e.getEventRelativeTo(this).getPosition();
        panStartView  = view.getViewPosition();
        return;
    }

    const int yContent = e.getEventRelativeTo(&content).y;

    // Ruler: set/drag playhead (snap to beat) if not in Zoom tool
    if (tool != ArrangerTool::Zoom && yContent < laneTop())
    {
        const double beat = std::round(canvas.beatsFromX(e.getEventRelativeTo(&content).x));
        setPlayheadBeats(beat);
        if (onPlayheadSet) onPlayheadSet(beat);
        playheadDragActive = true;
        return;
    }

    // Zoom tool logic
    if (tool == ArrangerTool::Zoom)
    {
        if (isRight)
        {
            rightClickRestoreArm = true; // restore on mouseUp
            return;
        }

        if (isLeft)
        {
            // Start marquee selection
            selectionActive = true;
            const auto p = e.getEventRelativeTo(&content).getPosition();
            selectionStartContent = p;
            selectionRectContent  = juce::Rectangle<int>(p.x, p.y, 1, 1);
            canvas.setSelectionRect(selectionRectContent);

            // Save pre-zoom view once at the beginning of a zoom session
            if (!preZoomView)
            {
                preZoomView = ViewState{ pixelsPerBeat, zoomScale,
                                         view.getViewPositionX(), view.getViewPositionY() };
            }
            return;
        }
    }

    if (auto* cc = dynamic_cast<ClipComponent*>(e.eventComponent))
    {
        if (cc->model.kind == ClipModel::Kind::MidiLoop && e.getNumberOfClicks() >= 2)
        {
            if (onOpenMidiLoop)
                onOpenMidiLoop(cc->model.loopId);
            return;
        }

        // right-click deletes the clip (any tool)
        if (e.mods.isRightButtonDown())
        {
            rightClickRestoreArm = false;

            selectedClip = &cc->model;
            updateClipSelectionVisuals();

            removeClip(&cc->model);
            activeClip        = nullptr;
            dragging          = DragMode::None;
            pasteArm          = false;
            startLaneIndex    = -1;
            targetLaneIndex   = -1;
            return;
        }

        activeClip   = cc;
        selectedClip = &cc->model; 
        updateClipSelectionVisuals();
        setSelectedLane(trackIndexForClip(cc->model));

        startLaneIndex  = trackIndexForClip(cc->model);
        targetLaneIndex = startLaneIndex;

        pushUndo();

        if (tool == ArrangerTool::Resize)
        {
            if (cc->leftHandle().contains(e.getEventRelativeTo(cc).getPosition()))
                dragging = DragMode::Left;
            else if (cc->rightHandle().contains(e.getEventRelativeTo(cc).getPosition()))
                dragging = DragMode::Right;
            else
                dragging = DragMode::None;
        }
        else if (tool == ArrangerTool::Slice)
        {
            auto beat = canvas.beatsFromX(e.getEventRelativeTo(&content).x);
            if (snapToGrid) beat = std::round(beat);
            performSliceAtBeat(*cc, beat);
            return;
        }
        else
        {
            dragging = DragMode::Move;
            pendingMoveValid = false;
            activeClip->toFront(false); // keep above neighbours for smooth preview
        }

        dragStartPos          = e.getEventRelativeTo(&content).getPosition();
        lastDragViewportPos   = e.getEventRelativeTo(&view).getPosition();
        dragAutoScrollActive  = true;
        startTimerHz(60);
        clipStartBeatsAtDown  = cc->model.startBeats;
        clipLenBeatsAtDown    = cc->model.lengthBeats;
        clipOffsetBeatsAtDown = cc->model.offsetBeats;
    }
    else
    {
        const bool inLanes = (yContent >= laneTop());

        // Arm quick-paste ONLY for a LEFT click in lanes using the Pointer tool
        if (tool == ArrangerTool::Pointer && inLanes && e.mods.isLeftButtonDown())
        {
            pasteArm = true; // will paste on mouseUp if it was just a click
        }
        else
        {
            pasteArm = false;
            if (!(tool == ArrangerTool::Pointer && inLanes))
            {
                selectedClip = nullptr;
                updateClipSelectionVisuals();
            }
        }
    }
}

void Arranger::mouseDrag(const juce::MouseEvent& e)
{
    // If the mouse actually moved, cancel quick-paste
    if (pasteArm && e.getDistanceFromDragStart() > 3) pasteArm = false;

    if (panActive)
    {
        auto cur = e.getEventRelativeTo(this).getPosition();
        auto delta = cur - panStartMouse;
        view.setViewPosition(juce::jmax(0, panStartView.x - delta.x),
                             juce::jmax(0, panStartView.y - delta.y));
        return;
    }

    if (playheadDragActive)
    {
        if (!isPlayingQuery || !isPlayingQuery())
        {
            const double beat = std::round(canvas.beatsFromX(e.getEventRelativeTo(&content).x));
            setPlayheadBeats(beat);
            if (onPlayheadSet) onPlayheadSet(beat);
        }
        return;
    }

    if (tool == ArrangerTool::Zoom && selectionActive)
    {
        const auto cur = e.getEventRelativeTo(&content).getPosition();
        const int x0 = juce::jlimit(0, content.getWidth(), selectionStartContent.x);
        const int y0 = juce::jlimit(0, content.getHeight(), selectionStartContent.y);
        const int x1 = juce::jlimit(0, content.getWidth(), cur.x);
        const int y1 = juce::jlimit(0, content.getHeight(), cur.y);
        selectionRectContent = juce::Rectangle<int>::leftTopRightBottom(
            juce::jmin(x0, x1), juce::jmin(y0, y1), juce::jmax(x0, x1), juce::jmax(y0, y1));
        canvas.setSelectionRect(selectionRectContent);
        return;
    }

    if (!activeClip) return;

    const auto pos = e.getEventRelativeTo(&content).getPosition();
    lastDragViewportPos = e.getEventRelativeTo(&view).getPosition();
    dragAutoScrollActive = true;
    updateActiveClipDragAt(pos);
}

void Arranger::mouseUp(const juce::MouseEvent& e)
{
    const bool wasRight = e.mods.testFlags(juce::ModifierKeys::rightButtonModifier);

    panActive = false;
    playheadDragActive = false;

    // --- Zoom tool completion / restore
    if (tool == ArrangerTool::Zoom)
    {
        if (rightClickRestoreArm && wasRight)
        {
            rightClickRestoreArm = false;
            restorePreZoomView();
            return;
        }

        if (selectionActive)
        {
            const auto rect = selectionRectContent;
            selectionActive = false;
            canvas.setSelectionRect(std::nullopt);

            const bool tiny = rect.getWidth() < 6 && rect.getHeight() < 6;
            if (tiny)
            {
                // Step zoom at click position: two stages (x1 then x2)
                const int xContent = e.getEventRelativeTo(&content).x;
                const double stepFactor = 2.0;
                if (clickZoomStage < 2)
                {
                    const int contentW = juce::jmax(content.getWidth(), view.getWidth());
                    const int ax = juce::jlimit(TrackLaneComponent::headerWidth, contentW - 1, xContent);

                    const double oldPPB = pixelsPerBeat;
                    const double beat   = juce::jmax(0.0, (ax - TrackLaneComponent::headerWidth) / oldPPB);
                    const int    xBefore= TrackLaneComponent::headerWidth + (int)std::round(beat * oldPPB);

                    pixelsPerBeat = juce::jlimit(minPixelsPerBeat, maxPixelsPerBeat, pixelsPerBeat * stepFactor);
                    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));

                    layoutClips();
                    extendContentToMaxClip();

                    const int xAfter = TrackLaneComponent::headerWidth + (int)std::round(beat * pixelsPerBeat);
                    const int dx     = xAfter - xBefore;

                    auto p   = view.getViewPosition();
                    const int maxX = juce::jmax(0, content.getWidth() - view.getWidth());
                    view.setViewPosition(juce::jlimit(0, maxX, p.getX() + dx), p.getY());

                    ++clickZoomStage;
                    canvas.repaint(); repaint();
                }
            }
            else
            {
                clickZoomStage = 1;
                zoomToSelectionAndCenter(rect);
            }
            return;
        }
    }

    // Left-click quick paste on grid (requires an already selected clip)
    if (tool != ArrangerTool::Zoom && pasteArm && selectedClip != nullptr)
    {
        if (!e.mouseWasDraggedSinceMouseDown())
        {
            const auto pos = e.getEventRelativeTo(&content).getPosition();

            if (pos.y >= laneTop())
            {
                if (tracks.empty())
                    addTrack("Audio 1");

                pushUndo();

                ClipModel copy = *selectedClip;
                copy.id = juce::Uuid().toString();

                double beat = canvas.beatsFromX(pos.x);
                if (snapToGrid) beat = std::round(beat);
                copy.startBeats = juce::jmax(0.0, beat);

                int laneIdx = laneIndexFromYAllowNew(pos.y);
                if (laneIdx == (int)tracks.size())
                    addTrack("Audio " + juce::String((int)tracks.size() + 1));
                laneIdx = juce::jlimit(0, (int)tracks.size() - 1, laneIdx);

                auto& lane = tracks[(size_t)laneIdx];
                lane.clips.push_back(std::move(copy));

                selectedClip = &lane.clips.back();
                refreshAll();
                if (onProjectChanged) onProjectChanged();

                activeClip = nullptr;
                dragging = DragMode::None;
                startLaneIndex = targetLaneIndex = -1;
                pasteArm = false;
                layoutClips();
                return;
            }
        }
    }
    pasteArm = false;

    if (activeClip)
    {
        if (dragging == DragMode::Move && targetLaneIndex >= 0)
        {
            if (targetLaneIndex == (int)tracks.size())
                addTrack("Audio " + juce::String((int)tracks.size() + 1));

            if (targetLaneIndex != startLaneIndex && selectedClip)
                moveClipToLane(selectedClip, startLaneIndex, targetLaneIndex);

            setSelectedLane(juce::jmin(targetLaneIndex, (int)tracks.size() - 1));

            // Commit the final position (snap here, once)
            if (pendingMoveValid && selectedClip)
            {
                double committed = pendingMoveStartBeats;
                if (snapToGrid) committed = std::round(committed);
                selectedClip->startBeats = juce::jmax(0.0, committed);
            }

            extendContentToMaxClip();
            layoutClips();
            if (onProjectChanged) onProjectChanged();
        }
    }

    activeClip = nullptr;
    dragging = DragMode::None;
    dragAutoScrollActive = false;
    stopTimer();
    startLaneIndex = targetLaneIndex = -1;
    pendingMoveValid = false;

    layoutClips();
}

/*** wheel handling ***/
void Arranger::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Ctrl + wheel = zoom at cursor anywhere
    if (e.mods.isCtrlDown())
    {
        const int xContent = e.getEventRelativeTo(&content).x;
        applyZoomAtContentX(wheel.deltaY, view.getViewPositionX() + xContent);
        return;
    }

    if (tool == ArrangerTool::Zoom)
    {
        // Vertical navigation between rows while in zoom mode
        auto p = view.getViewPosition();
        const int dy = (int)std::round(-wheel.deltaY * 240.0);
        const int maxY = juce::jmax(0, content.getHeight() - view.getHeight());
        view.setViewPosition(p.getX(), juce::jlimit(0, maxY, p.getY() + dy));
        return;
    }

    juce::Component::mouseWheelMove(e, wheel);
}

/*** model helpers ***/
int Arranger::trackIndexForClip(ClipModel& cm)
{
    for (size_t i=0;i<tracks.size();++i)
        for (auto& c : tracks[i].clips)
            if (&c == &cm) return (int)i;
    return 0;
}

void Arranger::moveClipToLane(ClipModel* cm, int fromLane, int toLane)
{
    if (!cm) return;
    if (fromLane < 0 || toLane < 0 || fromLane >= (int)tracks.size() || toLane >= (int)tracks.size()) return;
    if (fromLane == toLane) return;

    auto& src = tracks[(size_t)fromLane].clips;
    auto it = std::find_if(src.begin(), src.end(),
                           [&](ClipModel& c){ return &c == cm; });
    if (it == src.end()) return;

    ClipModel moved = *it;
    src.erase(it);
    tracks[(size_t)toLane].clips.push_back(std::move(moved));

    refreshAll();
    if (onProjectChanged) onProjectChanged();
}

void Arranger::recomputeClipBeatLengthsForTempo()
{
    for (auto& t : tracks)
        for (auto& c : t.clips)
        {
            if (!c.file.existsAsFile()) continue;

            const double secs = ArrangerFileUtils::secondsOfFile(fm, c.file);
            if (secs <= 0.0) continue;

            double srcBpm = ArrangerFileUtils::detectLoopBpm(fm, c.file);
            if (srcBpm <= 0.0) srcBpm = bpmValue;

            const double beats = (secs * srcBpm) / 60.0;

            double rounded = std::round(beats * 4.0) / 4.0;  // 16th grid
            const double bars = rounded / 4.0;
            const double nearestCommon = std::round(bars);
            if (std::abs(nearestCommon - bars) < 0.08)
                rounded = nearestCommon * 4.0;

            c.lengthBeats = juce::jmax(1.0, rounded);
        }

    layoutClips();
    extendContentToMaxClip();
}

void Arranger::performSliceAtBeat(ClipComponent& cc, double beat)
{
    ClipModel& clip = cc.model;
    const double clipStart = clip.startBeats;
    const double clipEnd   = clip.startBeats + clip.lengthBeats;
    if (beat <= clipStart || beat >= clipEnd) return;

    for (auto& t : tracks)
    {
        for (auto it = t.clips.begin(); it != t.clips.end(); ++it)
        {
            if (&(*it) == &clip)
            {
                const double leftLen  = beat - clipStart;
                const double rightLen = clipEnd - beat;

                it->lengthBeats = leftLen;

                ClipModel right;
                right.id          = juce::Uuid().toString();
                right.file        = it->file;
                right.startBeats  = beat;
                right.lengthBeats = rightLen;
                right.offsetBeats = it->offsetBeats + (beat - it->startBeats);

                t.clips.insert(it + 1, std::move(right));

                refreshAll();
                if (onProjectChanged) onProjectChanged();
                return;
            }
        }
    }
}

/*** buttons ***/
void Arranger::buttonClicked(juce::Button* b)
{
    if (b == &btnPointer)      setTool(ArrangerTool::Pointer);
    else if (b == &btnSlice)   setTool(btnSlice.getToggleState() && tool!=ArrangerTool::Slice ? ArrangerTool::Slice : ArrangerTool::Pointer);
    else if (b == &btnResize)  setTool(btnResize.getToggleState() && tool!=ArrangerTool::Resize? ArrangerTool::Resize: ArrangerTool::Pointer);
    else if (b == &btnZoomTool)setTool(btnZoomTool.getToggleState()&& tool!=ArrangerTool::Zoom  ? ArrangerTool::Zoom  : ArrangerTool::Pointer);
    else if (b == &btnFrameAll)frameAll();
    else if (b == &btnSnap)    setSnap(btnSnap.getToggleState());
    else if (b == &btnZoomIn)  zoomAtViewportCenter(+1.0);
    else if (b == &btnZoomOut) zoomAtViewportCenter(-1.0);
    else if (b == &btnLoopIcon || b == &btnLoops) { if (onLoopsClicked) onLoopsClicked(); }
    else if (b == &btnEddie)   { if (onEddieClicked) onEddieClicked(); }
}
