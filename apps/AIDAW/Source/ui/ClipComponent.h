#pragma once
#include <JuceHeader.h>
#include "Models.h"
#include "ThumbCache.h"

class ClipComponent : public juce::Component,
                      private juce::ChangeListener
{
public:
    ClipComponent(ClipModel& m,
                  juce::AudioFormatManager& fmIn,
                  juce::AudioThumbnailCache& cacheIn,
                  std::function<void()> changedCB);

    ~ClipComponent() override;

    void setActiveTool(ArrangerTool t);
    void setSelected(bool on);
    juce::Rectangle<int> leftHandle()  const;
    juce::Rectangle<int> rightHandle() const;

    ClipModel& model;
    std::function<void()> onChanged;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void updateCursorForPoint(juce::Point<int> p);

    juce::AudioFormatManager& fm;
    juce::AudioThumbnail       thumb;
    bool savedOnce { false };

    ArrangerTool   activeTool { ArrangerTool::Pointer };
    juce::Point<int> lastMousePos { -1, -1 };
    bool selected { false };
};
