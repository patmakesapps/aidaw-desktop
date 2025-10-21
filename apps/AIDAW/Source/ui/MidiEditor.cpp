#include "MidiEditor.h"

// ===== MidiEditor =====
MidiEditor::MidiEditor()
{
    setOpaque(true);
    setWantsKeyboardFocus(true);

    // --- Toolbar ---
    for (juce::TextButton* b : { &btnSelect, &btnDraw, &btnZoomTool, &btnFrameAll, &btnSnap, &btnZoomOut, &btnZoomIn })
    { addAndMakeVisible(b); b->addListener(this); b->setWantsKeyboardFocus(false); }

    btnSelect  .setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"⬚"))); btnSelect.setTooltip ("Select / Move (1)");
    btnDraw    .setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"✎"))); btnDraw.setTooltip   ("Draw (2)");
    btnZoomTool.setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"🔍"))); btnZoomTool.setTooltip("Zoom tool (4): L-drag=box, Right=reset to Frame All");
    btnFrameAll.setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"◰"))); btnFrameAll.setTooltip("Frame all (F)");
    btnSnap    .setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"⛓"))); btnSnap.setTooltip   ("Snap (G)");
    btnZoomOut .setButtonText("-");                                                           btnZoomOut.setTooltip("Zoom out (-)");
    btnZoomIn  .setButtonText("+");                                                           btnZoomIn.setTooltip ("Zoom in (+)");

    for (juce::TextButton* b : { &btnSelect, &btnDraw, &btnZoomTool }) b->setClickingTogglesState(true);
    btnSnap.setClickingTogglesState(true); btnSnap.setToggleState(true, juce::dontSendNotification);
    setTool(Tool::Select);

    // --- Viewport + content ---
    addAndMakeVisible(view);
    view.setViewedComponent(new Content(), true);
    auto* c = dynamic_cast<Content*>(view.getViewedComponent());
    jassert(c != nullptr);

    // Viewport: scrollbars and no built-in drag scroll
    view.setScrollBarsShown(true, true);
    view.setScrollOnDragEnabled(false);

    // Bridge refs (Content -> Editor)
    c->requestRepaint = [c]{ c->repaint(); };
    c->ensureVisible = [this](juce::Rectangle<int> r)
    {
        const auto va = view.getViewArea();
        const int vx = view.getViewPositionX();
        const int vy = view.getViewPositionY();
        const int vw = va.getWidth();
        const int vh = va.getHeight();

        int nx = vx, ny = vy;

        if (r.getX() < vx)                 nx = r.getX();
        else if (r.getRight() > vx + vw)   nx = r.getRight() - vw;

        if (r.getY() < vy)                 ny = r.getY();
        else if (r.getBottom() > vy + vh)  ny = r.getBottom() - vh;

        nx = juce::jmax(0, nx);
        ny = juce::jmax(0, ny);

        view.setViewPosition(nx, ny);
    };
    c->extendToPixelRight = this->extendToPixelRight;
    c->onRequestFrameAll  = [this] { this->frameAll(); };

    // Model refs
    c->notes           = &notes;
    c->snapToGridRef   = &snapToGrid;
    c->pixelsPerBeat   = &pixelsPerBeat;
    c->playheadBeats   = &playheadBeats;
    c->loopEnabled     = &loopEnabled;
    c->loopStartBeats  = &loopStartBeats;
    c->loopLengthBeats = &loopLengthBeats;

    // Defaults
    setBPM(120.0);
    setSnap(true);
    setHorizontalZoom(8.0);
    setPitchView(48, 84);
    setBeatsExtent(64.0);

    refreshContentSize();
    c->syncNoteComponents();
}

void MidiEditor::setNotes(const std::vector<MidiNote>& newNotes)
{
    notes = newNotes;
    if (autoFitBeatsOnSet)  autoFitBeatsToNotes(8.0);
    if (autoFitPitchOnSet)  autoFitPitchToNotes();
    refreshContentSize();

    if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
        c->syncNoteComponents();
}

std::vector<MidiNote>& MidiEditor::editNotes() { return notes; }
const std::vector<MidiNote>& MidiEditor::getNotes() const { return notes; }

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

void MidiEditor::setHorizontalZoom(double beatsPerScreen)
{
    beatsPerScreen = juce::jlimit(4.0, 64.0, beatsPerScreen);
    const int vw = juce::jmax(1, view.getWidth());
    const double desiredPPB = (double)vw / beatsPerScreen;
    pixelsPerBeat = juce::jlimit(minPPB, maxPPB, desiredPPB);
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
    refreshContentSize();
}

