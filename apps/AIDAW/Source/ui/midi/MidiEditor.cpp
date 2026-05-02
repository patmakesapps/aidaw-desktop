#include "MidiEditor.h"
#include "../shared/ThemeManager.h"

#include <algorithm>

// ===== MidiEditor =====
MidiEditor::MidiEditor()
{
    setOpaque(true);
    setWantsKeyboardFocus(true);

    // --- Toolbar ---
    juce::Button* allButtons[] = {
        static_cast<juce::Button*>(&btnSelect),
        static_cast<juce::Button*>(&btnDraw),
        static_cast<juce::Button*>(&btnZoomTool),
        static_cast<juce::Button*>(&btnFrameAll),
        static_cast<juce::Button*>(&btnSnap),
        static_cast<juce::Button*>(&btnZoomOut),
        static_cast<juce::Button*>(&btnZoomIn),
        static_cast<juce::Button*>(&btnLoopToggle),
        static_cast<juce::Button*>(&btnLoops),
        static_cast<juce::Button*>(&btnEddie),
    };
    for (auto* b : allButtons)
    { addAndMakeVisible(b); b->addListener(this); b->setWantsKeyboardFocus(false); }

    for (auto* ib : { &btnSelect, &btnDraw, &btnZoomTool, &btnFrameAll, &btnSnap,
                      &btnZoomOut, &btnZoomIn, &btnLoopToggle })
        ib->setIconScale(0.5f);

    btnFrameAll.setAccentTint(true);
    btnLoopToggle.setAccentTint(true);

    addAndMakeVisible(gridMenu);

    ThemeManager::get().addChangeListener(this);

    gridMenu.addItem("1 bar", 1);
    gridMenu.addItem("1/2",   2);
    gridMenu.addItem("1/4",   3);
    gridMenu.addItem("1/8",   4);
    gridMenu.addItem("1/16",  5);
    gridMenu.addItem("1/32",  6);
    gridMenu.addItem("1/64",  7);
    gridMenu.setSelectedId(5, juce::dontSendNotification);
    gridMenu.setTooltip("Piano roll grid");
    gridMenu.onChange = [this]
    {
        switch (gridMenu.getSelectedId())
        {
            case 1: setGridQuantum(4.0); break;
            case 2: setGridQuantum(2.0); break;
            case 3: setGridQuantum(1.0); break;
            case 4: setGridQuantum(0.5); break;
            case 5: setGridQuantum(0.25); break;
            case 6: setGridQuantum(0.125); break;
            case 7: setGridQuantum(0.0625); break;
            default: setGridQuantum(0.25); break;
        }
    };

    for (auto* b : { &btnSelect, &btnDraw, &btnZoomTool }) b->setClickingTogglesState(true);
    btnSnap.setClickingTogglesState(true); btnSnap.setToggleState(true, juce::dontSendNotification);
    btnLoopToggle.setClickingTogglesState(true);
    btnLoopToggle.setToggleState(false, juce::dontSendNotification);
    setTool(Tool::Select);

    // --- Viewport + content ---
    addAndMakeVisible(view);
    view.setViewedComponent(new Content(), true);
    auto* c = dynamic_cast<Content*>(view.getViewedComponent());
    jassert(c != nullptr);

    // Viewport: only middle mouse pans.
    view.setScrollBarsShown(true, true);
    view.setScrollOnDragEnabled(false);

    // Bridge refs
    c->onLayoutRequest = [this]{ refreshContentSize(); };
    c->onNotesChanged = [this]{ notifyNotesChanged(); };

    // Model refs
    c->notes           = &notes;
    c->snapToGridRef   = &snapToGrid;
    c->gridQuantumBeats = &gridQuantumBeats;
    c->pixelsPerBeat   = &pixelsPerBeat;
    c->playheadBeats   = &playheadBeats;
    c->loopEnabled     = &loopEnabled;
    c->loopStartBeats  = &loopStartBeats;
    c->loopLengthBeats = &loopLengthBeats;
    c->onPreviewNote   = [this] (int pitch, int velocity)
    {
        if (onPreviewNote)
            onPreviewNote(pitch, velocity);
    };

    // Defaults
    setBPM(120.0);
    setSnap(true);
    setHorizontalZoom(8.0);
    setGridQuantum(0.25);
    setPitchView(0, 127);
    setBeatsExtent(256.0);

    // allow content to grow when you drag notes or loop to the right
    c->extendToPixelRight = [this](int pxRight) {
        auto* cont = dynamic_cast<Content*>(view.getViewedComponent());
        if (!cont) return;
        if (pxRight > cont->getWidth())
        {
            cont->setSize(pxRight, cont->getHeight());
            view.autoScroll(0,0,0,0); // keep viewport sane
        }
    };

    refreshContentSize();
    c->syncNoteComponents();
}

MidiEditor::~MidiEditor()
{
    ThemeManager::get().removeChangeListener(this);
}

void MidiEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
        c->repaint();
    repaint();
}

void MidiEditor::setNotes(const std::vector<MidiNote>& newNotes)
{
    notes = newNotes;
    // assign stable UIDs to any notes missing one
    for (auto& n : notes) { if (n.uid == 0) n.uid = nextNoteUid++; }
    if (autoFitBeatsOnSet)  autoFitBeatsToNotes(8.0);
    if (autoFitPitchOnSet)  autoFitPitchToNotes();
    refreshContentSize();

    if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
        c->syncNoteComponents();

    notifyNotesChanged();
}

std::vector<MidiNote>& MidiEditor::editNotes() { return notes; }
const std::vector<MidiNote>& MidiEditor::getNotes() const { return notes; }

void MidiEditor::notifyNotesChanged()
{
    if (onNotesChanged)
        onNotesChanged(notes);
}

void MidiEditor::setBPM(double bpm)
{
    bpmValue      = juce::jlimit(40.0, 300.0, bpm);
    pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;
    pixelsPerBeat = juce::jlimit(minPPB, maxPPB, pixelsPerBeat);
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
    refreshContentSize();
}

void MidiEditor::setSnap(bool on)
{
    snapToGrid = on;
    btnSnap.setToggleState(on, juce::dontSendNotification);
    repaint();
}

void MidiEditor::setGridQuantum(double beats)
{
    gridQuantumBeats = juce::jlimit(0.0625, 4.0, beats);
    if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
        c->repaint();
}

void MidiEditor::setHorizontalZoom(double beatsPerScreen)
{
    beatsPerScreen = juce::jlimit(1.0, 64.0, beatsPerScreen);
    const int vw = juce::jmax(800, view.getWidth());
    const double desiredPPB = (double)vw / beatsPerScreen;
    pixelsPerBeat = juce::jlimit(minPPB, maxPPB, desiredPPB);
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
    refreshContentSize();
}

