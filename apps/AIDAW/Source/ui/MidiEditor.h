#pragma once
#include <JuceHeader.h>
#include <functional>
#include <optional>
#include <vector>
#include <algorithm>
#include "Theme.h"

/*** Model ***/
struct MidiNote
{
    int    pitch = 60;
    double startBeats = 0.0;
    double lengthBeats = 1.0;
    int    velocity = 100;
};

class MidiEditor : public juce::Component, private juce::Button::Listener
{
public:
    MidiEditor()
    {
        setOpaque(true);
        setWantsKeyboardFocus(true);

        // --- Toolbar (arranger-parity) ---
        for (auto* b : { &btnSelect, &btnDraw, &btnZoomTool, &btnFrameAll, &btnSnap, &btnZoomOut, &btnZoomIn })
        { addAndMakeVisible(b); b->addListener(this); b->setWantsKeyboardFocus(false); }
btnSelect  .setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"⬚"))); btnSelect.setTooltip ("Select / Move (1)");
btnDraw    .setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"✎"))); btnDraw.setTooltip   ("Draw (2)");
btnZoomTool.setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"🔍"))); btnZoomTool.setTooltip("Zoom tool (4): L=marquee, Mid=pan, Right=restore");
btnFrameAll.setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"◰"))); btnFrameAll.setTooltip("Frame all (F)");
btnSnap    .setButtonText(juce::String::fromUTF8(reinterpret_cast<const char*>(u8"⛓"))); btnSnap.setTooltip   ("Snap (G)");
btnZoomOut .setButtonText("-");                                                           btnZoomOut.setTooltip("Zoom out (-)");
btnZoomIn  .setButtonText("+");                                                           btnZoomIn.setTooltip ("Zoom in (+)");
   btnZoomIn.setTooltip ("Zoom in (+)");

        for (auto* b : { &btnSelect, &btnDraw, &btnZoomTool }) b->setClickingTogglesState(true);
        btnSnap.setClickingTogglesState(true); btnSnap.setToggleState(true, juce::dontSendNotification);
        setTool(Tool::Select);

        // --- Viewport + content ---
        addAndMakeVisible(view);
        view.setViewedComponent(&content, false);
        view.setScrollBarsShown(true, true);
        view.setScrollOnDragEnabled(true);

        // Bridges
        content.requestRepaint = [this]{ content.repaint(); };
        content.ensureVisible   = [this](juce::Rectangle<int> r)
        {
            auto va = view.getViewArea();
            int vx = va.getX(), vy = va.getY(), vw = va.getWidth(), vh = va.getHeight();
            int nx = vx, ny = vy;
            if (r.getX() < vx)              nx = r.getX();
            else if (r.getRight() > vx+vw)  nx = r.getRight() - vw;
            if (r.getY() < vy)              ny = r.getY();
            else if (r.getBottom() > vy+vh) ny = r.getBottom() - vh;
            nx = juce::jmax(0, nx); ny = juce::jmax(0, ny);

            // extend right "forever"
            const int padPx = 320;
            const int wantRight = r.getRight() + padPx;
            const int needContentRight = juce::jmax(wantRight, view.getViewPositionX() + view.getViewWidth() + padPx);
            if (needContentRight > content.getWidth())
            {
                const double needBeats = juce::jmax(0.0,
                    (needContentRight - Theme::keyWidth) / juce::jmax(1.0, pixelsPerBeat)) + 8.0;
                beatsExtent = juce::jmax(beatsExtent, needBeats);
                refreshContentSize();
            }
            view.setViewPosition(nx, ny);
        };
        content.getViewportWidth   = [this]{ return view.getViewArea().getWidth(); };
        content.extendToPixelRight = [this](int pxRight)
        {
            if (pxRight > content.getWidth() - 64)
            {
                const double needBeats = (pxRight - Theme::keyWidth) / juce::jmax(1.0, pixelsPerBeat) + 16.0;
                beatsExtent = juce::jmax(beatsExtent, needBeats);
                refreshContentSize();
            }
        };

        // Refs
        content.notes           = &notes;
        content.snapToGridRef   = &snapToGrid;
        content.bpmRef          = &bpmValue;
        content.pixelsPerBeat   = &pixelsPerBeat;
        content.playheadBeats   = &playheadBeats;
        content.loopEnabled     = &loopEnabled;
        content.loopStartBeats  = &loopStartBeats;
        content.loopLengthBeats = &loopLengthBeats;

        // Defaults
        setBPM(120.0);
        setSnap(true);
        setHorizontalZoom(8.0);
        setPitchView(48, 84);
        setBeatsExtent(64.0);

        refreshContentSize();
    }

    // ---------- External ----------
    void setNotes(const std::vector<MidiNote>& newNotes)
    {
        notes = newNotes;
        if (autoFitBeatsOnSet)  autoFitBeatsToNotes(8.0);
        if (autoFitPitchOnSet)  autoFitPitchToNotes();
        refreshContentSize(); content.repaint();
    }
    std::vector<MidiNote>& editNotes() { return notes; }

    void setBPM(double bpm)
    {
        bpmValue      = juce::jlimit(40.0, 300.0, bpm);
        pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;
        pixelsPerBeat = juce::jlimit(minPPB, maxPPB, pixelsPerBeat);
        zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
        refreshContentSize(); content.repaint();
    }
    void setSnap(bool on) { snapToGrid = on; btnSnap.setToggleState(on, juce::dontSendNotification); content.repaint(); }

    // Arranger-parity zoom API
    void setHorizontalZoom(double beatsPerScreen)
    {
        beatsPerScreen = juce::jlimit(4.0, 64.0, beatsPerScreen);
        const int vw = juce::jmax(1, view.getWidth());
        const double desiredPPB = (double)vw / beatsPerScreen;
        pixelsPerBeat = juce::jlimit(minPPB, maxPPB, desiredPPB);
        zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
        refreshContentSize(); content.repaint();
    }
    void zoomAtContentX(double steps, int anchorXContent)
    {
        const double oldPPB   = pixelsPerBeat;
        const double beat     = juce::jmax(0.0, (anchorXContent - Theme::keyWidth) / oldPPB);
        const int    xBefore  = Theme::keyWidth + (int)std::round(beat * oldPPB);

        zoomScale     = juce::jlimit(0.05, 30.0, zoomScale * std::pow(1.12, steps));
        pixelsPerBeat = juce::jlimit(minPPB, maxPPB, ppbAt120 * (120.0 / bpmValue) * zoomScale);

        refreshContentSize();
        const int xAfter = Theme::keyWidth + (int)std::round(beat * pixelsPerBeat);
        const int dx = xAfter - xBefore;

        const int maxX = juce::jmax(0, content.getWidth() - view.getViewWidth());
        view.setViewPosition(juce::jlimit(0, maxX, view.getViewPositionX() + dx),
                             view.getViewPositionY());
        content.repaint();
    }
    void frameAll()
    {
        double endB = 64.0;
        for (auto& n : notes) endB = std::max(endB, n.startBeats + n.lengthBeats);
        const int viewportW = juce::jmax(1, view.getViewWidth() - Theme::keyWidth - 40);
        const double target = (double)viewportW / juce::jmax(8.0, endB);
        pixelsPerBeat = juce::jlimit(minPPB, maxPPB, target);
        zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
        refreshContentSize();
        view.setViewPosition(0, view.getViewPositionY());
        content.repaint();
    }

    // pitch window
    void setPitchView(int minPitchInclusive, int maxPitchInclusive)
    {
        minPitch = juce::jlimit(0, 127, juce::jmin(minPitchInclusive, maxPitchInclusive));
        maxPitch = juce::jlimit(0, 127, juce::jmax(minPitchInclusive, maxPitchInclusive));
        content.topPitch   = maxPitch;
        content.totalRows  = (maxPitch - minPitch + 1);
        refreshContentSize(); content.repaint();
    }
    void autoFitPitchToNotes()
    {
        if (notes.empty()) return;
        int lo = 127, hi = 0;
        for (auto& n : notes){ lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch); }
        setPitchView(juce::jmax(0, lo-3), juce::jmin(127, hi+3));
    }
    void setAutoFitPitchOnSet(bool on) { autoFitPitchOnSet = on; }

    // timeline extent
    void setBeatsExtent(double beats) { beatsExtent = juce::jmax(1.0, beats); refreshContentSize(); content.repaint(); }
    void autoFitBeatsToNotes(double padBeats = 4.0)
    {
        double endB = 0.0;
        for (auto& n : notes) endB = std::max(endB, n.startBeats + n.lengthBeats);
        beatsExtent = juce::jmax(16.0, endB + padBeats);
        refreshContentSize(); content.repaint();
    }
    void setAutoFitBeatsOnSet(bool on) { autoFitBeatsOnSet = on; }

    // playhead + loop
    void setPlayheadBeats(double beats) { playheadBeats = juce::jmax(0.0, beats); content.repaint(); }
    void setLoopEnabled(bool on)                 { loopEnabled = on; content.repaint(); }
    void setLoopRegion(double s, double len)     { loopStartBeats = juce::jmax(0.0, s); loopLengthBeats = juce::jmax(0.0, len); content.repaint(); }
    std::function<void(double,double)> onLoopChanged;

    // JUCE
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(Theme::colBgMain));
        g.setColour(juce::Colour(Theme::colHeaderDiv));
        g.drawRect(getLocalBounds());
    }
    void resized() override
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
    }

