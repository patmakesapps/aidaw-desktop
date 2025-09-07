#pragma once
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>
#include <algorithm>

/* ---------- UTF-8 helper for symbols/emoji on Windows ---------- */
static inline juce::String U8(const char* s) {
    return juce::String(juce::CharPointer_UTF8(s));
}
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811
static inline juce::String U8(const char8_t* s) {
    return juce::String(juce::CharPointer_UTF8(reinterpret_cast<const char*>(s)));
}
#endif

/* ===================== Data ===================== */

struct ClipModel
{
    juce::String id;
    double startBeats  = 0.0;
    double lengthBeats = 4.0;
    juce::File file;
};

struct TrackModel
{
    juce::String id;
    juce::String name = "Audio";
    std::vector<ClipModel> clips;
};

enum class ArrangerTool { Pointer, Slice, Resize, Zoom };

/* ===================== Persistent thumbnail cache (on disk) ===================== */

struct ThumbPersistence
{
    static uint64_t fnv1a64(const void* data, size_t len)
    {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        uint64_t h = 1469598103934665603ull;
        const uint64_t prime = 1099511628211ull;
        for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= prime; }
        return h;
    }

    static juce::String hexFromU64(uint64_t v)
    {
        return juce::String::toHexString(&v, sizeof(v), 0);
    }

    static juce::File cacheDir()
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("AIDAW").getChildFile("ThumbCache");
        base.createDirectory();
        return base;
    }

    static juce::File cacheFileFor(const juce::File& audio)
    {
        const juce::String key = audio.getFullPathName() + "|" +
                                 juce::String(audio.getLastModificationTime().toMilliseconds()) + "|" +
                                 juce::String((juce::int64) audio.getSize());

        const char* bytes = key.toRawUTF8();
        const size_t len  = (size_t) key.getNumBytesAsUTF8() - 1;
        const uint64_t h  = fnv1a64(bytes, len);

        return cacheDir().getChildFile(hexFromU64(h) + ".athumb");
    }

    static bool load(juce::AudioThumbnail& thumb, const juce::File& audio)
    {
        auto f = cacheFileFor(audio);
        if (!f.existsAsFile()) return false;
        if (auto in = f.createInputStream())
            return thumb.loadFrom(*in);
        return false;
    }

    static void save(const juce::AudioThumbnail& thumb, const juce::File& audio)
    {
        auto f = cacheFileFor(audio);
        if (auto out = f.createOutputStream())
            thumb.saveTo(*out);
    }
};

/* ===================== Utility ===================== */

static juce::String ellipsizeToWidth(const juce::String& s, const juce::Font& f, int maxW)
{
    if (f.getStringWidth(s) <= maxW) return s;
    const juce::String dots("...");
    const int dotsW = f.getStringWidth(dots);
    juce::String out = s;
    while (out.isNotEmpty() && f.getStringWidth(out) + dotsW > maxW)
        out = out.dropLastCharacters(1);
    return out + dots;
}

/* ===================== UI: Clip ===================== */

class ClipComponent : public juce::Component,
                      private juce::ChangeListener
{
public:
    ClipComponent(ClipModel& m,
                  juce::AudioFormatManager& fmIn,
                  juce::AudioThumbnailCache& cacheIn,
                  std::function<void()> changedCB)
        : model(m),
          onChanged(std::move(changedCB)),
          fm(fmIn),
          thumb(512, fmIn, cacheIn)
    {
        setInterceptsMouseClicks(true, true);
        setWantsKeyboardFocus(false);
        setBufferedToImage(true);

        if (model.file.existsAsFile())
        {
            (void) ThumbPersistence::load(thumb, model.file);
            thumb.setSource(new juce::FileInputSource(model.file)); // takes ownership
        }

        thumb.addChangeListener(this);
    }

    ~ClipComponent() override { thumb.removeChangeListener(this); }

    void setActiveTool(ArrangerTool t) { activeTool = t; updateCursorForPoint(lastMousePos); }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // Body
        g.setColour(juce::Colour(0xFF373737));
        g.fillRoundedRectangle(r, 8.0f);

        auto inner = r.reduced(8.0f, 6.0f).toNearestInt();
        if (thumb.getTotalLength() > 0.0)
        {
            g.setColour(juce::Colour(0xFFB4B4B4));
            thumb.drawChannels(g, inner, 0.0, thumb.getTotalLength(), 1.0f);
        }
        else
        {
            g.setColour(juce::Colours::white);
            auto font = juce::Font(12.0f).boldened();
            g.setFont(font);
            auto base = model.file.existsAsFile()
                            ? model.file.getFileNameWithoutExtension()
                            : juce::String("Clip");
            const auto text = ellipsizeToWidth(base, font, getWidth() - 16);
            g.drawText(text, getLocalBounds().reduced(8, 6), juce::Justification::centredLeft, true);
        }

        // Border
        g.setColour(juce::Colour(0x66FFFFFF));
        g.drawRoundedRectangle(r, 8.0f, 1.0f);
    }

    juce::Rectangle<int> leftHandle()  const { return getLocalBounds().withWidth(6); }
    juce::Rectangle<int> rightHandle() const { return getLocalBounds().withX(getRight()-6).withWidth(6); }

    void mouseMove(const juce::MouseEvent& e) override
    {
        lastMousePos = e.getPosition();
        updateCursorForPoint(lastMousePos);
    }

    ClipModel& model;
    std::function<void()> onChanged;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        if (model.file.existsAsFile() && thumb.getTotalLength() > 0.0 && !savedOnce)
        {
            ThumbPersistence::save(thumb, model.file);
            savedOnce = true;
        }
        repaint();
        if (onChanged) onChanged();
    }

    void updateCursorForPoint(juce::Point<int> p)
    {
        using MC = juce::MouseCursor;

        if (activeTool == ArrangerTool::Resize)
        {
            setMouseCursor((leftHandle().contains(p) || rightHandle().contains(p))
                            ? MC::LeftRightResizeCursor
                            : MC::NormalCursor);
            return;
        }

        switch (activeTool)
        {
            case ArrangerTool::Pointer: setMouseCursor(MC::DraggingHandCursor);  break;
            case ArrangerTool::Slice:   setMouseCursor(MC::CrosshairCursor);     break;
            case ArrangerTool::Zoom:    setMouseCursor(MC::PointingHandCursor);  break; // portable stand-in
            case ArrangerTool::Resize:  break;
        }
    }

    juce::AudioFormatManager& fm;
    juce::AudioThumbnail       thumb;
    bool savedOnce { false };

    ArrangerTool   activeTool { ArrangerTool::Pointer };
    juce::Point<int> lastMousePos { -1, -1 };
};