void MidiEditor::zoomAtContentX(double steps, int anchorXContent)
{
    const double oldPPB   = pixelsPerBeat;
    const double beat     = juce::jmax(0.0, (anchorXContent - Theme::keyWidth) / oldPPB);
    const int    xBefore  = Theme::keyWidth + (int)std::round(beat * oldPPB);

    zoomScale     = juce::jlimit(0.02, 80.0, zoomScale * std::pow(1.12, steps));
    pixelsPerBeat = juce::jlimit(minPPB, maxPPB, ppbAt120 * (120.0 / bpmValue) * zoomScale);

    refreshContentSize();
    const int xAfter = Theme::keyWidth + (int)std::round(beat * pixelsPerBeat);
    const int dx = xAfter - xBefore;

    const int maxX = juce::jmax(0, view.getViewedComponent()->getWidth() - view.getViewWidth());
    view.setViewPosition(juce::jlimit(0, maxX, view.getViewPositionX() + dx),
                         view.getViewPositionY());

    // keep at least one note in focus after zoom
    centerOnNearestNote(beat);

    repaint();
}

void MidiEditor::frameAll()
{
    double endB = 64.0;
    for (auto& n : notes) endB = std::max(endB, n.startBeats + n.lengthBeats);
    const int viewportW = juce::jmax(1, view.getViewWidth() - Theme::keyWidth - 40);
    const double target = (double)viewportW / juce::jmax(8.0, endB);
    pixelsPerBeat = juce::jlimit(minPPB, maxPPB, target);
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
    refreshContentSize();
    view.setViewPosition(0, view.getViewPositionY());

    // also ensure a note is visible (for empty note lists, just stay at 0)
    centerOnNearestNote(0.0);
}

void MidiEditor::frameAllView() { frameAll(); }

void MidiEditor::setPitchView(int minPitchInclusive, int maxPitchInclusive)
{
    minPitch = juce::jlimit(0, 127, juce::jmin(minPitchInclusive, maxPitchInclusive));
    maxPitch = juce::jlimit(0, 127, juce::jmax(minPitchInclusive, maxPitchInclusive));
    if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
    {
        c->topPitch  = maxPitch;
        c->totalRows = (maxPitch - minPitch + 1);
    }
    refreshContentSize();
}

void MidiEditor::autoFitPitchToNotes()
{
    if (notes.empty()) return;
    int lo = 127, hi = 0;
    for (auto& n : notes){ lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch); }
    setPitchView(juce::jmax(0, lo-3), juce::jmin(127, hi+3));
}
void MidiEditor::setAutoFitPitchOnSet(bool on) { autoFitPitchOnSet = on; }

void MidiEditor::setBeatsExtent(double beats) { beatsExtent = juce::jmax(1.0, beats); refreshContentSize(); }
void MidiEditor::autoFitBeatsToNotes(double padBeats)
{
    double endB = 0.0;
    for (auto& n : notes) endB = std::max(endB, n.startBeats + n.lengthBeats);
    beatsExtent = juce::jmax(16.0, endB + padBeats);
    refreshContentSize();
}
void MidiEditor::setAutoFitBeatsOnSet(bool on) { autoFitBeatsOnSet = on; }

void MidiEditor::setPlayheadBeats(double beats)
{
    playheadBeats = juce::jmax(0.0, beats);
    repaint();
}
void MidiEditor::setLoopEnabled(bool on)
{
    loopEnabled = on;
    btnLoopToggle.setToggleState(on, juce::dontSendNotification);
    if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
        c->repaint();
    repaint();
}
void MidiEditor::setLoopRegion(double s, double len)
{
    loopStartBeats = juce::jmax(0.0, s);
    loopLengthBeats = juce::jmax(0.0, len);
    repaint();
}

void MidiEditor::zoomDeltaFromWheel(double wheelDelta, int screenX)
{
    const int localX = screenX - getScreenX();
    const int clampedLocalX = juce::jlimit(0, view.getViewedComponent()->getWidth(), localX);
    const int anchorXContent = view.getViewPositionX() + clampedLocalX;
    zoomAtContentX(wheelDelta, anchorXContent);
}

void MidiEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::colBgMain));
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.drawRect(getLocalBounds());
}

void MidiEditor::paintOverChildren(juce::Graphics& g)
{
    auto* c = dynamic_cast<Content*>(view.getViewedComponent());
    if (!c)
        return;

    const auto vr = view.getBounds();
    const int vpX = view.getViewPositionX();
    const int vpY = view.getViewPositionY();

    auto ruler = vr.withHeight(Theme::rulerH);
    g.setColour(juce::Colour(Theme::colBgRuler).withAlpha(0.96f));
    g.fillRect(ruler);
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.fillRect(ruler.getX(), ruler.getBottom() - 1, ruler.getWidth(), 1);

    const int firstBeat = juce::jmax(0, (int)std::floor((double)vpX / pixelsPerBeat) - 1);
    const int lastBeat = (int)std::ceil((double)(vpX + vr.getWidth()) / pixelsPerBeat) + 1;
    for (int beat = firstBeat; beat <= lastBeat; ++beat)
    {
        if ((beat % 4) != 0)
            continue;

        const int x = vr.getX() + Theme::keyWidth + (int)std::round(beat * pixelsPerBeat) - vpX;
        if (x < vr.getX() + Theme::keyWidth || x > vr.getRight())
            continue;

        g.setColour(juce::Colour(Theme::colBarLabel));
        g.setFont(12.0f);
        g.drawFittedText(juce::String((beat / 4) + 1), x + 6, ruler.getY() + 2, 36, Theme::rulerH - 4,
                         juce::Justification::centredLeft, 1);
    }

    if (loopEnabled && loopLengthBeats > 0.0)
    {
        const int xs = vr.getX() + Theme::keyWidth + (int)std::round(loopStartBeats * pixelsPerBeat) - vpX;
        const int xe = vr.getX() + Theme::keyWidth + (int)std::round((loopStartBeats + loopLengthBeats) * pixelsPerBeat) - vpX;
        const int clippedXs = juce::jlimit(vr.getX() + Theme::keyWidth, vr.getRight(), xs);
        const int clippedXe = juce::jlimit(vr.getX() + Theme::keyWidth, vr.getRight(), xe);

        if (clippedXe > clippedXs)
        {
            g.setColour(juce::Colour(Theme::colLoopFill).withAlpha(0.9f));
            g.fillRect(clippedXs, ruler.getY(), clippedXe - clippedXs, Theme::rulerH);
        }

        auto drawHandle = [&] (int x)
        {
            if (x < vr.getX() + Theme::keyWidth - 8 || x > vr.getRight() + 8)
                return;

            g.setColour(juce::Colour(Theme::colLoop));
            g.fillRect(x - 1, ruler.getY(), 2, Theme::rulerH);
            g.drawRoundedRectangle({ (float)x - 7.0f, (float)ruler.getY() + 2.0f, 14.0f, (float)Theme::rulerH - 4.0f },
                                   3.0f, 2.0f);
        };
        drawHandle(xs);
        drawHandle(xe);
    }

    auto keys = vr.withWidth(Theme::keyWidth).withTrimmedTop(Theme::rulerH);
    g.setColour(juce::Colour(Theme::colBgMain));
    g.fillRect(keys);

    const int firstRow = juce::jlimit(0, c->totalRows - 1, (vpY - Theme::rulerH) / c->rowHeight);
    const int lastRow = juce::jlimit(0, c->totalRows - 1, (vpY + vr.getHeight() - Theme::rulerH) / c->rowHeight + 1);
    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int pitch = c->topPitch - row;
        const int pc = pitch % 12;
        const bool black = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        const int y = vr.getY() + Theme::rulerH + row * c->rowHeight - vpY;
        juce::Rectangle<int> key(vr.getX(), y, Theme::keyWidth, c->rowHeight);

        g.setColour(juce::Colour(black ? Theme::colKeyBlack : Theme::colKeyWhite).withAlpha(0.97f));
        g.fillRect(key);
        if (pc == 0)
        {
            g.setColour(juce::Colour(Theme::colText));
            g.setFont(12.0f);
            g.drawText("C" + juce::String((pitch / 12) - 1), key.reduced(8, 3), juce::Justification::centredLeft);
        }
        g.setColour(juce::Colour(Theme::colKeySep));
        g.fillRect(key.withY(key.getBottom() - 1).withHeight(1));
    }

    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.fillRect(vr.getX() + Theme::keyWidth - 1, vr.getY(), 1, vr.getHeight());
}

