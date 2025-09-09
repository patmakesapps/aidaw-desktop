#include "Arranger.h"

Arranger::Arranger()
    : canvas(tracks, bpmValue, snapToGrid, pixelsPerBeat, playheadBeats,
             [this]{ if(onProjectChanged) onProjectChanged(); })
{
    setWantsKeyboardFocus(true);
    fm.registerBasicFormats();

    // Buttons + ASCII tooltips (no mojibake)
    btnPointer.setButtonText(U8(u8"⬚"));
    btnSlice.setButtonText  (U8(u8"✂"));
    btnResize.setButtonText (U8(u8"↔"));
    btnZoomTool.setButtonText(U8(u8"🔍"));
    btnFrameAll.setButtonText(U8(u8"◰"));
    btnSnap.setButtonText   (U8(u8"⛓"));

    btnPointer.setTooltip("Pointer (1) - move/select");
    btnSlice.setTooltip  ("Slice (2) - cut clip");
    btnResize.setTooltip ("Resize (3) - drag clip edges");
    btnZoomTool.setTooltip("Zoom (4) drag: L=in / R=out");
    btnFrameAll.setTooltip("Frame all (F)");
    btnSnap.setTooltip   ("Snap to grid (G)");

    for (auto* b : { &btnPointer, &btnSlice, &btnResize, &btnZoomTool }) b->setClickingTogglesState(true);
    btnSnap.setClickingTogglesState(true);
    btnSnap.setToggleState(true, juce::dontSendNotification);

    for (auto* b : { &btnPointer, &btnSlice, &btnResize, &btnZoomTool, &btnFrameAll, &btnSnap, &btnZoomIn, &btnZoomOut })
    { b->addListener(this); addAndMakeVisible(b); }

    btnZoomIn.setButtonText("+"); btnZoomOut.setButtonText("-");
    btnZoomIn.setTooltip("Zoom in (+)"); btnZoomOut.setTooltip("Zoom out (-)");

    setTool(ArrangerTool::Pointer);

    addAndMakeVisible(view);
    view.setViewedComponent(&content, false);
    content.addAndMakeVisible(canvas);

    setBPM(120.0);
    setSnap(true);

    refreshAll();
    applyCursorForToolEverywhere();
}

Arranger::~Arranger()
{
    for (auto* b : { &btnPointer, &btnSlice, &btnResize, &btnZoomTool, &btnFrameAll, &btnSnap, &btnZoomIn, &btnZoomOut })
        b->removeListener(this);
}

void Arranger::setBPM(double bpm)
{
    const double old = bpmValue;
    bpmValue = juce::jlimit(40.0, 300.0, bpm);
    pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;
    if (std::abs(old - bpmValue) > 1e-6) { pushUndo(); recomputeClipBeatLengthsForTempo(); }
    layoutClips(); repaint();
}

void Arranger::setPlayheadBeats(double beats) { playheadBeats = juce::jmax(0.0, beats); canvas.repaint(); }
double Arranger::getPlayheadBeats() const     { return playheadBeats; }

void Arranger::setSnap(bool on){ snapToGrid = on; btnSnap.setToggleState(on, juce::dontSendNotification); repaint(); }

void Arranger::setTool(ArrangerTool t)
{
    tool = t;
    auto mark = [&](juce::TextButton& b, bool on){
        b.setToggleState(on, juce::dontSendNotification);
        b.setColour(juce::TextButton::buttonColourId, on ? activeCol : idleCol);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    };
    mark(btnPointer, t == ArrangerTool::Pointer);
    mark(btnSlice,   t == ArrangerTool::Slice);
    mark(btnResize,  t == ArrangerTool::Resize);
    mark(btnZoomTool,t == ArrangerTool::Zoom);
    applyCursorForToolEverywhere(); repaint();
}

bool Arranger::isInterestedInFileDrag(const juce::StringArray&) { return true; }