/* ===================== UI: Track Lane ===================== */

class TrackLaneComponent : public juce::Component
{
public:
    TrackLaneComponent(TrackModel& m,
                       std::function<void(TrackModel&)> onSelect,
                       std::function<void(size_t)> onDuplicate,
                       std::function<void(TrackLaneComponent&, int yInContent)> onDragLaneCb,
                       std::function<void(TrackLaneComponent&)> onDragStartCb,
                       std::function<void(TrackLaneComponent&)> onDragEndCb,
                       size_t myIndex)
        : model(m),
          onSelected(std::move(onSelect)),
          onDuplicateLane(std::move(onDuplicate)),
          onDragLane(std::move(onDragLaneCb)),
          onDragStart(std::move(onDragStartCb)),
          onDragEnd(std::move(onDragEndCb)),
          index(myIndex)
    {
        title.setText(model.name, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centredLeft);
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        title.setEditable(true);
        title.setMinimumHorizontalScale(0.8f);
        title.onTextChange = [this]() { model.name = title.getText(); repaint(); };
        addAndMakeVisible(title);
    }

    void setSelected(bool on) { selected = on; repaint(); }
    bool isSelected() const { return selected; }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds();

        // Lane body
        g.setColour(juce::Colour(0xFF0F0F0F));
        g.fillRect(r);

        // Header block on the left
        auto header = r.removeFromLeft(headerWidth);
        g.setColour(selected ? juce::Colour(0xFF1E1E1E) : juce::Colour(0xFF141414));
        g.fillRect(header);

        // Header border + selection accent
        g.setColour(juce::Colour(0x22FFFFFF));
        g.drawRect(header);

        if (selected)
        {
            g.setColour(juce::Colour(0xFF3B82F6)); // stronger highlight
            g.drawRect(header, 2);
            g.drawRect(getLocalBounds(), 2);
        }

        // Row bottom hairline
        g.setColour(juce::Colour(0x11FFFFFF));
        g.fillRect(0, getHeight()-1, getWidth(), 1);
    }

    void resized() override
    {
        title.setBounds(8, 6, headerWidth - 16, getHeight() - 12);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const bool inHeader = e.position.x <= (float) headerWidth;
        if (onSelected) onSelected(model);
        draggingHeader = inHeader;   // reorder only when header grabbed
        if (draggingHeader && onDragStart) onDragStart(*this);

        // Duplicate on double-click header (or press 'D' hotkey in Arranger)
        if (e.getNumberOfClicks() >= 2 && inHeader && onDuplicateLane)
            onDuplicateLane(index);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!draggingHeader || onDragLane == nullptr) return;
        if (auto* parent = getParentComponent())
            onDragLane(*this, e.getEventRelativeTo(parent).getPosition().y);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (draggingHeader && onDragEnd) onDragEnd(*this);
        draggingHeader = false;
    }

    static constexpr int headerWidth = 180;

    TrackModel&      model;

private:
    juce::Label      title;

    std::function<void(TrackModel&)>                         onSelected;
    std::function<void(size_t)>                              onDuplicateLane;
    std::function<void(TrackLaneComponent&, int yInContent)> onDragLane;
    std::function<void(TrackLaneComponent&)>                 onDragStart;
    std::function<void(TrackLaneComponent&)>                 onDragEnd;

    size_t index { 0 };
    bool selected { false };
    bool draggingHeader { false };
};

/* ===================== UI: Arranger Canvas ===================== */