void MidiEditor::resized()
{
    auto r = getLocalBounds().reduced(8, 6);
    auto tools = r.removeFromTop(34);
    const int ib = 36;
    btnSelect  .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnDraw    .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnZoomTool.setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(12);
    btnFrameAll.setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnSnap    .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(12);
    btnZoomOut .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(4);
    btnZoomIn  .setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(12);
    gridMenu   .setBounds(tools.removeFromLeft(82)); tools.removeFromLeft(8);
    btnLoopToggle.setBounds(tools.removeFromLeft(ib)); tools.removeFromLeft(12);
    btnLoops   .setBounds(tools.removeFromLeft(72));
    tools.removeFromLeft(6);
    btnEddie   .setBounds(tools.removeFromLeft(74));

    r.removeFromTop(22);
    view.setBounds(r);
    refreshContentSize();

    if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
    {
        c->syncNoteComponents();
        if (!initialPitchCentered && view.getViewHeight() > 0)
        {
            const int middleRow = juce::jlimit(0, c->totalRows - 1, maxPitch - 60);
            const int middleCY = Theme::rulerH + middleRow * c->rowHeight + c->rowHeight / 2;
            c->centerContentOn(view.getViewWidth() / 2, middleCY);
            view.setViewPosition(0, view.getViewPositionY());
            initialPitchCentered = true;
        }
    }

    if (tool != Tool::Draw)
        setTool(Tool::Draw);
}

bool MidiEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getTextCharacter() == '1') { setTool(Tool::Select);   return true; }
    if (key.getTextCharacter() == '2') { setTool(Tool::Draw);     return true; }
    if (key.getTextCharacter() == '4') { setTool(Tool::Zoom);     return true; }
    if (key.getTextCharacter() == 'g' || key.getTextCharacter() == 'G')
    { setSnap(!btnSnap.getToggleState()); return true; }
    if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
    { frameAll(); return true; }
    if (key.getTextCharacter() == '+')
    { zoomAtContentX(+1.0, view.getViewPositionX() + view.getViewWidth()/2); return true; }
    if (key.getTextCharacter() == '-')
    { zoomAtContentX(-1.0, view.getViewPositionX() + view.getViewWidth()/2); return true; }
    return Component::keyPressed(key);
}

void MidiEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        const int xContent = e.getEventRelativeTo(view.getViewedComponent()).x;
        zoomAtContentX(wheel.deltaY, view.getViewPositionX() + xContent);
        return;
    }
    Component::mouseWheelMove(e, wheel);
}

void MidiEditor::setTool(Tool t)
{
    tool = t;
    btnSelect  .setToggleState(t == Tool::Select, juce::dontSendNotification);
    btnDraw    .setToggleState(t == Tool::Draw,   juce::dontSendNotification);
    btnZoomTool.setToggleState(t == Tool::Zoom,   juce::dontSendNotification);

    if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
    {
        c->activeTool = t;
        c->setMouseCursor(t==Tool::Zoom ? juce::MouseCursor::PointingHandCursor
                                        : juce::MouseCursor::NormalCursor);
        c->repaint();
    }
}

void MidiEditor::buttonClicked(juce::Button* b)
{
    if (b == &btnSelect)      setTool(Tool::Select);
    else if (b == &btnDraw)   setTool(Tool::Draw);
    else if (b == &btnZoomTool) setTool(Tool::Zoom);
    else if (b == &btnFrameAll) frameAll();
    else if (b == &btnSnap)     setSnap(btnSnap.getToggleState());
    else if (b == &btnZoomIn)   zoomAtContentX(+1.0, view.getViewPositionX() + view.getViewWidth()/2);
    else if (b == &btnZoomOut)  zoomAtContentX(-1.0, view.getViewPositionX() + view.getViewWidth()/2);
    else if (b == &btnLoopToggle) setLoopEnabled(btnLoopToggle.getToggleState());
    else if (b == &btnLoops)    { if (onShowLoops) onShowLoops(); }
    else if (b == &btnEddie)    { if (onOpenSynth) onOpenSynth(); }
}

void MidiEditor::refreshContentSize()
{
    auto* c = dynamic_cast<Content*>(view.getViewedComponent());
    if (!c) return;

    double maxEnd = beatsExtent;
    for (auto& n : notes) maxEnd = std::max(maxEnd, n.startBeats + n.lengthBeats + 4.0);

    const int widthFromBeats = Theme::keyWidth + (int)std::ceil(maxEnd * pixelsPerBeat);
    const int minOpenWidth   = juce::jmax(3200, view.getWidth() + 2400);
    const int width = juce::jmax(widthFromBeats, minOpenWidth);

    c->rowHeight = Theme::rowHeight;
    c->totalRows = (maxPitch - minPitch + 1);
    c->topPitch  = maxPitch;
    const int height  = Theme::rulerH + c->totalRows * c->rowHeight + Theme::velocityH;

    c->setSize(width, height);
    c->syncNoteComponents();
    c->repaint();
}

void MidiEditor::centerOnNearestNote(double anchorBeat)
{
    auto* c = dynamic_cast<Content*>(view.getViewedComponent());
    if (!c || notes.empty()) return;

    // viewport in content coords
    const auto vpX = view.getViewPositionX();
    const auto vpW = view.getViewWidth();
    const int left  = vpX;
    const int right = vpX + vpW;

    // compute horizontal bounds of notes
    double minB = 1e9, maxB = -1e9;
    for (const auto& n : notes)
    {
        minB = std::min(minB, n.startBeats);
        maxB = std::max(maxB, n.startBeats + n.lengthBeats);
    }
    const int minX = Theme::keyWidth + (int)std::round(minB * pixelsPerBeat);
    const int maxX = Theme::keyWidth + (int)std::round(maxB * pixelsPerBeat);

    const bool intersects = !(right < minX || left > maxX);
    if (intersects) return;

    // find nearest beat to anchor
    double targetBeat = minB;
    if (anchorBeat < minB) targetBeat = minB;
    else if (anchorBeat > maxB) targetBeat = maxB;
    else targetBeat = anchorBeat;

    const int targetX = Theme::keyWidth + (int)std::round(targetBeat * pixelsPerBeat);
    c->centerContentOn(targetX, Theme::rulerH + c->gridHeight()/2);
}