void Arranger::filesDropped(const juce::StringArray& files, int x, int y)
{
    if (files.isEmpty()) return;
    pushUndo();

    const int contentX = x + view.getViewPositionX();
    const double dropBeats = canvas.beatsFromX(contentX);
    const double startBeats = snapToGrid ? std::round(dropBeats) : dropBeats;

    const int yContent = y + view.getViewPositionY();
    int laneIdx = laneIndexFromYAllowNew(yContent);

    for (int i=0;i<files.size();++i)
    {
        const juce::File f(files[i]);
        if (!f.existsAsFile()) continue;

        while (laneIdx >= (int)tracks.size())
            addTrack("Audio " + juce::String((int)tracks.size()+1), /*push=*/false);

        ClipModel clip;
        clip.id = juce::Uuid().toString();
        clip.file = f;
        clip.startBeats  = startBeats;
        clip.lengthBeats = estimateBeatsFromFile(f, bpmValue);
        clip.offsetBeats = 0.0;

        auto& lane = tracks[(size_t)laneIdx];
        auto name = extractTrackNameFromAudio(f);
        lane.name = name.isNotEmpty() ? name : f.getFileNameWithoutExtension();
        lane.clips.push_back(std::move(clip));
        ++laneIdx;
    }

    refreshAll();
    if (onProjectChanged) onProjectChanged();
}

void Arranger::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colour(0x22FFFFFF));
    g.drawRect(getLocalBounds());
}