class ArrangerCanvas : public juce::Component
{
public:
    ArrangerCanvas(std::vector<TrackModel>& tracks, double& bpm, bool& snap, double& pxPerBeat,
                   std::function<void()> notifyChanged)
        : tracks(tracks), bpmRef(bpm), snapToGridRef(snap), pixelsPerBeatRef(pxPerBeat), notifyChanged(std::move(notifyChanged))
    {
        setInterceptsMouseClicks(true, true);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF0B0B0B));

        g.setColour(juce::Colour(0x22FFFFFF));
        g.fillRect(TrackLaneComponent::headerWidth, 0, 1, getHeight());

        const int rulerH = 20;
        auto full = getLocalBounds();
        auto ruler = full.removeFromTop(rulerH);
        juce::ignoreUnused(ruler);

        const int gridX0 = TrackLaneComponent::headerWidth;
        const double ppb = pixelsPerBeatRef;
        const int totalBeats = (int)std::ceil((getWidth() - gridX0) / ppb);

        for (int beat = 0; beat <= totalBeats; ++beat)
        {
            const int x = gridX0 + (int)std::round(beat * ppb);

            const bool isBar = (beat % 4) == 0;
            g.setColour(isBar ? juce::Colour(0x33FFFFFF) : juce::Colour(0x12FFFFFF));
            g.fillRect(x, rulerH, 1, getHeight() - rulerH);

            if (isBar)
            {
                g.setColour(juce::Colour(0x44FFFFFF));
                g.fillRect(x, 0, 1, rulerH);
                g.setColour(juce::Colour(0x66FFFFFF));
                g.setFont(11.0f);
                const int barIdx = (beat / 4) + 1;
                g.drawFittedText(juce::String(barIdx), x + 4, 2, 30, rulerH-4, juce::Justification::centredLeft, 1);
            }
        }

        if (ppb >= 80.0)
        {
            g.setColour(juce::Colour(0x0EFFFFFF));
            const int subDivs = (ppb >= 140.0) ? 4 : 2;
            for (int beat = 0; beat <= totalBeats; ++beat)
                for (int s = 1; s < subDivs; ++s)
                {
                    const double frac = (double)s / (double)subDivs;
                    const int x = gridX0 + (int)std::round((beat + frac) * ppb);
                    g.fillRect(x, 20, 1, getHeight()-20);
                }
        }
    }

    int xFromBeats(double beats) const
    {
        return TrackLaneComponent::headerWidth + (int)std::round(beats * pixelsPerBeatRef);
    }
    double beatsFromX(int x) const
    {
        return juce::jmax(0, x - TrackLaneComponent::headerWidth) / pixelsPerBeatRef;
    }

private:
    std::vector<TrackModel>& tracks;
    double& bpmRef;
    bool&   snapToGridRef;
    double& pixelsPerBeatRef;
    std::function<void()> notifyChanged;
};

/* ===================== Arranger (viewport + lanes + tools) ===================== */

class Arranger  : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  private juce::Button::Listener
{
public:
    Arranger()
        : cache(128)
    {
        setWantsKeyboardFocus(true);           // for hotkeys & Delete
        fm.registerBasicFormats();

        // Tool buttons (emoji/symbols – UTF-8 + font set in Main.cpp)
        btnPointer.setButtonText(U8(u8"⬚"));
        btnSlice.setButtonText  (U8(u8"✂"));
        btnResize.setButtonText (U8(u8"↔"));
        btnZoomTool.setButtonText(U8(u8"🔍"));
        btnFrameAll.setButtonText(U8(u8"◰"));
        btnSnap.setButtonText   (U8(u8"⛓"));

        // Tooltips
        btnPointer.setTooltip("Pointer (1) – move/select");
        btnSlice.setTooltip("Slice (2) – cut clip" "\n" "Grid on: cut to nearest beat");
        btnResize.setTooltip("Resize (3) – drag clip edges");
        btnZoomTool.setTooltip("Zoom (4) – drag: L=in / R=out");
        btnFrameAll.setTooltip("Frame all (F)");
        btnSnap.setTooltip("Snap to grid (G)");

        // Visual on/off & toggles
        for (auto* b : { &btnPointer, &btnSlice, &btnResize, &btnZoomTool })
            b->setClickingTogglesState(true);
        btnSnap.setClickingTogglesState(true);
        btnSnap.setToggleState(true, juce::dontSendNotification);

        for (auto* b : { &btnPointer, &btnSlice, &btnResize, &btnZoomTool, &btnFrameAll, &btnSnap })
        {
            b->addListener(this);
            addAndMakeVisible(b);
        }
        setTool(ArrangerTool::Pointer);

        // Zoom +/- controls
        btnZoomIn.setButtonText("+");
        btnZoomOut.setButtonText("-");
        btnZoomIn.setTooltip("Zoom in (+)");
        btnZoomOut.setTooltip("Zoom out (-)");
        for (auto* b : { &btnZoomOut, &btnZoomIn })
        {
            b->addListener(this);
            addAndMakeVisible(b);
        }

        addAndMakeVisible(view);
        view.setViewedComponent(&content, false);
        content.addAndMakeVisible(canvas);

        setBPM(120.0);
        setSnap(true);

        refreshAll();
        applyCursorForToolEverywhere();
    }

    ~Arranger() override
    {
        for (auto* b : { &btnPointer, &btnSlice, &btnResize, &btnZoomTool, &btnFrameAll, &btnSnap, &btnZoomIn, &btnZoomOut })
            b->removeListener(this);
    }

    /* --------- External API / hooks --------- */

    // Callbacks for transport (wired by Main.cpp). 'play' should restart if at end.
    std::function<void()> onPlayPressed;
    std::function<void()> onStopPressed;

    void setBPM(double bpm)
    {
        const double old = bpmValue;
        bpmValue = juce::jlimit(40.0, 300.0, bpm);
        pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;

        if (std::abs(old - bpmValue) > 1e-6) { pushUndo(); recomputeClipBeatLengthsForTempo(); }

        layoutClips();
        repaint();
    }
    double getBPM() const { return bpmValue; }

    void setSnap(bool on)
    {
        snapToGrid = on;
        btnSnap.setToggleState(on, juce::dontSendNotification);
        repaint();
    }