/* ============================================================
   Content
   ============================================================ */
MidiEditor::Content::Content() { setInterceptsMouseClicks(true, false); }

void MidiEditor::Content::syncNoteComponents()
{
    if (!notes) return;

    // allocate components to match note count
    while ((int)noteComps.size() < (int)notes->size())
    {
        auto& n = (*notes)[noteComps.size()];
        if (n.uid == 0) n.uid = juce::Random::getSystemRandom().nextInt64(); // fallback uid
        auto up = std::make_unique<NoteComponent>(
            n.uid,
            [this](uint32 uid, juce::Point<int> p, const juce::ModifierKeys& mods, bool& didSelect, int& edge){ handleNoteMouseDown(uid, p, mods, didSelect, edge); },
            [this](uint32 uid, juce::Point<int> p){ handleNoteMouseDrag(uid, p); },
            [this](uint32 uid){ handleNoteMouseUp(uid); },
            [this](uint32 uid){ eraseNoteByUid(uid); }
        );
        addAndMakeVisible(up.get());
        noteComps.emplace_back(std::move(up));
    }
    while ((int)noteComps.size() > (int)notes->size())
        noteComps.pop_back();

    // layout + colours
    for (size_t i=0;i<noteComps.size();++i)
    {
        auto& n = (*notes)[i];
        noteComps[i]->noteId = n.uid;

        const int x = xFromBeats(n.startBeats);
        const int y = Theme::rulerH + pitchToRow(n.pitch) * rowHeight + 3;
        const int w = juce::jmax(8, (int)std::round(n.lengthBeats * ppb()));
        const int h = rowHeight - 6;

        noteComps[i]->setBounds(x, y, w, h);

        const auto body = juce::Colour((i % 2 == 0) ? Theme::colNoteA : Theme::colNoteB);
        const auto rim  = juce::Colour(body).brighter(Theme::noteBorderGain);
        noteComps[i]->setSelected(selection.contains((int) i));
        noteComps[i]->setColors(juce::Colour(body).darker(0.30f), rim);
        noteComps[i]->setLabel(pitchName(n.pitch));
    }
}

void MidiEditor::Content::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::colBgMain));
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.fillRect(Theme::keyWidth, 0, 1, getHeight());

    paintRuler(g);
    paintKeys(g);
    paintGrid(g);
    paintLoopOverlay(g);
    paintVelocities(g);
    paintPlayhead(g);

    if (marqueeActive)
    {
        g.setColour(juce::Colour(Theme::colSelect));
        g.fillRect(marquee);
        g.setColour(juce::Colour(Theme::colSelectBd));
        g.drawRect(marquee);
    }
}

void MidiEditor::Content::paintRuler(juce::Graphics& g)
{
    auto r = juce::Rectangle<int>(0, 0, getWidth(), Theme::rulerH);
    g.setColour(juce::Colour(Theme::colBgRuler)); g.fillRect(r);
    g.setColour(juce::Colour(Theme::colHeaderDiv)); g.fillRect(r.withY(r.getBottom()-1).withHeight(1));

    const int totalBeats = (int)std::ceil((double)(getWidth()-Theme::keyWidth)/ppb());
    for (int beat = 0; beat <= totalBeats; ++beat)
        if ((beat % 4) == 0)
        {
            const int x = xFromBeats((double)beat);
            g.setColour(juce::Colour(Theme::colBarLabel));
            g.setFont(12.0f);
            g.drawFittedText(juce::String((beat / 4) + 1), x + 6, 2, 36, Theme::rulerH-4,
                             juce::Justification::centredLeft, 1);
        }

    if (loopEnabled && *loopEnabled && *loopLengthBeats > 0.0)
    {
        const int xs = xFromBeats(*loopStartBeats);
        const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);
        auto edgeW = 6;
        g.setColour(juce::Colour(Theme::colLoop));
        g.fillRect(xs, 0, 2, Theme::rulerH);
        g.fillRect(xe-2, 0, 2, Theme::rulerH);

        auto handle = [&](int cx){
            juce::Rectangle<float> hf((float)cx - edgeW, 3.0f, (float)edgeW*2.0f, (float)Theme::rulerH - 6.0f);
            g.setColour(juce::Colour(Theme::colLoop).withAlpha(hoverOnLeftEdge || hoverOnRightEdge ? 0.9f : 0.6f));
            g.drawRoundedRectangle(hf, 3.0f, 2.0f);
        };
        handle(xs); handle(xe);

        g.setColour(juce::Colour(Theme::colLoop).withAlpha(hoverOnBody ? 0.4f : 0.25f));
        g.fillRect(xs+2, 3, (xe-xs)-4, Theme::rulerH-6);
    }
}

void MidiEditor::Content::paintKeys(juce::Graphics& g)
{
    auto r = juce::Rectangle<int>(0, Theme::rulerH, Theme::keyWidth, gridHeight());
    int y = r.getY();
    for (int row = 0; row < totalRows; ++row)
    {
        const int pitch = rowToPitch(row);
        const int pc = pitch % 12;
        const bool black = (pc==1||pc==3||pc==6||pc==8||pc==10);

        juce::Rectangle<int> key(0, y, Theme::keyWidth, rowHeight);
        g.setColour(juce::Colour(black ? Theme::colKeyBlack : Theme::colKeyWhite));
        g.fillRect(key);

        if ((pc == 0) && ((pitch % 24) == 0))
        {
            g.setColour(juce::Colour(Theme::colText));
            g.setFont(12.0f);
            const int octave = (pitch / 12) - 1;
            g.drawText("C" + juce::String(octave), key.reduced(8, 3), juce::Justification::centredLeft);
        }

        g.setColour(juce::Colour(Theme::colKeySep));
        g.fillRect(key.removeFromBottom(1));
        y += rowHeight;
    }
}

