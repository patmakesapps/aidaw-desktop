#include "SamplePreviewSource.h"

namespace aidaw
{

SamplePreviewSource::SamplePreviewSource()
{
    formatManager.registerBasicFormats();
}

SamplePreviewSource::~SamplePreviewSource()
{
    stop();
}

void SamplePreviewSource::previewFile(const juce::File& file)
{
    {
        const juce::ScopedLock sl(lock);
        playing = false;
        readPosition = 0;

        if (file == currentFile && reader != nullptr)
        {
            playing = true;
            return;
        }

        reader.reset();
        currentFile = {};
    }

    if (! file.existsAsFile())
        return;

    std::unique_ptr<juce::AudioFormatReader> newReader(formatManager.createReaderFor(file));
    if (newReader == nullptr)
        return;

    {
        const juce::ScopedLock sl(lock);
        reader = std::move(newReader);
        currentFile = file;
        readPosition = 0;
        playing = true;
    }
}

void SamplePreviewSource::stop()
{
    const juce::ScopedLock sl(lock);
    playing = false;
    readPosition = 0;
    currentFile = {};
}

void SamplePreviewSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected);
    const juce::ScopedLock sl(lock);
    lastSampleRate = sampleRate;
}

void SamplePreviewSource::releaseResources()
{
    const juce::ScopedLock sl(lock);
    reader.reset();
    playing = false;
    readPosition = 0;
    currentFile = {};
}

void SamplePreviewSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    const juce::ScopedLock sl(lock);
    if (! playing || reader == nullptr)
        return;

    const auto samplesRemaining = reader->lengthInSamples - readPosition;
    const int samplesToRead = (int) juce::jmin<int64>(info.numSamples, juce::jmax<int64>(0, samplesRemaining));
    if (samplesToRead <= 0)
    {
        playing = false;
        return;
    }

    reader->read(info.buffer,
                 info.startSample,
                 samplesToRead,
                 readPosition,
                 true,
                 true);
    readPosition += samplesToRead;

    if (samplesToRead < info.numSamples)
        playing = false;
}

} // namespace aidaw
