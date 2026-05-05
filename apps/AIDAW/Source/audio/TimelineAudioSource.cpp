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
    pitchStates.clear();
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

        if (std::abs (model->pitchSemitones) > 0.01
            && model->stretchMode == ClipModel::StretchMode::Stretch)
            processPitchShift (scratch, segLen, model->pitchSemitones, pitchStates[i]);

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
    pitchStates.clear();

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

                PitchShiftState state;
                state.delay.setSize (2, juce::jmax (4096, (int) std::ceil (deviceSampleRate * 0.10)), false, false, true);
                state.delay.clear();
                pitchStates.push_back (std::move (state));
            }
        }
    }

    scratch.setSize (2, 512);
}

void TimelineAudioSource::processPitchShift (juce::AudioBuffer<float>& buffer,
                                             int numSamples,
                                             double semitones,
                                             PitchShiftState& state)
{
    if (deviceSampleRate <= 0.0 || numSamples <= 0)
        return;

    const double factor = std::pow (2.0, semitones / 12.0);
    if (std::abs (factor - 1.0) < 0.001)
        return;

    const int channels = juce::jmin (buffer.getNumChannels(), state.delay.getNumChannels());
    const int delaySize = state.delay.getNumSamples();
    if (channels <= 0 || delaySize < 8)
        return;

    const double windowSamples = juce::jlimit (512.0, (double) delaySize - 4.0, deviceSampleRate * 0.055);
    const double phaseStep = juce::jlimit (0.00001, 0.45, std::abs (factor - 1.0) / windowSamples);
    const bool pitchUp = factor > 1.0;

    auto readDelayed = [&state, delaySize] (int channel, double delaySamples)
    {
        double read = (double) state.writePos - delaySamples;
        while (read < 0.0)
            read += delaySize;
        while (read >= delaySize)
            read -= delaySize;

        const int i0 = (int) read;
        const int i1 = (i0 + 1) % delaySize;
        const float frac = (float) (read - (double) i0);
        const float a = state.delay.getSample (channel, i0);
        const float b = state.delay.getSample (channel, i1);
        return a + (b - a) * frac;
    };

    for (int n = 0; n < numSamples; ++n)
    {
        const double p1 = state.phase;
        const double p2 = p1 < 0.5 ? p1 + 0.5 : p1 - 0.5;
        const double d1 = (pitchUp ? (1.0 - p1) : p1) * windowSamples + 2.0;
        const double d2 = (pitchUp ? (1.0 - p2) : p2) * windowSamples + 2.0;
        const float g1 = 0.5f - 0.5f * std::cos ((float) (p1 * juce::MathConstants<double>::twoPi));
        const float g2 = 1.0f - g1;

        for (int ch = 0; ch < channels; ++ch)
        {
            const float in = buffer.getSample (ch, n);
            state.delay.setSample (ch, state.writePos, in);
            const float shifted = readDelayed (ch, d1) * g1 + readDelayed (ch, d2) * g2;
            buffer.setSample (ch, n, shifted);
        }

        if (++state.writePos >= delaySize)
            state.writePos = 0;

        state.phase += phaseStep;
        if (state.phase >= 1.0)
            state.phase -= 1.0;
    }
}

} // namespace aidaw