private:
    enum class Tool { Select, Draw, Zoom };
    void setTool(Tool t)
    {
        tool = t;
        auto mark = [&](juce::TextButton& b, bool on){
            b.setToggleState(on, juce::dontSendNotification);
            b.setColour(juce::TextButton::buttonColourId, on ? juce::Colour(0xFF1C1F26) : juce::Colour(0xFF232B38));
            b.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.92f));
        };
        mark(btnSelect,  t==Tool::Select);
        mark(btnDraw,    t==Tool::Draw);
        mark(btnZoomTool,t==Tool::Zoom);

        // mirror to content
        content.activeTool = t;
        content.setMouseCursor(t==Tool::Zoom ? juce::MouseCursor::PointingHandCursor
                                             : juce::MouseCursor::NormalCursor);
        content.repaint();
    }

    // -------- Buttons --------
    void buttonClicked(juce::Button* b) override
    {
        if (b == &btnSelect)      setTool(Tool::Select);
        else if (b == &btnDraw)   setTool(Tool::Draw);
        else if (b == &btnZoomTool) setTool(Tool::Zoom);
        else if (b == &btnFrameAll) frameAll();
        else if (b == &btnSnap)     setSnap(btnSnap.getToggleState());
        else if (b == &btnZoomIn)   zoomAtContentX(+1.0, view.getViewPositionX() + view.getViewWidth()/2);
        else if (b == &btnZoomOut)  zoomAtContentX(-1.0, view.getViewPositionX() + view.getViewWidth()/2);
    }

    // ---------- Inner content ----------
    class Content : public juce::Component
    {
    public:
        // State refs
        std::vector<MidiNote>* notes = nullptr;
        bool*   snapToGridRef   = nullptr;
        double* bpmRef          = nullptr;
        double* pixelsPerBeat   = nullptr;
        double* playheadBeats   = nullptr;
        bool*   loopEnabled     = nullptr;
        double* loopStartBeats  = nullptr;
        double* loopLengthBeats = nullptr;

        // Bridge
        std::function<void()>                     requestRepaint;
        std::function<void(juce::Rectangle<int>)> ensureVisible;
        std::function<int()>                      getViewportWidth;
        std::function<void(int)>                  extendToPixelRight;

        // Layout
        int   totalRows = 37;
        int   rowHeight = Theme::rowHeight;
        int   topPitch  = 84;

        // Tool
        MidiEditor::Tool activeTool { MidiEditor::Tool::Select };

        // Drag/selection state
        enum class DragMode { None, Move, ResizeL, ResizeR, Draw, Velocity, Erase, LoopSet, Panning, Marquee };
        DragMode drag { DragMode::None };
        int      hitIndex = -1;
        juce::Point<int> dragStart {0,0};
        double   startBeatAtDown { 0.0 };
        double   lenBeatAtDown   { 0.0 };
        int      pitchAtDown     { 60 };
        int      velAtDown       { 100 };
        double   loopStartAtDown { 0.0 };
        juce::Rectangle<int> marquee;
        bool marqueeActive { false };

        // Group selection
        juce::Array<int> selection; // indices into *notes

        // Helpers
        double ppb() const { return *pixelsPerBeat; }
        int    gridHeight() const { return getHeight() - Theme::rulerH - Theme::velocityH; }
        double beatsFromX(int xContent) const { return Theme::beatsFromX(xContent, ppb(), Theme::keyWidth); }
        int    xFromBeats(double beats) const { return Theme::xFromBeats(beats, ppb(), Theme::keyWidth); }

        int pitchToRow(int pitch) const
        {
            const int bottomPitch = topPitch - totalRows + 1;
            const int clamped     = juce::jlimit(bottomPitch, topPitch, pitch);
            return topPitch - clamped;
        }
        int rowToPitch(int row) const
        {
            const int p = topPitch - row;
            const int bottomPitch = topPitch - totalRows + 1;
            return juce::jlimit(bottomPitch, topPitch, p);
        }

        int    currentSubDivs() const        { return Theme::subDivisions(ppb()); }
        double snapQuantum() const           { return Theme::snapQuantumBeats(ppb(), *snapToGridRef); }
        double snapToGrid(double beats) const
        {
            const double q = snapQuantum();
            return (q > 0.0) ? std::round(beats / q) * q : beats;
        }

        // ---- Painting ----
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(Theme::colBgMain));
            g.setColour(juce::Colour(Theme::colHeaderDiv));
            g.fillRect(Theme::keyWidth, 0, 1, getHeight());

            paintRuler(g);
            paintKeys(g);
            paintGrid(g);
            paintLoopOverlay(g);
            paintNotes(g);
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

        void paintRuler(juce::Graphics& g)
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
                g.setColour(juce::Colour(Theme::colLoop));
                g.fillRect(xs, 0, 2, Theme::rulerH);
                g.fillRect(xe, 0, 2, Theme::rulerH);
            }
        }

        void paintKeys(juce::Graphics& g)
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

                if ((pc == 0) && ((pitch % 24) == 0)) // reduce clutter
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

        void paintGrid(juce::Graphics& g)
        {
            const auto r = juce::Rectangle<int>(Theme::keyWidth, Theme::rulerH,
                                                getWidth() - Theme::keyWidth, gridHeight());

            // rows + octave lines
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

            // bars / beats / subs
            const double ppbVal = ppb();
            const int totalBeats = (int)std::ceil((getWidth() - Theme::keyWidth) / ppbVal);

            for (int beat = 0; beat <= totalBeats; ++beat)
            {
                const int x = Theme::keyWidth + (int)std::round(beat * ppbVal);
                const bool isBar = (beat % 4) == 0;

                if (isBar) { g.setColour(juce::Colour(Theme::colGridBar));  g.fillRect(x, Theme::rulerH, 2, getHeight() - Theme::rulerH); }
                else       { g.setColour(juce::Colour(Theme::colGridBeat)); g.fillRect(x, Theme::rulerH, 1, getHeight() - Theme::rulerH); }
            }

            const int subDivs = Theme::subDivisions(ppbVal);
            if (subDivs > 0)
            {
                g.setColour(juce::Colour(Theme::colGridSub));
                for (int beat = 0; beat <= totalBeats; ++beat)
                    for (int s = 1; s < subDivs; ++s)
                    {
                        const double frac = (double) s / (double) subDivs;
                        const int x = Theme::keyWidth + (int) std::round((beat + frac) * ppbVal);
                        g.fillRect(x, Theme::rulerH, 1, getHeight() - Theme::rulerH);
                    }
            }
        }

        void paintLoopOverlay(juce::Graphics& g)
        {
            if (!(loopEnabled && *loopEnabled && *loopLengthBeats > 0.0)) return;
            const int xs = xFromBeats(*loopStartBeats);
            const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);
            juce::Rectangle<int> loopRect(xs, Theme::rulerH, xe - xs, gridHeight());
            g.setColour(juce::Colour(Theme::colLoopFill));
            g.fillRect(loopRect);
        }

        void paintNotes(juce::Graphics& g)
        {
            if (!notes) return;
            const int h = rowHeight - 6;

            for (size_t i=0;i<notes->size();++i)
            {
                const auto& n = (*notes)[i];
                const int row = pitchToRow(n.pitch);
                const int x   = xFromBeats(n.startBeats);
                const int y   = Theme::rulerH + row * rowHeight + 3;
                const int w   = juce::jmax(8, (int)std::round(n.lengthBeats * ppb()));

                const bool selected = selection.contains((int)i);
                const auto body   = juce::Colour((i % 2 == 0) ? Theme::colNoteA : Theme::colNoteB)
                                      .withMultipliedBrightness(selected ? 1.08f : 1.0f);
                const auto border = juce::Colour(body).brighter(Theme::noteBorderGain);

                juce::Rectangle<float> rf((float)x, (float)y, (float)w, (float)h);
                g.setColour(body.darker(0.30f)); g.fillRoundedRectangle(rf, 7.0f);
                g.setColour(border);              g.drawRoundedRectangle(rf, 7.0f, 1.2f);

                g.setColour(juce::Colour(0x4CFFFFFF));
                g.fillRect(x, y+1, 2, h-2);
                g.fillRect(x + w - 2, y+1, 2, h-2);
            }
        }

        void paintVelocities(juce::Graphics& g)
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
                g.setColour(juce::Colour(Theme::colPlayhead));
                g.fillRect(x-1, baseY - h, 2, h);
            }
        }

        void paintPlayhead(juce::Graphics& g)
        {
            if (!playheadBeats) return;
            const int x = xFromBeats(*playheadBeats);
            g.setColour(juce::Colour(Theme::colPlayhead));
            g.fillRect(x-1, 0, 2, getHeight());
        }

        // Hit testing
        enum class HitEdge { None, Left, Right, Body };
        int noteAtPoint(juce::Point<int> p, HitEdge& edgeOut) const
        {
            edgeOut = HitEdge::None;
            if (!notes) return -1;

            for (int i=(int)notes->size()-1; i>=0; --i)
            {
                const auto& n = (*notes)[(size_t)i];
                const int row = pitchToRow(n.pitch);
                const int x   = xFromBeats(n.startBeats);
                const int w   = juce::jmax(8, (int)std::round(n.lengthBeats * ppb()));
                const int y   = Theme::rulerH + row * rowHeight + 3;
                const int h   = rowHeight - 6;
                juce::Rectangle<int> box(x, y, w, h);

                if (box.contains(p))
                {
                    auto left  = juce::Rectangle<int>(x, y, 3, h);
                    auto right = juce::Rectangle<int>(x+w-3, y, 3, h);
                    edgeOut = left.contains(p) ? HitEdge::Left
                             : right.contains(p) ? HitEdge::Right
                             : HitEdge::Body;
                    return i;
                }
            }
            return -1;
        }

        // ---------- Interaction ----------
        void mouseDown(const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            if (!notes) return;

            const auto p = e.getPosition();
            dragStart = p;

            if (e.mods.isMiddleButtonDown()) { drag = DragMode::Panning; return; }

            // SHIFT on ruler = set loop region
            if (p.y < Theme::rulerH && e.mods.isShiftDown())
            {
                drag = DragMode::LoopSet;
                loopStartAtDown = beatsFromX(p.x);
                loopStartAtDown = snapToGrid(loopStartAtDown);
                if (loopEnabled) *loopEnabled = true;
                if (loopStartBeats) *loopStartBeats = loopStartAtDown;
                if (loopLengthBeats) *loopLengthBeats = 0.0;
                repaintMe(); return;
            }

            // Velocity lane?
            if (p.y >= getHeight()-Theme::velocityH)
            {
                drag = DragMode::Velocity;
                hitIndex = closestNoteToX(p.x);
                if (hitIndex >= 0) velAtDown = (*notes)[(size_t)hitIndex].velocity;
                return;
            }

            // Right mouse = erase scrub
            if (e.mods.isRightButtonDown())
            {
                drag = DragMode::Erase;
                HitEdge where; hitIndex = noteAtPoint(p, where);
                if (hitIndex >= 0){ notes->erase(notes->begin() + hitIndex); hitIndex = -1; repaintMe(); }
                return;
            }

            // Zoom tool: start marquee zoom
            if (activeTool == MidiEditor::Tool::Zoom)
            {
                drag = DragMode::Marquee;
                marqueeActive = true;
                marquee = juce::Rectangle<int>(p.x, p.y, 1, 1);
                repaintMe(); return;
            }

            // Hit existing note?
            HitEdge where; hitIndex = noteAtPoint(p, where);

            if (hitIndex >= 0)
            {
                // selection logic: if note not in selection or no ctrl, replace; ctrl toggles
                if (!e.mods.isCommandDown())
                {
                    if (!selection.contains(hitIndex))
                    { selection.clearQuick(); selection.add(hitIndex); }
                }
                else
                {
                    if (selection.contains(hitIndex)) selection.removeAllInstancesOf(hitIndex);
                    else selection.add(hitIndex);
                }

                const auto& n = (*notes)[(size_t)hitIndex];
                startBeatAtDown = n.startBeats;
                lenBeatAtDown   = n.lengthBeats;
                pitchAtDown     = n.pitch;
                drag = (where == HitEdge::Left)  ? DragMode::ResizeL
                     : (where == HitEdge::Right) ? DragMode::ResizeR
                                                 : DragMode::Move;
            }
            else
            {
                if (activeTool == MidiEditor::Tool::Draw)
                {
                    // Draw a new note
                    const int row = juce::jlimit(0, totalRows-1, (p.y - Theme::rulerH) / rowHeight);
                    const int pitch = rowToPitch(row);
                    double beat = snapToGrid(beatsFromX(p.x));

                    MidiNote n; n.pitch = pitch; n.startBeats = beat; n.lengthBeats = 1.0; n.velocity = 100;
                    notes->push_back(n);
                    hitIndex = (int)notes->size() - 1;
                    selection.clearQuick(); selection.add(hitIndex);

                    startBeatAtDown = beat; lenBeatAtDown = 1.0; pitchAtDown = pitch;
                    if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(beat), Theme::rulerH + row*rowHeight, 40, rowHeight));
                    repaintMe();
                    drag = DragMode::Draw; return;
                }

                // Select tool in empty space → marquee select
                if (activeTool == MidiEditor::Tool::Select)
                {
                    drag = DragMode::Marquee;
                    marqueeActive = true;
                    marquee = juce::Rectangle<int>(p.x, p.y, 1, 1);
                    repaintMe(); return;
                }
            }
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!notes) return;
            const auto p = e.getPosition();

            if (drag == DragMode::Panning)
            {
                if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                {
                    auto pos = vp->getViewPosition();
                    vp->setViewPosition(pos - (e.getOffsetFromDragStart()));
                }
                return;
            }

            if (drag == DragMode::Marquee)
            {
                const int x0 = juce::jlimit(0, getWidth(), dragStart.x);
                const int y0 = juce::jlimit(0, getHeight(), dragStart.y);
                const int x1 = juce::jlimit(0, getWidth(), p.x);
                const int y1 = juce::jlimit(0, getHeight(), p.y);
                marquee = juce::Rectangle<int>::leftTopRightBottom(juce::jmin(x0,x1), juce::jmin(y0,y1),
                                                                    juce::jmax(x0,x1), juce::jmax(y0,y1));
                // update selection live (no Ctrl adds, Ctrl toggles)
                juce::Array<int> newSel;
                for (int i=0;i<(int)notes->size();++i)
                {
                    auto& n = (*notes)[(size_t)i];
                    const int row = pitchToRow(n.pitch);
                    const int x   = xFromBeats(n.startBeats);
                    const int y   = Theme::rulerH + row * rowHeight + 3;
                    const int w   = juce::jmax(8, (int)std::round(n.lengthBeats * ppb()));
                    const int h   = rowHeight - 6;
                    if (marquee.intersects(juce::Rectangle<int>(x,y,w,h)))
                        newSel.add(i);
                }
                selection = newSel;
                repaintMe(); return;
            }

            if (drag == DragMode::LoopSet && loopLengthBeats && loopStartBeats)
            {
                double b0 = loopStartAtDown;
                double b1 = snapToGrid(beatsFromX(p.x));
                if (b1 < b0) std::swap(b0, b1);
                *loopStartBeats  = b0;
                *loopLengthBeats = juce::jmax(0.0, b1 - b0);
                if (extendToPixelRight) extendToPixelRight(xFromBeats(b1) + 64);
                repaintMe(); return;
            }

            if (drag == DragMode::Erase)
            {
                HitEdge where; int idx = noteAtPoint(p, where);
                if (idx >= 0 && idx < (int)notes->size())
                { notes->erase(notes->begin() + idx); repaintMe(); }
                return;
            }

            if (drag == DragMode::Velocity)
            {
                if (hitIndex < 0) return;
                auto& n = (*notes)[(size_t)hitIndex];
                const int baseY = getHeight() - 10;
                int h = juce::jlimit(0, Theme::velocityH-16, baseY - p.y);
                n.velocity = juce::jlimit(1, 127, (int)std::round((h / (double)(Theme::velocityH-16))*127.0));
                repaintMe(); return;
            }

            if (selection.isEmpty()) return;

            const double dxBeats = (p.x - dragStart.x) / ppb();
            const int rowDelta   = (p.y - dragStart.y) / rowHeight;

            if (drag == DragMode::Move)
            {
                for (int i=0;i<selection.size();++i)
                {
                    auto& n = (*notes)[(size_t)selection[i]];
                    double b = startBeatAtDownFor(selection[i]) + dxBeats;
                    b = snapToGrid(b);
                    n.startBeats = juce::jmax(0.0, b);
                    n.pitch      = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDownFor(selection[i]) - rowDelta);
                }
                repaintMe(); return;
            }
            else if (drag == DragMode::ResizeL)
            {
                for (int i=0;i<selection.size();++i)
                {
                    auto& n = (*notes)[(size_t)selection[i]];
                    double ns = startBeatAtDownFor(selection[i]) + dxBeats;
                    double nl = lenBeatAtDownFor(selection[i])   - dxBeats;
                    ns = snapToGrid(ns);
                    nl = juce::jmax(0.03125, snapToGrid(nl));
                    n.lengthBeats = nl;
                    n.startBeats  = juce::jmax(0.0, ns);
                    n.pitch       = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDownFor(selection[i]) - rowDelta);
                }
                repaintMe(); return;
            }
            else if (drag == DragMode::ResizeR)
            {
                for (int i=0;i<selection.size();++i)
                {
                    auto& n = (*notes)[(size_t)selection[i]];
                    double nl = lenBeatAtDownFor(selection[i]) + dxBeats;
                    n.lengthBeats = juce::jmax(0.03125, snapToGrid(nl));
                    n.pitch       = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDownFor(selection[i]) - rowDelta);
                }
                repaintMe(); return;
            }
            else if (drag == DragMode::Draw && hitIndex >= 0)
            {
                auto& n = (*notes)[(size_t)hitIndex];
                double len = juce::jmax(0.03125, (p.x - dragStart.x) / ppb());
                len = snapToGrid(len);
                n.lengthBeats = len;
                if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(n.startBeats + len), Theme::rulerH, 8, rowHeight));
                repaintMe(); return;
            }
        }

        void mouseUp(const juce::MouseEvent&)
        {
            if (drag == DragMode::LoopSet && loopChangedCallback && loopEnabled && *loopEnabled && loopLengthBeats)
                loopChangedCallback(*loopStartBeats, *loopLengthBeats);

            marqueeActive = false;
            drag = DragMode::None; hitIndex = -1;
            repaintMe();
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (!notes) return false;

            // delete selected notes
            if (key.getKeyCode() == juce::KeyPress::deleteKey)
            {
                if (!selection.isEmpty())
                {
                    std::vector<int> idx(selection.begin(), selection.end());
                    std::sort(idx.begin(), idx.end()); std::reverse(idx.begin(), idx.end());
                    for (int i : idx) if (i >= 0 && i < (int)notes->size())
                        notes->erase(notes->begin() + i);
                    selection.clearQuick();
                    repaintMe(); return true;
                }
            }
            return false;
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
        {
            // Shift = vertical pitch scroll, Ctrl = parent zoom, else horizontal scroll
            if (e.mods.isShiftDown()) { scrollPitch(wheel.deltaY > 0 ? +2 : -2); return; }
            if (e.mods.isCtrlDown())  { if (auto* p = getParentComponent()) p->mouseWheelMove(e, wheel); return; }

            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            {
                auto p = vp->getViewPosition();
                const int dx = (int)std::round(-wheel.deltaY * 160.0);
                const int maxX = juce::jmax(0, getWidth() - vp->getViewWidth());
                int nx = juce::jlimit(0, maxX, p.getX() + dx);
                vp->setViewPosition(nx, p.getY());
                if (extendToPixelRight) extendToPixelRight(nx + vp->getViewWidth());
            }
        }
        void scrollPitch(int rows)
        {
            topPitch = juce::jlimit(0, 127, topPitch + rows);
            repaintMe();
        }

        int closestNoteToX(int x) const
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

        // Per-note snapshots for group ops
        double startBeatAtDownFor(int idx) const { return (*notes)[(size_t)idx].startBeats; }
        double lenBeatAtDownFor  (int idx) const { return (*notes)[(size_t)idx].lengthBeats; }
        int    pitchAtDownFor    (int idx) const { return (*notes)[(size_t)idx].pitch; }

        void repaintMe(){ if (requestRepaint) requestRepaint(); }
        std::function<void(double,double)> loopChangedCallback;
    } content;

