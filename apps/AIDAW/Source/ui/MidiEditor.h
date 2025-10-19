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

class MidiEditor : public juce::Component,
                   private juce::Button::Listener
{
public:
    MidiEditor();
    ~MidiEditor() override = default;

    // External API
    void setNotes(const std::vector<MidiNote>& newNotes);
    std::vector<MidiNote>& editNotes();

    void setBPM(double bpm);
    void setSnap(bool on);

    void setHorizontalZoom(double beatsPerScreen);
    void zoomAtContentX(double steps, int anchorXContent);
    void frameAll();

    void setPitchView(int minPitchInclusive, int maxPitchInclusive);
    void autoFitPitchToNotes();
    void setAutoFitPitchOnSet(bool on);

    void setBeatsExtent(double beats);
    void autoFitBeatsToNotes(double padBeats = 4.0);
    void setAutoFitBeatsOnSet(bool on);

    void setPlayheadBeats(double beats);
    void setLoopEnabled(bool on);
    void setLoopRegion(double s, double len);
    std::function<void(double,double)> onLoopChanged;

    const std::vector<MidiNote>& getNotes() const;

    // Mouse wheel zoom anchor
    void zoomDeltaFromWheel(double wheelDelta, int screenX);

    // JUCE
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    enum class Tool { Select, Draw, Zoom };
    void setTool(Tool t);

    // Buttons
    void buttonClicked(juce::Button* b) override;

    // ---------- Inner content ----------
    class Content : public juce::Component
    {
    public:
        // State refs
        std::vector<MidiNote>* notes = nullptr;
        bool*   snapToGridRef   = nullptr;
        double* pixelsPerBeat   = nullptr;
        double* playheadBeats   = nullptr;
        bool*   loopEnabled     = nullptr;
        double* loopStartBeats  = nullptr;
        double* loopLengthBeats = nullptr;

        // Bridges
        std::function<void()>                     requestRepaint;
        std::function<void(juce::Rectangle<int>)> ensureVisible;
        std::function<int()>                      getViewportWidth;
        std::function<void(int)>                  extendToPixelRight;
        std::function<void()>                     onLayoutRequest;

        // Layout
        int   totalRows = 37;
        int   rowHeight = Theme::rowHeight;
        int   topPitch  = 84;

        // Tool
        MidiEditor::Tool activeTool { MidiEditor::Tool::Select };

        // Selection
        juce::Array<int> selection;

        // Constructor/behaviour
        Content();
        void syncNoteComponents();

        // Drawing
        void paint(juce::Graphics& g) override;
        void paintRuler(juce::Graphics& g);
        void paintKeys(juce::Graphics& g);
        void paintGrid(juce::Graphics& g);
        void paintLoopOverlay(juce::Graphics& g);
        void paintVelocities(juce::Graphics& g);
        void paintPlayhead(juce::Graphics& g);

        // Interaction
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp  (const juce::MouseEvent& e) override;
        bool keyPressed (const juce::KeyPress& key) override;
        void resized() override;

        // Helpers
        double ppb() const;
        int    gridHeight() const;
        double beatsFromX(int xContent) const;
        int    xFromBeats(double beats) const;
        int    pitchToRow(int pitch) const;
        int    rowToPitch(int row) const;

        double snapQuantum() const;
        double snapToGrid(double beats) const;
        int    closestNoteToX(int x) const;

        // Zoom helpers
        void zoomToRect(juce::Rectangle<int> rect);
        void restoreZoom();
        void centerContentOn(int cx, int cy);
        juce::Point<int> getParentViewportPos() const;
        int getParentViewportW() const;
        void setParentViewportPos(juce::Point<int> p);

        // Note interaction hooks used by NoteComponent
        void handleNoteMouseDown(int idx, juce::Point<int> pInContent, bool& didSelect, int& hitEdge);
        void handleNoteMouseDrag(int idx, juce::Point<int> pInContent);
        void handleNoteMouseUp  (int idx);
        void eraseNote(int idx);

        // edge hit test: 1 left | 2 right | 0 body
        int hitEdgeAt(int idx, juce::Point<int> p) const;

        // Custom note component
        class NoteComponent : public juce::Component
        {
        public:
            NoteComponent(
                int idx,
                std::function<void(int, juce::Point<int>, bool& didSelect, int& hitEdge)> onDown,
                std::function<void(int, juce::Point<int>)> onDrag,
                std::function<void(int)> onUp,
                std::function<void(int)> onRightErase);

            void setSelected(bool s);
            void setColors(juce::Colour body, juce::Colour rim);
            void paint(juce::Graphics& g) override;
            void mouseDown (const juce::MouseEvent& e) override;
            void mouseDrag (const juce::MouseEvent& e) override;
            void mouseUp   (const juce::MouseEvent& e) override;

            int index = -1;
            int hitEdge = 0;

        private:
            bool selected = false;
            juce::Colour bodyCol { juce::Colours::mediumpurple };
            juce::Colour rimCol  { juce::Colours::white };

            std::function<void(int, juce::Point<int>, bool&, int&)> onMouseDown;
            std::function<void(int, juce::Point<int>)>              onMouseDragCB;
            std::function<void(int)>                                onMouseUpCB;
            std::function<void(int)>                                onRightEraseCB;
        };

        // Drag caches
        juce::Array<double> startAtDown, lenAtDown;
        juce::Array<int>    pitchAtDown;
        juce::Point<int>    dragAnchorContent {0,0};

        // States
        bool bypassSnap { false };
        bool panActive { false };
        juce::Point<int> panStart {0,0}, panViewStart {0,0};
        bool selectionDragActive { false };
        juce::Point<int> selStart {0,0};
        bool loopDragActive { false };
        double loopStartAtDown { 0.0 };
        bool velDragActive { false };
        int  velNearest { -1 };
        bool marqueeActive { false };
        juce::Rectangle<int> marquee;

        // --- Draw tool state ---
        bool   creatingNote { false };
        int    createdIndex { -1 };
        double createdStartBeats { 0.0 };

        std::vector<std::unique_ptr<NoteComponent>> noteComps;
    };

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

    void refreshContentSize();
};