void MidiEditor::zoomDeltaFromWheel(double delta, int screenX)
{
    // Convert screen X -> content X, so zoom anchors at mouse
    int anchorContentX = Theme::keyWidth + view.getViewPositionX();
    if (auto* content = view.getViewedComponent())
    {
        const int contentScreenX = content->getScreenX();
        anchorContentX = juce::jlimit(0, content->getWidth(),
                                      screenX - contentScreenX);
    }
    zoomAtContentX(delta, anchorContentX);
}

void MidiEditor::zoomAtContentX(double steps, int anchorXContent)
{
    const double oldPPB   = pixelsPerBeat;
    const double beat     = juce::jmax(0.0, (anchorXContent - Theme::keyWidth) / oldPPB);
    const int    xBefore  = Theme::keyWidth + (int)std::round(beat * oldPPB);

    zoomScale     = juce::jlimit(0.05, 30.0, zoomScale * std::pow(1.12, steps));
    pixelsPerBeat = juce::jlimit(minPPB, maxPPB, ppbAt120 * (120.0 / bpmValue) * zoomScale);

    refreshContentSize();
    const int xAfter = Theme::keyWidth + (int)std::round(beat * pixelsPerBeat);
    const int dx = xAfter - xBefore;

    const int maxX = juce::jmax(0, view.getViewedComponent()->getWidth() - view.getViewWidth());
    view.setViewPosition(juce::jlimit(0, maxX, view.getViewPositionX() + dx),
                         view.getViewPositionY());
    repaint();
}

void MidiEditor::frameAll()
{
    double endB = 64.0;
    for (auto& n : notes) endB = std::max(endB, n.startBeats + n.lengthBeats);
    endB = std::max(endB, loopEnabled ? (loopStartBeats + std::max(loopLengthBeats, 0.25)) : 0.0);

    const int viewportW = juce::jmax(1, view.getViewWidth() - Theme::keyWidth - 40);
    const double target = (double)viewportW / juce::jmax(8.0, endB);
    pixelsPerBeat = juce::jlimit(minPPB, maxPPB, target);
    zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
    refreshContentSize();
    view.setViewPosition(0, view.getViewPositionY());
}

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
    if (onLoopChanged) onLoopChanged(loopStartBeats, loopLengthBeats);
    repaint();
}
void MidiEditor::setLoopRegion(double s, double len)
{
    loopStartBeats  = juce::jmax(0.0, s);
    loopLengthBeats = juce::jmax(0.0, len);
    if (onLoopChanged) onLoopChanged(loopStartBeats, loopLengthBeats);
    repaint();
}

void MidiEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::colBgMain));
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.drawRect(getLocalBounds());
}

void MidiEditor::resized()
{
    auto r = getLocalBounds().reduced(8, 6);
    auto tools = r.removeFromTop(34);
    btnSelect  .setBounds(tools.removeFromLeft(40)); tools.removeFromLeft(6);
    btnDraw    .setBounds(tools.removeFromLeft(40)); tools.removeFromLeft(6);
    btnZoomTool.setBounds(tools.removeFromLeft(40)); tools.removeFromLeft(6);
    btnFrameAll.setBounds(tools.removeFromLeft(40)); tools.removeFromLeft(12);
    btnSnap    .setBounds(tools.removeFromLeft(56)); tools.removeFromLeft(12);
    btnZoomOut .setBounds(tools.removeFromLeft(36)); tools.removeFromLeft(4);
    btnZoomIn  .setBounds(tools.removeFromLeft(36));

    view.setBounds(r);
    refreshContentSize();

    if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
        c->syncNoteComponents();
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
    auto mark = [&](juce::TextButton& b, bool on){
        b.setToggleState(on, juce::dontSendNotification);
        b.setColour(juce::TextButton::buttonColourId, on ? juce::Colour(Theme::colBtnActive) : juce::Colour(Theme::colBtnIdle));
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::colBtnText));
    };
    mark(btnSelect,  t==Tool::Select);
    mark(btnDraw,    t==Tool::Draw);
    mark(btnZoomTool,t==Tool::Zoom);

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
}

void MidiEditor::refreshContentSize()
{
    auto* c = dynamic_cast<Content*>(view.getViewedComponent());
    if (!c) return;

    double maxEnd = beatsExtent;
    for (auto& n : notes) maxEnd = std::max(maxEnd, n.startBeats + n.lengthBeats + 4.0);
    if (loopEnabled) maxEnd = std::max(maxEnd, loopStartBeats + loopLengthBeats + 4.0);

    const int widthFromBeats = Theme::keyWidth + (int)std::ceil(maxEnd * pixelsPerBeat);
    const int minOpenWidth   = juce::jmax(1600, view.getWidth() + 900);
    const int width = juce::jmax(widthFromBeats, minOpenWidth);

    c->rowHeight = Theme::rowHeight;
    c->totalRows = (maxPitch - minPitch + 1);
    c->topPitch  = maxPitch;
    const int height  = Theme::rulerH + c->totalRows * c->rowHeight + Theme::velocityH;

    c->setSize(width, height);
    c->syncNoteComponents();
    c->repaint();
}

