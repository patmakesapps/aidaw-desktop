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
    canvas.setInterceptsMouseClicks(false, false);

    // Route background (empty grid) clicks to Arranger, too
    content.addMouseListener(this, true);

    // Pinned header overlay (always visible while horizontally scrolling).
    addAndMakeVisible(headerStrip);
    headerStrip.toFront(false);

    // Sync the header overlay's vertical position with the viewport.
    view.onVisibleAreaChanged = [this](const juce::Rectangle<int>& r)
    {
        headerStrip.setVerticalScroll(r.getY());
    };

    // "+" button wiring (lives inside the pinned header strip)
    headerStrip.plusButton.onClick = [this]
    {
        addTrack("Audio " + juce::String((int)tracks.size() + 1));
        view.setViewPosition(view.getViewPositionX(),
                             juce::jmax(0, contentHeight() - view.getHeight()));
    };

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
    headerStrip.plusButton.setColour(juce::TextButton::buttonColourId,    juce::Colour(Theme::colBtnIdle));
    headerStrip.plusButton.setColour(juce::TextButton::textColourOffId,   juce::Colour(Theme::colTextDim));
    for (auto& lane : laneComps)
        if (lane) lane->repaint();
    content.repaint();
    headerStrip.repaint();
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

juce::Point<int> Arranger::contentPointFromScreen(const juce::MouseEvent& e) const
{
    const auto p = e.getScreenPosition();
    return { p.getX() - content.getScreenX(), p.getY() - content.getScreenY() };
}

juce::Point<int> Arranger::viewPointFromScreen(const juce::MouseEvent& e) const
{
    const auto p = e.getScreenPosition();
    return { p.getX() - view.getScreenX(), p.getY() - view.getScreenY() };
}

double Arranger::snapBeat(double beat, bool bypassSnap) const
{
    beat = juce::jmax(0.0, beat);

    const double quantum = Theme::snapQuantumBeats(pixelsPerBeat, snapToGrid && !bypassSnap);
    if (quantum <= 0.0)
        return beat;

    return juce::jmax(0.0, std::round(beat / quantum) * quantum);
}

