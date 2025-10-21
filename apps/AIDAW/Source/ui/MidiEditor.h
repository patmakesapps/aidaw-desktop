#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>
#include "Theme.h"

// ===== Model =====
struct MidiNote
{
    int    pitch        = 60;
    double startBeats   = 0.0;
    double lengthBeats  = 1.0;
    int    velocity     = 96;
};

// ===== MidiEditor =====
class MidiEditor : public juce::Component,
                   private juce::Button::Listener
{
public:
    MidiEditor();
    ~MidiEditor() override = default;

    enum class Tool { Select, Draw, Zoom };

    // ---- External callbacks ----
    // Fired whenever loop is changed via UI or set programmatically here.
    std::function<void(double /*loopStart*/, double /*loopLength*/)> onLoopChanged;

    // Model API
    void setNotes(const std::vector<MidiNote>& newNotes);
    std::vector<MidiNote>& editNotes();
    const std::vector<MidiNote>& getNotes() const;

    // Transport / View
    void setBPM(double bpm);
    void setSnap(bool on);
    void setHorizontalZoom(double beatsPerScreen);
    void frameAll();
    void setPitchView(int minPitchInclusive, int maxPitchInclusive);
    void autoFitPitchToNotes();
    void setAutoFitPitchOnSet(bool on);
    void setBeatsExtent(double beats);
    void autoFitBeatsToNotes(double padBeats);
    void setAutoFitBeatsOnSet(bool on);
    void setPlayheadBeats(double beats);
    void setLoopEnabled(bool on);
    void setLoopRegion(double startBeats, double lengthBeats);

    // Loop accessors (optional external sync)
    bool   isLoopEnabled() const { return loopEnabled; }
    double loopStart()     const { return loopStartBeats; }
    double loopLength()    const { return loopLengthBeats; }

    // Ctrl+wheel zoom support (anchor at screen X like your Arranger)
    void zoomDeltaFromWheel(double delta, int screenX);

    // Component overrides
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    // ===== Content =====
    class Content : public juce::Component
    {
    public:
        Content();

        // wired to the editor (parent) immediately after creation
        std::function<void()>                         requestRepaint;
        std::function<void(juce::Rectangle<int>)>     ensureVisible;
        std::function<void(int)>                      extendToPixelRight; // optional, can be nullptr
        std::function<void()>                         onRequestFrameAll;   // for zoom reset

        // references to editor state
        std::vector<MidiNote>* notes           = nullptr;
        bool*   snapToGridRef                  = nullptr;
        double* pixelsPerBeat                  = nullptr;
        double* playheadBeats                  = nullptr;
        bool*   loopEnabled                    = nullptr;
        double* loopStartBeats                 = nullptr;
        double* loopLengthBeats                = nullptr;

        // editor tool
        MidiEditor::Tool activeTool = MidiEditor::Tool::Select;

        // layout info (owned by content)
        int rowHeight = Theme::rowHeight;
        int totalRows = 60;
        int topPitch  = 84;

        // core API
        void syncNoteComponents();
        void repaintNotesOnly();

        // Component
        void paint(juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress& key) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp  (const juce::MouseEvent& e) override;

        // helpers
        double ppb() const;
        int    gridHeight() const;
        double beatsFromX(int xContent) const;
        int    xFromBeats(double beats) const;
        int    pitchToRow(int pitch) const;
        int    rowToPitch(int row) const;
        double snapQuantum() const;
        double snapToGrid(double beats) const;
        int    closestNoteToX(int x) const;
        void   zoomToRect(juce::Rectangle<int> rect);
        void   restoreZoom();
        juce::Point<int> getParentViewportPos() const;
        int    getParentViewportW() const;
        void   setParentViewportPos(juce::Point<int> p);
        void   centerContentOn(int x, int y);

        // selection
        juce::Array<int> selection;

        // state for marquee and panning etc.
        bool  marqueeActive       = false;
        bool  selectionDragActive = false;
        bool  additiveMarquee     = false;
        juce::Point<int> selStart {0,0};
        juce::Array<int> preMarqueeSelection;

        bool  panActive           = false;
        juce::Point<int> panStart {0,0};
        juce::Point<int> panViewStart {0,0};

        // right-click erase should not nuke multiple: keep scoped
        int lastFocusedIndex = -1;

        // Loop editing
        enum class LoopDrag { None, Move, ResizeLeft, ResizeRight };
        LoopDrag loopDragMode = LoopDrag::None;
        bool     loopDragActive = false;
        double   loopStartAtDown = 0.0;
        double   loopLenAtDown   = 0.0;

        // drawing note state
        bool creatingNote = false;
        int  createdIndex = -1;
        double createdStartBeats = 0.0;

        // velocity editing
        bool velDragActive = false;
        int  velNearest    = -1;

        // snap bypass with Alt
        bool bypassSnap = false;

        // drag snapshot for moving/resize notes
        juce::Point<int> dragAnchorContent {0,0};
        juce::Array<double> startAtDown, lenAtDown;
        juce::Array<int>    pitchAtDown;

        // zoom memory (for potential future use)
        juce::Rectangle<int> lastZoomRect {0,0,0,0};

        // note component
        struct NoteComponent : public juce::Component
        {
            NoteComponent(
                int idx,
                std::function<void(int, juce::Point<int>, bool&, int&)> onDown,
                std::function<void(int, juce::Point<int>)>              onDrag,
                std::function<void(int)>                                 onUp,
                std::function<void(int)>                                 onRightErase);

            int index = -1;
            void setSelected(bool s);
            void setColors(juce::Colour body, juce::Colour rim);

            void paint(juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent& e) override;
            void mouseDrag (const juce::MouseEvent& e) override;
            void mouseUp   (const juce::MouseEvent& e) override;

            // hit-test result for edge (0 move, 1 left, 2 right)
            int hitEdge = 0;

        private:
            bool selected = false;
            juce::Colour bodyCol = juce::Colours::cornflowerblue;
            juce::Colour rimCol  = juce::Colours::white;

            std::function<void(int, juce::Point<int>, bool&, int&)> onMouseDown;
            std::function<void(int, juce::Point<int>)>              onMouseDragCB;
            std::function<void(int)>                                 onMouseUpCB;
            std::function<void(int)>                                 onRightEraseCB;
        };

        std::vector<std::unique_ptr<NoteComponent>> noteComps;

        // note manipulation
        void handleNoteMouseDown(int idx, juce::Point<int> pInContent, bool& didSelect, int& edge);
        void handleNoteMouseDrag(int idx, juce::Point<int> pInContent);
        void handleNoteMouseUp  (int idx);
        void eraseNote(int idx);
        int  hitEdgeAt(int idx, juce::Point<int> p) const;
    };

    // internal helpers
    void setTool(Tool t);
    void buttonClicked(juce::Button* b) override;

    void refreshContentSize();
    void zoomAtContentX(double steps, int anchorXContent);

    // UI
    juce::TextButton btnSelect, btnDraw, btnZoomTool, btnFrameAll, btnSnap, btnZoomOut, btnZoomIn;
    juce::Viewport   view;

    // state
    std::vector<MidiNote> notes;

    // zoom/grid state
    double bpmValue      = 120.0;
    double ppbAt120      = 240.0; // pixels per beat at 120 BPM, base scale
    double pixelsPerBeat = 240.0;
    double zoomScale     = 1.0;
    static constexpr double minPPB = 16.0;
    static constexpr double maxPPB = 4096.0;

    // pitch view
    int minPitch = 48;
    int maxPitch = 84;

    // extent
    double beatsExtent = 64.0;

    // options
    bool snapToGrid = true;
    bool autoFitBeatsOnSet = false;
    bool autoFitPitchOnSet = false;

    // transport
    double playheadBeats   = 0.0;
    bool   loopEnabled     = false;
    double loopStartBeats  = 0.0;
    double loopLengthBeats = 0.0;

    // tool
    Tool tool = Tool::Select;

    // content-extenders (wired into content)
    std::function<void(int)> extendToPixelRight = [this](int px)
    {
        if (auto* c = dynamic_cast<Content*>(view.getViewedComponent()))
        {
            if (px > c->getWidth())
                c->setSize(px, c->getHeight());
        }
    };
};