public:
    const std::vector<MidiNote>& getNotes() const { return notes; }

    // Public zoom from mouse wheel (arranger parity)
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCtrlDown())
        {
            const int xContent = e.getEventRelativeTo(&content).x;
            zoomAtContentX(wheel.deltaY, view.getViewPositionX() + xContent);
            return;
        }
        Component::mouseWheelMove(e, wheel);
    }

// Zoom anchored to a screen X (called from Main.cpp)
void zoomDeltaFromWheel(double wheelDelta, int screenX)
{
    // screen -> editor local
    const int localX = screenX - getScreenX();

    // clamp to content width
    const int clampedLocalX = juce::jlimit(0, content.getWidth(), localX);

    // editor local -> content X
    const int anchorXContent = view.getViewPositionX() + clampedLocalX;

    zoomAtContentX(wheelDelta, anchorXContent);
}


private:
    // View + data
    juce::Viewport view;
    std::vector<MidiNote> notes;

    // toolbar
    juce::TextButton btnSelect, btnDraw, btnZoomTool, btnFrameAll, btnSnap, btnZoomOut, btnZoomIn;
    Tool tool { Tool::Select };

    // grid/time
    double bpmValue { 120.0 };
    bool   snapToGrid { true };

    // zoom mapping
    double ppbAt120 { 64.0 };
    double pixelsPerBeat { 64.0 };
    double zoomScale { 1.0 };
    static constexpr double minPPB = 24.0;
    static constexpr double maxPPB = 1024.0;

    // pitch window
    int minPitch { 48 };
    int maxPitch { 84 };

    // content extent floor
    double beatsExtent { 64.0 };

    // auto-fit flags
    bool autoFitPitchOnSet { true };
    bool autoFitBeatsOnSet { true };

    // playhead & loop
    double playheadBeats { 0.0 };
    bool   loopEnabled { false };
    double loopStartBeats { 0.0 };
    double loopLengthBeats { 0.0 };

    void refreshContentSize()
    {
        double maxEnd = beatsExtent;
        for (auto& n : notes) maxEnd = std::max(maxEnd, n.startBeats + n.lengthBeats + 4.0);

        const int widthFromBeats = Theme::keyWidth + (int)std::ceil(maxEnd * pixelsPerBeat);
        const int minOpenWidth   = juce::jmax(1600, view.getWidth() + 900);
        const int width = juce::jmax(widthFromBeats, minOpenWidth);

        content.rowHeight = Theme::rowHeight;
        content.totalRows = (maxPitch - minPitch + 1);
        content.topPitch  = maxPitch;
        const int height  = Theme::rulerH + content.totalRows * content.rowHeight + Theme::velocityH;

        content.setSize(width, height);

        content.loopChangedCallback = [this](double s, double l)
        {
            if (onLoopChanged) onLoopChanged(s, l);
        };
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditor)
};