double Arranger::minimumClipLengthBeats(bool bypassSnap) const
{
    const double quantum = Theme::snapQuantumBeats(pixelsPerBeat, snapToGrid && !bypassSnap);
    return juce::jmax(1.0 / 16.0, quantum > 0.0 ? quantum : 1.0 / 16.0);
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

    const int contentX = x - view.getX() + view.getViewPositionX();
    const double startBeats = snapBeat(canvas.beatsFromX(contentX));

    const int yContent = y - view.getY() + view.getViewPositionY();
    int laneIdx = laneIndexFromYAllowNew(yContent);
    bool changed = false;
    juce::String selectedId;
    int selectedLaneAfterDrop = -1;

    // Stack multiple files end-to-end on the same lane (FL/Ableton-style),
    // rather than splitting each onto a separate track.
    double cursorBeats = startBeats;

    for (int i=0;i<files.size();++i)
    {
        const juce::File f(files[i]);
        if (!f.existsAsFile()) continue;

        if (!changed)
        {
            pushUndo();
            changed = true;
        }

        while (laneIdx >= (int)tracks.size())
            addTrack("Audio " + juce::String((int)tracks.size()+1), /*push=*/false);

        ClipModel clip;
        clip.id = juce::Uuid().toString();
        clip.kind = ClipModel::Kind::Audio;
        clip.file = f;
        clip.startBeats  = cursorBeats;
        clip.lengthBeats = ArrangerFileUtils::estimateBeatsFromFile(fm, f, bpmValue);
        clip.offsetBeats = 0.0;
        clip.showImportSpinner = true;

        auto& lane = tracks[(size_t)laneIdx];

        // Only adopt the file's name as the track name if the lane is empty
        // and still has its default placeholder name. Don't clobber an
        // existing track with clips on it.
        const bool laneIsDefault = lane.clips.empty()
            && (lane.name.startsWith("Track ") || lane.name.startsWith("Audio")
                || lane.name.startsWith("MIDI") || lane.name.isEmpty());
        if (laneIsDefault)
        {
            auto name = ArrangerFileUtils::extractTrackNameFromAudio(fm, f);
            lane.name = name.isNotEmpty() ? name : f.getFileNameWithoutExtension();
        }

        const double clipLen = clip.lengthBeats;
        lane.clips.push_back(std::move(clip));
        selectedId = lane.clips.back().id;
        selectedLaneAfterDrop = laneIdx;

        // Advance cursor so subsequent files in the same drop stack
        // end-to-end after this clip on the same lane.
        cursorBeats += juce::jmax(1.0, clipLen);
    }

    if (!changed)
        return;

    selectedClip = nullptr;
    selectedLaneIndex = selectedLaneAfterDrop;
    for (size_t ti = 0; ti < tracks.size(); ++ti)
        for (auto& c : tracks[ti].clips)
            if (c.id == selectedId)
            {
                selectedClip = &c;
                selectedLaneIndex = (int)ti;
                break;
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
    double beat = snapBeat(canvas.beatsFromX(contentX));

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
    // Breathing room between the toolbar and the timeline ruler, and again
    // below the horizontal scrollbar — without the bottom gap the ruler
    // and scrollbar read as a single dark band on first paint.
    r.removeFromTop(6);
    r.removeFromBottom(8);
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

    // Pinned header overlay sits ON TOP of the viewport's left strip so
    // track names remain visible while the user scrolls horizontally.
    headerStrip.setBounds(view.getX(), view.getY(),
                          TrackLaneComponent::headerWidth, view.getHeight());
    headerStrip.toFront(false);
    headerStrip.setLaneTop(laneTop());
    headerStrip.setLaneHeight(laneHeight());
    headerStrip.setVerticalScroll(view.getViewPositionY());

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
    clip.startBeats = snapBeat(startBeats);
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
    const int xInView = screenX - view.getScreenX();
    const int anchorX = view.getViewPositionX() + juce::jlimit(0, view.getWidth(), xInView);
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

    const juce::Rectangle<int> gridArea(TrackLaneComponent::headerWidth, 0,
                                        juce::jmax(0, content.getWidth() - TrackLaneComponent::headerWidth),
                                        content.getHeight());
    const auto clipped = rectContent.getIntersection(gridArea);
    if (clipped.getWidth() < 24)
        return;

    bool hasAnyClip = false;
    bool intersectsClip = false;
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const int y = laneTop() + (int)i * laneHeight() + 10;
        for (const auto& c : tracks[i].clips)
        {
            hasAnyClip = true;
            const int x = canvas.xFromBeats(c.startBeats);
            const int w = juce::jmax(24, (int)std::round(c.lengthBeats * pixelsPerBeat));
            const juce::Rectangle<int> clipRect(x, y, w, laneHeight() - 20);
            if (clipRect.intersects(clipped))
            {
                intersectsClip = true;
                break;
            }
        }
        if (intersectsClip)
            break;
    }

    if (hasAnyClip && !intersectsClip)
        return;

    const double leftBeat = canvas.beatsFromX(clipped.getX());
    const double rightBeat = canvas.beatsFromX(clipped.getRight());
    const double beatSpan = juce::jmax(4.0, rightBeat - leftBeat);
    const double anchorBeat = (leftBeat + rightBeat) * 0.5;

    const int viewportW = juce::jmax(1, view.getWidth() - TrackLaneComponent::headerWidth - 32);
    const double targetPPB = juce::jlimit(minPixelsPerBeat, maxPixelsPerBeat,
                                          (double)viewportW / beatSpan);

    pixelsPerBeat = targetPPB;
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
    layoutClips();
    extendContentToMaxClip();

    const int desiredContentCenter = TrackLaneComponent::headerWidth
                                   + (int)std::round(anchorBeat * pixelsPerBeat);
    const int newViewX = juce::jlimit(0,
        juce::jmax(0, content.getWidth() - view.getWidth()),
        desiredContentCenter - view.getWidth() / 2);

    int newViewY = view.getViewPositionY();
    if (clipped.getHeight() >= 24)
    {
        newViewY = juce::jlimit(0,
            juce::jmax(0, content.getHeight() - view.getHeight()),
            clipped.getCentreY() - view.getHeight() / 2);
    }

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
    clampViewPosition();

    // Clear selection and state
    selectionActive = false;
    canvas.setSelectionRect(std::nullopt);
    clickZoomStage = 0;
    canvas.repaint();
    repaint();
}

