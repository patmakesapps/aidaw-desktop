#include "MetronomeSource.h"

#include <cmath>

namespace aidaw
{

namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
}

void MetronomeSource::prepareToPlay (int, double sampleRateIn)
{
    sampleRate = sampleRateIn;
    updateTiming();
    phase = 0.0;
    reset();
}

void MetronomeSource::releaseResources()
{
}

void MetronomeSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    auto* left = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample);
    auto* right = bufferToFill.buffer->getNumChannels() > 1
                    ? bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample)
                    : nullptr;

    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        float sample = 0.0f;

        if (playing && clickEnabled && sampleRate > 0.0)
        {
            if (samplesUntilTick <= 0)
            {
                burstSamplesRemaining = (int) (0.008 * sampleRate);
                samplesUntilTick += (int) samplesPerBeat;
            }

            if (burstSamplesRemaining > 0)
            {
                const auto env = (float) burstSamplesRemaining / (float) (0.008 * sampleRate);
                sample = (float) std::sin (phase) * 0.25f * env;
                phase += twoPi * 1000.0 / sampleRate;
                if (phase > twoPi)
                    phase -= twoPi;

                --burstSamplesRemaining;
            }

            --samplesUntilTick;
        }

        left[i] += sample;
        if (right != nullptr)
            right[i] += sample;
    }
}

void MetronomeSource::setBpm (double bpmIn)
{
    bpm = juce::jlimit (40.0, 300.0, bpmIn);
    updateTiming();
}

void MetronomeSource::setPlaying (bool isOn)
{
    playing = isOn;
}

void MetronomeSource::setClickEnabled (bool on)
{
    clickEnabled = on;
}

void MetronomeSource::setPlayheadBeats (double beats)
{
    if (sampleRate <= 0.0 || samplesPerBeat <= 0.0)
        return;

    const double wholeBeat = std::floor(juce::jmax(0.0, beats));
    const double phaseBeats = juce::jlimit(0.0, 1.0, juce::jmax(0.0, beats) - wholeBeat);
    samplesUntilTick = phaseBeats < 1.0e-6 ? 0 : (int) std::llround((1.0 - phaseBeats) * samplesPerBeat);
    burstSamplesRemaining = 0;
    phase = 0.0;
}

void MetronomeSource::reset()
{
    samplesUntilTick = 0;
    burstSamplesRemaining = 0;
}

void MetronomeSource::updateTiming()
{
    if (sampleRate > 0.0)
        samplesPerBeat = sampleRate * (60.0 / bpm);
}

} // namespace aidaw
