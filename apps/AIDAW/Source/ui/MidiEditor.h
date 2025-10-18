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
    int    pitch = 60;        // MIDI note number (C4=60)
    double startBeats = 0.0;  // start on timeline
    double lengthBeats = 1.0; // duration
    int    velocity = 100;    // 1..127
};

/*** Piano-roll editor (host provides notes) ***/
class MidiEditor : public juce::Component
{
public:
    MidiEditor()
    {
        setOpaque(true);
        setWantsKeyboardFocus(true);

        // Viewport + content
        addAndMakeVisible(view);
        view.setViewedComponent(&content, false);
        view.setScrollBarsShown(true, true);
        view.setScrollOnDragEnabled(true);

        // Content <-> parent bridge
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

            // Extend content for “infinite” right scroll
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

        // State refs
        content.notes           = &notes;
        content.snapToGridRef   = &snapToGrid;
        content.bpmRef          = &bpmValue;
        content.pixelsPerBeat   = &pixelsPerBeat;
        content.playheadBeats   = &playheadBeats;
        content.loopEnabled     = &loopEnabled;
        content.loopStartBeats  = &loopStartBeats;
        content.loopLengthBeats = &loopLengthBeats;

        // Defaults — bigger, calmer, FL-like
        setBPM(120.0);
        setSnap(true);
        setHorizontalZoom(8.0);      // show ~2 bars by default (bigger cells)
        setPitchView(48, 84);        // C3..C6
        setBeatsExtent(64.0);        // open wide

        refreshContentSize();
    }

    // ---------------- External control ----------------
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

    // Horizontal zoom: beats that fit in current viewport width
    void setHorizontalZoom(double beatsPerScreen)
    {
        beatsPerScreen = juce::jlimit(4.0, 64.0, beatsPerScreen);
        const int vw = juce::jmax(1, view.getWidth());
        const double desiredPPB = (double)vw / beatsPerScreen;
        pixelsPerBeat = juce::jlimit(minPPB, maxPPB, desiredPPB);
        zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
        refreshContentSize(); content.repaint();
    }

    // zoom at cursor (Ctrl+Wheel)
    void zoomDeltaFromWheel(double deltaY, int screenX)
    {
        const double oldPPB = pixelsPerBeat;
        const double steps = (deltaY > 0 ? +1.0 : -1.0);
        pixelsPerBeat = juce::jlimit(minPPB, maxPPB, pixelsPerBeat * std::pow(1.12, steps));
        zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));

        int mouseX = screenX - view.getScreenX();
        mouseX = juce::jlimit(0, view.getWidth(), mouseX);

        const int viewX     = view.getViewPositionX();
        const int contentX  = viewX + mouseX;
        const double beatAtMouse = Theme::beatsFromX(contentX, oldPPB, Theme::keyWidth);

        refreshContentSize();
        const int newContentX = Theme::xFromBeats(beatAtMouse, pixelsPerBeat, Theme::keyWidth);
        view.setViewPosition(juce::jmax(0, newContentX - mouseX), view.getViewPositionY());
        content.repaint();
    }
    void zoomDeltaFromWheel(double deltaY) { zoomDeltaFromWheel(deltaY, view.getScreenX()); }

    // Vertical pitch window
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

    // Timeline extent
    void setBeatsExtent(double beats) { beatsExtent = juce::jmax(1.0, beats); refreshContentSize(); content.repaint(); }
    void autoFitBeatsToNotes(double padBeats = 4.0)
    {
        double endB = 0.0;
        for (auto& n : notes) endB = std::max(endB, n.startBeats + n.lengthBeats);
        beatsExtent = juce::jmax(16.0, endB + padBeats);
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
        g.fillAll(juce::Colour(Theme::colBgMain));
        g.setColour(juce::Colour(Theme::colHeaderDiv));
        g.drawRect(getLocalBounds());
    }
    void resized() override
    {
        view.setBounds(getLocalBounds());
        refreshContentSize();
    }

    const std::vector<MidiNote>& getNotes() const { return notes; }