void Arranger::applyVerticalZoom(double wheelDelta, int anchorYContent, int anchorYInView)
{
    const int oldH = laneHeight();
    if (oldH <= 0)
        return;

    const int laneCount = juce::jmax(1, (int)tracks.size());
    const int anchorLane = juce::jlimit(0, laneCount - 1,
                                        (anchorYContent - laneTop()) / oldH);
    const int anchorOffsetInLane = juce::jlimit(0, oldH,
                                                anchorYContent - laneTop() - anchorLane * oldH);

    const double scaled = oldH * std::pow(1.35, wheelDelta);
    int newH = juce::jlimit(kMinLaneHeight, kMaxLaneHeight, (int)std::round(scaled));

    if (newH == oldH && std::abs(wheelDelta) > 0.0)
        newH = juce::jlimit(kMinLaneHeight, kMaxLaneHeight, oldH + (wheelDelta > 0.0 ? 4 : -4));

    if (newH == oldH)
        return;

    laneHeightPx = newH;
    layoutLanes();
    layoutClips();
    extendContentToMaxClip();

    const double offsetRatio = (double)anchorOffsetInLane / (double)oldH;
    const int newAnchorYContent = laneTop() + anchorLane * newH
                                + (int)std::round(offsetRatio * newH);
    const int maxY = juce::jmax(0, content.getHeight() - view.getHeight());
    view.setViewPosition(view.getViewPositionX(),
                         juce::jlimit(0, maxY, newAnchorYContent - anchorYInView));

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
    updateActiveClipDragAt({ p.x + lastDragViewportPos.x, p.y + lastDragViewportPos.y },
                           lastDragBypassSnap);
}

