#pragma once

#include <JuceHeader.h>

#include "../audio/EddieSynth.h"
#include "../audio/MetronomeSource.h"
#include "../audio/TimelineAudioSource.h"
#include "../ui/arranger/Arranger.h"
#include "../ui/midi/MidiEditor.h"
#include "../ui/mixer/MixerComponent.h"
#include "../ui/shell/TopBar.h"

namespace aidaw
{

class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent (juce::MixerAudioSource& mix, MetronomeSource& metro);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    void timerCallback() override;
    void setTransportPlaying (bool shouldPlay);
    void stopTransport();
    void openLoopsModal();

    TopBar topBar;
    Arranger arranger;
    MidiEditor midi;
    MixerComponent mixerUI;

    juce::MixerAudioSource& mixer;
    MetronomeSource& metronome;
    TimelineAudioSource timeline;
    EddieSynthAudioSource eddie;

    bool playing { false };
    bool recordArmed { false };
    double currentBpm { 120.0 };

    bool loopEnabled { true };
    double loopStartBeats { 0.0 };
    double loopLengthBeats { 4.0 };

    juce::TooltipWindow tooltip { this, 350 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace aidaw