void Arranger::resized()
{
    auto r = getLocalBounds().reduced(8, 6);

    auto tools = r.removeFromTop(34);
    btnPointer .setBounds(tools.removeFromLeft(40)); tools.removeFromLeft(6);
    btnSlice   .setBounds(tools.removeFromLeft(40)); tools.removeFromLeft(6);
    btnResize  .setBounds(tools.removeFromLeft(40)); tools.removeFromLeft(6);
    btnZoomTool.setBounds(tools.removeFromLeft(40)); tools.removeFromLeft(6);
    btnFrameAll.setBounds(tools.removeFromLeft(40)); tools.removeFromLeft(12);
    btnSnap    .setBounds(tools.removeFromLeft(56)); tools.removeFromLeft(12);
    btnZoomOut .setBounds(tools.removeFromLeft(36)); tools.removeFromLeft(4);
    btnZoomIn  .setBounds(tools.removeFromLeft(36));

    view.setBounds(r);
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

    if (key.getTextCharacter() == '+') { zoomDelta(+1); return true; }
    if (key.getTextCharacter() == '-') { zoomDelta(-1); return true; }

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

void Arranger::zoomDelta(double delta, int centerXInContent)
{
    zoomScale = juce::jlimit(0.25, 6.0, zoomScale * (delta > 0 ? 1.15 : 0.87));
    pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;

    if (centerXInContent >= 0)
    {
        const double beatAtCursor = canvas.beatsFromX(centerXInContent);
        const int newX = canvas.xFromBeats(beatAtCursor);
        const int dx = newX - centerXInContent;
        auto p = view.getViewPosition();
        view.setViewPosition(p.getX() + dx, p.getY());
    }

    layoutClips();
    extendContentToMaxClip();
    canvas.repaint();
    repaint();
}

void Arranger::zoomDeltaFromWheel(double wheelDelta, int centerXInScreen)
{
    auto centerInContent = centerXInScreen + view.getViewPositionX();
    zoomDelta(wheelDelta, centerInContent);
}

void Arranger::frameAll()
{
    double maxEndBeats = 64.0;
    for (auto& t : tracks)
        for (auto& c : t.clips)
            maxEndBeats = std::max(maxEndBeats, c.startBeats + c.lengthBeats);

    const int viewportW = juce::jmax(1, view.getWidth() - TrackLaneComponent::headerWidth - 40);
    const double ppbNow = juce::jlimit(10.0, 300.0, (double)viewportW / juce::jmax(8.0, maxEndBeats));

    ppbAt120      = ppbNow * (bpmValue / 120.0);
    zoomScale     = 1.0;
    pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;

    extendContentToMaxClip();
    layoutClips();
    view.setViewPosition(0, view.getViewPositionY());
    repaint();
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

    for (auto& cc : clipComps) content.removeChildComponent(cc.get());
    clipComps.clear();

    for (auto& t : tracks)
    {
        for (auto& c : t.clips)
        {
            auto* cc = new ClipComponent(c, fm, cache, bpmValue,
    [this]{ extendContentToMaxClip(); if(onProjectChanged) onProjectChanged(); });

            cc->addMouseListener(this, true);
            cc->setSelected(selectedClip == &c);
            content.addAndMakeVisible(cc);
            clipComps.emplace_back(cc);
            clipComps.back()->setActiveTool(tool);
        }
    }

    content.addAndMakeVisible(canvas);
    canvas.toBack();

    layoutLanes();
    layoutClips();
    extendContentToMaxClip();
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
    const int rulerH = 20;
    int y = yInContent - rulerH;
    int laneH = laneHeight();
    if (y < laneTop()) return 0;
    int idx = (y - laneTop()) / laneH;
    if (tracks.empty()) return 0;
    return juce::jlimit(0, (int)tracks.size()-1, idx);
}
int Arranger::laneIndexFromYAllowNew(int yInContent) const
{
    const int rulerH = 20;
    int y = yInContent - rulerH;
    int laneH = laneHeight();
    if (y < laneTop()) return 0;
    if (tracks.empty()) return 0;
    const int idx = (y - laneTop()) / laneH;
    return juce::jlimit(0, (int)tracks.size(), idx);
}

void Arranger::layoutLanes()
{
    canvas.setBounds(0, 0, juce::jmax(2000, content.getWidth()), juce::jmax(600, content.getHeight()));

    int y = laneTop();
    int w = juce::jmax(2000, getWidth());

    for (auto& lc : laneComps) { lc->setBounds(0, y, w, laneHeight()); y += laneHeight(); }

    content.setSize(juce::jmax(w, getWidth()*2), juce::jmax(y+120, getHeight()*2));
}

void Arranger::layoutClips()
{
    const int laneH = laneHeight();
    const int y0 = laneTop();

    auto yForTrack = [&](TrackModel* tPtr)
    {
        int laneIndex = 0;
        for (size_t i=0;i<tracks.size();++i) if (&tracks[i] == tPtr) { laneIndex = (int)i; break; }
        return y0 + laneIndex * laneH;
    };

    for (auto& ccUP : clipComps)
    {
        auto& cc = *ccUP;
        TrackModel* track = nullptr; ClipModel* cm = nullptr;
        for (auto& t : tracks) { for (auto& c : t.clips) if (&c == &cc.model) { track = &t; cm = &c; break; } if (cm) break; }
        if (!cm || !track) continue;

        const int x = canvas.xFromBeats(cm->startBeats);
        const int w = (int)std::round(cm->lengthBeats * pixelsPerBeat);
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

    const int neededW = TrackLaneComponent::headerWidth + (int)std::ceil(maxEndBeats * pixelsPerBeat) + 400;
    content.setSize(juce::jmax(neededW, content.getWidth()), content.getHeight());
    canvas.setBounds(0, 0, content.getWidth(), content.getHeight());
    repaint();
}

/*** mouse routing ***/
void Arranger::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isMiddleButtonDown())
    { panActive = true; panStartMouse = e.getEventRelativeTo(this).getPosition(); panStartView = view.getViewPosition(); return; }

    const bool rightClick = e.mods.isRightButtonDown();
    const int yContent = e.getEventRelativeTo(&content).y;

    // Ruler: set/drag playhead (snap to beat)
    if (yContent < laneTop())
    {
        const double beat = std::round(canvas.beatsFromX(e.getEventRelativeTo(&content).x));
        setPlayheadBeats(beat);
        if (onPlayheadSet) onPlayheadSet(beat);
        playheadDragActive = true;
        return;
    }

    if (tool == ArrangerTool::Zoom)
    {
        zoomDragActive = true;
        zoomDragStartXContent = e.getEventRelativeTo(&content).x;
        zoomDelta(rightClick ? -1 : +1, zoomDragStartXContent);
        return;
    }

    if (auto* cc = dynamic_cast<ClipComponent*>(e.eventComponent))
    {
        activeClip = cc;
        selectedClip = &cc->model; updateClipSelectionVisuals();
        setSelectedLane(trackIndexForClip(cc->model));

        startLaneIndex = trackIndexForClip(cc->model);
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
        else { dragging = DragMode::Move; }

        dragStartPos = e.getEventRelativeTo(&content).getPosition();
        clipStartBeatsAtDown = cc->model.startBeats;
        clipLenBeatsAtDown   = cc->model.lengthBeats;
        clipOffsetBeatsAtDown= cc->model.offsetBeats;
    }
    else
    {
        selectedClip = nullptr; updateClipSelectionVisuals();
    }
}