void MidiEditor::Content::paintGrid(juce::Graphics& g)
{
    const auto r = juce::Rectangle<int>(Theme::keyWidth, Theme::rulerH,
                                        getWidth() - Theme::keyWidth, gridHeight());

    int y = r.getY();
    for (int row = 0; row < totalRows; ++row)
    {
        const bool odd  = (row & 1) != 0;
        g.setColour(juce::Colour(odd ? Theme::colRowOdd : Theme::colRowEven));
        g.fillRect(juce::Rectangle<int>(r.getX(), y, r.getWidth(), rowHeight));

        const int pitch = rowToPitch(row);
        if ((pitch % 12) == 0)
        {
            g.setColour(juce::Colour(Theme::colOctave));
            g.fillRect(r.getX(), y, r.getWidth(), 1);
        }
        y += rowHeight;
    }

    const double ppbVal = ppb();
    const int totalBeats = (int)std::ceil((getWidth() - Theme::keyWidth) / ppbVal);

    for (int beat = 0; beat <= totalBeats; ++beat)
    {
        const int x = Theme::keyWidth + (int)std::round(beat * ppbVal);
        const bool isBar = (beat % 4) == 0;
        if (isBar) { g.setColour(juce::Colour(Theme::colGridBar));  g.fillRect(x, Theme::rulerH, 2, getHeight() - Theme::rulerH); }
        else       { g.setColour(juce::Colour(Theme::colGridBeat)); g.fillRect(x, Theme::rulerH, 1, getHeight() - Theme::rulerH); }
    }

    const double requestedQuantum = gridQuantumBeats ? *gridQuantumBeats : 0.25;
    double visibleQuantum = juce::jlimit(0.0625, 4.0, requestedQuantum);
    while (visibleQuantum * ppbVal < 5.0 && visibleQuantum < 4.0)
        visibleQuantum *= 2.0;

    if (visibleQuantum < 4.0)
    {
        g.setColour(juce::Colour(Theme::colGridSub));
        const int steps = (int) std::ceil(totalBeats / visibleQuantum);
        for (int i = 1; i <= steps; ++i)
        {
            const double beat = i * visibleQuantum;
            if (std::abs(beat - std::round(beat)) < 0.0001)
                continue;

            const int x = Theme::keyWidth + (int) std::round(beat * ppbVal);
            g.fillRect(x, Theme::rulerH, 1, getHeight() - Theme::rulerH);
        }
    }
}

void MidiEditor::Content::paintLoopOverlay(juce::Graphics& g)
{
    if (!(loopEnabled && *loopEnabled && *loopLengthBeats > 0.0)) return;
    const int xs = xFromBeats(*loopStartBeats);
    const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);
    juce::Rectangle<int> loopRect(xs, Theme::rulerH, xe - xs, gridHeight());
    g.setColour(juce::Colour(Theme::colLoopFill));
    g.fillRect(loopRect);
}

void MidiEditor::Content::paintVelocities(juce::Graphics& g)
{
    auto r = juce::Rectangle<int>(0, getHeight()-Theme::velocityH, getWidth(), Theme::velocityH);
    g.setColour(juce::Colour(Theme::colVelLane)); g.fillRect(r);
    g.setColour(juce::Colour(0x223B82F6)); g.fillRect(r.removeFromTop(1));

    if (!notes) return;
    const int baseY = getHeight() - 10;
    for (const auto& n : *notes)
    {
        const int x = xFromBeats(n.startBeats + n.lengthBeats * 0.5);
        const int h = juce::jlimit(2, Theme::velocityH-16, (int)std::round((n.velocity/127.0) * (Theme::velocityH-16)));
        const auto barCol = juce::Colour(((n.pitch + 5) % 2 == 0) ? Theme::colNoteA : Theme::colNoteB);
        g.setColour(barCol);
        g.fillRect(x-1, baseY - h, 2, h);
    }
}

void MidiEditor::Content::paintPlayhead(juce::Graphics& g)
{
    if (!playheadBeats) return;
    const int x = xFromBeats(*playheadBeats);
    g.setColour(juce::Colour(Theme::colPlayhead));
    g.fillRect(x-1, 0, 2, getHeight());
}

// ========= Interactions =========
void MidiEditor::Content::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (!notes) return;

    const auto p = e.getPosition();
    bypassSnap = e.mods.isAltDown();

    // middle mouse = pan only
    if (e.mods.isMiddleButtonDown())
    {
        panActive = true; panStart = p; panViewStart = getParentViewportPos(); return;
    }

    // Zoom tool: left = start marquee; right = Frame All
    if (activeTool == MidiEditor::Tool::Zoom)
    {
        if (e.mods.isRightButtonDown())
        {
            if (auto* editor = findParentComponentOfClass<MidiEditor>()) editor->frameAllView();
            return;
        }
        selectionDragActive = true;
        marqueeActive = true;
        selStart = p;
        marquee = juce::Rectangle<int>(p.x, p.y, 1, 1);
        repaint(); return;
    }

    // Right-click delete ONE note under cursor (no sweep)
    if (e.mods.isRightButtonDown())
    {
        const int idx = indexAtPointNoteArea(p);
        if (idx >= 0) { (*notes).erase(notes->begin() + idx); syncNoteComponents(); if (onNotesChanged) onNotesChanged(); repaint(); }
        return;
    }

    const int stickyKeyLeft = getParentViewportPos().x;
    if (p.x >= stickyKeyLeft && p.x < stickyKeyLeft + Theme::keyWidth
        && p.y >= Theme::rulerH && p.y < getHeight() - Theme::velocityH)
    {
        auditionKeyAt(p);
        return;
    }

    // Ruler: loop drag (no modifier)
    if (p.y < Theme::rulerH)
    {
        if (!(loopEnabled && *loopEnabled)) return;

        const int xs = xFromBeats(*loopStartBeats);
        const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);

        const bool onLeft  = std::abs(p.x - xs) <= 6;
        const bool onRight = std::abs(p.x - xe) <= 6;
        const bool inBody  = (p.x > xs + 6 && p.x < xe - 6);

        if (onLeft)       loopDragMode = LoopDrag::ResizeLeft;
        else if (onRight) loopDragMode = LoopDrag::ResizeRight;
        else if (inBody)  loopDragMode = LoopDrag::Move;
        else              loopDragMode = LoopDrag::None;

        if (loopDragMode != LoopDrag::None)
        {
            loopStartAtDown = *loopStartBeats;
            loopLenAtDown   = *loopLengthBeats;
            selectionDragActive = false;
            repaint();
            return;
        }
    }

    // Velocity lane drag
    if (p.y >= getHeight()-Theme::velocityH)
    {
        velDragActive = true;
        velNearest = closestNoteToX(p.x);
        return;
    }

    // --- DRAW tool: create a note and drag to set length ---
    if (activeTool == MidiEditor::Tool::Draw && p.y >= Theme::rulerH)
    {
        const int row = juce::jlimit(0, totalRows-1, (p.y - Theme::rulerH) / rowHeight);
        const int pitch = rowToPitch(row);
        double start = snapToGrid(beatsFromX(p.x));
        createdStartBeats = start;

        MidiNote n;
        n.pitch = pitch;
        n.startBeats = start;
        n.lengthBeats = juce::jmax(0.03125, rememberedNoteLengthBeats);
        n.uid = juce::Random::getSystemRandom().nextInt64();
        notes->push_back(n);
        createdIndex = (int)notes->size() - 1;
        creatingNote = true;
        createdLengthAtDown = n.lengthBeats;
        createdMouseDownX = p.x;
        createdPitchAtDown = pitch;
        previewPitch(pitch);
        if (onNotesChanged) onNotesChanged();
        syncNoteComponents(); repaint(); return;
    }

    // Empty grid + Select => marquee
    if (activeTool == MidiEditor::Tool::Select)
    {
        selectionDragActive = true;
        marqueeActive = true;
        selStart = p;
        marquee = juce::Rectangle<int>(p.x, p.y, 1, 1);
        repaint(); return;
    }
}