// ===== Content =====
MidiEditor::Content::Content()
{
    setInterceptsMouseClicks(true, false);
}

void MidiEditor::Content::syncNoteComponents()
{
    if (!notes) return;

    // Add missing
    while ((int)noteComps.size() < (int)notes->size())
    {
        int idx = (int)noteComps.size();
        auto up = std::make_unique<NoteComponent>(
            idx,
            [this](int i, juce::Point<int> p, bool& didSelect, int& edge){ handleNoteMouseDown(i, p, didSelect, edge); },
            [this](int i, juce::Point<int> p){ handleNoteMouseDrag(i, p); },
            [this](int i){ handleNoteMouseUp(i); },
            [this](int i){ eraseNote(i); }
        );
        addAndMakeVisible(up.get());
        noteComps.emplace_back(std::move(up));
    }
    // Remove extras
    while ((int)noteComps.size() > (int)notes->size())
        noteComps.pop_back();

    // Layout + colors
    for (size_t i=0;i<noteComps.size();++i)
    {
        auto& n = (*notes)[i];
        const int x = xFromBeats(n.startBeats);
        const int y = Theme::rulerH + pitchToRow(n.pitch) * rowHeight + 3;
        const int w = juce::jmax(8, (int)std::round(n.lengthBeats * ppb()));
        const int h = rowHeight - 6;

        noteComps[i]->setBounds(x, y, w, h);

        const bool sel = selection.contains((int)i);
        const auto body = juce::Colour((i % 2 == 0) ? Theme::colNoteA : Theme::colNoteB)
                            .withMultipliedBrightness(sel ? 1.10f : 1.0f);
        const auto rim  = juce::Colour(body).brighter(Theme::noteBorderGain);
        noteComps[i]->setSelected(sel);
        noteComps[i]->setColors(juce::Colour(body).darker(0.30f), rim);
        noteComps[i]->index = (int)i;
    }
}

void MidiEditor::Content::repaintNotesOnly()
{
    for (auto& n : noteComps) if (n) n->repaint();
}