    void setTool(ArrangerTool t)
    {
        tool = t;
        auto mark = [&](juce::TextButton& b, bool on)
        {
            b.setToggleState(on, juce::dontSendNotification);
            b.setColour(juce::TextButton::buttonColourId, on ? activeCol : idleCol);
            b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            b.repaint();
        };
        mark(btnPointer, t == ArrangerTool::Pointer);
        mark(btnSlice,   t == ArrangerTool::Slice);
        mark(btnResize,  t == ArrangerTool::Resize);
        mark(btnZoomTool,t == ArrangerTool::Zoom);

        applyCursorForToolEverywhere();
        repaint();
    }

    std::function<void()> onProjectChanged;
    std::vector<TrackModel> tracks;

    /* --------- File drag/drop --------- */
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    { juce::ignoreUnused(files); return true; }

    void filesDropped(const juce::StringArray& files, int x, int y) override
    {
        if (files.isEmpty()) return;
        pushUndo();

        const int contentX = x + view.getViewPositionX();
        const double dropBeats = canvas.beatsFromX(contentX);
        const double startBeats = snapToGrid ? std::round(dropBeats) : dropBeats;

        const int yContent = y + view.getViewPositionY();
        int laneIdx = laneIndexFromYAllowNew(yContent);

        for (int i = 0; i < files.size(); ++i)
        {
            const juce::File f(files[i]);
            if (!f.existsAsFile()) continue;

            while (laneIdx >= (int)tracks.size())
                addTrack("Audio " + juce::String((int)tracks.size()+1), /*push=*/false);

            ClipModel clip;
            clip.id = juce::Uuid().toString();
            clip.file = f;
            clip.startBeats = startBeats;
            clip.lengthBeats = estimateBeatsFromFile(f, bpmValue);

            auto& lane = tracks[(size_t)laneIdx];
            auto name = extractTrackNameFromAudio(f);
            lane.name = name.isNotEmpty() ? name : f.getFileNameWithoutExtension();
            lane.clips.push_back(std::move(clip));

            ++laneIdx;
        }

        refreshAll();
        if (onProjectChanged) onProjectChanged();
    }

    /* --------- Component plumbing --------- */

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colour(0x22FFFFFF));
        g.drawRect(getLocalBounds());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8, 6);

        auto tools = r.removeFromTop(34);
        btnPointer .setBounds(tools.removeFromLeft(40));
        tools.removeFromLeft(6);
        btnSlice   .setBounds(tools.removeFromLeft(40));
        tools.removeFromLeft(6);
        btnResize  .setBounds(tools.removeFromLeft(40));
        tools.removeFromLeft(6);
        btnZoomTool.setBounds(tools.removeFromLeft(40));
        tools.removeFromLeft(6);
        btnFrameAll.setBounds(tools.removeFromLeft(40));
        tools.removeFromLeft(12);
        btnSnap    .setBounds(tools.removeFromLeft(56));
        tools.removeFromLeft(12);
        btnZoomOut .setBounds(tools.removeFromLeft(36));
        tools.removeFromLeft(4);
        btnZoomIn  .setBounds(tools.removeFromLeft(36));

        view.setBounds(r);
        layoutLanes();
        layoutClips();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        const bool ctrl = key.getModifiers().isCommandDown();

        // Transport
        if (key.getKeyCode() == juce::KeyPress::spaceKey) { if (onPlayPressed) onPlayPressed(); return true; }

        // Undo / Redo
        if (ctrl && (key.getTextCharacter() == 'z' || key.getTextCharacter() == 'Z'))
        {
            if (key.getModifiers().isShiftDown()) redo(); else undo();
            return true;
        }

        // Tools
        if (key.getTextCharacter() == '1') { setTool(ArrangerTool::Pointer); return true; }
        if (key.getTextCharacter() == '2') { setTool(ArrangerTool::Slice);   return true; }
        if (key.getTextCharacter() == '3') { setTool(ArrangerTool::Resize);  return true; }
        if (key.getTextCharacter() == '4') { setTool(ArrangerTool::Zoom);    return true; }

        // Grid snap toggle
        if (key.getTextCharacter() == 'g' || key.getTextCharacter() == 'G')
        { setSnap(!snapToGrid); return true; }

        // Frame all
        if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
        { frameAll(); return true; }

        // Zoom +/- keys
        if (key.getTextCharacter() == '+') { zoomDelta(+1); return true; }
        if (key.getTextCharacter() == '-') { zoomDelta(-1); return true; }

        // Duplicate & Delete
        if (key.getTextCharacter() == 'd' || key.getTextCharacter() == 'D')
        {
            if (selectedLaneIndex >= 0) { duplicateTrack((size_t)selectedLaneIndex); return true; }
        }

        if (key.getKeyCode() == juce::KeyPress::deleteKey)
        {
            if (selectedLaneIndex >= 0 && selectedLaneIndex < (int)tracks.size())
            {
                auto id = tracks[(size_t)selectedLaneIndex].id;
                removeTrackById(id);
                setSelectedLane(-1);
                return true;
            }
        }
        return false;
    }

    /* --------- Public helpers --------- */

    void addTrack(const juce::String& name = "Audio", bool push = true)
    {
        if (push) pushUndo();
        TrackModel t;
        t.id = juce::Uuid().toString();
        t.name = name;
        tracks.emplace_back(std::move(t));
        refreshAll();
    }

    void duplicateTrack(size_t idx)
    {
        if (idx >= tracks.size()) return;
        pushUndo();
        TrackModel copy = tracks[idx];
        copy.id = juce::Uuid().toString();
        tracks.insert(tracks.begin() + (ptrdiff_t)idx + 1, std::move(copy));
        refreshAll();
        setSelectedLane((int)idx + 1);
    }

    void removeTrackById(const juce::String& id)
    {
        pushUndo();
        const auto it = std::find_if(tracks.begin(), tracks.end(),
            [&](const TrackModel& t){ return t.id == id; });
        if (it != tracks.end()) tracks.erase(it);
        refreshAll();
    }

    // zoom deltas (+/-), optional cursor focus
    void zoomDelta(double delta, int centerXInContent = -1)
    {
        zoomScale = juce::jlimit(0.25, 6.0, zoomScale * (delta > 0 ? 1.15 : 0.87));
        pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;

        if (centerXInContent >= 0)
        {
            const double beatAtCursor = canvas.beatsFromX(centerXInContent);
            const int newX = canvas.xFromBeats(beatAtCursor);
            const int dx = newX - centerXInContent;
            auto p = view.getViewPosition();
            view.setViewPosition(p.getX() + dx, p.getY());
        }

        layoutClips();
        extendContentToMaxClip();
        repaint();
    }

    void zoomDeltaFromWheel(double wheelDelta, int centerXInScreen)
    {
        auto centerInContent = centerXInScreen + view.getViewPositionX();
        zoomDelta(wheelDelta, centerInContent);
    }

    void frameAll()
    {
        double maxEndBeats = 64.0;
        for (auto& t : tracks)
            for (auto& c : t.clips)
                maxEndBeats = std::max(maxEndBeats, c.startBeats + c.lengthBeats);

        const int viewportW = juce::jmax(1, view.getWidth() - TrackLaneComponent::headerWidth - 40);
        const double ppbNow = juce::jlimit(10.0, 300.0, (double)viewportW / juce::jmax(8.0, maxEndBeats));

        ppbAt120      = ppbNow * (bpmValue / 120.0);
        zoomScale     = 1.0;
        pixelsPerBeat = ppbAt120 * (120.0 / bpmValue) * zoomScale;

        extendContentToMaxClip();
        layoutClips();
        view.setViewPosition(0, view.getViewPositionY());
        repaint();
    }