void Arranger::updateActiveClipDragAt(juce::Point<int> pos, bool bypassSnap)
{
    if (!activeClip)
        return;
    if (dragging == DragMode::None)
        return;

    const int dxPx = pos.x - dragStartPos.x;
    const double dxBeats = dxPx / pixelsPerBeat;
    auto& cm = activeClip->model;

    if (dragging == DragMode::Move)
    {
        double preview = clipStartBeatsAtDown + dxBeats;
        preview = snapBeat(juce::jlimit(0.0, 100000.0, preview), bypassSnap);

        pendingMoveStartBeats = preview;
        pendingMoveValid = true;

        const int laneH = laneHeight();
        const int clipVisualHeight = laneH - 20;

        // Clip follows the mouse cursor 1:1 — no lane snapping during the
        // drag. The clip stays exactly where the user is pointing it. The
        // final lane snap happens on mouse release in mouseUp / refreshAll.
        const int grabOffsetInClip = dragStartPos.y - (laneTop() + startLaneIndex * laneH + 10);
        const int desiredClipTop   = pos.y - grabOffsetInClip;

        // Target lane is wherever the clip's center is currently over,
        // used by mouseUp to commit the move to the model.
        const int desiredClipCenter = desiredClipTop + clipVisualHeight / 2;
        const int laneFromCenter = (int) std::floor(
            (double)(desiredClipCenter - laneTop()) / (double) juce::jmax(1, laneH));
        targetLaneIndex = juce::jlimit(0, (int)tracks.size(), laneFromCenter);

        const int w = (int)std::round(cm.lengthBeats * pixelsPerBeat);
        const int x = canvas.xFromBeats(preview);

        activeClip->setBounds(x, desiredClipTop, juce::jmax(24, w), clipVisualHeight);
        return;
    }

    if (dragging == DragMode::Left)
    {
        const double oldEnd = clipStartBeatsAtDown + clipLenBeatsAtDown;
        const double minLen = minimumClipLengthBeats(bypassSnap);
        double newStart = snapBeat(clipStartBeatsAtDown + dxBeats, bypassSnap);
        newStart = juce::jlimit(0.0, oldEnd - minLen, newStart);
        const double newLen = oldEnd - newStart;

        const double delta = newStart - clipStartBeatsAtDown;
        cm.startBeats = newStart;
        cm.lengthBeats = newLen;
        cm.offsetBeats = juce::jmax(0.0, clipOffsetBeatsAtDown + delta);
    }
    else if (dragging == DragMode::Right)
    {
        const double minLen = minimumClipLengthBeats(bypassSnap);
        const double oldEnd = clipStartBeatsAtDown + clipLenBeatsAtDown;
        double newEnd = snapBeat(oldEnd + dxBeats, bypassSnap);
        newEnd = juce::jmax(clipStartBeatsAtDown + minLen, newEnd);
        cm.lengthBeats = newEnd - clipStartBeatsAtDown;
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
    const double targetBeatsVisible = juce::jmax(16.0, maxEndBeats + 4.0);
    const double ppbNow = juce::jlimit(minPixelsPerBeat, maxPixelsPerBeat,
                                       (double)viewportW / targetBeatsVisible);

    pixelsPerBeat = ppbNow;
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));

    if (!tracks.empty())
    {
        const int availableTrackH = juce::jmax(kMinLaneHeight,
                                               view.getHeight() - laneTop() - 40);
        laneHeightPx = juce::jlimit(kMinLaneHeight, kDefaultLaneHeight,
                                    availableTrackH / (int)tracks.size());
    }

    preZoomView.reset();
    selectionActive = false;
    canvas.setSelectionRect(std::nullopt);
    layoutLanes();
    layoutClips();
    extendContentToMaxClip();
    view.setViewPosition(0, 0);
    canvas.repaint();
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

    // rebuild pinned header rows in lockstep
    std::vector<std::unique_ptr<TrackHeaderRow>> newHeaderRows;
    newHeaderRows.reserve(tracks.size());

    for (size_t i=0;i<tracks.size();++i)
    {
        auto& tm = tracks[i];
        auto lane = std::make_unique<TrackLaneComponent>(
            tm,
            [this, i](TrackModel&) { setSelectedLane((int)i); selectedClip = nullptr; updateClipSelectionVisuals(); },
            [this](size_t idx){ duplicateTrack(idx); },
            [this](TrackLaneComponent&, int yInContent)
            {
                updateTrackReorder(yInContent);
            },
            [this, i](TrackLaneComponent&){ beginTrackReorder((int)i); },
            [this](TrackLaneComponent&){ endTrackReorder(); },
            i
        );
        lane->setSelected((int)i == selectedLaneIndex);
        content.addAndMakeVisible(lane.get());
        laneComps.emplace_back(std::move(lane));

        auto headerRow = std::make_unique<TrackHeaderRow>(
            tm,
            [this, i](TrackModel&) { setSelectedLane((int)i); selectedClip = nullptr; updateClipSelectionVisuals(); },
            [this](size_t idx){ duplicateTrack(idx); },
            [this](TrackHeaderRow&, int yInStrip)
            {
                // yInStrip is in HeaderStrip-local coords; convert to content Y.
                updateTrackReorder(yInStrip + view.getViewPositionY());
            },
            [this, i](TrackHeaderRow&){ beginTrackReorder((int)i); },
            [this](TrackHeaderRow&){ endTrackReorder(); },
            i
        );
        headerRow->setSelected((int)i == selectedLaneIndex);
        newHeaderRows.emplace_back(std::move(headerRow));
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

    // keep overlay on top of content
    content.addAndMakeVisible(canvas);
    canvas.toFront(false);      // grid above lanes/clips

    // hand the new rows to the pinned strip
    headerStrip.setLaneTop(laneTop());
    headerStrip.setLaneHeight(laneHeight());
    headerStrip.setRows(std::move(newHeaderRows));
    headerStrip.setVerticalScroll(view.getViewPositionY());
    headerStrip.toFront(false);

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
    for (int i=0;i<headerStrip.rowCount();++i)
        if (auto* hr = headerStrip.row(i))
            hr->setSelected(i == selectedLaneIndex);
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

int Arranger::reorderInsertIndexFromY(int yInContent) const
{
    if (tracks.empty())
        return 0;

    const int laneH = juce::jmax(1, laneHeight());
    const int y = yInContent - laneTop();
    const int idx = (int) std::floor(((double) y + laneH * 0.5) / (double) laneH);
    return juce::jlimit(0, (int) tracks.size(), idx);
}

void Arranger::beginTrackReorder(int index)
{
    if (index < 0 || index >= (int) tracks.size())
        return;

    draggingLaneIndex = index;
    reorderInsertIndex = index;

    for (int i = 0; i < (int) laneComps.size(); ++i)
        if (auto& lane = laneComps[(size_t) i])
            lane->setAlpha(i == draggingLaneIndex ? 0.55f : 1.0f);

    const int yContent = laneTop() + index * laneHeight() + laneHeight() / 2;
    headerStrip.setReorderPreview(draggingLaneIndex, reorderInsertIndex,
                                  yContent - view.getViewPositionY());
    canvas.setTrackReorderIndicator(laneTop() + reorderInsertIndex * laneHeight());
}

void Arranger::updateTrackReorder(int yInContent)
{
    if (draggingLaneIndex < 0)
        return;

    reorderInsertIndex = reorderInsertIndexFromY(yInContent);

    headerStrip.setReorderPreview(draggingLaneIndex, reorderInsertIndex,
                                  yInContent - view.getViewPositionY());
    canvas.setTrackReorderIndicator(laneTop() + reorderInsertIndex * laneHeight());
}

void Arranger::endTrackReorder()
{
    if (draggingLaneIndex < 0)
        return;

    const int source = draggingLaneIndex;
    const int insert = juce::jlimit(0, (int) tracks.size(), reorderInsertIndex);

    draggingLaneIndex = -1;
    reorderInsertIndex = -1;

    headerStrip.clearReorderPreview();
    canvas.setTrackReorderIndicator(std::nullopt);

    for (auto& lane : laneComps)
        if (lane) lane->setAlpha(1.0f);

    if (insert == source || insert == source + 1)
    {
        repaint();
        return;
    }

    pushUndo();

    auto moved = std::move(tracks[(size_t) source]);
    tracks.erase(tracks.begin() + source);

    const int destination = insert > source ? insert - 1 : insert;
    tracks.insert(tracks.begin() + destination, std::move(moved));

    if (selectedLaneIndex == source)
        selectedLaneIndex = destination;
    else if (source < selectedLaneIndex && selectedLaneIndex < insert)
        --selectedLaneIndex;
    else if (insert <= selectedLaneIndex && selectedLaneIndex < source)
        ++selectedLaneIndex;

    if (onProjectChanged)
        onProjectChanged();

    refreshAll();
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

    // height only; width handled elsewhere
    content.setSize(w, contentHeight());

    // overlay covers everything and draws on top
    canvas.setBounds(0, 0, content.getWidth(), content.getHeight());
    canvas.toFront(false);

    // Pinned strip mirrors the lane geometry.
    headerStrip.setLaneTop(laneTop());
    headerStrip.setLaneHeight(laneHeight());
    headerStrip.setVerticalScroll(view.getViewPositionY());
    headerStrip.toFront(false);

    clampViewPosition();
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

    if (activeClip != nullptr && pendingMoveValid)
        maxEndBeats = std::max(maxEndBeats,
                               pendingMoveStartBeats + activeClip->model.lengthBeats);

    const int neededW = TrackLaneComponent::headerWidth
                      + (int)std::ceil(maxEndBeats * pixelsPerBeat) + 400;

    const int minW = juce::jmax(1400, view.getWidth());
    content.setSize(juce::jmax(neededW, minW),
                    content.getHeight());

    canvas.setBounds(0, 0, content.getWidth(), content.getHeight());
    canvas.toFront(false);
    headerStrip.toFront(false);
    clampViewPosition();
    repaint();
}

void Arranger::clampViewPosition()
{
    const int maxX = juce::jmax(0, content.getWidth() - view.getWidth());
    const int maxY = juce::jmax(0, content.getHeight() - view.getHeight());
    const int x = juce::jlimit(0, maxX, view.getViewPositionX());
    const int y = juce::jlimit(0, maxY, view.getViewPositionY());

    if (x != view.getViewPositionX() || y != view.getViewPositionY())
        view.setViewPosition(x, y);
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

    const auto contentPosAtDown = contentPointFromScreen(e);
    const int yContent = contentPosAtDown.y;

    // Ruler: set/drag playhead (snap to beat) if not in Zoom tool
    if (tool != ArrangerTool::Zoom && yContent < laneTop())
    {
        const double beat = snapBeat(canvas.beatsFromX(contentPosAtDown.x), e.mods.isAltDown());
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
            selectionStartContent = contentPosAtDown;
            selectionRectContent  = juce::Rectangle<int>(contentPosAtDown.x, contentPosAtDown.y, 1, 1);
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

        if (tool == ArrangerTool::Resize)
        {
            if (cc->leftHandle().contains(e.getEventRelativeTo(cc).getPosition()))
                dragging = DragMode::Left;
            else if (cc->rightHandle().contains(e.getEventRelativeTo(cc).getPosition()))
                dragging = DragMode::Right;
            else
            {
                dragging = DragMode::None;
                activeClip = nullptr;
                return;
            }
        }
        else if (tool == ArrangerTool::Slice)
        {
            auto beat = canvas.beatsFromX(contentPosAtDown.x);
            beat = snapBeat(beat, e.mods.isAltDown());
            pushUndo();
            performSliceAtBeat(*cc, beat);
            activeClip = nullptr;
            return;
        }
        else
        {
            dragging = DragMode::Move;
            pendingMoveValid = false;
            activeClip->toFront(false); // keep above neighbours for smooth preview
        }

        pushUndo();
        activeClip->setDragPreview(true);

        dragStartPos          = contentPosAtDown;
        lastDragViewportPos   = viewPointFromScreen(e);
        lastDragBypassSnap    = e.mods.isAltDown();
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
            const double beat = snapBeat(canvas.beatsFromX(contentPointFromScreen(e).x),
                                         e.mods.isAltDown());
            setPlayheadBeats(beat);
            if (onPlayheadSet) onPlayheadSet(beat);
        }
        return;
    }

    if (tool == ArrangerTool::Zoom && selectionActive)
    {
        const auto cur = contentPointFromScreen(e);
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

    const auto pos = contentPointFromScreen(e);
    lastDragViewportPos = viewPointFromScreen(e);
    lastDragBypassSnap = e.mods.isAltDown();
    dragAutoScrollActive = true;
    updateActiveClipDragAt(pos, lastDragBypassSnap);
}

void Arranger::mouseUp(const juce::MouseEvent& e)
{
    panActive = false;
    playheadDragActive = false;

    // --- Zoom tool completion / restore
    if (tool == ArrangerTool::Zoom)
    {
        if (rightClickRestoreArm)
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

            const bool tiny = rect.getWidth() < 24;
            if (tiny)
            {
                clickZoomStage = 0;
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
            const auto pos = contentPointFromScreen(e);

            if (pos.y >= laneTop())
            {
                if (tracks.empty())
                    addTrack("Audio 1", false);

                pushUndo();

                ClipModel copy = *selectedClip;
                copy.id = juce::Uuid().toString();
                copy.showImportSpinner = false;

                copy.startBeats = snapBeat(canvas.beatsFromX(pos.x), e.mods.isAltDown());

                int laneIdx = laneIndexFromYAllowNew(pos.y);
                if (laneIdx == (int)tracks.size())
                    addTrack("Audio " + juce::String((int)tracks.size() + 1), false);
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
        activeClip->setDragPreview(false);

        if (dragging == DragMode::Move && targetLaneIndex >= 0)
        {
            bool changed = false;
            const juce::String movingClipId = selectedClip != nullptr ? selectedClip->id : juce::String();

            if (pendingMoveValid && selectedClip)
            {
                if (std::abs(selectedClip->startBeats - pendingMoveStartBeats) > 1.0e-6)
                    changed = true;

                selectedClip->startBeats = juce::jmax(0.0, pendingMoveStartBeats);
            }

            if (targetLaneIndex == (int)tracks.size())
            {
                TrackModel t;
                t.id = juce::Uuid().toString();
                t.name = "Audio " + juce::String((int)tracks.size() + 1);
                tracks.emplace_back(std::move(t));

                if (movingClipId.isNotEmpty())
                {
                    selectedClip = nullptr;
                    for (auto& track : tracks)
                        for (auto& clip : track.clips)
                            if (clip.id == movingClipId)
                                selectedClip = &clip;
                }

                changed = true;
            }

            if (targetLaneIndex != startLaneIndex && selectedClip)
            {
                selectedClip = moveClipToLane(selectedClip, startLaneIndex, targetLaneIndex);
                activeClip = nullptr;
                changed = true;
            }

            setSelectedLane(juce::jmin(targetLaneIndex, (int)tracks.size() - 1));

            pendingMoveValid = false;
            extendContentToMaxClip();
            if (activeClip == nullptr)
                refreshAll();
            else
                layoutClips();
            if (changed)
            {
                if (onProjectChanged) onProjectChanged();
            }
            else if (!undoStack.empty())
            {
                undoStack.pop_back();
            }
        }
    }

    activeClip = nullptr;
    dragging = DragMode::None;
    dragAutoScrollActive = false;
    stopTimer();
    startLaneIndex = targetLaneIndex = -1;
    pendingMoveValid = false;
    lastDragBypassSnap = false;

    layoutClips();
}

/*** wheel handling ***/
void Arranger::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Ctrl + wheel = zoom at cursor anywhere
    if (e.mods.isCtrlDown())
    {
        applyZoomAtContentX(wheel.deltaY, e.getEventRelativeTo(&content).x);
        return;
    }

    // Alt + wheel = vertical playlist zoom, matching common DAW/FL workflow.
    if (e.mods.isAltDown())
    {
        applyVerticalZoom(wheel.deltaY,
                          e.getEventRelativeTo(&content).y,
                          e.getEventRelativeTo(&view).y);
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

ClipModel* Arranger::moveClipToLane(ClipModel* cm, int fromLane, int toLane)
{
    if (!cm) return nullptr;
    if (fromLane < 0 || toLane < 0 || fromLane >= (int)tracks.size() || toLane >= (int)tracks.size()) return cm;
    if (fromLane == toLane) return cm;

    auto& src = tracks[(size_t)fromLane].clips;
    auto it = std::find_if(src.begin(), src.end(),
                           [&](ClipModel& c){ return &c == cm; });
    if (it == src.end()) return cm;

    ClipModel moved = *it;
    src.erase(it);
    tracks[(size_t)toLane].clips.push_back(std::move(moved));
    auto* movedClip = &tracks[(size_t)toLane].clips.back();
    selectedClip = movedClip;
    return movedClip;
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

                ClipModel right = *it;
                right.id          = juce::Uuid().toString();
                right.startBeats  = beat;
                right.lengthBeats = rightLen;
                right.offsetBeats = it->offsetBeats + (beat - it->startBeats);
                right.showImportSpinner = false;

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
