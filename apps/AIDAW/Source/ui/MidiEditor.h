#pragma once
#include <JuceHeader.h>
#include <functional>
#include <optional>
#include <vector>
#include <algorithm>

/*** Model ***/
struct MidiNote
{
    int    pitch = 60;        // MIDI note number (C4=60)
    double startBeats = 0.0;  // start on timeline
    double lengthBeats = 1.0; // duration
    int    velocity = 100;    // 1..127
};

/*** Piano-roll editor (host provides data; no seeding) ***/
class MidiEditor : public juce::Component
{
public:
    MidiEditor()
    {
        setOpaque(true);
        setWantsKeyboardFocus(true);

        addAndMakeVisible(view);
        view.setViewedComponent(&content, false);
        view.setScrollBarsShown(true, true);
        view.setScrollOnDragEnabled(true);

        // Content <-> parent glue
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

            // Auto-extend to enable infinite right scroll
            const int padPx = 320;
            const int wantRight = r.getRight() + padPx;
            const int needContentRight = juce::jmax(wantRight, view.getViewPositionX() + view.getViewWidth() + padPx);
            if (needContentRight > content.getWidth())
            {
                const double needBeats = juce::jmax(0.0,
                    (needContentRight - Content::keyWidth) / juce::jmax(1.0, pixelsPerBeat)) + 8.0;
                beatsExtent = juce::jmax(beatsExtent, needBeats);
                refreshContentSize();
            }
            view.setViewPosition(nx, ny);
        };
        content.getViewportWidth = [this]{ return view.getViewArea().getWidth(); };
        content.extendToPixelRight = [this](int pxRight)
        {
            if (pxRight > content.getWidth() - 64)
            {
                const double needBeats = (pxRight - Content::keyWidth) / juce::jmax(1.0, pixelsPerBeat) + 16.0;
                beatsExtent = juce::jmax(beatsExtent, needBeats);
                refreshContentSize();
            }
        };

        // State refs
        content.notes         = &notes;
        content.snapToGridRef = &snapToGrid;
        content.bpmRef        = &bpmValue;
        content.pixelsPerBeat = &pixelsPerBeat;
        content.playheadBeats = &playheadBeats;
        content.loopEnabled   = &loopEnabled;
        content.loopStartBeats= &loopStartBeats;
        content.loopLengthBeats=&loopLengthBeats;

        // Defaults (host can override)
        setBPM(120.0);
        setSnap(true);
        setHorizontalZoom(16.0);         // beats per viewport by default
        setPitchView(48, 84);            // C3..C6 initial window
        setBeatsExtent(16.0);            // 4 bars