void MidiEditor::Content::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::colBgMain));
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.fillRect(Theme::keyWidth, 0, 1, getHeight());

    // Ruler / Keys / Grid / Loop / Vel / Playhead
    auto paintRuler = [&](juce::Graphics& gg)
    {
        auto r = juce::Rectangle<int>(0, 0, getWidth(), Theme::rulerH);
        gg.setColour(juce::Colour(Theme::colBgRuler)); gg.fillRect(r);
        gg.setColour(juce::Colour(Theme::colHeaderDiv)); gg.fillRect(r.withY(r.getBottom()-1).withHeight(1));

        const int totalBeats = (int)std::ceil((double)(getWidth()-Theme::keyWidth)/ppb());
        for (int beat = 0; beat <= totalBeats; ++beat)
            if ((beat % 4) == 0)
            {
                const int x = xFromBeats((double)beat);
                gg.setColour(juce::Colour(Theme::colBarLabel));
                gg.setFont(12.0f);
                gg.drawFittedText(juce::String((beat / 4) + 1), x + 6, 2, 36, Theme::rulerH-4,
                                  juce::Justification::centredLeft, 1);
            }

        // Loop handles
        if (loopEnabled && *loopEnabled && *loopLengthBeats > 0.0)
        {
            const int xs = xFromBeats(*loopStartBeats);
            const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);

            gg.setColour(juce::Colour(Theme::colLoop));
            gg.fillRect(xs, 0, 2, Theme::rulerH);
            gg.fillRect(xe, 0, 2, Theme::rulerH);

            juce::Path left;  left.addTriangle((float)xs, (float)Theme::rulerH, (float)xs+8.0f, 0.0f, (float)xs+16.0f, (float)Theme::rulerH);
            juce::Path right; right.addTriangle((float)xe, (float)Theme::rulerH, (float)xe-8.0f, 0.0f, (float)xe-16.0f, (float)Theme::rulerH);
            gg.fillPath(left); gg.fillPath(right);
        }
    };

    auto paintKeys = [&](juce::Graphics& gg)
    {
        auto r = juce::Rectangle<int>(0, Theme::rulerH, Theme::keyWidth, gridHeight());
        int y = r.getY();
        for (int row = 0; row < totalRows; ++row)
        {
            const int pitch = rowToPitch(row);
            const int pc = pitch % 12;
            const bool black = (pc==1||pc==3||pc==6||pc==8||pc==10);

            juce::Rectangle<int> key(0, y, Theme::keyWidth, rowHeight);
            gg.setColour(juce::Colour(black ? Theme::colKeyBlack : Theme::colKeyWhite));
            gg.fillRect(key);

            if ((pc == 0) && ((pitch % 24) == 0))
            {
                gg.setColour(juce::Colour(Theme::colText));
                gg.setFont(12.0f);
                const int octave = (pitch / 12) - 1;
                gg.drawText("C" + juce::String(octave), key.reduced(8, 3), juce::Justification::centredLeft);
            }

            gg.setColour(juce::Colour(Theme::colKeySep));
            gg.fillRect(key.removeFromBottom(1));
            y += rowHeight;
        }
    };

    auto paintGrid = [&](juce::Graphics& gg)
    {
        const auto r = juce::Rectangle<int>(Theme::keyWidth, Theme::rulerH,
                                            getWidth() - Theme::keyWidth, gridHeight());

        int y = r.getY();
        for (int row = 0; row < totalRows; ++row)
        {
            const bool odd  = (row & 1) != 0;
            gg.setColour(juce::Colour(odd ? Theme::colRowOdd : Theme::colRowEven));
            gg.fillRect(juce::Rectangle<int>(r.getX(), y, r.getWidth(), rowHeight));

            const int pitch = rowToPitch(row);
            if ((pitch % 12) == 0)
            {
                gg.setColour(juce::Colour(Theme::colOctave));
                gg.fillRect(r.getX(), y, r.getWidth(), 1);
            }
            y += rowHeight;
        }

        const double ppbVal = ppb();
        const int totalBeats = (int)std::ceil((getWidth() - Theme::keyWidth) / ppbVal);

        for (int beat = 0; beat <= totalBeats; ++beat)
        {
            const int x = Theme::keyWidth + (int)std::round(beat * ppbVal);
            const bool isBar = (beat % 4) == 0;
            if (isBar) { gg.setColour(juce::Colour(Theme::colGridBar));  gg.fillRect(x, Theme::rulerH, 2, getHeight() - Theme::rulerH); }
            else       { gg.setColour(juce::Colour(Theme::colGridBeat)); gg.fillRect(x, Theme::rulerH, 1, getHeight() - Theme::rulerH); }
        }

        const int subDivs = Theme::subDivisions(ppbVal);
        if (subDivs > 0)
        {
            gg.setColour(juce::Colour(Theme::colGridSub));
            for (int beat = 0; beat <= totalBeats; ++beat)
                for (int s = 1; s < subDivs; ++s)
                {
                    const double frac = (double) s / (double) subDivs;
                    const int x = Theme::keyWidth + (int) std::round((beat + frac) * ppbVal);
                    gg.fillRect(x, Theme::rulerH, 1, getHeight() - Theme::rulerH);
                }
        }
    };

    auto paintLoopOverlay = [&](juce::Graphics& gg)
    {
        if (!(loopEnabled && *loopEnabled && *loopLengthBeats > 0.0)) return;
        const int xs = xFromBeats(*loopStartBeats);
        const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);
        juce::Rectangle<int> loopRect(xs, Theme::rulerH, xe - xs, gridHeight());
        gg.setColour(juce::Colour(Theme::colLoopFill));
        gg.fillRect(loopRect);
    };

    auto paintVelocities = [&](juce::Graphics& gg)
    {
        auto r = juce::Rectangle<int>(0, getHeight()-Theme::velocityH, getWidth(), Theme::velocityH);
        gg.setColour(juce::Colour(Theme::colVelLane)); gg.fillRect(r);
        gg.setColour(juce::Colour(0x223B82F6)); gg.fillRect(r.removeFromTop(1));

        if (!notes) return;
        const int baseY = getHeight() - 10;
        for (const auto& n : *notes)
        {
            const int x = xFromBeats(n.startBeats + n.lengthBeats * 0.5);
            const int h = juce::jlimit(2, Theme::velocityH-16, (int)std::round((n.velocity/127.0) * (Theme::velocityH-16)));
            const auto barCol = juce::Colour(((n.pitch + 5) % 2 == 0) ? Theme::colNoteA : Theme::colNoteB);
            gg.setColour(barCol);
            gg.fillRect(x-1, baseY - h, 2, h);
        }
    };

    auto paintPlayhead = [&](juce::Graphics& gg)
    {
        if (!playheadBeats) return;
        const int x = xFromBeats(*playheadBeats);
        gg.setColour(juce::Colour(Theme::colPlayhead));
        gg.fillRect(x-1, 0, 2, getHeight());
    };

    paintRuler(g);
    paintKeys(g);
    paintGrid(g);
    paintLoopOverlay(g);
    paintVelocities(g);
    paintPlayhead(g);

    if (marqueeActive)
    {
        g.setColour(juce::Colour(Theme::colSelect));
        auto rect = juce::Rectangle<int>(
            juce::jmin(selStart.x, getMouseXYRelative().x),
            juce::jmin(selStart.y, getMouseXYRelative().y),
            std::abs(getMouseXYRelative().x - selStart.x),
            std::abs(getMouseXYRelative().y - selStart.y));
        g.fillRect(rect);
        g.setColour(juce::Colour(Theme::colSelectBd));
        g.drawRect(rect);
    }
}

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

    // Zoom tool: left = start marquee; right = reset (frame all)
    if (activeTool == MidiEditor::Tool::Zoom)
    {
        if (e.mods.isRightButtonDown()) { if (onRequestFrameAll) onRequestFrameAll(); return; }
        selectionDragActive = true;
        marqueeActive = true;
        additiveMarquee = e.mods.isShiftDown();
        preMarqueeSelection = selection;
        selStart = p;
        repaint(); return;
    }

    // Ruler interactions:
    if (p.y < Theme::rulerH)
    {
        const double beatAtMouse = snapToGrid(beatsFromX(p.x));
        const int xs = (loopEnabled && *loopEnabled) ? xFromBeats(*loopStartBeats) : -100000;
        const int xe = (loopEnabled && *loopEnabled) ? xFromBeats(*loopStartBeats + *loopLengthBeats) : -100000;

        bool overLeft  = (std::abs(p.x - xs) <= 6);
        bool overRight = (std::abs(p.x - xe) <= 6);
        bool inside    = (loopEnabled && *loopEnabled && p.x >= xs && p.x <= xe);

        // start loop tool if SHIFT held or user clicks near handles / inside loop
        if (e.mods.isShiftDown() || overLeft || overRight || inside)
        {
            if (!loopEnabled || !*loopEnabled) *loopEnabled = true;

            loopDragActive = true;
            loopStartAtDown = (loopStartBeats ? *loopStartBeats : 0.0);
            loopLenAtDown   = (loopLengthBeats ? *loopLengthBeats : 0.0);

            if (overLeft)      loopDragMode = LoopDrag::ResizeLeft;
            else if (overRight)loopDragMode = LoopDrag::ResizeRight;
            else               loopDragMode = LoopDrag::Move;

            // if no loop yet, initialize a short one at mouse
            if (loopLenAtDown <= 0.0)
            {
                const double minLen = 0.25;
                if (loopStartBeats)  *loopStartBeats  = beatAtMouse;
                if (loopLengthBeats) *loopLengthBeats = minLen;
                loopStartAtDown = *loopStartBeats; loopLenAtDown = *loopLengthBeats;
                // notify
                if (auto* ed = findParentComponentOfClass<MidiEditor>())
                    if (ed->onLoopChanged) ed->onLoopChanged(*loopStartBeats, *loopLengthBeats);
            }
            repaint(); return;
        }
    }

    // Right-click anywhere in grid to erase closest note under cursor row (single note)
    if (e.mods.isRightButtonDown() && p.y >= Theme::rulerH && p.y < (getHeight()-Theme::velocityH))
    {
        int victim = -1;
        int row = (p.y - Theme::rulerH) / rowHeight;
        int pitch = rowToPitch(row);
        double beat = beatsFromX(p.x);
        double best = 1e6;
        for (int i=0;i<(int)notes->size();++i)
        {
            auto& n = (*notes)[(size_t)i];
            if (n.pitch != pitch) continue;
            double c = n.startBeats + n.lengthBeats*0.5;
            double d = std::abs(c - beat);
            if (d < best && d < 2.0) { best = d; victim = i; }
        }
        if (victim >= 0) { notes->erase(notes->begin()+victim); syncNoteComponents(); repaint(); }
        return;
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
        MidiNote n; n.pitch = pitch; n.startBeats = start; n.lengthBeats = juce::jmax(0.25, snapQuantum() > 0 ? snapQuantum() : 0.25);
        notes->push_back(n);
        createdIndex = (int)notes->size() - 1;
        creatingNote = true;
        selection.clearQuick(); selection.add(createdIndex);
        syncNoteComponents(); repaint(); return;
    }

    // Empty grid + Select => marquee
    if (activeTool == MidiEditor::Tool::Select)
    {
        selectionDragActive = true;
        marqueeActive = true;
        additiveMarquee = e.mods.isShiftDown();
        preMarqueeSelection = selection;
        selStart = p;
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

    if (creatingNote && createdIndex >= 0 && createdIndex < (int)notes->size())
    {
        auto& n = (*notes)[(size_t)createdIndex];
        double end = snapToGrid(beatsFromX(p.x));
        if (end <= createdStartBeats) end = createdStartBeats + (snapQuantum() > 0 ? snapQuantum() : 0.25);
        n.lengthBeats = juce::jmax(0.03125, end - createdStartBeats);
        // allow vertical drag to move pitch while drawing
        int rowDelta = (p.y - (Theme::rulerH + pitchToRow(n.pitch)*rowHeight + rowHeight/2)) / rowHeight;
        n.pitch = juce::jlimit(topPitch-totalRows+1, topPitch, n.pitch - rowDelta);
        syncNoteComponents(); repaint(); return;
    }

    if (selectionDragActive && activeTool != MidiEditor::Tool::Zoom)
    {
        // marquee selection (live)
        const int x0 = juce::jlimit(0, getWidth(), selStart.x);
        const int y0 = juce::jlimit(0, getHeight(), selStart.y);
        const int x1 = juce::jlimit(0, getWidth(), p.x);
        const int y1 = juce::jlimit(0, getHeight(), p.y);
        auto marquee = juce::Rectangle<int>::leftTopRightBottom(
            juce::jmin(x0,x1), juce::jmin(y0,y1), juce::jmax(x0,x1), juce::jmax(y0,y1));

        juce::Array<int> newSel;
        for (int i=0;i<(int)notes->size();++i)
        {
            auto& n = (*notes)[(size_t)i];
            const int x   = xFromBeats(n.startBeats);
            const int y   = Theme::rulerH + pitchToRow(n.pitch)*rowHeight + 3;
            const int w   = juce::jmax(8, (int)std::round(n.lengthBeats * ppb()));
            const int h   = rowHeight - 6;
            if (marquee.intersects(juce::Rectangle<int>(x,y,w,h)))
                newSel.add(i);
        }
        if (additiveMarquee)
        {
            selection = preMarqueeSelection;
            for (int i=0;i<newSel.size();++i) if (!selection.contains(newSel[i])) selection.add(newSel[i]);
        }
        else selection = newSel;

        syncNoteComponents();
        repaint(); return;
    }

    if (selectionDragActive && activeTool == MidiEditor::Tool::Zoom)
    {
        repaint(); // marquee repaint handled in paint()
        return;
    }

    // Loop drag
    if (loopDragActive && loopEnabled && loopStartBeats && loopLengthBeats)
    {
        const double curBeat = snapToGrid(beatsFromX(p.x));
        const double minLen  = 0.25;

        if (loopDragMode == LoopDrag::ResizeRight)
        {
            double newLen = juce::jmax(minLen, curBeat - loopStartAtDown);
            *loopStartBeats  = loopStartAtDown;
            *loopLengthBeats = newLen;
        }
        else if (loopDragMode == LoopDrag::ResizeLeft)
        {
            double newStart = juce::jmax(0.0, curBeat);
            double newLen   = juce::jmax(minLen, loopLenAtDown + (loopStartAtDown - newStart));
            *loopStartBeats  = newStart;
            *loopLengthBeats = newLen;
        }
        else // Move
        {
            const double delta = curBeat - loopStartAtDown;
            double ns = juce::jmax(0.0, loopStartAtDown + delta);
            *loopStartBeats  = ns;
            *loopLengthBeats = juce::jmax(minLen, loopLenAtDown);
        }

        if (extendToPixelRight)
        {
            const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);
            extendToPixelRight(xe + 200);
        }
        if (auto* ed = findParentComponentOfClass<MidiEditor>())
            if (ed->onLoopChanged) ed->onLoopChanged(*loopStartBeats, *loopLengthBeats);

        repaint(); return;
    }

    // Velocity drag
    if (velDragActive)
    {
        if (velNearest < 0 || !notes) return;
        auto& n = (*notes)[(size_t)velNearest];
        const int baseY = getHeight() - 10;
        int h = juce::jlimit(0, Theme::velocityH-16, baseY - p.y);
        n.velocity = juce::jlimit(1, 127, (int)std::round((h / (double)(Theme::velocityH-16))*127.0));
        repaint(); return;
    }
}

