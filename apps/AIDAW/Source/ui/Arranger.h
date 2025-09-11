#pragma once
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <optional>
#include "Models.h"
#include "ClipComponent.h"
#include "TrackLaneComponent.h"
#include "ArrangerCanvas.h"

class Arranger  : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  private juce::Button::Listener
{
public:
    Arranger();
    ~Arranger() override;

    // hooks / API
    std::function<void()>        onProjectChanged;
    std::function<void(double)>  onPlayheadSet;   // Arranger -> engine seek
    std::function<bool()>        isPlayingQuery;  // ask transport if currently playing
    std::function<void()>        onPlayPressed;
    std::function<void()>        onStopPressed;

    void setBPM(double bpm);
    void setSnap(bool on);
    void setTool(ArrangerTool t);

    void setPlayheadBeats(double beats);
    double getPlayheadBeats() const;

    // zoom helpers / limits
    void setZoomLimits(double minPPB, double maxPPB);
    void zoomDelta(double steps);                       // fallback (viewport center)
    void zoomDeltaFromWheel(double wheelDelta, int screenX);  // anchor at mouse X (screen)
    void frameAll();

    // model
    std::vector<TrackModel> tracks;

    // JUCE
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int x, int y) override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    // content layer inside the viewport
    class ContentComp : public juce::Component
    {
        void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF0A0A0A)); }
    } content;

    juce::Viewport  view;
    ArrangerCanvas  canvas;                // overlay grid/ruler/playhead/selection (on top)
    juce::TextButton plusButton { "+" };   // add-track button (sibling to lanes)

    // tools
    juce::TextButton btnPointer, btnSlice, btnResize, btnZoomTool, btnFrameAll, btnSnap, btnZoomIn, btnZoomOut;
    ArrangerTool tool { ArrangerTool::Pointer };
    juce::Colour activeCol = juce::Colour(0xFF3B82F6), idleCol = juce::Colour(0xFF2A2A2A);

    // timing + grid
    double bpmValue { 120.0 };
    bool   snapToGrid { true };
    double ppbAt120 { 52.0 };
    double pixelsPerBeat { 52.0 };
    double zoomScale { 1.0 };

    // enforced zoom limits to avoid “clipping” at extreme zooms
    double minPixelsPerBeat { 8.0 };       // far view
    double maxPixelsPerBeat { 1024.0 };    // close view for vocal editing

    // playhead
    double playheadBeats { 0.0 };

    // audio utils
    juce::AudioFormatManager fm;
    juce::AudioThumbnailCache cache { 128 };

    // lanes/clips
    std::vector<std::unique_ptr<TrackLaneComponent>> laneComps;
    std::vector<std::unique_ptr<ClipComponent>>      clipComps;

    // selection & dragging
    int  selectedLaneIndex { -1 };
    ClipModel* selectedClip { nullptr };

    enum class DragMode { None, Move, Left, Right };
    DragMode dragging { DragMode::None };
    ClipComponent* activeClip { nullptr };
    juce::Point<int> dragStartPos;
    double clipStartBeatsAtDown { 0.0 }, clipLenBeatsAtDown { 0.0 }, clipOffsetBeatsAtDown { 0.0 };
    int startLaneIndex { -1 }, targetLaneIndex { -1 };

    // zoom + pan + playhead drag
    bool panActive { false };
    juce::Point<int> panStartMouse;
    juce::Point<int> panStartView;
    bool playheadDragActive { false };

    // Zoom tool: marquee selection & view restore
    bool selectionActive { false };
    juce::Point<int> selectionStartContent { 0, 0 };
    juce::Rectangle<int> selectionRectContent { 0, 0, 0, 0 };
    int   clickZoomStage { 0 }; // 0=none, 1=x1, 2=x2

    struct ViewState { double ppb; double scale; int vx; int vy; };
    std::optional<ViewState> preZoomView;          // saved on first zoom action
    bool rightClickRestoreArm { false };           // arm restore on RMB press/release

    // lane reorder
    int draggingLaneIndex { -1 };

    // undo/redo
    std::vector<std::vector<TrackModel>> undoStack, redoStack;

    // ---- layout helpers ----
    static constexpr int kDefaultTrackCount = 8;
    static constexpr double kZoomBase = 1.12;   // smooth exponential zoom
    static constexpr double kMinZoom  = 0.05;   // legacy safety (unused when ppb clamped)
    static constexpr double kMaxZoom  = 24.0;   // legacy safety (unused when ppb clamped)

    int  laneTop()     const { return 20; }     // ruler height
    int  laneHeight()  const { return 76; }     // single source of truth
    int  contentHeight() const
    {
        const int n = (int) tracks.size();
        const int lanesH = n * laneHeight();
        const int plusH  = 28;
        const int pad    = 40;
        return laneTop() + lanesH + 6 + plusH + pad;
    }

    void ensureDefaultTracks(int n);

    void layoutLanes();     // places lanes + plusButton, resizes content height (only height)
    void layoutClips();
    void extendContentToMaxClip(); // computes content width (only width)

    // internals
    void pushUndo();
    void undo();
    void redo();

    void applyCursorForToolEverywhere();
    void refreshAll();
    void updateClipSelectionVisuals();

    void setSelectedLane(int idx);
    int  laneIndexFromY(int yInContent) const;
    int  laneIndexFromYAllowNew(int yInContent) const;

    // mouse routing
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp  (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // zoom impl
    void zoomAtViewportCenter(double steps);
    void applyZoomAtContentX(double steps, int anchorXContent);
    void zoomToSelectionAndCenter(const juce::Rectangle<int>& rectContent);
    void restorePreZoomView();

    // model helpers
    int  trackIndexForClip(ClipModel& cm);
    void moveClipToLane(ClipModel* cm, int fromLane, int toLane);
    void removeClip(ClipModel* clip);
    void addTrack(const juce::String& name = "Audio", bool push = true);
    void duplicateTrack(size_t idx);
    void removeTrackById(const juce::String& id);

    juce::String extractTrackNameFromAudio(const juce::File& f);
    double estimateBeatsFromFile(const juce::File& f, double bpm);
    double detectLoopBpm(const juce::File& f);
    double secondsOfFile(const juce::File& f);
    void   recomputeClipBeatLengthsForTempo();
    void   performSliceAtBeat(ClipComponent& cc, double beat);

    // buttons
    void buttonClicked(juce::Button*) override;
};