        refreshContentSize();
    }

    // -------- External control --------
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
    void setSnap(bool on) { snapToGrid = on; content.repaint(); }

    // Horizontal zoom (beats that fit on screen)
    void setHorizontalZoom(double beatsPerScreen)
    {
        beatsPerScreen = juce::jlimit(4.0, 64.0, beatsPerScreen);
        const int vw = juce::jmax(1, view.getWidth());
        const double desiredPPB = (double)vw / beatsPerScreen;
        pixelsPerBeat = juce::jlimit(minPPB, maxPPB, desiredPPB);
        zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
        refreshContentSize(); content.repaint();
    }

    // Zoom at mouse position (Arranger parity) — uses screenX anchor (no ambiguous getLocalPoint)
    void zoomDeltaFromWheel(double deltaY, int screenX)
    {
        const double oldPPB = pixelsPerBeat;
        const double steps = deltaY > 0 ? +1.0 : -1.0;
        pixelsPerBeat = juce::jlimit(minPPB, maxPPB, pixelsPerBeat * std::pow(1.12, steps));
        zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));

        // mouse X within the viewport
        int mouseX = screenX - view.getScreenX();
        mouseX = juce::jlimit(0, view.getWidth(), mouseX);

        const int viewX     = view.getViewPositionX();
        const int contentX  = viewX + mouseX;
        const double beatAtMouse = juce::jmax(0.0, (contentX - Content::keyWidth) / oldPPB);

        refreshContentSize();
        const int newContentX = Content::keyWidth + (int)std::round(beatAtMouse * pixelsPerBeat);
        view.setViewPosition(juce::jmax(0, newContentX - mouseX), view.getViewPositionY());
        content.repaint();
    }
    // Back-compat call site
    void zoomDeltaFromWheel(double deltaY) { zoomDeltaFromWheel(deltaY, view.getScreenX()); }

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

    void setBeatsExtent(double beats)
    {
        beatsExtent = juce::jmax(1.0, beats);
        refreshContentSize(); content.repaint();
    }
    void autoFitBeatsToNotes(double padBeats = 4.0)
    {
        double endB = 0.0;
        for (auto& n : notes) endB = std::max(endB, n.startBeats + n.lengthBeats);
        beatsExtent = juce::jmax(4.0, endB + padBeats);
        refreshContentSize(); content.repaint();
    }
    void setAutoFitBeatsOnSet(bool on) { autoFitBeatsOnSet = on; }

    // Playhead + loop
    void setPlayheadBeats(double beats) { playheadBeats = juce::jmax(0.0, beats); content.repaint(); }
    void setLoopEnabled(bool on)                 { loopEnabled = on; content.repaint(); }
    void setLoopRegion(double s, double len)     { loopStartBeats = juce::jmax(0.0, s); loopLengthBeats = juce::jmax(0.0, len); content.repaint(); }
    std::function<void(double startBeats, double lengthBeats)> onLoopChanged;

    // JUCE
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colour(0x22FFFFFF));
        g.drawRect(getLocalBounds());
    }
    void resized() override
    {
        view.setBounds(getLocalBounds());
        refreshContentSize();
    }

    const std::vector<MidiNote>& getNotes() const { return notes; }