void MidiEditor::Content::mouseUp(const juce::MouseEvent& e)
{
    if (selectionDragActive && activeTool == MidiEditor::Tool::Zoom)
    {
        const auto p1 = selStart;
        const auto p2 = getMouseXYRelative();
        juce::Rectangle<int> rect(
            juce::jmin(p1.x, p2.x), juce::jmin(p1.y, p2.y),
            std::abs(p2.x - p1.x),  std::abs(p2.y - p1.y));
        if (rect.getWidth() > 8 && rect.getHeight() > 8) { lastZoomRect = rect; zoomToRect(rect); }
    }
    marqueeActive = false;
    selectionDragActive = false;
    loopDragActive = false;
    velDragActive = false;
    creatingNote = false;
    createdIndex = -1;
    repaint();
}

bool MidiEditor::Content::keyPressed (const juce::KeyPress& key)
{
    if (!notes) return false;
    const bool ctrl = key.getModifiers().isCommandDown();
    const bool alt  = key.getModifiers().isAltDown();

    auto q     = snapQuantum();
    auto small = (q > 0.0 ? q : 0.25);

    if (key.getKeyCode() == juce::KeyPress::deleteKey)
    {
        // Delete ONLY the current selection
        if (!selection.isEmpty())
        {
            std::vector<int> idx(selection.begin(), selection.end());
            std::sort(idx.begin(), idx.end());
            for (int i=(int)idx.size()-1; i>=0; --i)
                if (idx[i] >= 0 && idx[i] < (int)notes->size())
                    notes->erase(notes->begin() + idx[i]);
            selection.clearQuick(); syncNoteComponents(); repaint(); return true;
        }
    }

    if (ctrl && (key.getTextCharacter() == 'q' || key.getTextCharacter() == 'Q'))
    {
        if (selection.isEmpty()) return true;
        const double quantum = (q > 0.0 ? q : 1.0);
        for (int i=0;i<selection.size();++i)
        {
            auto& n = (*notes)[(size_t)selection[i]];
            n.startBeats  = std::round(n.startBeats / quantum) * quantum;
            n.lengthBeats = juce::jmax(quantum, std::round(n.lengthBeats / quantum) * quantum);
        }
        syncNoteComponents(); repaint(); return true;
    }

    if (!selection.isEmpty())
    {
        if (key.getKeyCode() == juce::KeyPress::leftKey)
        {
            for (int i=0;i<selection.size();++i)
                (*notes)[(size_t)selection[i]].startBeats = juce::jmax(0.0, (*notes)[(size_t)selection[i]].startBeats - (alt ? 0.25 : small));
            syncNoteComponents(); repaint(); return true;
        }
        if (key.getKeyCode() == juce::KeyPress::rightKey)
        {
            for (int i=0;i<selection.size();++i)
                (*notes)[(size_t)selection[i]].startBeats += (alt ? 0.25 : small);
            syncNoteComponents(); repaint(); return true;
        }
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            for (int i=0;i<selection.size();++i)
                (*notes)[(size_t)selection[i]].pitch = juce::jmin(127, (*notes)[(size_t)selection[i]].pitch + 1);
            syncNoteComponents(); repaint(); return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            for (int i=0;i<selection.size();++i)
                (*notes)[(size_t)selection[i]].pitch = juce::jmax(0, (*notes)[(size_t)selection[i]].pitch - 1);
            syncNoteComponents(); repaint(); return true;
        }
    }

    return false;
}

