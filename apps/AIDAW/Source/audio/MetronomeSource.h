#pragma once

#include <JuceHeader.h>

namespace aidaw
{

class MetronomeSource : public juce::AudioSource
{
public:
    void prepareToPlay (int samplesPerBlockExpected, double sampleRateIn) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

    void setBpm (double bpmIn);
    void setPlaying (bool isOn);
    void setClickEnabled (bool on);
    void reset();

private:
    void updateTiming();

    double sampleRate { 0.0 };
    double bpm { 120.0 };
    double samplesPerBeat { 0.0 };
    bool playing { false };
    bool clickEnabled { true };
    int samplesUntilTick { 0 };
    int burstSamplesRemaining { 0 };
    double phase { 0.0 };
};

} // namespace aidaw