private:
    // ---------- Inner content ----------
    class Content : public juce::Component
    {
    public:
        // state refs
        std::vector<MidiNote>* notes = nullptr;
        bool*   snapToGridRef   = nullptr;
        double* bpmRef          = nullptr;
        double* pixelsPerBeat   = nullptr;
        double* playheadBeats   = nullptr;

        // loop refs
        bool*   loopEnabled       = nullptr;
        double* loopStartBeats    = nullptr;
        double* loopLengthBeats   = nullptr;

        // bridge to parent
        std::function<void()>                     requestRepaint;
        std::function<void(juce::Rectangle<int>)> ensureVisible;
        std::function<int()>                      getViewportWidth;
        std::function<void(int)>                  extendToPixelRight;

        // constants (keyboard column replaces Arranger header)
        static constexpr int keyWidth = 72;
        static constexpr int rulerH   = 24;
        static constexpr int velLaneH = 64;

        // layout
        int   totalRows = 37;
        int   rowHeight = 22;
        int   topPitch  = 84;

        // state
        enum class DragMode { None, Move, ResizeL, ResizeR, Draw, Velocity, Erase, LoopSet, Panning };
        DragMode drag { DragMode::None };
        int      hitIndex = -1;
        juce::Point<int> dragStart {0,0};
        double   startBeatAtDown { 0.0 };
        double   lenBeatAtDown   { 0.0 };
        int      pitchAtDown     { 60 };
        int      velAtDown       { 100 };
        double   loopStartAtDown { 0.0 };

        // helpers
        double ppb() const { return *pixelsPerBeat; }
        int    gridHeight() const { return getHeight() - rulerH - velLaneH; }
        double beatsFromX(int xContent) const { return juce::jmax(0, xContent - keyWidth) / ppb(); }
        int    xFromBeats(double beats) const { return keyWidth + (int)std::round(beats * ppb()); }

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

        // subdivision like ArrangerCanvas (thresholds)
        // also used for snap granularity
        int currentSubDivs() const
        {
            const double ppb = *pixelsPerBeat;
            if      (ppb >= 320.0) return 16;
            else if (ppb >= 200.0) return 8;
            else if (ppb >= 120.0) return 4;
            else if (ppb >= 80.0)  return 2;
            return 0;
        }
        double currentSubdivisionBeats() const
        {
            const int s = currentSubDivs();
            return s > 0 ? (1.0 / (double)s) : 1.0; // if no minor lines drawn, snap to beat
        }

        // ---- Drawing (mirrors ArrangerCanvas style) ----
        void paint(juce::Graphics& g) override
        {
            // background & divider to match app
            g.fillAll(juce::Colour(0xFF0E1218));
            g.setColour(juce::Colour(0x22FFFFFF));
            g.fillRect(keyWidth, 0, 1, getHeight());

            paintRuler(g);
            paintKeys(g);
            paintGrid(g);
            paintLoopOverlay(g);
            paintNotes(g);
            paintVelocities(g);
            paintPlayhead(g);
        }

        void paintRuler(juce::Graphics& g)
        {
            auto r = juce::Rectangle<int>(0, 0, getWidth(), rulerH);
            g.setColour(juce::Colour(0xFF0D1016)); g.fillRect(r);
            g.setColour(juce::Colour(0x22FFFFFF)); g.fillRect(r.withY(r.getBottom()-1).withHeight(1));

            const int totalBeats = (int)std::ceil((double)(getWidth()-keyWidth)/ppb());
            for (int beat = 0; beat <= totalBeats; ++beat)
            {
                const int x = xFromBeats((double)beat);
                const bool isBar = (beat % 4) == 0;

                if (isBar)
                {
                    g.setColour(juce::Colour(0x66FFFFFF));
                    g.setFont(11.0f);
                    g.drawFittedText(juce::String((beat / 4) + 1), x + 4, 2, 30, rulerH-4,
                                     juce::Justification::centredLeft, 1);
                }
            }

            // Loop handles on ruler
            if (loopEnabled && *loopEnabled && *loopLengthBeats > 0.0)
            {
                const int xs = xFromBeats(*loopStartBeats);
                const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);
                g.setColour(juce::Colour(0xFF1EB980));
                g.fillRect(xs, 0, 2, rulerH);
                g.fillRect(xe, 0, 2, rulerH);
            }
        }

        void paintKeys(juce::Graphics& g)
        {
            auto r = juce::Rectangle<int>(0, rulerH, keyWidth, gridHeight());
            int y = r.getY();
            for (int row = 0; row < totalRows; ++row)
            {
                const int pitch = rowToPitch(row);
                const int pc = pitch % 12;
                const bool black = (pc==1||pc==3||pc==6||pc==8||pc==10);

                juce::Rectangle<int> key(0, y, keyWidth, rowHeight);
                g.setColour(black ? juce::Colour(0xFF111419) : juce::Colour(0xFF161A21));
                g.fillRect(key);

                if (pc == 0)  // C labels
                {
                    g.setColour(juce::Colour(0x66FFFFFF));
                    g.setFont(11.0f);
                    const int octave = (pitch / 12) - 1;
                    g.drawText("C" + juce::String(octave), key.reduced(8, 3), juce::Justification::centredLeft);
                }

                g.setColour(juce::Colour(0x222B3340));
                g.fillRect(key.removeFromBottom(1));
                y += rowHeight;
            }
        }

        void paintGrid(juce::Graphics& g)
        {
            auto r = juce::Rectangle<int>(keyWidth, rulerH, getWidth()-keyWidth, gridHeight());

            // zebra octaves (subtle)
            int y = r.getY();
            for (int row = 0; row < totalRows; ++row)
            {
                const bool zebra = ((row / 12) % 2) == 0;
                g.setColour(zebra ? juce::Colour(0xFF0F131A) : juce::Colour(0xFF10151D));
                g.fillRect(juce::Rectangle<int>(r.getX(), y, r.getWidth(), rowHeight));
                y += rowHeight;
            }

            // Bars & beats (ArrangerCanvas style)
            const double ppb = *pixelsPerBeat;
            const int totalBeats = (int)std::ceil((getWidth() - keyWidth) / ppb);
            for (int beat = 0; beat <= totalBeats; ++beat)
            {
                const int x = keyWidth + (int)std::round(beat * ppb);
                const bool isBar = (beat % 4) == 0;

                g.setColour(isBar ? juce::Colour(0x44FFFFFF) : juce::Colour(0x18FFFFFF));
                g.fillRect(x, rulerH, 1, getHeight() - rulerH);
            }

            // Subdivisions (same thresholds as ArrangerCanvas)
            const int subDivs = currentSubDivs();
            if (subDivs > 0)
            {
                g.setColour(juce::Colour(0x11FFFFFF));
                for (int beat = 0; beat <= totalBeats; ++beat)
                    for (int s = 1; s < subDivs; ++s)
                    {
                        const double frac = (double)s / (double)subDivs;
                        const int x = keyWidth + (int)std::round((beat + frac) * ppb);
                        g.fillRect(x, rulerH, 1, getHeight() - rulerH);
                    }
            }
        }

        void paintLoopOverlay(juce::Graphics& g)
        {
            if (!(loopEnabled && *loopEnabled && *loopLengthBeats > 0.0)) return;
            const int xs = xFromBeats(*loopStartBeats);
            const int xe = xFromBeats(*loopStartBeats + *loopLengthBeats);
            juce::Rectangle<int> loopRect(xs, rulerH, xe - xs, gridHeight());
            g.setColour(juce::Colour(0x1532E6A0));
            g.fillRect(loopRect);
        }

        void paintNotes(juce::Graphics& g)
        {
            if (!notes) return;
            const int h = rowHeight - 5;

            for (size_t i=0;i<notes->size();++i)
            {
                const auto& n = (*notes)[i];
                const int row = pitchToRow(n.pitch);
                const int x   = xFromBeats(n.startBeats);
                const int y   = rulerH + row * rowHeight + 3;
                const int w   = juce::jmax(6, (int)std::round(n.lengthBeats * ppb()));

                const auto body   = (i % 2 == 0) ? juce::Colour(0xFF2C72FA) : juce::Colour(0xFF7A3BFF);
                const auto border = body.brighter(0.35f);

                juce::Rectangle<float> rf((float)x, (float)y, (float)w, (float)h);
                g.setColour(body.darker(0.35f)); g.fillRoundedRectangle(rf, 6.0f);
                g.setColour(border);              g.drawRoundedRectangle(rf, 6.0f, 1.2f);

                g.setColour(juce::Colour(0x88FFFFFF));
                g.fillRect(x, y, 3, h);
                g.fillRect(x + w - 3, y, 3, h);
            }
        }

        void paintVelocities(juce::Graphics& g)
        {
            auto r = juce::Rectangle<int>(0, getHeight()-velLaneH, getWidth(), velLaneH);
            g.setColour(juce::Colour(0xFF0C1016)); g.fillRect(r);
            g.setColour(juce::Colour(0x223B82F6)); g.fillRect(r.removeFromTop(1));

            if (!notes) return;
            const int baseY = getHeight() - 10;
            for (const auto& n : *notes)
            {
                const int x = xFromBeats(n.startBeats + n.lengthBeats * 0.5);
                const int h = juce::jlimit(2, velLaneH-16, (int)std::round((n.velocity/127.0) * (velLaneH-16)));
                g.setColour(juce::Colour(0xFF3B82F6));
                g.fillRect(x-1, baseY - h, 2, h);
            }
        }

        void paintPlayhead(juce::Graphics& g)
        {
            if (!playheadBeats) return;
            const int x = xFromBeats(*playheadBeats);
            g.setColour(juce::Colour(0xFF3B82F6));
            g.fillRect(x-1, 0, 2, getHeight());
        }

        // ---- Hit testing ----
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
                const int w   = juce::jmax(6, (int)std::round(n.lengthBeats * ppb()));
                const int y   = rulerH + row * rowHeight + 3;
                const int h   = rowHeight - 5;
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

        // ---- Interaction ----
        void mouseDown(const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            if (!notes) return;

            const auto p = e.getPosition();
            dragStart = p;

            // Middle mouse -> panning
            if (e.mods.isMiddleButtonDown())
            {
                drag = DragMode::Panning;
                return;
            }

            // SHIFT on ruler = set loop region
            if (p.y < rulerH && e.mods.isShiftDown())
            {
                drag = DragMode::LoopSet;
                loopStartAtDown = beatsFromX(p.x);
                if (*snapToGridRef) loopStartAtDown = snapToSubdivision(loopStartAtDown);
                if (loopEnabled) *loopEnabled = true;
                if (loopStartBeats) *loopStartBeats = loopStartAtDown;
                if (loopLengthBeats) *loopLengthBeats = 0.0;
                repaintMe();
                return;
            }

            // velocity lane?
            if (p.y >= getHeight()-velLaneH)
            {
                drag = DragMode::Velocity;
                hitIndex = closestNoteToX(p.x);
                if (hitIndex >= 0) velAtDown = (*notes)[(size_t)hitIndex].velocity;
                return;
            }

            // Right-click erase mode
            if (e.mods.isRightButtonDown())
            {
                drag = DragMode::Erase;
                HitEdge where; hitIndex = noteAtPoint(p, where);
                if (hitIndex >= 0)
                {
                    notes->erase(notes->begin() + hitIndex);
                    hitIndex = -1;
                    repaintMe();
                }
                return;
            }

            HitEdge where; hitIndex = noteAtPoint(p, where);

            if (hitIndex >= 0)
            {
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
                // Draw a fresh note at snapped position
                drag = DragMode::Draw;
                const int row = juce::jlimit(0, totalRows-1, (p.y - rulerH) / rowHeight);
                const int pitch = rowToPitch(row);
                double beat = beatsFromX(p.x);
                if (*snapToGridRef) beat = snapToSubdivision(beat);

                MidiNote n; n.pitch = pitch; n.startBeats = beat; n.lengthBeats = 1.0; n.velocity = 100;
                notes->push_back(n);
                hitIndex = (int)notes->size() - 1;
                startBeatAtDown = beat; lenBeatAtDown = 1.0; pitchAtDown = pitch;

                if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(beat), rulerH + row*rowHeight, 40, rowHeight));
                repaintMe();
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

            if (drag == DragMode::LoopSet && loopLengthBeats && loopStartBeats)
            {
                double b0 = loopStartAtDown;
                double b1 = beatsFromX(p.x);
                if (*snapToGridRef) b1 = snapToSubdivision(b1);
                if (b1 < b0) std::swap(b0, b1);
                *loopStartBeats  = b0;
                *loopLengthBeats = juce::jmax(0.0, b1 - b0);
                if (extendToPixelRight) extendToPixelRight(xFromBeats(b1) + 64);
                repaintMe();
                return;
            }

            if (drag == DragMode::Erase)
            {
                HitEdge where; int idx = noteAtPoint(p, where);
                if (idx >= 0 && idx < (int)notes->size())
                {
                    notes->erase(notes->begin() + idx);
                    repaintMe();
                }
                return;
            }

            if (drag == DragMode::Velocity)
            {
                if (hitIndex < 0) return;
                auto& n = (*notes)[(size_t)hitIndex];
                const int baseY = getHeight() - 10;
                int h = juce::jlimit(0, velLaneH-16, baseY - p.y);
                n.velocity = juce::jlimit(1, 127, (int)std::round((h / (double)(velLaneH-16))*127.0));
                repaintMe(); return;
            }

            if (hitIndex < 0) return;
            auto& n = (*notes)[(size_t)hitIndex];
            const double dxBeats = (p.x - dragStart.x) / ppb();
            const int rowDelta   = (p.y - dragStart.y) / rowHeight;

            if (drag == DragMode::Move)
            {
                double b = startBeatAtDown + dxBeats;
                if (*snapToGridRef) b = snapToSubdivision(b);
                n.startBeats = juce::jmax(0.0, b);
                n.pitch      = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDown - rowDelta);
                if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(n.startBeats + n.lengthBeats), rulerH, 8, rowHeight));
            }
            else if (drag == DragMode::ResizeL)
            {
                double newStart = startBeatAtDown + dxBeats;
                double newLen   = lenBeatAtDown   - dxBeats;
                if (*snapToGridRef) { newStart = snapToSubdivision(newStart); newLen = snapToSubdivision(newLen); }
                newStart = juce::jmax(0.0, newStart);
                newLen   = juce::jmax(0.03125, newLen); // >= 1/32
                n.lengthBeats = newLen;
                n.startBeats  = newStart;
                n.pitch       = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDown - rowDelta);
            }
            else if (drag == DragMode::ResizeR)
            {
                double newLen = lenBeatAtDown + dxBeats;
                if (*snapToGridRef) newLen = snapToSubdivision(newLen);
                n.lengthBeats = juce::jmax(0.03125, newLen);
                n.pitch       = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDown - rowDelta);
                if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(n.startBeats + n.lengthBeats), rulerH, 8, rowHeight));
            }
            else if (drag == DragMode::Draw)
            {
                double len = juce::jmax(0.03125, (p.x - dragStart.x) / ppb());
                if (*snapToGridRef) len = snapToSubdivision(len);
                n.lengthBeats = len;
                if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(n.startBeats + len), rulerH, 8, rowHeight));
            }
            repaintMe();
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            if (drag == DragMode::LoopSet && onLoopChanged && loopEnabled && *loopEnabled && loopLengthBeats)
                onLoopChanged(*loopStartBeats, *loopLengthBeats);
            drag = DragMode::None; hitIndex = -1;
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (!notes) return false;
            if (key.getKeyCode() == juce::KeyPress::deleteKey && hitIndex >= 0)
            { notes->erase(notes->begin() + hitIndex); hitIndex = -1; repaintMe(); return true; }
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

        double snapToSubdivision(double beats) const
        {
            const double sub = currentSubdivisionBeats();
            return std::round(beats / sub) * sub;
        }

        void repaintMe(){ if (requestRepaint) requestRepaint(); }

        std::function<void(double,double)> onLoopChanged;
    } content;

    // ---- State ----
    juce::Viewport view;
    std::vector<MidiNote> notes;

    double bpmValue { 120.0 };
    bool   snapToGrid { true };

    double ppbAt120 { 52.0 };
    double pixelsPerBeat { 52.0 };
    double zoomScale { 1.0 };
    static constexpr double minPPB = 12.0;
    static constexpr double maxPPB = 1024.0;

    int minPitch { 48 };
    int maxPitch { 84 };

    double beatsExtent { 16.0 };

    bool autoFitPitchOnSet { true };
    bool autoFitBeatsOnSet { true };

    double playheadBeats { 0.0 };

    // loop
    bool   loopEnabled { false };
    double loopStartBeats { 0.0 };
    double loopLengthBeats { 0.0 };

    void refreshContentSize()
    {
        // width
        double maxEnd = beatsExtent;
        for (auto& n : notes) maxEnd = std::max(maxEnd, n.startBeats + n.lengthBeats + 4.0);
        const int width = Content::keyWidth + (int)std::ceil(maxEnd * pixelsPerBeat);

        // height from pitch window
        content.rowHeight = 22;
        content.totalRows = (maxPitch - minPitch + 1);
        content.topPitch  = maxPitch;
        const int height  = Content::rulerH + content.totalRows * content.rowHeight + Content::velLaneH;

        content.setSize(width, height);

        content.onLoopChanged = [this](double s, double l){ if (onLoopChanged) onLoopChanged(s, l); };
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditor)
};