void MidiEditor::Content::resized()
{
    syncNoteComponents();
}

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
    return Theme::snapQuantumBeats(ppb(), true);
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
void MidiEditor::Content::zoomToRect(juce::Rectangle<int> rect)
{
    if (!pixelsPerBeat) return;
    const int viewportW = juce::jmax(1, getParentViewportW() - Theme::keyWidth - 32);
    const int selW      = juce::jmax(1, rect.getWidth());
    const double targetPPB = juce::jlimit(24.0, 1024.0, (double)viewportW / (double)selW * ppb());
    *pixelsPerBeat = targetPPB;
    if (requestRepaint) requestRepaint();
    centerContentOn(rect.getCentreX(), rect.getCentreY());
}
void MidiEditor::Content::restoreZoom()
{
    if (onRequestFrameAll) onRequestFrameAll();
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

// ==== NoteComponent ====
MidiEditor::Content::NoteComponent::NoteComponent(
    int idx,
    std::function<void(int, juce::Point<int>, bool&, int&)> onDown,
    std::function<void(int, juce::Point<int>)> onDrag,
    std::function<void(int)> onUp,
    std::function<void(int)> onRightErase)
    : index(idx), onMouseDown(onDown), onMouseDragCB(onDrag), onMouseUpCB(onUp), onRightEraseCB(onRightErase)
{
    setInterceptsMouseClicks(true, false);
    setRepaintsOnMouseActivity(true);
}
void MidiEditor::Content::NoteComponent::setSelected(bool s){ selected = s; repaint(); }
void MidiEditor::Content::NoteComponent::setColors(juce::Colour body, juce::Colour rim){ bodyCol = body; rimCol = rim; repaint(); }
void MidiEditor::Content::NoteComponent::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float radius = juce::jmin(r.getHeight()*0.42f, 8.0f);

    juce::DropShadow ds(juce::Colours::black.withAlpha(0.5f), 8, {});
    ds.drawForRectangle(g, getLocalBounds());

    juce::Colour top  = bodyCol.brighter(0.18f);
    juce::Colour base = bodyCol.darker  (0.18f);
    juce::ColourGradient grad(top, r.getX(), r.getY(), base, r.getX(), r.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, radius);

    g.setColour(rimCol.withAlpha(selected ? 1.0f : 0.7f));
    g.drawRoundedRectangle(r, radius, selected ? 2.0f : 1.2f);

    g.setColour(juce::Colours::white.withAlpha(0.20f));
    auto stripe = r.reduced(2.0f).withWidth(2.0f);
    g.fillRect(stripe);
}
void MidiEditor::Content::NoteComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) { if (onRightEraseCB) onRightEraseCB(index); return; }
    bool didSelect=false; int edge=0;
    if (onMouseDown) onMouseDown(index, e.getEventRelativeTo(getParentComponent()).getPosition(), didSelect, edge);
    hitEdge = edge;
}
void MidiEditor::Content::NoteComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) { if (onRightEraseCB) onRightEraseCB(index); return; }
    if (onMouseDragCB) onMouseDragCB(index, e.getEventRelativeTo(getParentComponent()).getPosition());
}
void MidiEditor::Content::NoteComponent::mouseUp (const juce::MouseEvent& e)
{
    if (onMouseUpCB) onMouseUpCB(index);
}