private:
    // ---------------- Inner content ----------------
    class Content : public juce::Component
    {
    public:
        // State refs
        std::vector<MidiNote>* notes = nullptr;
        bool*   snapToGridRef   = nullptr;
        double* bpmRef          = nullptr;
        double* pixelsPerBeat   = nullptr;
        double* playheadBeats   = nullptr;

        // Loop refs
        bool*   loopEnabled       = nullptr;
        double* loopStartBeats    = nullptr;
        double* loopLengthBeats   = nullptr;

        // Bridge to parent
        std::function<void()>                     requestRepaint;
        std::function<void(juce::Rectangle<int>)> ensureVisible;
        std::function<int()>                      getViewportWidth;
        std::function<void(int)>                  extendToPixelRight;

        // Layout
        int   totalRows = 37;
        int   rowHeight = Theme::rowHeight;
        int   topPitch  = 84;

        // Drag state
        enum class DragMode { None, Move, ResizeL, ResizeR, Draw, Velocity, Erase, LoopSet, Panning };
        DragMode drag { DragMode::None };
        int      hitIndex = -1;
        juce::Point<int> dragStart {0,0};
        double   startBeatAtDown { 0.0 };
        double   lenBeatAtDown   { 0.0 };
        int      pitchAtDown     { 60 };
        int      velAtDown       { 100 };
        double   loopStartAtDown { 0.0 };

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

        // --------------- Painting ---------------
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(Theme::colBgMain));

            // Header divider
            g.setColour(juce::Colour(Theme::colHeaderDiv));
            g.fillRect(Theme::keyWidth, 0, 1, getHeight());

            paintRuler(g);
            paintKeys(g);
            paintGrid(g);       // simpler grid
            paintLoopOverlay(g);
            paintNotes(g);
            paintVelocities(g);
            paintPlayhead(g);
        }

        void paintRuler(juce::Graphics& g)
        {
            auto r = juce::Rectangle<int>(0, 0, getWidth(), Theme::rulerH);
            g.setColour(juce::Colour(Theme::colBgRuler)); g.fillRect(r);
            g.setColour(juce::Colour(Theme::colHeaderDiv)); g.fillRect(r.withY(r.getBottom()-1).withHeight(1));

            const int totalBeats = (int)std::ceil((double)(getWidth()-Theme::keyWidth)/ppb());
            for (int beat = 0; beat <= totalBeats; ++beat)
            {
                if ((beat % 4) == 0)
                {
                    const int x = xFromBeats((double)beat);
                    g.setColour(juce::Colour(Theme::colBarLabel));
                    g.setFont(12.0f);
                    g.drawFittedText(juce::String((beat / 4) + 1), x + 6, 2, 36, Theme::rulerH-4,
                                     juce::Justification::centredLeft, 1);
                }
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

                if ((pc == 0) && ((pitch % 24) == 0)) // label every other C to reduce clutter
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

            // Horizontal rows (light zebra + octave accents)
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

            // Vertical: bars (2px), beats (1px), subs (faint; only when zoomed)
            const double ppbVal = ppb();
            const int totalBeats = (int)std::ceil((getWidth() - Theme::keyWidth) / ppbVal);

            for (int beat = 0; beat <= totalBeats; ++beat)
            {
                const int x = Theme::keyWidth + (int)std::round(beat * ppbVal);
                const bool isBar = (beat % 4) == 0;

                if (isBar)
                {
                    g.setColour(juce::Colour(Theme::colGridBar));
                    g.fillRect(x, Theme::rulerH, 2, getHeight() - Theme::rulerH);
                }
                else
                {
                    g.setColour(juce::Colour(Theme::colGridBeat));
                    g.fillRect(x, Theme::rulerH, 1, getHeight() - Theme::rulerH);
                }
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

                const auto body   = juce::Colour((i % 2 == 0) ? Theme::colNoteA : Theme::colNoteB);
                const auto border = body.brighter(Theme::noteBorderGain);

                juce::Rectangle<float> rf((float)x, (float)y, (float)w, (float)h);
                g.setColour(body.darker(0.30f)); g.fillRoundedRectangle(rf, 7.0f);
                g.setColour(border);              g.drawRoundedRectangle(rf, 7.0f, 1.2f);

                // Small edge grips (softer)
                g.setColour(juce::Colour(0x66FFFFFF));
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

        // --------------- Hit testing ---------------
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

        // --------------- Interaction ---------------
        void mouseDown(const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            if (!notes) return;

            const auto p = e.getPosition();
            dragStart = p;

            if (e.mods.isMiddleButtonDown()) { drag = DragMode::Panning; return; }

            // SHIFT on ruler = set loop region (drag to size)
            if (p.y < Theme::rulerH && e.mods.isShiftDown())
            {
                drag = DragMode::LoopSet;
                loopStartAtDown = beatsFromX(p.x);
                loopStartAtDown = snapToGrid(loopStartAtDown);
                if (loopEnabled) *loopEnabled = true;
                if (loopStartBeats) *loopStartBeats = loopStartAtDown;
                if (loopLengthBeats) *loopLengthBeats = 0.0;
                repaintMe();
                return;
            }

            // Velocity lane?
            if (p.y >= getHeight()-Theme::velocityH)
            {
                drag = DragMode::Velocity;
                hitIndex = closestNoteToX(p.x);
                if (hitIndex >= 0) velAtDown = (*notes)[(size_t)hitIndex].velocity;
                return;
            }

            // Right-click erase (hold + scrub like FL)
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

            // Hit existing note?
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
                // Draw fresh note at snapped position
                drag = DragMode::Draw;
                const int row = juce::jlimit(0, totalRows-1, (p.y - Theme::rulerH) / rowHeight);
                const int pitch = rowToPitch(row);
                double beat = beatsFromX(p.x);
                beat = snapToGrid(beat);

                MidiNote n; n.pitch = pitch; n.startBeats = beat; n.lengthBeats = 1.0; n.velocity = 100;
                notes->push_back(n);
                hitIndex = (int)notes->size() - 1;
                startBeatAtDown = beat; lenBeatAtDown = 1.0; pitchAtDown = pitch;

                if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(beat), Theme::rulerH + row*rowHeight, 40, rowHeight));
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
                double b1 = snapToGrid(beatsFromX(p.x));
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
                int h = juce::jlimit(0, Theme::velocityH-16, baseY - p.y);
                n.velocity = juce::jlimit(1, 127, (int)std::round((h / (double)(Theme::velocityH-16))*127.0));
                repaintMe(); return;
            }

            if (hitIndex < 0) return;
            auto& n = (*notes)[(size_t)hitIndex];
            const double dxBeats = (p.x - dragStart.x) / ppb();
            const int rowDelta   = (p.y - dragStart.y) / rowHeight;

            if (drag == DragMode::Move)
            {
                double b = startBeatAtDown + dxBeats;
                b = snapToGrid(b);
                n.startBeats = juce::jmax(0.0, b);
                n.pitch      = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDown - rowDelta);
                if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(n.startBeats + n.lengthBeats), Theme::rulerH, 8, rowHeight));
            }
            else if (drag == DragMode::ResizeL)
            {
                double newStart = startBeatAtDown + dxBeats;
                double newLen   = lenBeatAtDown   - dxBeats;
                newStart = snapToGrid(newStart);
                newLen   = juce::jmax(0.03125, snapToGrid(newLen));
                n.lengthBeats = newLen;
                n.startBeats  = juce::jmax(0.0, newStart);
                n.pitch       = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDown - rowDelta);
            }
            else if (drag == DragMode::ResizeR)
            {
                double newLen = lenBeatAtDown + dxBeats;
                n.lengthBeats = juce::jmax(0.03125, snapToGrid(newLen));
                n.pitch       = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDown - rowDelta);
                if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(n.startBeats + n.lengthBeats), Theme::rulerH, 8, rowHeight));
            }
            else if (drag == DragMode::Draw)
            {
                double len = juce::jmax(0.03125, (p.x - dragStart.x) / ppb());
                len = snapToGrid(len);
                n.lengthBeats = len;
                if (ensureVisible) ensureVisible(juce::Rectangle<int>(xFromBeats(n.startBeats + len), Theme::rulerH, 8, rowHeight));
            }
            repaintMe();
        }

        void mouseUp(const juce::MouseEvent&)
        {
            if (drag == DragMode::LoopSet && loopChangedCallback && loopEnabled && *loopEnabled && loopLengthBeats)
                loopChangedCallback(*loopStartBeats, *loopLengthBeats);

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

        void repaintMe(){ if (requestRepaint) requestRepaint(); }

        std::function<void(double,double)> loopChangedCallback;
    } content;

    // --------------- State ---------------
    juce::Viewport view;
    std::vector<MidiNote> notes;

    // grid/time
    double bpmValue { 120.0 };
    bool   snapToGrid { true };

    // zoom mapping (larger default pixels-per-beat for bigger grid)
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

    // playhead
    double playheadBeats { 0.0 };

    // loop
    bool   loopEnabled { false };
    double loopStartBeats { 0.0 };
    double loopLengthBeats { 0.0 };

    void refreshContentSize()
    {
        // Width: from beats extent, but never below a roomy minimum
        double maxEnd = beatsExtent;
        for (auto& n : notes) maxEnd = std::max(maxEnd, n.startBeats + n.lengthBeats + 4.0);

        const int widthFromBeats = Theme::keyWidth + (int)std::ceil(maxEnd * pixelsPerBeat);
        const int minOpenWidth   = juce::jmax(1600, view.getWidth() + 900);
        const int width = juce::jmax(widthFromBeats, minOpenWidth);

        // Height from pitch window
        content.rowHeight = Theme::rowHeight;
        content.totalRows = (maxPitch - minPitch + 1);
        content.topPitch  = maxPitch;
        const int height  = Theme::rulerH + content.totalRows * content.rowHeight + Theme::velocityH;

        content.setSize(width, height);

        // Loop change callback
        content.loopChangedCallback = [this](double s, double l)
        {
            if (onLoopChanged) onLoopChanged(s, l);
        };
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditor)
};
