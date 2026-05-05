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

        if (file == currentFile && readerSource != nullptr)
        {
            readerSource->setNextReadPosition(0);
            if (resampler != nullptr)
                resampler->flushBuffers();
            playing = true;
            return;
        }

        resampler.reset();
        readerSource.reset();
        currentFile = {};
    }

    if (! file.existsAsFile())
        return;

    std::unique_ptr<juce::AudioFormatReader> newReader(formatManager.createReaderFor(file));
    if (newReader == nullptr)
        return;

    {
        const juce::ScopedLock sl(lock);
        const double sourceRate = newReader->sampleRate;
        readerSource = std::make_unique<juce::AudioFormatReaderSource>(newReader.release(), true);
        resampler = std::make_unique<juce::ResamplingAudioSource>(readerSource.get(), false, 2);
        resampler->setResamplingRatio(sourceRate > 0.0 && lastSampleRate > 0.0 ? sourceRate / lastSampleRate : 1.0);
        resampler->prepareToPlay(512, lastSampleRate);
        currentFile = file;
        playing = true;
    }
}

void SamplePreviewSource::stop()
{
    const juce::ScopedLock sl(lock);
    playing = false;
    currentFile = {};
}

void SamplePreviewSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected);
    const juce::ScopedLock sl(lock);
    lastSampleRate = sampleRate;
    if (resampler != nullptr && readerSource != nullptr)
    {
        resampler->setResamplingRatio(readerSource->getAudioFormatReader()->sampleRate / juce::jmax(1.0, lastSampleRate));
        resampler->prepareToPlay(samplesPerBlockExpected, lastSampleRate);
    }
}

void SamplePreviewSource::releaseResources()
{
    const juce::ScopedLock sl(lock);
    resampler.reset();
    readerSource.reset();
    playing = false;
    currentFile = {};
}

void SamplePreviewSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    const juce::ScopedLock sl(lock);
    if (! playing || readerSource == nullptr || resampler == nullptr)
        return;

    resampler->getNextAudioBlock(info);

    if (readerSource->getNextReadPosition() >= readerSource->getTotalLength())
    {
        playing = false;
        return;
    }
}

} // namespace aidaw