// ==== Note interactions ====
void MidiEditor::Content::handleNoteMouseDown(int idx, juce::Point<int> pInContent, bool& didSelect, int& edge)
{
    didSelect = false;
    if (!selection.contains(idx)) { selection.clearQuick(); selection.add(idx); didSelect = true; }
    lastFocusedIndex = idx;
    dragAnchorContent = pInContent;
    // snapshot selection
    startAtDown.clear(); lenAtDown.clear(); pitchAtDown.clear();
    for (int i=0;i<selection.size();++i)
    {
        startAtDown.add((*notes)[(size_t)selection[i]].startBeats);
        lenAtDown  .add((*notes)[(size_t)selection[i]].lengthBeats);
        pitchAtDown.add((*notes)[(size_t)selection[i]].pitch);
    }
    edge = hitEdgeAt(idx, pInContent);
}
void MidiEditor::Content::handleNoteMouseDrag(int idx, juce::Point<int> pInContent)
{
    if (!notes) return;
    const double dxBeats = (pInContent.x - dragAnchorContent.x) / ppb();
    const int rowDelta   = (pInContent.y - dragAnchorContent.y) / rowHeight;

    const int edge = noteComps[(size_t)idx]->hitEdge;
    if (edge == 1) // left
    {
        for (int si=0; si<selection.size(); ++si)
        {
            auto& n = (*notes)[(size_t)selection[si]];
            double ns = snapToGrid(startAtDown[si] + dxBeats);
            double nl = snapToGrid(lenAtDown  [si] - dxBeats);
            n.startBeats  = juce::jmax(0.0, ns);
            n.lengthBeats = juce::jmax(0.03125, nl);
            n.pitch = juce::jlimit(topPitch-totalRows+1, topPitch, pitchAtDown[si] - rowDelta);
        }
    }
    else if (edge == 2) // right
    {
        for (int si=0; si<selection.size(); ++si)
        {
            auto& n = (*notes)[(size_t)selection[si]];
            double nl = snapToGrid(lenAtDown[si] + dxBeats);
            n.lengthBeats = juce::jmax(0.03125, nl);
            n.pitch = juce::jlimit(topPitch-totalRows+1, topPitch, pitchAtDown[si] - rowDelta);
        }
    }
    else // move
    {
        for (int si=0; si<selection.size();++si)
        {
            auto& n = (*notes)[(size_t)selection[si]];
            double b = snapToGrid(startAtDown[si] + dxBeats);
            n.startBeats = juce::jmax(0.0, b);
            n.pitch = juce::jlimit(topPitch-totalRows+1, topPitch, pitchAtDown[si] - rowDelta);
        }
    }
    syncNoteComponents();
    if (extendToPixelRight)
    {
        double maxEnd = 0.0;
        for (auto& n : *notes) maxEnd = std::max(maxEnd, n.startBeats + n.lengthBeats);
        extendToPixelRight(xFromBeats(maxEnd) + 200);
    }
    repaint();
}
void MidiEditor::Content::handleNoteMouseUp(int) { /* commit already in model */ }

void MidiEditor::Content::eraseNote(int idx)
{
    if (!notes || idx < 0 || idx >= (int)notes->size()) return;
    // erase only this one note (do NOT erase full selection)
    notes->erase(notes->begin() + idx);
    // keep selection consistent
    selection.clearQuick();
    syncNoteComponents();
    repaint();
}

int MidiEditor::Content::hitEdgeAt(int idx, juce::Point<int> p) const
{
    const auto& n = (*notes)[(size_t)idx];
    const int x   = xFromBeats(n.startBeats);
    const int w   = juce::jmax(8, (int)std::round(n.lengthBeats * ppb()));
    const int y   = Theme::rulerH + pitchToRow(n.pitch) * rowHeight + 3;
    const int h   = rowHeight - 6;
    juce::Rectangle<int> box(x,y,w,h);
    if (!box.contains(p)) return 0;
    if (juce::Rectangle<int>(x,y,3,h).contains(p)) return 1;
    if (juce::Rectangle<int>(x+w-3,y,3,h).contains(p)) return 2;
    return 0;
}
