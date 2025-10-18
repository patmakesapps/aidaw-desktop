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

/*** Piano-roll editor (data provided by host — no seeding) ***/
class MidiEditor : public juce::Component
{
public:
    MidiEditor()
    {
        setOpaque(true);
        setWantsKeyboardFocus(true);

        addAndMakeVisible(view);
        view.setViewedComponent(&content, false);

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
            view.setViewPosition(nx, ny);
        };
        content.getViewportWidth = [this]{ return view.getViewArea().getWidth(); };

        // State refs
        content.notes         = &notes;
        content.snapToGridRef = &snapToGrid;
        content.bpmRef        = &bpmValue;
        content.pixelsPerBeat = &pixelsPerBeat;
        content.playheadBeats = &playheadBeats;

        // Defaults (host can override)
        setBPM(120.0);
        setSnap(true);
        setHorizontalZoom(16.0);         // beats per viewport by default
        setPitchView(48, 84);            // C3..C6 initial window
        setBeatsExtent(16.0);            // 4 bars initial length

        refreshContentSize();
    }

    // -------- External control (NO HARDCODED DATA) --------
    void setNotes(const std::vector<MidiNote>& newNotes)
    {
        notes = newNotes;
        if (autoFitBeatsOnSet)  autoFitBeatsToNotes(8.0);
        if (autoFitPitchOnSet)  autoFitPitchToNotes();
        refreshContentSize(); content.repaint();
    }

    std::vector<MidiNote>& editNotes() { return notes; }

    // Time/grid
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

    // New one-arg API
    void zoomDeltaFromWheel(double deltaY /* +up/-down */)
    {
        const double steps = deltaY > 0 ? +1.0 : -1.0;
        pixelsPerBeat = juce::jlimit(minPPB, maxPPB, pixelsPerBeat * std::pow(1.12, steps));
        zoomScale     = pixelsPerBeat / (ppbAt120 * (120.0 / bpmValue));
        refreshContentSize(); content.repaint();
    }
    // Back-compat with existing Main.cpp call site
    void zoomDeltaFromWheel(double deltaY, int /*screenX*/)
    {
        zoomDeltaFromWheel(deltaY);
    }

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
    void setBeatsExtent(double beats) // fallback when there are no notes
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

    // Playhead
    void setPlayheadBeats(double beats) { playheadBeats = juce::jmax(0.0, beats); content.repaint(); }

    // JUCE
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF0B0E12));
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

        // bridge to parent
        std::function<void()>                     requestRepaint;
        std::function<void(juce::Rectangle<int>)> ensureVisible;
        std::function<int()>                      getViewportWidth;

        // constants
        static constexpr int keyWidth = 66;
        static constexpr int rulerH   = 22;
        static constexpr int velLaneH = 64;

        // layout
        int   totalRows = 37;   // updated by parent via setPitchView
        int   rowHeight = 22;
        int   topPitch  = 84;   // updated by parent via setPitchView

        // state
        enum class DragMode { None, Move, ResizeL, ResizeR, Draw, Velocity }; // ← SINGLE definition
        DragMode drag { DragMode::None };
        int      hitIndex = -1;
        juce::Point<int> dragStart {0,0};
        double   startBeatAtDown { 0.0 };
        double   lenBeatAtDown   { 0.0 };
        int      pitchAtDown     { 60 };
        int      velAtDown       { 100 };

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

        // ---- Drawing ----
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF0E1218));
            g.setColour(juce::Colour(0x222B3340));
            g.fillRect(keyWidth, 0, 1, getHeight());

            paintRuler(g);
            paintKeys(g);
            paintGrid(g);
            paintNotes(g);
            paintVelocities(g);
            paintPlayhead(g);
        }

        void paintRuler(juce::Graphics& g)
        {
            auto r = juce::Rectangle<int>(keyWidth, 0, getWidth()-keyWidth, rulerH);
            g.setColour(juce::Colour(0xFF0D1016)); g.fillRect(r);
            g.setColour(juce::Colour(0x223B82F6)); g.fillRect(r.removeFromBottom(1));

            const int totalBeats = (int)std::ceil((double)(getWidth()-keyWidth)/ppb());
            for (int beat = 0; beat <= totalBeats; ++beat)
            {
                const int x = keyWidth + (int)std::round(beat * ppb());
                const bool isBar = (beat % 4) == 0;

                g.setColour(isBar ? juce::Colour(0x3348A3FF) : juce::Colour(0x18405062));
                g.fillRect(x, rulerH, 1, getHeight()-rulerH);

                if (isBar)
                {
                    g.setColour(juce::Colour(0x77FFFFFF));
                    g.setFont(11.0f);
                    g.drawText(juce::String((beat / 4) + 1), x + 4, 2, 28, rulerH-4, juce::Justification::centredLeft);
                }
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

                if (pc == 0)  // C label
                {
                    g.setColour(juce::Colour(0x66FFFFFF));
                    g.setFont(11.0f);
                    const int octave = (pitch / 12) - 1;
                    g.drawText("C" + juce::String(octave), key.reduced(6, 2), juce::Justification::centredLeft);
                }

                g.setColour(juce::Colour(0x222B3340));
                g.fillRect(key.removeFromBottom(1));
                y += rowHeight;
            }
        }

        void paintGrid(juce::Graphics& g)
        {
            auto r = juce::Rectangle<int>(keyWidth, rulerH, getWidth()-keyWidth, gridHeight());
            int y = r.getY();
            for (int row = 0; row < totalRows; ++row)
            {
                const bool zebra = ((row / 12) % 2) == 0;
                g.setColour(zebra ? juce::Colour(0xFF0F131A) : juce::Colour(0xFF10151D));
                g.fillRect(juce::Rectangle<int>(r.getX(), y, r.getWidth(), rowHeight));
                y += rowHeight;
            }
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

                const bool alt = (i % 2) == 0;
                const auto body   = juce::Colour(alt ? 0xFF2C72FA : 0xFF7A3BFF);
                const auto border = body.brighter(0.35f);

                juce::Rectangle<float> rf((float)x, (float)y, (float)w, (float)h);
                g.setColour(body.darker(0.35f)); g.fillRoundedRectangle(rf, 6.0f);
                g.setColour(border);             g.drawRoundedRectangle(rf, 6.0f, 1.5f);

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
            g.setColour(juce::Colour(0xFF20C997));
            g.fillRect(x-1, 0, 2, getHeight());
        }

        // ---- Interaction ----
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

        void mouseDown(const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            if (!notes) return;

            const auto p = e.getPosition();
            dragStart = p;

            // velocity lane?
            if (p.y >= getHeight()-velLaneH)
            {
                drag = DragMode::Velocity;
                hitIndex = closestNoteToX(p.x);
                if (hitIndex >= 0) velAtDown = (*notes)[(size_t)hitIndex].velocity;
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
                if (*snapToGridRef) beat = std::round(beat * 4.0) / 4.0;

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
            if (!notes || hitIndex < 0) return;
            auto& n = (*notes)[(size_t)hitIndex];
            const auto p = e.getPosition();

            if (drag == DragMode::Velocity)
            {
                const int baseY = getHeight() - 10;
                int h = juce::jlimit(0, velLaneH-16, baseY - p.y);
                n.velocity = juce::jlimit(1, 127, (int)std::round((h / (double)(velLaneH-16))*127.0));
                repaintMe(); return;
            }

            const double dxBeats = (p.x - dragStart.x) / ppb();
            const int rowDelta   = (p.y - dragStart.y) / rowHeight;

            if (drag == DragMode::Move)
            {
                double b = startBeatAtDown + dxBeats;
                if (*snapToGridRef) b = std::round(b * 4.0)/4.0;
                n.startBeats = juce::jmax(0.0, b);
                n.pitch      = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDown - rowDelta);
            }
            else if (drag == DragMode::ResizeL)
            {
                double newStart = startBeatAtDown + dxBeats;
                double newLen   = lenBeatAtDown   - dxBeats;
                if (*snapToGridRef) { newStart = std::round(newStart * 4.0)/4.0; newLen = std::round(newLen*4.0)/4.0; }
                newStart = juce::jmax(0.0, newStart);
                newLen   = juce::jmax(0.125, newLen);
                n.lengthBeats = newLen;
                n.startBeats  = newStart;
                n.pitch       = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDown - rowDelta);
            }
            else if (drag == DragMode::ResizeR)
            {
                double newLen = lenBeatAtDown + dxBeats;
                if (*snapToGridRef) newLen = std::round(newLen*4.0)/4.0;
                n.lengthBeats = juce::jmax(0.125, newLen);
                n.pitch       = juce::jlimit(topPitch - totalRows + 1, topPitch, pitchAtDown - rowDelta);
            }
            else if (drag == DragMode::Draw)
            {
                double len = juce::jmax(0.125, (p.x - dragStart.x) / ppb());
                if (*snapToGridRef) len = std::round(len*4.0)/4.0;
                n.lengthBeats = len;
            }
            repaintMe();
        }

        void mouseUp(const juce::MouseEvent&) override
        { drag = DragMode::None; hitIndex = -1; }

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
                vp->setViewPosition(juce::jlimit(0, maxX, p.getX() + dx), p.getY());
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
    } content;

    // ---- State (no hardcoded demo data) ----
    juce::Viewport view;
    std::vector<MidiNote> notes;

    // grid/time
    double bpmValue { 120.0 };
    bool   snapToGrid { true };

    // zoom mapping (kept as internal tunables)
    double ppbAt120 { 52.0 };
    double pixelsPerBeat { 52.0 };
    double zoomScale { 1.0 };
    static constexpr double minPPB = 12.0;
    static constexpr double maxPPB = 1024.0;

    // pitch window (externally controllable)
    int minPitch { 48 };
    int maxPitch { 84 };

    // content extent if there are no notes / or as a floor
    double beatsExtent { 16.0 };

    // auto-fit flags for convenience (you can turn these off)
    bool autoFitPitchOnSet { true };
    bool autoFitBeatsOnSet { true };

    // playhead
    double playheadBeats { 0.0 };

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
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditor)
};