void MidiEditor::Content::mouseDrag(const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    bypassSnap = e.mods.isAltDown();

    if (panActive)
    {
        auto dv = p - panStart;
        setParentViewportPos({ juce::jmax(0, panViewStart.x - dv.x), juce::jmax(0, panViewStart.y - dv.y) });
        return;
    }

    if (auditionedPitch >= 0)
    {
        auditionKeyAt(p);
        return;
    }

    // Loop drag
    if (loopEnabled && *loopEnabled && (loopDragMode != LoopDrag::None))
    {
        const double startAtDown = loopStartAtDown;
        const double lenAtDown   = loopLenAtDown;
        const double mouseBeats  = snapToGrid(beatsFromX(p.x));

        if (loopDragMode == LoopDrag::Move)
        {
            const double delta = mouseBeats - startAtDown;
            *loopStartBeats  = juce::jmax(0.0, startAtDown + delta);
            *loopLengthBeats = juce::jmax(0.0, lenAtDown);
        }
        else if (loopDragMode == LoopDrag::ResizeLeft)
        {
            const double newStart = juce::jmin(mouseBeats, startAtDown + lenAtDown - 0.25);
            const double newLen   = juce::jmax(0.25, (startAtDown + lenAtDown) - newStart);
            *loopStartBeats  = juce::jmax(0.0, newStart);
            *loopLengthBeats = newLen;
        }
        else if (loopDragMode == LoopDrag::ResizeRight)
        {
            const double newEnd = juce::jmax(mouseBeats, startAtDown + 0.25);
            const double newLen = juce::jmax(0.25, newEnd - startAtDown);
            *loopStartBeats  = juce::jmax(0.0, startAtDown);
            *loopLengthBeats = newLen;
        }

        if (onLayoutRequest) onLayoutRequest();

        if (auto* editor = findParentComponentOfClass<MidiEditor>())
            if (editor->onLoopChanged) editor->onLoopChanged(*loopStartBeats, *loopLengthBeats);

        repaint();
        return;
    }

    if (creatingNote && createdIndex >= 0 && createdIndex < (int)notes->size())
    {
        auto& n = (*notes)[(size_t)createdIndex];
        const int dx = std::abs(p.x - createdMouseDownX);
        if (dx > 12)
        {
            double end = snapToGrid(beatsFromX(p.x));
            if (end <= createdStartBeats) end = createdStartBeats + (snapQuantum() > 0 ? snapQuantum() : createdLengthAtDown);
            n.lengthBeats = juce::jmax(0.03125, end - createdStartBeats);
            rememberedNoteLengthBeats = n.lengthBeats;
        }

        const int row = juce::jlimit(0, totalRows - 1, (p.y - Theme::rulerH) / rowHeight);
        n.pitch = rowToPitch(row);
        if (n.pitch != createdPitchAtDown)
        {
            createdPitchAtDown = n.pitch;
            previewPitch(n.pitch);
        }
        if (onNotesChanged) onNotesChanged();
        syncNoteComponents(); repaint(); return;
    }

    if (selectionDragActive)
    {
        const int x0 = juce::jlimit(0, getWidth(), selStart.x);
        const int y0 = juce::jlimit(0, getHeight(), selStart.y);
        const int x1 = juce::jlimit(0, getWidth(), p.x);
        const int y1 = juce::jlimit(0, getHeight(), p.y);
        marquee = juce::Rectangle<int>::leftTopRightBottom(
            juce::jmin(x0,x1), juce::jmin(y0,y1), juce::jmax(x0,x1), juce::jmax(y0,y1));
        repaint(); return;
    }

    if (velDragActive)
    {
        if (velNearest < 0 || !notes) return;
        auto& n = (*notes)[(size_t)velNearest];
        const int baseY = getHeight() - 10;
        int h = juce::jlimit(0, Theme::velocityH-16, baseY - p.y);
        n.velocity = juce::jlimit(1, 127, (int)std::round((h / (double)(Theme::velocityH-16))*127.0));
        if (onNotesChanged) onNotesChanged();
        repaint(); return;
    }
}

void MidiEditor::Content::mouseUp(const juce::MouseEvent&)
{
    if (selectionDragActive && activeTool == MidiEditor::Tool::Zoom)
    {
        const auto rect = marquee;
        if (rect.getWidth() > 8 && rect.getHeight() > 8) zoomToRect(rect);
    }
    else if (selectionDragActive && activeTool == MidiEditor::Tool::Select)
    {
        selectNotesInRect(marquee);
        syncNoteComponents();
    }

    marqueeActive = false;
    selectionDragActive = false;
    panActive = false;
    velDragActive = false;
    creatingNote = false;
    createdIndex = -1;
    auditionedPitch = -1;
    dragPreviewPitch = -1;
    dragEdgeAtDown = 0;

    // reset loop drag state
    loopDragMode = LoopDrag::None;

    repaint();
}

void MidiEditor::Content::mouseMove(const juce::MouseEvent& e)
{
    // loop hover affordances (on ruler only)
    hoverOnLeftEdge = hoverOnRightEdge = hoverOnBody = false;

    if (!(loopEnabled && *loopEnabled && *loopLengthBeats > 0.0)) { setMouseCursor(juce::MouseCursor::NormalCursor); return; }

    const auto p = e.getPosition();
    if (p.y >= 0 && p.y < Theme::rulerH)
    {
        const int xs = xFromBeats(*loopStartBeats);
        const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);
        const bool onLeft  = std::abs(p.x - xs) <= 6;
        const bool onRight = std::abs(p.x - xe) <= 6;
        const bool inBody  = (p.x > xs + 6 && p.x < xe - 6);

        hoverOnLeftEdge  = onLeft;
        hoverOnRightEdge = onRight;
        hoverOnBody      = inBody;

        if      (onLeft || onRight) setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        else if (inBody)            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        else                        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    repaint();
}

bool MidiEditor::Content::keyPressed (const juce::KeyPress& key)
{
    if (!notes) return false;

    // Delete: remove the note closest to viewport center (single)
    if (key.getKeyCode() == juce::KeyPress::deleteKey)
    {
        const int vpX = getParentViewportPos().x;
        const int vpW = getParentViewportW();
        const int centerX = vpX + vpW/2;
        const int idx = closestNoteToX(centerX);
        if (!selection.isEmpty())
        {
            std::vector<int> selected;
            for (int i = 0; i < selection.size(); ++i)
                selected.push_back(selection[i]);

            std::sort(selected.begin(), selected.end(), std::greater<int>());
            for (const int selectedIndex : selected)
                if (juce::isPositiveAndBelow(selectedIndex, (int)notes->size()))
                    notes->erase(notes->begin() + selectedIndex);

            selection.clearQuick();
            syncNoteComponents();
            if (onNotesChanged) onNotesChanged();
            repaint();
            return true;
        }

        if (idx >= 0) { notes->erase(notes->begin() + idx); syncNoteComponents(); if (onNotesChanged) onNotesChanged(); repaint(); return true; }
    }
    return false;
}

