#pragma once
#include <JuceHeader.h>

// Make sure this is the header that DEFINES ArrangerTool
#include "../shared/Models.h"
#include "../shared/ThumbCache.h"
#include "../shared/Theme.h"


class ClipComponent : public juce::Component,
                      public juce::ChangeListener,
                      private juce::Timer
{
public:
    ClipComponent(ClipModel& m,
                  juce::AudioFormatManager& fmIn,
                  juce::AudioThumbnailCache& cacheIn,
                  double& bpmRefIn,
                  std::function<void()> changedCB);
    ~ClipComponent() override;

    // Selection & tool state
    void setActiveTool(ArrangerTool t);
    void setSelected(bool on);
    void setDragPreview(bool on);

    // Resize handles
    juce::Rectangle<int> leftHandle()  const;
    juce::Rectangle<int> rightHandle() const;

    // JUCE overrides
    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    // Exposed model
    ClipModel& model;

private:
    void updateCursorForPoint(juce::Point<int> p);

    std::function<void()> onChanged;

    juce::AudioFormatManager& fm;
    double&                  bpmRef;   // project BPM (for beats -> seconds)
    juce::AudioThumbnail     thumb;

    bool          selected { false };
    bool          dragPreview { false };
    ArrangerTool  activeTool { ArrangerTool::Pointer };
    juce::Point<int> lastMousePos { 0, 0 };
    bool          savedOnce { false };
    float         spinnerAngle { 0.0f };
    juce::uint32  spinnerVisibleUntilMs { 0 };
};
