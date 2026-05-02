#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

#include "../ui/shared/Models.h"

namespace aidaw
{

class TimelineAudioSource : public juce::AudioSource
{
public:
    TimelineAudioSource (std::vector<TrackModel>& tracksRef, double& bpmRefIn);

    void setPlaying (bool on);
    void resetToStart();
    void requestRebuild();

    double getPlayheadBeats() const;
    void setPlayheadBeats (double beats);

    void prepareToPlay (int samplesPerBlockExpected, double sampleRateIn) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;

private:
    void buildReaders();

    std::vector<TrackModel>& tracks;
    double& bpmRef;

    juce::AudioFormatManager fmt;
    double deviceSampleRate { 0.0 };

    std::atomic<bool> playing { false };
    std::atomic<bool> needsRebuild { true };
    std::atomic<int64> playheadSamples { 0 };
    std::atomic<double> pendingSeekBeats { 0.0 };

    std::vector<std::unique_ptr<juce::AudioFormatReaderSource>> sources;
    std::vector<std::unique_ptr<juce::ResamplingAudioSource>> resamplers;
    std::vector<double> resampleRatios;
    std::vector<ClipModel*> clipModels;

    juce::AudioBuffer<float> scratch;
};

} // namespace aidaw
