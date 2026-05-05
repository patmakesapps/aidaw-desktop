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
    clipGains.clear();
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
        if (model->muted)
            continue;

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

        const float gain = clipGains[i];
        if (std::abs (gain - 1.0f) > 0.0001f)
            scratch.applyGain (0, segLen, gain);

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
    clipGains.clear();

    for (auto& track : tracks)
    {
        for (auto& clip : track.clips)
        {
            if (clip.kind != ClipModel::Kind::Audio)
                continue;

            if (! clip.file.existsAsFile())
                continue;

            if (auto* raw = fmt.createReaderFor (clip.file))
            {
                auto src = std::make_unique<juce::AudioFormatReaderSource> (raw, true);
                auto res = std::make_unique<juce::ResamplingAudioSource> (src.get(), false, 2);

                double ratio = raw->sampleRate / deviceSampleRate;
                if (clip.stretchMode == ClipModel::StretchMode::Resample)
                {
                    const double fileSeconds = raw->sampleRate > 0.0
                                             ? (double) raw->lengthInSamples / raw->sampleRate
                                             : 0.0;
                    const double timelineSeconds = clip.lengthBeats * 60.0 / juce::jmax (1.0, bpmRef);
                    if (fileSeconds > 0.0 && timelineSeconds > 0.0)
                        ratio *= fileSeconds / timelineSeconds;
                }

                if (clip.stretchMode == ClipModel::StretchMode::Resample)
                    ratio *= std::pow (2.0, clip.pitchSemitones / 12.0);

                res->setResamplingRatio (std::max (0.01, ratio));
                res->prepareToPlay (512, deviceSampleRate);

                float gain = juce::Decibels::decibelsToGain ((float) clip.gainDb);
                if (clip.normalize)
                {
                    juce::Range<float> levels[2];
                    raw->readMaxLevels (0, raw->lengthInSamples, levels, 2);
                    float peak = 0.0f;
                    for (auto& level : levels)
                        peak = juce::jmax (peak, std::abs (level.getStart()), std::abs (level.getEnd()));
                    if (peak > 0.0001f)
                        gain *= 1.0f / peak;
                }

                clipModels.push_back (&clip);
                sources.emplace_back (std::move (src));
                resamplers.emplace_back (std::move (res));
                resampleRatios.push_back (ratio);
                clipGains.push_back (gain);
            }
        }
    }

    scratch.setSize (2, 512);
}

} // namespace aidaw