private:
    /* ---- internals ---- */

    class ContentComp : public juce::Component
    {
        void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF0A0A0A)); }
    } content;

public:
    ArrangerCanvas canvas { tracks, bpmValue, snapToGrid, pixelsPerBeat, [this]{ if(onProjectChanged) onProjectChanged(); } };

private:
    juce::Viewport view;

    // tool row
    juce::TextButton btnPointer, btnSlice, btnResize, btnZoomTool, btnFrameAll, btnSnap, btnZoomIn, btnZoomOut;
    ArrangerTool tool { ArrangerTool::Pointer };
    juce::Colour activeCol = juce::Colour(0xFF3B82F6), idleCol = juce::Colour(0xFF2A2A2A);

    // timing + grid
    double bpmValue { 120.0 };
    bool   snapToGrid { true };
    double ppbAt120 { 52.0 };
    double pixelsPerBeat { 52.0 };
    double zoomScale { 1.0 };

    // audio utils
    juce::AudioFormatManager fm;
    juce::AudioThumbnailCache cache { 128 };

    // lanes/clips
    std::vector<std::unique_ptr<TrackLaneComponent>> laneComps;
    std::vector<std::unique_ptr<ClipComponent>>      clipComps;

    // selection & dragging
    int  selectedLaneIndex { -1 };
    enum class DragMode { None, Move, Left, Right };
    DragMode dragging { DragMode::None };
    ClipComponent* activeClip { nullptr };
    juce::Point<int> dragStartPos;
    double clipStartBeatsAtDown { 0.0 }, clipLenBeatsAtDown { 0.0 };
    int startLaneIndex { -1 };
    int targetLaneIndex { -1 };

    // zoom + middle mouse pan
    bool zoomDragActive { false };
    int  zoomDragStartXContent { -1 };
    bool panActive { false };
    juce::Point<int> panStartMouse;
    juce::Point<int> panStartView;

    // lane reorder
    int draggingLaneIndex { -1 };

    // simple undo/redo stacks (snapshot based)
    std::vector<std::vector<TrackModel>> undoStack, redoStack;

    /* ---- helpers ---- */

    void pushUndo()
    {
        undoStack.push_back(tracks);
        redoStack.clear();
    }
    void undo()
    {
        if (undoStack.empty()) return;
        redoStack.push_back(tracks);
        tracks = std::move(undoStack.back());
        undoStack.pop_back();
        refreshAll();
    }
    void redo()
    {
        if (redoStack.empty()) return;
        undoStack.push_back(tracks);
        tracks = std::move(redoStack.back());
        redoStack.pop_back();
        refreshAll();
    }

    void applyCursorForToolEverywhere()
    {
        using MC = juce::MouseCursor;
        MC c = MC::NormalCursor;

        switch (tool)
        {
            case ArrangerTool::Pointer: c = MC::DraggingHandCursor;    break;
            case ArrangerTool::Slice:   c = MC::CrosshairCursor;       break;
            case ArrangerTool::Resize:  c = MC::LeftRightResizeCursor; break;
            case ArrangerTool::Zoom:    c = MC::PointingHandCursor;    break; // stand-in
        }

        setMouseCursor(c);
        view.setMouseCursor(c);
        canvas.setMouseCursor(c);
        for (auto& up : clipComps) up->setActiveTool(tool);
    }

    void refreshAll()
    {
        for (auto& lc : laneComps) content.removeChildComponent(lc.get());
        laneComps.clear();

        for (size_t i=0;i<tracks.size();++i)
        {
            auto& tm = tracks[i];
            auto lane = std::make_unique<TrackLaneComponent>(
                tm,
[this, i](TrackModel&) { setSelectedLane((int)i); },

                [this](size_t idx){ duplicateTrack(idx); },
                [this](TrackLaneComponent&, int yInContent)
                {
                    const int idx = laneIndexFromY(yInContent);
                    if (draggingLaneIndex >= 0 && idx != draggingLaneIndex)
                    {
                        std::swap(tracks[(size_t)draggingLaneIndex], tracks[(size_t)idx]);
                        draggingLaneIndex = idx;
                        refreshAll();
                    }
                },
                [this, i](TrackLaneComponent&)
                {
                    draggingLaneIndex = (int)i;
                },
                [this](TrackLaneComponent&)
                {
                    draggingLaneIndex = -1;
                    refreshAll();
                },
                i
            );
            
            lane->setSelected((int)i == selectedLaneIndex);
            content.addAndMakeVisible(lane.get());
            laneComps.emplace_back(std::move(lane));
        }

        for (auto& cc : clipComps) content.removeChildComponent(cc.get());
        clipComps.clear();

        for (auto& t : tracks)
        {
            for (auto& c : t.clips)
            {
                auto* cc = new ClipComponent(c, fm, cache,
                    [this]{ extendContentToMaxClip(); if(onProjectChanged) onProjectChanged(); });
                cc->addMouseListener(this, true);
                content.addAndMakeVisible(cc);
                clipComps.emplace_back(cc);
                clipComps.back()->setActiveTool(tool); // keep cursors in sync
            }
        }

        content.addAndMakeVisible(canvas);
        canvas.toBack();

        layoutLanes();
        layoutClips();
        extendContentToMaxClip();
        repaint();
    }

    void setSelectedLane(int idx)
    {
        selectedLaneIndex = (idx >= 0 && idx < (int)tracks.size()) ? idx : -1;
        for (int i=0;i<(int)laneComps.size();++i)
            laneComps[(size_t)i]->setSelected(i == selectedLaneIndex);
        grabKeyboardFocus();
        repaint();
    }

    int laneIndexFromY(int yInContent) const
    {
        const int rulerH = 20;
        int y = yInContent - rulerH;
        int laneH = laneHeight();
        if (y < laneTop()) return 0;
        int idx = (y - laneTop()) / laneH;
        if (tracks.empty()) return 0;
        return juce::jlimit(0, (int)tracks.size()-1, idx);
    }

    int laneIndexFromYAllowNew(int yInContent) const
    {
        const int rulerH = 20;
        int y = yInContent - rulerH;
        int laneH = laneHeight();
        if (y < laneTop()) return 0;
        if (tracks.empty()) return 0;
        const int idx = (y - laneTop()) / laneH;
        return juce::jlimit(0, (int)tracks.size(), idx);
    }

    int laneTop() const    { return 20; }
    int laneHeight() const { return 76; }

    void layoutLanes()
    {
        canvas.setBounds(0, 0, juce::jmax(2000, content.getWidth()), juce::jmax(600, content.getHeight()));

        int y = laneTop();
        int w = juce::jmax(2000, getWidth());

        for (auto& lc : laneComps)
        {
            lc->setBounds(0, y, w, laneHeight());
            y += laneHeight();
        }

        content.setSize(juce::jmax(w, getWidth()*2), juce::jmax(y+120, getHeight()*2));
    }

    void layoutClips()
    {
        const int laneH = laneHeight();
        const int y0 = laneTop();

        auto yForTrack = [&](TrackModel* tPtr)
        {
            int laneIndex = 0;
            for (size_t i=0;i<tracks.size();++i) if (&tracks[i] == tPtr) { laneIndex = (int)i; break; }
            return y0 + laneIndex * laneH;
        };

        for (auto& ccUP : clipComps)
        {
            auto& cc = *ccUP;
            TrackModel* track = nullptr; ClipModel* cm = nullptr;
            for (auto& t : tracks)
            {
                for (auto& c : t.clips)
                    if (&c == &cc.model) { track = &t; cm = &c; break; }
                if (cm) break;
            }
            if (!cm || !track) continue;

            const int x = canvas.xFromBeats(cm->startBeats);
            const int w = (int)std::round(cm->lengthBeats * pixelsPerBeat);
            const int y = yForTrack(track);
            cc.setBounds(x, y + 10, juce::jmax(24, w), laneH - 20);
        }
    }

    void extendContentToMaxClip()
    {
        double maxEndBeats = 64.0;
        for (auto& t : tracks)
            for (auto& c : t.clips)
                maxEndBeats = std::max(maxEndBeats, c.startBeats + c.lengthBeats);

        const int neededW = TrackLaneComponent::headerWidth + (int)std::ceil(maxEndBeats * pixelsPerBeat) + 400;
        content.setSize(juce::jmax(neededW, content.getWidth()), content.getHeight());
        canvas.setBounds(0, 0, content.getWidth(), content.getHeight());
        repaint();
    }

    /* ---- Mouse routing on clips (move / slice / resize / zoom + middle-pan) ---- */

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Middle mouse: begin panning regardless of tool
        if (e.mods.isMiddleButtonDown())
        {
            panActive = true;
            panStartMouse = e.getEventRelativeTo(this).getPosition();
            panStartView = view.getViewPosition();
            return;
        }

        const bool rightClick = e.mods.isRightButtonDown();

        if (tool == ArrangerTool::Zoom)
        {
            zoomDragActive = true;
            zoomDragStartXContent = e.getEventRelativeTo(&content).x;
            zoomDelta(rightClick ? -1 : +1, zoomDragStartXContent);
            return;
        }

        if (auto* cc = dynamic_cast<ClipComponent*>(e.eventComponent))
        {
            activeClip = cc;
            setSelectedLane(trackIndexForClip(cc->model));

            startLaneIndex = trackIndexForClip(cc->model);
            targetLaneIndex = startLaneIndex;

            pushUndo(); // start an edit transaction

            if (tool == ArrangerTool::Resize)
            {
                if (cc->leftHandle().contains(e.getEventRelativeTo(cc).getPosition()))
                    dragging = DragMode::Left;
                else if (cc->rightHandle().contains(e.getEventRelativeTo(cc).getPosition()))
                    dragging = DragMode::Right;
                else
                    dragging = DragMode::None;
            }
            else if (tool == ArrangerTool::Slice)
            {
                auto beat = canvas.beatsFromX(e.getEventRelativeTo(&content).x);
                if (snapToGrid) beat = std::round(beat); // cut to grid
                performSliceAtBeat(*cc, beat);
                return;
            }
            else
            {
                dragging = DragMode::Move;
            }

            dragStartPos = e.getEventRelativeTo(&content).getPosition();
            clipStartBeatsAtDown = cc->model.startBeats;
            clipLenBeatsAtDown   = cc->model.lengthBeats;
        }
        else
        {
            setSelectedLane(-1); // clicked background
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        // Middle mouse pan
        if (panActive)
        {
            auto cur = e.getEventRelativeTo(this).getPosition();
            auto delta = cur - panStartMouse;
            view.setViewPosition(juce::jmax(0, panStartView.x - delta.x),
                                 juce::jmax(0, panStartView.y - delta.y));
            return;
        }

        if (zoomDragActive && tool == ArrangerTool::Zoom)
        {
            const bool rightClick = e.mods.isRightButtonDown();
            const int xContent = e.getEventRelativeTo(&content).x;
            const int dx = xContent - zoomDragStartXContent;
            if (std::abs(dx) > 1)
            {
                const double steps = (double)dx / 24.0;
                zoomDelta(rightClick ? -steps : +steps, xContent);
                zoomDragStartXContent = xContent;
            }
            return;
        }

        if (!activeClip) return;

        const auto pos = e.getEventRelativeTo(&content).getPosition();
        const int dxPx = pos.x - dragStartPos.x;
        const double dxBeats = dxPx / pixelsPerBeat;

        auto& cm = activeClip->model;

        if (dragging == DragMode::Move)
        {
            double nb = clipStartBeatsAtDown + dxBeats;
            if (snapToGrid) nb = std::round(nb);
            cm.startBeats = juce::jlimit(0.0, 100000.0, nb);

            int hoverLane = laneIndexFromYAllowNew(pos.y);
            targetLaneIndex = juce::jlimit(0, (int)tracks.size(), hoverLane);

            const int laneH = laneHeight();
            const int y0    = laneTop();
            const int previewLaneY = y0 + juce::jmin(targetLaneIndex, (int)tracks.size()-1) * laneH + 10;
            const int w     = (int)std::round(cm.lengthBeats * pixelsPerBeat);
            const int x     = canvas.xFromBeats(cm.startBeats);
            activeClip->setBounds(x, previewLaneY, juce::jmax(24, w), laneH - 20);
        }
        else if (dragging == DragMode::Left)
        {
            double newStart = clipStartBeatsAtDown + dxBeats;
            double newLen   = clipLenBeatsAtDown - dxBeats;
            if (snapToGrid) { newStart = std::round(newStart); newLen = std::round(newLen); }
            newStart = juce::jmax(0.0, newStart);
            newLen   = juce::jmax(1.0, newLen);
            cm.startBeats  = newStart;
            cm.lengthBeats = newLen;
        }
        else if (dragging == DragMode::Right)
        {
            double newLen = clipLenBeatsAtDown + dxBeats;
            if (snapToGrid) newLen = std::round(newLen);
            cm.lengthBeats = juce::jmax(1.0, newLen);
        }

        extendContentToMaxClip();
        if (onProjectChanged) onProjectChanged();
        if (dragging != DragMode::Move) layoutClips();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        panActive = false;
        zoomDragActive = false;

        if (activeClip)
        {
            if (dragging == DragMode::Move && targetLaneIndex >= 0)
            {
                if (targetLaneIndex == (int)tracks.size())
                    addTrack("Audio " + juce::String((int)tracks.size()+1));

                if (targetLaneIndex != startLaneIndex)
                    moveClipToLane(activeClip->model, startLaneIndex, targetLaneIndex);

                setSelectedLane(juce::jmin(targetLaneIndex, (int)tracks.size()-1));
            }
        }

        activeClip = nullptr;
        dragging = DragMode::None;
        startLaneIndex = targetLaneIndex = -1;

        layoutClips();
    }

    /* ---- model helpers ---- */

    int trackIndexForClip(ClipModel& cm)
    {
        for (size_t i=0;i<tracks.size();++i)
            for (auto& c : tracks[i].clips)
                if (&c == &cm) return (int)i;
        return 0;
    }

    void moveClipToLane(ClipModel& cm, int fromLane, int toLane)
    {
        if (fromLane < 0 || toLane < 0 || fromLane >= (int)tracks.size() || toLane >= (int)tracks.size()) return;
        if (fromLane == toLane) return;

        auto& src = tracks[(size_t)fromLane].clips;
        auto it = std::find_if(src.begin(), src.end(), [&](const ClipModel& c){ return &c == &cm; });
        if (it == src.end()) return;

        ClipModel moved = *it;
        src.erase(it);
        tracks[(size_t)toLane].clips.push_back(std::move(moved));

        refreshAll();
        if (onProjectChanged) onProjectChanged();
    }

    juce::String extractTrackNameFromAudio(const juce::File& f)
    {
        std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
        if (r)
        {
            auto& meta = r->metadataValues;
            for (auto key : { "title", "artist", "name" })
                if (meta.containsKey(key))
                    return meta[key];
        }
        return {};
    }

    double estimateBeatsFromFile(const juce::File& f, double bpm)
    {
        std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
        if (r && r->sampleRate > 0.0)
        {
            const double secs  = (double)r->lengthInSamples / r->sampleRate;
            const double beats = (secs * bpm) / 60.0;
            double bars = std::max(1.0, std::round(beats / 4.0));
            return bars * 4.0;
        }
        return 16.0;
    }

    double detectLoopBpm(const juce::File& f)
    {
        std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
        if (r)
        {
            auto& meta = r->metadataValues;
            for (auto key : { "tempo", "bpm", "BPM", "Tempo" })
                if (meta.containsKey(key))
                {
                    auto v = meta[key].retainCharacters("0123456789.");
                    auto d = v.getDoubleValue();
                    if (d > 20.0 && d < 300.0) return d;
                }
        }
        auto name = f.getFileNameWithoutExtension().toLowerCase();
        juce::String digits;
        for (int i=0;i<name.length();++i)
        {
            const juce_wchar ch = name[i];
            if (juce::CharacterFunctions::isDigit(ch) || ch == '.')
                digits << juce::String::charToString(ch);
            else
            {
                if (digits.length() >= 2)
                {
                    double val = digits.getDoubleValue();
                    if (val > 20.0 && val < 300.0) return val;
                }
                digits.clear();
            }
        }
        if (digits.isNotEmpty())
        {
            double val = digits.getDoubleValue();
            if (val > 20.0 && val < 300.0) return val;
        }
        return 0.0;
    }

    double secondsOfFile(const juce::File& f)
    {
        std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
        if (r && r->sampleRate > 0.0)
            return (double) r->lengthInSamples / r->sampleRate;
        return 0.0;
    }

    void recomputeClipBeatLengthsForTempo()
    {
        for (auto& t : tracks)
            for (auto& c : t.clips)
            {
                if (!c.file.existsAsFile()) continue;

                const double secs = secondsOfFile(c.file);
                if (secs <= 0.0) continue;

                double srcBpm = detectLoopBpm(c.file);
                if (srcBpm <= 0.0) srcBpm = bpmValue;

                const double beats = (secs * srcBpm) / 60.0;

                double rounded = std::round(beats * 4.0) / 4.0;  // ~16th grid
                const double bars = rounded / 4.0;
                const double nearestCommon = std::round(bars);
                if (std::abs(nearestCommon - bars) < 0.08)
                    rounded = nearestCommon * 4.0;

                c.lengthBeats = juce::jmax(1.0, rounded);
            }

        layoutClips();
        extendContentToMaxClip();
    }

    // Slice helper
    void performSliceAtBeat(ClipComponent& cc, double beat)
    {
        ClipModel& clip = cc.model;
        const double clipStart = clip.startBeats;
        const double clipEnd   = clip.startBeats + clip.lengthBeats;

        if (beat <= clipStart || beat >= clipEnd) return;

        for (auto& t : tracks)
        {
            for (auto it = t.clips.begin(); it != t.clips.end(); ++it)
            {
                if (&(*it) == &clip)
                {
                    const double leftLen  = beat - clipStart;
                    const double rightLen = clipEnd - beat;

                    it->lengthBeats = leftLen;

                    ClipModel right;
                    right.id          = juce::Uuid().toString();
                    right.file        = it->file;
                    right.startBeats  = beat;
                    right.lengthBeats = rightLen;

                    t.clips.insert(it + 1, std::move(right));

                    refreshAll();
                    if (onProjectChanged) onProjectChanged();
                    return;
                }
            }
        }
    }

    /* ---- buttons ---- */
    void buttonClicked(juce::Button* b) override
    {
        if (b == &btnPointer)      setTool(ArrangerTool::Pointer);
        else if (b == &btnSlice)   setTool(btnSlice.getToggleState() && tool!=ArrangerTool::Slice ? ArrangerTool::Slice : ArrangerTool::Pointer);
        else if (b == &btnResize)  setTool(btnResize.getToggleState() && tool!=ArrangerTool::Resize? ArrangerTool::Resize: ArrangerTool::Pointer);
        else if (b == &btnZoomTool)setTool(btnZoomTool.getToggleState()&& tool!=ArrangerTool::Zoom  ? ArrangerTool::Zoom  : ArrangerTool::Pointer);
        else if (b == &btnFrameAll)frameAll();
        else if (b == &btnSnap)    setSnap(btnSnap.getToggleState());
        else if (b == &btnZoomIn)  zoomDelta(+1);
        else if (b == &btnZoomOut) zoomDelta(-1);
    }
};
