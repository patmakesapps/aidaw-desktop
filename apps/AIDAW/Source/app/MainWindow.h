#pragma once

#include <JuceHeader.h>

#include "../audio/MetronomeSource.h"

namespace aidaw
{

class MainWindow : public juce::DocumentWindow,
                   private juce::KeyListener
{
public:
    MainWindow (juce::MixerAudioSource& mix, MetronomeSource& metro);

    void closeButtonPressed() override;

private:
    bool keyPressed (const juce::KeyPress& key, juce::Component*) override;
};

} // namespace aidaw
