#pragma once
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <optional>
#include "../shared/Models.h"
#include "../shared/Theme.h"
#include "../shared/Icons.h"
#include "ClipComponent.h"
#include "TrackLaneComponent.h"
#include "ArrangerHeaderStrip.h"
#include "ArrangerCanvas.h"

class Arranger  : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  public juce::DragAndDropTarget,
                  public juce::ChangeListener,
                  private juce::Timer,
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

    // open the Loops modal from the Arranger toolbar
    std::function<void()>        onLoopsClicked;
    std::function<void(uint32)>  onOpenMidiLoop;

    // open the Eddie synth panel from the Arranger toolbar
    std::function<void()>        onEddieClicked;
    std::function<void()>        onSamplesClicked;

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
    void refreshFromModel();
    void addMidiLoopClip(uint32 loopId, double startBeats = 0.0, int laneIndex = 0);
    void removeClipsForLoop(uint32 loopId);

    // model
    std::vector<TrackModel> tracks;

    // JUCE
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int x, int y) override;
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

private:
    // content layer inside the viewport
    class ContentComp : public juce::Component
    {
        void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(Theme::colBgMain)); }
    } content;

    // Viewport that fires a callback whenever its visible region changes,
    // so the pinned header strip can scroll vertically in lockstep.
    class ArrangerViewport : public juce::Viewport
    {
    public:
        std::function<void(const juce::Rectangle<int>&)> onVisibleAreaChanged;
        void visibleAreaChanged(const juce::Rectangle<int>& r) override
        {
            juce::Viewport::visibleAreaChanged(r);
            if (onVisibleAreaChanged) onVisibleAreaChanged(r);
        }
    };

    ArrangerViewport view;
    ArrangerCanvas   canvas;                // overlay grid/ruler/playhead/selection (on top)
    HeaderStrip      headerStrip;           // pinned track-header panel (always visible)

    // tools (icon buttons)
    IconButton btnPointer  { "Pointer (1)",         Icons::pointer(),   IconButton::Style::Filled };
    IconButton btnSlice    { "Slice (2)",           Icons::scissors(),  IconButton::Style::Stroked };
    IconButton btnResize   { "Resize (3)",          Icons::resizeH(),   IconButton::Style::Stroked };
    IconButton btnZoomTool { "Zoom (4)",            Icons::magnifier(), IconButton::Style::Stroked };
    IconButton btnFrameAll { "Frame all (F)",       Icons::frame(),     IconButton::Style::Stroked };
    IconButton btnSnap     { "Snap to grid (G)",    Icons::magnet(),    IconButton::Style::Stroked };
    IconButton btnZoomIn   { "Zoom in (+)",         Icons::plus(),      IconButton::Style::Stroked };
    IconButton btnZoomOut  { "Zoom out (-)",        Icons::minus(),     IconButton::Style::Stroked };
    IconButton btnLoopIcon { "Open Loops",          Icons::loopGlyph(), IconButton::Style::Stroked };
    juce::TextButton btnLoops { "Loops" };
    juce::TextButton btnEddie { "Eddie" };
    juce::TextButton btnSamples { "Samples" };

    ArrangerTool tool { ArrangerTool::Pointer };

    // timing + grid
    double bpmValue { 120.0 };
    bool   snapToGrid { true };
    double ppbAt120 { 80.0 };
    double pixelsPerBeat { 80.0 };
    double zoomScale { 1.0 };

    // enforced zoom limits — match MIDI editor for parity
    double minPixelsPerBeat { 8.0 };
    double maxPixelsPerBeat { 4096.0 };

    // playhead
    double playheadBeats { 0.0 };

    // FL-style vertical zoom for playlist tracks
    int laneHeightPx { 76 };

    // audio utils
    juce::AudioFormatManager fm;
    juce::AudioThumbnailCache cache { 512 };

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

    bool pasteArm = false; // arm quick-paste when clicking empty lanes

    // Smooth-drag preview (no model writes until mouseUp)
    double pendingMoveStartBeats { 0.0 };
    bool   pendingMoveValid { false };

    // zoom + pan + playhead drag
    bool panActive { false };
    juce::Point<int> panStartMouse;
    juce::Point<int> panStartView;
    bool playheadDragActive { false };
    bool dragAutoScrollActive { false };
    juce::Point<int> lastDragViewportPos { 0, 0 };
    bool lastDragBypassSnap { false };

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
    int reorderInsertIndex { -1 };

    // undo/redo
    std::vector<std::vector<TrackModel>> undoStack, redoStack;

    // ---- layout helpers ----
    static constexpr int kDefaultTrackCount = 8;
    static constexpr int kDefaultLaneHeight = 76;
    static constexpr int kMinLaneHeight = 44;
    static constexpr int kMaxLaneHeight = 140;
    static constexpr double kZoomBase = 1.12;   // smooth exponential zoom
    static constexpr double kMinZoom  = 0.05;   // legacy safety (unused when ppb clamped)
    static constexpr double kMaxZoom  = 24.0;   // legacy safety (unused when ppb clamped)

    int  laneTop()     const { return 20; }     // ruler height
    int  laneHeight()  const { return laneHeightPx; }
    int  contentHeight() const
    {
        const int n = (int) tracks.size();
        const int lanesH = n * laneHeight();
        const int plusH  = 28;
        const int pad    = 40;
        return laneTop() + lanesH + 6 + plusH + pad;
    }

    void ensureDefaultTracks(int n);

    void layoutLanes();     // places lane components and resizes content height
    void layoutClips();
    void extendContentToMaxClip(); // computes content width (only width)
    void clampViewPosition();

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
    int  reorderInsertIndexFromY(int yInContent) const;
    void beginTrackReorder(int index);
    void updateTrackReorder(int yInContent);
    void endTrackReorder();

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
    void autoScrollDuringDrag();
    void updateActiveClipDragAt(juce::Point<int> pos, bool bypassSnap);
    double snapBeat(double beat, bool bypassSnap = false) const;
    double minimumClipLengthBeats(bool bypassSnap = false) const;
    void applyVerticalZoom(double wheelDelta, int anchorYContent, int anchorYInView);
    juce::Point<int> contentPointFromScreen(const juce::MouseEvent& e) const;
    juce::Point<int> viewPointFromScreen(const juce::MouseEvent& e) const;

    // model helpers
    int  trackIndexForClip(ClipModel& cm);
    ClipModel* moveClipToLane(ClipModel* cm, int fromLane, int toLane);
    void removeClip(ClipModel* clip);
    void addTrack(const juce::String& name = "Audio", bool push = true);
    void duplicateTrack(size_t idx);
    void removeTrackById(const juce::String& id);

    void   recomputeClipBeatLengthsForTempo();
    void   performSliceAtBeat(ClipComponent& cc, double beat);

    // buttons
    void buttonClicked(juce::Button*) override;
};