void MidiEditor::Content::resized() { syncNoteComponents(); }

// ===== helpers =====
double MidiEditor::Content::ppb() const { return *pixelsPerBeat; }
int    MidiEditor::Content::gridHeight() const { return getHeight() - Theme::rulerH - Theme::velocityH; }
double MidiEditor::Content::beatsFromX(int xContent) const { return Theme::beatsFromX(xContent, ppb(), Theme::keyWidth); }
int    MidiEditor::Content::xFromBeats(double beats) const { return Theme::xFromBeats(beats, ppb(), Theme::keyWidth); }
int    MidiEditor::Content::pitchToRow(int pitch) const
{
    const int bottomPitch = topPitch - totalRows + 1;
    const int clamped     = juce::jlimit(bottomPitch, topPitch, pitch);
    return topPitch - clamped;
}
int    MidiEditor::Content::rowToPitch(int row) const
{
    const int p = topPitch - row;
    const int bottomPitch = topPitch - totalRows + 1;
    return juce::jlimit(bottomPitch, topPitch, p);
}
double MidiEditor::Content::snapQuantum() const
{
    const bool snap = (*snapToGridRef && !bypassSnap);
    if (!snap) return 0.0;
    return gridQuantumBeats ? *gridQuantumBeats : 0.25;
}
double MidiEditor::Content::snapToGrid(double beats) const
{
    const double q = snapQuantum();
    return (q > 0.0) ? std::round(beats / q) * q : beats;
}
int MidiEditor::Content::closestNoteToX(int x) const
{
    if (!notes || notes->empty()) return -1;
    double best = 1e9; int bi = -1;
    for (int i=0;i<(int)notes->size();++i)
    {
        const auto& n = (*notes)[(size_t)i];
        const int cx = xFromBeats(n.startBeats + n.lengthBeats * 0.5);
        const double d = std::abs((double)cx - x);
        if (d < best) { best = d; bi = i; }
    }
    return bi;
}
int MidiEditor::Content::noteIndexByUid(uint32 uid) const
{
    if (!notes) return -1;
    for (int i=0;i<(int)notes->size();++i) if ((*notes)[(size_t)i].uid == uid) return i;
    return -1;
}
int MidiEditor::Content::indexAtPointNoteArea(juce::Point<int> p) const
{
    if (!notes) return -1;
    if (p.y < Theme::rulerH || p.y >= (getHeight() - Theme::velocityH)) return -1;
    const int row = (p.y - Theme::rulerH) / rowHeight;
    const int pitch = rowToPitch(juce::jlimit(0, totalRows-1, row));
    const double beat = beatsFromX(p.x);

    int best = -1; double bestDist = 1e9;
    for (int i=0;i<(int)notes->size();++i)
    {
        auto& n = (*notes)[(size_t)i];
        if (n.pitch != pitch) continue;
        const double c = n.startBeats + 0.5 * n.lengthBeats;
        const double d = std::abs(c - beat);
        if (d < bestDist && d < 2.0) { bestDist = d; best = i; }
    }
    return best;
}

void MidiEditor::Content::previewPitch(int pitch)
{
    pitch = juce::jlimit(0, 127, pitch);
    if (pitch == dragPreviewPitch)
        return;

    dragPreviewPitch = pitch;
    if (onPreviewNote)
        onPreviewNote(pitch, 105);
}

juce::String MidiEditor::Content::pitchName(int pitch)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    pitch = juce::jlimit(0, 127, pitch);
    return juce::String(names[pitch % 12]) + juce::String((pitch / 12) - 1);
}

void MidiEditor::Content::auditionKeyAt(juce::Point<int> p)
{
    if (!onPreviewNote)
        return;

    const int stickyKeyLeft = getParentViewportPos().x;
    if (p.x < stickyKeyLeft || p.x >= stickyKeyLeft + Theme::keyWidth
        || p.y < Theme::rulerH || p.y >= getHeight() - Theme::velocityH)
        return;

    const int row = juce::jlimit(0, totalRows - 1, (p.y - Theme::rulerH) / rowHeight);
    const int pitch = rowToPitch(row);
    if (pitch == auditionedPitch)
        return;

    auditionedPitch = pitch;
    onPreviewNote(pitch, 105);
}

juce::Rectangle<int> MidiEditor::Content::noteBounds(int idx) const
{
    if (!notes || !juce::isPositiveAndBelow(idx, (int)notes->size()))
        return {};

    const auto& n = (*notes)[(size_t)idx];
    return { xFromBeats(n.startBeats),
             Theme::rulerH + pitchToRow(n.pitch) * rowHeight + 3,
             juce::jmax(8, (int)std::round(n.lengthBeats * ppb())),
             rowHeight - 6 };
}

void MidiEditor::Content::selectNotesInRect(juce::Rectangle<int> rect)
{
    selection.clearQuick();
    if (!notes || rect.getWidth() <= 2 || rect.getHeight() <= 2)
        return;

    for (int i = 0; i < (int)notes->size(); ++i)
        if (rect.intersects(noteBounds(i)))
            selection.add(i);
}

void MidiEditor::Content::zoomToRect(juce::Rectangle<int> rect)
{
    if (!onLayoutRequest || !pixelsPerBeat) return;
    const int viewportW = juce::jmax(1, getParentViewportW() - Theme::keyWidth - 32);
    const int selW      = juce::jmax(1, rect.getWidth());
    const double targetPPB = juce::jlimit(24.0, 4096.0, (double)viewportW / (double)selW * ppb());
    *pixelsPerBeat = targetPPB;
    onLayoutRequest();
    centerContentOn(rect.getCentreX(), rect.getCentreY());

    // keep at least one note in view
    const double anchorBeat = beatsFromX(rect.getCentreX());
    ensureViewportShowsNotesNearBeat(anchorBeat);
}
void MidiEditor::Content::restoreZoom()
{
    if (auto* editor = findParentComponentOfClass<MidiEditor>()) editor->frameAllView();
}
juce::Point<int> MidiEditor::Content::getParentViewportPos() const
{
    if (auto* vp = findParentComponentOfClass<juce::Viewport>()) return vp->getViewPosition();
    return {0,0};
}
int MidiEditor::Content::getParentViewportW() const
{
    if (auto* vp = findParentComponentOfClass<juce::Viewport>()) return vp->getViewWidth();
    return getWidth();
}
void MidiEditor::Content::setParentViewportPos(juce::Point<int> p)
{
    if (auto* vp = findParentComponentOfClass<juce::Viewport>()) vp->setViewPosition(p);
}
void MidiEditor::Content::centerContentOn(int x, int y)
{
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        const int vw = vp->getViewWidth();
        const int vh = vp->getViewHeight();

        const int targetX = x - vw / 2;
        const int targetY = y - vh / 2;

        const int maxX = juce::jmax(0, getWidth()  - vw);
        const int maxY = juce::jmax(0, getHeight() - vh);

        vp->setViewPosition(juce::jlimit(0, maxX, targetX),
                            juce::jlimit(0, maxY, targetY));
    }
}