void Arranger::mouseDrag(const juce::MouseEvent& e)
{
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

    if (zoomDragActive && tool == ArrangerTool::Zoom)
    {
        const bool rightClick = e.mods.isRightButtonDown();
        const int xContent = e.getEventRelativeTo(&content).x;
        const int dx = xContent - zoomDragStartXContent;
        if (std::abs(dx) > 1)
        {
            const double steps = (double)dx / 24.0;
            zoomDelta(rightClick ? -steps : +steps, xContent);
            zoomDragStartXContent = xContent;
        }
        return;
    }

    if (!activeClip) return;

    const auto pos = e.getEventRelativeTo(&content).getPosition();
    const int dxPx = pos.x - dragStartPos.x;
    const double dxBeats = dxPx / pixelsPerBeat;

    auto& cm = activeClip->model;

    if (dragging == DragMode::Move)
    {
        double nb = clipStartBeatsAtDown + dxBeats;
        if (snapToGrid) nb = std::round(nb);
        cm.startBeats = juce::jlimit(0.0, 100000.0, nb);

        int hoverLane = laneIndexFromYAllowNew(pos.y);
        targetLaneIndex = juce::jlimit(0, (int)tracks.size(), hoverLane);

        const int laneH = laneHeight();
        const int y0    = laneTop();
        const int previewLaneY = y0 + juce::jmin(targetLaneIndex, (int)tracks.size()-1) * laneH + 10;
        const int w     = (int)std::round(cm.lengthBeats * pixelsPerBeat);
        const int x     = canvas.xFromBeats(cm.startBeats);
        activeClip->setBounds(x, previewLaneY, juce::jmax(24, w), laneH - 20);
    }
    else if (dragging == DragMode::Left)
    {
        double newStart = clipStartBeatsAtDown + dxBeats;
        double newLen   = clipLenBeatsAtDown - dxBeats;
        if (snapToGrid) { newStart = std::round(newStart); newLen = std::round(newLen); }
        newStart = juce::jmax(0.0, newStart);
        newLen   = juce::jmax(1.0, newLen);

        const double delta = newStart - clipStartBeatsAtDown;
        cm.startBeats  = newStart;
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
    if (dragging != DragMode::Move) layoutClips();
}

void Arranger::mouseUp(const juce::MouseEvent&)
{
    panActive = false;
    zoomDragActive = false;
    playheadDragActive = false;

    if (activeClip)
    {
        if (dragging == DragMode::Move && targetLaneIndex >= 0)
        {
            if (targetLaneIndex == (int)tracks.size())
                addTrack("Audio " + juce::String((int)tracks.size()+1));

            if (targetLaneIndex != startLaneIndex)
                moveClipToLane(activeClip->model, startLaneIndex, targetLaneIndex);

            setSelectedLane(juce::jmin(targetLaneIndex, (int)tracks.size()-1));
        }
    }

    activeClip = nullptr;
    dragging = DragMode::None;
    startLaneIndex = targetLaneIndex = -1;

    layoutClips();
}

/*** model helpers ***/
int Arranger::trackIndexForClip(ClipModel& cm)
{
    for (size_t i=0;i<tracks.size();++i)
        for (auto& c : tracks[i].clips)
            if (&c == &cm) return (int)i;
    return 0;
}

void Arranger::moveClipToLane(ClipModel& cm, int fromLane, int toLane)
{
    if (fromLane < 0 || toLane < 0 || fromLane >= (int)tracks.size() || toLane >= (int)tracks.size()) return;
    if (fromLane == toLane) return;

    auto& src = tracks[(size_t)fromLane].clips;
    auto it = std::find_if(src.begin(), src.end(), [&](const ClipModel& c){ return &c == &cm; });
    if (it == src.end()) return;

    ClipModel moved = *it;
    src.erase(it);
    tracks[(size_t)toLane].clips.push_back(std::move(moved));

    refreshAll();
    if (onProjectChanged) onProjectChanged();
}

juce::String Arranger::extractTrackNameFromAudio(const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r)
    {
        auto& meta = r->metadataValues;
        for (auto key : { "title", "artist", "name" })
            if (meta.containsKey(key)) return meta[key];
    }
    return {};
}

