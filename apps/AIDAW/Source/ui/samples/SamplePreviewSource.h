#pragma once

#include <JuceHeader.h>

namespace aidaw
{

class SamplePreviewSource final : public juce::AudioSource
{
public:
    SamplePreviewSource();
    ~SamplePreviewSource() override;

    void previewFile(const juce::File& file);
    void stop();

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::CriticalSection lock;
    juce::File currentFile;
    int64 readPosition { 0 };
    bool playing { false };

    double lastSampleRate { 44100.0 };
};

} // namespace aidaw
