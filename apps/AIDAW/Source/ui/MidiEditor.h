#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Guard for external projects that might already declare MidiNote
#ifndef AIDAW_MIDI_NOTE_DEFINED
#define AIDAW_MIDI_NOTE_DEFINED 1
struct MidiNote
{
    int    pitch        { 60 };
    double startBeats   { 0.0 };
    double lengthBeats  { 1.0 };
    int    velocity     { 100 };
    uint32 uid          { 0 }; // stable id for safe deletion while dragging
};
#endif

class MidiEditor : public juce::Component,
                   private juce::Button::Listener
{
public:
    MidiEditor();
    ~MidiEditor() override = default;

    // ==== External model control ====
    void setNotes(const std::vector<MidiNote>& newNotes);
    std::vector<MidiNote>& editNotes();
    const std::vector<MidiNote>& getNotes() const;

    void setBPM(double bpm);
    void setSnap(bool on);
    void setHorizontalZoom(double beatsPerScreen);
    void setPitchView(int minPitchInclusive, int maxPitchInclusive);
    void setBeatsExtent(double beats);
    void autoFitBeatsToNotes(double padBeats = 8.0);
    void setAutoFitBeatsOnSet(bool on);
    void autoFitPitchToNotes();
    void setAutoFitPitchOnSet(bool on);

    void setPlayheadBeats(double beats);
    void setLoopEnabled(bool on);
    void setLoopRegion(double startBeats, double lengthBeats);

    // mouse-wheel zoom anchor by screenX
    void zoomDeltaFromWheel(double wheelDelta, int screenX);

    // callback when loop changed by user (start, length)
    std::function<void(double, double)> onLoopChanged;

    // called by toolbar "Loops" button
    std::function<void()> onShowLoops;

    enum class Tool { Select = 0, Draw = 1, Zoom = 2 };
    void setTool(Tool t);

    // JUCE
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Expose Frame All for child
    void frameAllView(); // calls private frameAll()

    // expose for MainComponent
    void setLoopEnabledInternal(bool on) { loopEnabled = on; }

private:
    // ===== Content =====
    class Content : public juce::Component
    {
    public:
        Content();

        // brushes from the host editor
        std::vector<MidiNote>* notes { nullptr };
        bool*   snapToGridRef   { nullptr };
        double* pixelsPerBeat   { nullptr };
        double* playheadBeats   { nullptr };
        bool*   loopEnabled     { nullptr };
        double* loopStartBeats  { nullptr };
        double* loopLengthBeats { nullptr };

        // host layout hooks
        std::function<void()> onLayoutRequest;

        // accessors
        double ppb() const;
        int    gridHeight() const;
        double beatsFromX(int xContent) const;
        int    xFromBeats(double beats) const;

        // state controlled by host
        int  totalRows { 0 };
        int  topPitch  { 84 };
        int  rowHeight { Theme::rowHeight };
        Tool activeTool { Tool::Select };

        // bridge helpers
        void syncNoteComponents();

        // JUCE
        void paint(juce::Graphics& g) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress& key) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp   (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;

        // public utility for parent
        void zoomToRect(juce::Rectangle<int> rect);
        void restoreZoom(); // calls parent->frameAllView()

        // for parent viewport control
        juce::Point<int> getParentViewportPos() const;
        int getParentViewportW() const;
        void setParentViewportPos(juce::Point<int> p);
        void centerContentOn(int x, int y);

        // injected by parent to extend content width lazily
        std::function<void(int)> extendToPixelRight;

        // ensure that after zoom, view contains at least one note
        void ensureViewportShowsNotesNearBeat(double anchorBeatIfNeeded);

    private:
        // visuals
        void paintRuler(juce::Graphics& g);
        void paintKeys(juce::Graphics& g);
        void paintGrid(juce::Graphics& g);
        void paintLoopOverlay(juce::Graphics& g);
        void paintVelocities(juce::Graphics& g);
        void paintPlayhead(juce::Graphics& g);

        // conversions
        int    pitchToRow(int pitch) const;
        int    rowToPitch(int row) const;
        double snapQuantum() const;
        double snapToGrid(double beats) const;

        // interaction helpers
        int closestNoteToX(int x) const;
        int noteIndexByUid(uint32 uid) const;
        int indexAtPointNoteArea(juce::Point<int> p) const;

        // ===== NoteComponent =====
        class NoteComponent : public juce::Component
        {
        public:
            NoteComponent(
                uint32 noteUid,
                std::function<void(uint32, juce::Point<int>, bool&, int&)> onDown,
                std::function<void(uint32, juce::Point<int>)> onDrag,
                std::function<void(uint32)> onUp,
                std::function<void(uint32)> onRightErase);

            void setSelected(bool s);
            void setColors(juce::Colour body, juce::Colour rim);
            void paint(juce::Graphics& g) override;
            void mouseDown (const juce::MouseEvent& e) override;
            void mouseDrag (const juce::MouseEvent& e) override;
            void mouseUp   (const juce::MouseEvent& e) override;

            uint32 noteId { 0 };
            int hitEdge { 0 }; // 0=body,1=left,2=right
        private:
            bool selected { false };
            juce::Colour bodyCol, rimCol;
            std::function<void(uint32, juce::Point<int>, bool&, int&)> onMouseDown;
            std::function<void(uint32, juce::Point<int>)> onMouseDragCB;
            std::function<void(uint32)> onMouseUpCB;
            std::function<void(uint32)> onRightEraseCB;
        };

        // internal note interactions
        void handleNoteMouseDown(uint32 uid, juce::Point<int> pInContent, bool& didSelect, int& edge);
        void handleNoteMouseDrag(uint32 uid, juce::Point<int> pInContent);
        void handleNoteMouseUp(uint32 uid);
        void eraseNoteByUid(uint32 uid);

        int hitEdgeAt(int idx, juce::Point<int> p) const;

        // marquee
        bool marqueeActive { false };
        bool selectionDragActive { false };
        juce::Rectangle<int> marquee;
        juce::Point<int>     selStart;

        // selection
        juce::Array<int> selection;
        juce::Array<double> startAtDown, lenAtDown;
        juce::Array<int>    pitchAtDown;

        // panning
        bool panActive { false };
        juce::Point<int> panStart { 0,0 }, panViewStart { 0,0 };

        // draw create
        bool creatingNote { false };
        int createdIndex { -1 };
        double createdStartBeats { 0.0 };

        // velocity drag
        bool velDragActive { false };
        int velNearest { -1 };

        // snap bypass
        bool bypassSnap { false };

        // drag anchor
        juce::Point<int> dragAnchorContent { 0,0 };

        // ===== loop UI =====
        enum class LoopDrag { None, Move, ResizeLeft, ResizeRight };
        LoopDrag loopDragMode { LoopDrag::None };
        double   loopStartAtDown { 0.0 };
        double   loopLenAtDown   { 0.0 };
        bool     hoverOnLeftEdge  { false };
        bool     hoverOnRightEdge { false };
        bool     hoverOnBody      { false };

        // components for each note
        std::vector<std::unique_ptr<NoteComponent>> noteComps;
    };

    // ===== host helpers =====
    void refreshContentSize();
    void frameAll();
    void zoomAtContentX(double steps, int anchorXContent);
    void centerOnNearestNote(double anchorBeat);

    // JUCE
    void buttonClicked(juce::Button* b) override;

    // ===== state =====
    std::vector<MidiNote> notes;
    bool snapToGrid { true };
    double bpmValue { 120.0 };
    double pixelsPerBeat { 120.0 };
    double ppbAt120 { 120.0 };
    double zoomScale { 1.0 };
    static constexpr double minPPB = 16.0;
    static constexpr double maxPPB = 4096.0; // allow super-zoom

    int minPitch { 48 }, maxPitch { 84 };
    double beatsExtent { 64.0 };

    double playheadBeats { 0.0 };
    bool   loopEnabled   { true };
    double loopStartBeats{ 0.0 };
    double loopLengthBeats{4.0};

    bool autoFitBeatsOnSet { false };
    bool autoFitPitchOnSet { false };

    uint32 nextNoteUid { 1 };

    // UI
    juce::Viewport view;
    juce::TextButton btnSelect, btnDraw, btnZoomTool, btnFrameAll, btnSnap, btnZoomOut, btnZoomIn, btnLoops;

    Tool tool { Tool::Select };
};
