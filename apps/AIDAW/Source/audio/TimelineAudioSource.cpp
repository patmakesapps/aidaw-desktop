#include "TimelineAudioSource.h"

#include <algorithm>
#include <cmath>

namespace aidaw
{

TimelineAudioSource::TimelineAudioSource (std::vector<TrackModel>& tracksRef, double& bpmRefIn)
    : tracks (tracksRef), bpmRef (bpmRefIn)
{
    fmt.registerBasicFormats();
}

void TimelineAudioSource::setPlaying (bool on)
{
    playing.store (on);
}

void TimelineAudioSource::resetToStart()
{
    playheadSamples.store (0);
}

void TimelineAudioSource::requestRebuild()
{
    needsRebuild.store (true);
}

double TimelineAudioSource::getPlayheadBeats() const
{
    if (deviceSampleRate <= 0.0 || bpmRef <= 0.0)
        return 0.0;

    const double secPerBeat = 60.0 / bpmRef;
    return (double) playheadSamples.load() / deviceSampleRate / secPerBeat;
}

void TimelineAudioSource::setPlayheadBeats (double beats)
{
    if (deviceSampleRate <= 0.0 || bpmRef <= 0.0)
    {
        pendingSeekBeats.store (juce::jmax (0.0, beats));
        return;
    }

    const double secPerBeat = 60.0 / bpmRef;
    const auto target = (int64) std::llround (juce::jmax (0.0, beats) * secPerBeat * deviceSampleRate);
    playheadSamples.store (target);
}

void TimelineAudioSource::prepareToPlay (int, double sampleRateIn)
{
    deviceSampleRate = sampleRateIn;
    buildReaders();

    if (pendingSeekBeats.load() > 0.0)
    {
        setPlayheadBeats (pendingSeekBeats.load());
        pendingSeekBeats.store (0.0);
    }
}

void TimelineAudioSource::releaseResources()
{
    resamplers.clear();
    sources.clear();
    clipModels.clear();
    resampleRatios.clear();
    scratch.setSize (0, 0);
}

void TimelineAudioSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    if (deviceSampleRate <= 0.0 || ! playing.load())
        return;

    if (needsRebuild.exchange (false))
        buildReaders();

    if (scratch.getNumChannels() < juce::jmax (2, info.buffer->getNumChannels())
        || scratch.getNumSamples() < info.numSamples)
    {
        scratch.setSize (juce::jmax (2, info.buffer->getNumChannels()),
                         info.numSamples, false, false, true);
    }

    const int64 blockStart = playheadSamples.load();
    const int64 blockEnd = blockStart + info.numSamples;

    for (size_t i = 0; i < sources.size(); ++i)
    {
        auto* model = clipModels[i];
        const double secPerBeat = 60.0 / juce::jmax (1.0, bpmRef);

        const int64 clipStart = (int64) std::floor (model->startBeats * secPerBeat * deviceSampleRate);
        const int64 clipLen = (int64) std::ceil (model->lengthBeats * secPerBeat * deviceSampleRate);
        const int64 clipEnd = clipStart + clipLen;

        const int64 segStart = std::max (blockStart, clipStart);
        const int64 segEnd = std::min (blockEnd, clipEnd);
        const int segLen = (int) std::max<int64> (0, segEnd - segStart);
        if (segLen <= 0)
            continue;

        const int outOffset = (int) (segStart - blockStart);

        const int64 offsetOutSamples = (int64) std::llround (model->offsetBeats * secPerBeat * deviceSampleRate);
        const int64 clipPosOutSamples = (segStart - clipStart) + offsetOutSamples;

        const double ratio = resampleRatios[i];
        int64 clipPosInSamples = (int64) std::llround ((double) clipPosOutSamples * ratio);
        if (clipPosInSamples < 0)
            clipPosInSamples = 0;

        sources[i]->setNextReadPosition (clipPosInSamples);

        scratch.clear();
        juce::AudioSourceChannelInfo segInfo (&scratch, 0, segLen);
        resamplers[i]->getNextAudioBlock (segInfo);

        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
        {
            const float* src = scratch.getReadPointer (juce::jmin (ch, scratch.getNumChannels() - 1));
            float* dst = info.buffer->getWritePointer (ch, info.startSample + outOffset);
            juce::FloatVectorOperations::add (dst, src, segLen);
        }
    }

    playheadSamples.fetch_add (info.numSamples);
}

void TimelineAudioSource::buildReaders()
{
    resamplers.clear();
    sources.clear();
    clipModels.clear();
    resampleRatios.clear();

    for (auto& track : tracks)
    {
        for (auto& clip : track.clips)
        {
            if (! clip.file.existsAsFile())
                continue;

            if (auto* raw = fmt.createReaderFor (clip.file))
            {
                auto src = std::make_unique<juce::AudioFormatReaderSource> (raw, true);
                auto res = std::make_unique<juce::ResamplingAudioSource> (src.get(), false, 2);
                const double ratio = raw->sampleRate / deviceSampleRate;
                res->setResamplingRatio (std::max (0.01, ratio));
                res->prepareToPlay (512, deviceSampleRate);

                clipModels.push_back (&clip);
                sources.emplace_back (std::move (src));
                resamplers.emplace_back (std::move (res));
                resampleRatios.push_back (ratio);
            }
        }
    }

    scratch.setSize (2, 512);
}

} // namespace aidaw