double Arranger::estimateBeatsFromFile(const juce::File& f, double bpm)
{
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r && r->sampleRate > 0.0)
    {
        const double secs  = (double)r->lengthInSamples / r->sampleRate;
        const double beats = (secs * bpm) / 60.0;
        double bars = std::max(1.0, std::round(beats / 4.0));
        return bars * 4.0;
    }
    return 16.0;
}

double Arranger::detectLoopBpm(const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r)
    {
        auto& meta = r->metadataValues;
        for (auto key : { "tempo", "bpm", "BPM", "Tempo" })
            if (meta.containsKey(key))
            {
                auto v = meta[key].retainCharacters("0123456789.");
                auto d = v.getDoubleValue();
                if (d > 20.0 && d < 300.0) return d;
            }
    }
    auto name = f.getFileNameWithoutExtension().toLowerCase();
    juce::String digits;
    for (int i=0;i<name.length();++i)
    {
        const juce_wchar ch = name[i];
        if (juce::CharacterFunctions::isDigit(ch) || ch == '.')
            digits << juce::String::charToString(ch);
        else
        {
            if (digits.length() >= 2)
            {
                double val = digits.getDoubleValue();
                if (val > 20.0 && val < 300.0) return val;
            }
            digits.clear();
        }
    }
    if (digits.isNotEmpty())
    {
        double val = digits.getDoubleValue();
        if (val > 20.0 && val < 300.0) return val;
    }
    return 0.0;
}

double Arranger::secondsOfFile(const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r && r->sampleRate > 0.0) return (double) r->lengthInSamples / r->sampleRate;
    return 0.0;
}

void Arranger::recomputeClipBeatLengthsForTempo()
{
    for (auto& t : tracks)
        for (auto& c : t.clips)
        {
            if (!c.file.existsAsFile()) continue;

            const double secs = secondsOfFile(c.file);
            if (secs <= 0.0) continue;

            double srcBpm = detectLoopBpm(c.file);
            if (srcBpm <= 0.0) srcBpm = bpmValue;

            const double beats = (secs * srcBpm) / 60.0;

            double rounded = std::round(beats * 4.0) / 4.0;  // ~16th grid
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
                right.offsetBeats = it->offsetBeats + (beat - it->startBeats); // audible offset preserved

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
    else if (b == &btnZoomIn)  zoomDelta(+1);
    else if (b == &btnZoomOut) zoomDelta(-1);
}