void MidiEditor::Content::ensureViewportShowsNotesNearBeat(double anchorBeatIfNeeded)
{
    if (!notes || notes->empty()) return;
    const int vpX = getParentViewportPos().x;
    const int vpW = getParentViewportW();
    const int left  = vpX;
    const int right = vpX + vpW;

    double minB = 1e9, maxB = -1e9;
    for (const auto& n : *notes)
    {
        minB = std::min(minB, n.startBeats);
        maxB = std::max(maxB, n.startBeats + n.lengthBeats);
    }
    const int minX = Theme::keyWidth + (int)std::round(minB * ppb());
    const int maxX = Theme::keyWidth + (int)std::round(maxB * ppb());
    const bool intersects = !(right < minX || left > maxX);
    if (intersects) return;

    const double targetBeat =
        (anchorBeatIfNeeded < minB) ? minB :
        (anchorBeatIfNeeded > maxB) ? maxB : anchorBeatIfNeeded;

    const int targetX = Theme::keyWidth + (int)std::round(targetBeat * ppb());
    centerContentOn(targetX, Theme::rulerH + gridHeight()/2);
}
// ==== Content note interaction helpers =====================================

void MidiEditor::Content::handleNoteMouseDown(uint32 uid, juce::Point<int> pInContent,
                                              const juce::ModifierKeys& mods,
                                              bool& didSelect, int& edge)
{
    didSelect = false;
    const int idx = noteIndexByUid(uid);
    if (idx < 0 || !notes) { edge = 0; return; }

    const bool additive = mods.isShiftDown() || mods.isCommandDown() || mods.isCtrlDown();
    if (additive)
    {
        if (selection.contains(idx))
            selection.removeFirstMatchingValue(idx);
        else
            selection.add(idx);
    }
    else if (!selection.contains(idx))
    {
        selection.clearQuick();
        selection.add(idx);
    }
    else if (selection.isEmpty())
    {
        selection.add(idx);
    }

    didSelect = true;
    rememberedNoteLengthBeats = juce::jmax(0.03125, (*notes)[(size_t) idx].lengthBeats);

    dragAnchorContent = pInContent;
    dragPreviewPitch = -1;

    // snapshot selection state
    startAtDown.clear(); lenAtDown.clear(); pitchAtDown.clear();
    for (int i=0; i<selection.size(); ++i)
    {
        startAtDown.add((*notes)[(size_t)selection[i]].startBeats);
        lenAtDown  .add((*notes)[(size_t)selection[i]].lengthBeats);
        pitchAtDown.add((*notes)[(size_t)selection[i]].pitch);
    }

    edge = hitEdgeAt(idx, pInContent);
    dragEdgeAtDown = edge;
    previewPitch((*notes)[(size_t) idx].pitch);
    syncNoteComponents();
}

void MidiEditor::Content::handleNoteMouseDrag(uint32 /*uid*/, juce::Point<int> pInContent)
{
    if (!notes) return;

    const double dxBeats = (pInContent.x - dragAnchorContent.x) / ppb();
    const int    rowDelta = (pInContent.y - dragAnchorContent.y) / rowHeight;

    const int edge = dragEdgeAtDown;

    if (edge == 1) // resize left
    {
        for (int si=0; si<selection.size(); ++si)
        {
            auto& n = (*notes)[(size_t)selection[si]];
            double ns = snapToGrid(startAtDown[si] + dxBeats);
            double nl = snapToGrid(lenAtDown  [si] - dxBeats);
            n.startBeats  = juce::jmax(0.0, ns);
            n.lengthBeats = juce::jmax(0.03125, nl);
            rememberedNoteLengthBeats = n.lengthBeats;
            n.pitch = juce::jlimit(topPitch-totalRows+1, topPitch, pitchAtDown[si] - rowDelta);
        }
    }
    else if (edge == 2) // resize right
    {
        for (int si=0; si<selection.size(); ++si)
        {
            auto& n = (*notes)[(size_t)selection[si]];
            double nl = snapToGrid(lenAtDown[si] + dxBeats);
            n.lengthBeats = juce::jmax(0.03125, nl);
            rememberedNoteLengthBeats = n.lengthBeats;
            n.pitch = juce::jlimit(topPitch-totalRows+1, topPitch, pitchAtDown[si] - rowDelta);
        }
    }
    else // move
    {
        for (int si=0; si<selection.size(); ++si)
        {
            auto& n = (*notes)[(size_t)selection[si]];
            double b = snapToGrid(startAtDown[si] + dxBeats);
            n.startBeats = juce::jmax(0.0, b);
            n.pitch = juce::jlimit(topPitch-totalRows+1, topPitch, pitchAtDown[si] - rowDelta);
        }
    }

    if (selection.size() > 0)
        previewPitch((*notes)[(size_t)selection[0]].pitch);

    syncNoteComponents();
    if (onNotesChanged) onNotesChanged();

    if (extendToPixelRight)
    {
        double maxEnd = 0.0;
        for (auto& n : *notes) maxEnd = std::max(maxEnd, n.startBeats + n.lengthBeats);
        extendToPixelRight(xFromBeats(maxEnd) + 200);
    }

    repaint();
}

void MidiEditor::Content::handleNoteMouseUp(uint32 /*uid*/)
{
    // changes are already applied live
}

void MidiEditor::Content::eraseNoteByUid(uint32 uid)
{
    if (!notes) return;
    const int idx = noteIndexByUid(uid);
    if (idx < 0) return;
    notes->erase(notes->begin() + idx);
    selection.clearQuick();
    syncNoteComponents();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

int MidiEditor::Content::hitEdgeAt(int idx, juce::Point<int> p) const
{
    if (!notes) return 0;

    const auto& n = (*notes)[(size_t)idx];
    const int x   = xFromBeats(n.startBeats);
    const int w   = juce::jmax(8, (int)std::round(n.lengthBeats * ppb()));
    const int y   = Theme::rulerH + pitchToRow(n.pitch) * rowHeight + 3;
    const int h   = rowHeight - 6;

    juce::Rectangle<int> box(x, y, w, h);
    if (!box.contains(p)) return 0;
    const int handleW = juce::jlimit(5, 10, w / 5);
    if (juce::Rectangle<int>(x,             y, handleW, h).contains(p)) return 1; // left
    if (juce::Rectangle<int>(x+w-handleW,   y, handleW, h).contains(p)) return 2; // right
    return 0; // body
}
