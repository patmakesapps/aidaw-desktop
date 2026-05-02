#include "EddieSynth.h"

#include <algorithm>
#include <cmath>

namespace aidaw
{

namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;

double midiNoteToHz (int midiNote)
{
    return 440.0 * std::pow (2.0, ((double) midiNote - 69.0) / 12.0);
}

double msToSeconds (float ms)
{
    return juce::jmax (0.001, (double) ms * 0.001);
}

void softLimitBuffer (const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer == nullptr)
        return;

    for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
    {
        auto* data = info.buffer->getWritePointer (ch, info.startSample);
        for (int i = 0; i < info.numSamples; ++i)
            data[i] = std::tanh (data[i] * 1.15f) / 1.15f;
    }
}
}

void EddieSynthVoiceRenderer::renderNote (const MidiNote& note,
                                          const EddieSynthSettings& settings,
                                          double sampleRate,
                                          double bpm,
                                          int64 blockStartSample,
                                          juce::AudioBuffer<float>& buffer,
                                          int bufferStartSample,
                                          int blockOffsetSample,
                                          int numSamples)
{
    auto localSettings = settings;
    if (sampleRate <= 0.0 || bpm <= 0.0 || note.lengthBeats <= 0.0)
        return;

    const double secondsPerBeat = 60.0 / bpm;
    const double noteStartSeconds = note.startBeats * secondsPerBeat;
    const double noteEndSeconds = (note.startBeats + note.lengthBeats) * secondsPerBeat;
    const double noteStartSample = noteStartSeconds * sampleRate;
    const double noteEndSample = noteEndSeconds * sampleRate;

    const double frequency = midiNoteToHz (note.pitch);
    const float velocityGain = juce::jlimit (1, 127, note.velocity) / 127.0f;
    const int channels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const int bufferIndex = bufferStartSample + i;
        const double absoluteSample = (double) blockStartSample + blockOffsetSample + i;
        const double secondsFromStart = (absoluteSample - noteStartSample) / sampleRate;
        const double secondsFromEnd = (absoluteSample - noteEndSample) / sampleRate;

        const float env = envelopeGain (secondsFromStart, secondsFromEnd, localSettings);
        if (env <= 0.0f)
            continue;

        const double phaseCycles = secondsFromStart * frequency;
        const float sample = oscillatorSample (phaseCycles, localSettings.sawMix, localSettings.subMix)
                           * env * velocityGain * localSettings.outputGain;

        for (int ch = 0; ch < channels; ++ch)
            buffer.addSample (ch, bufferIndex, sample);
    }
}

float EddieSynthVoiceRenderer::oscillatorSample (double phaseCycles, float sawMix, float subMix)
{
    const double wrapped = phaseCycles - std::floor (phaseCycles);
    const float saw = (float) (2.0 * wrapped - 1.0);
    const float sine = (float) std::sin (wrapped * twoPi);

    const double subWrapped = phaseCycles * 0.5 - std::floor (phaseCycles * 0.5);
    const float sub = (float) std::sin (subWrapped * twoPi);

    const float main = saw * sawMix + sine * (1.0f - sawMix);
    return juce::jlimit (-1.0f, 1.0f, main + sub * subMix);
}

float EddieSynthVoiceRenderer::envelopeGain (double secondsFromStart,
                                             double secondsFromEnd,
                                             const EddieSynthSettings& settings)
{
    if (secondsFromStart < 0.0)
        return 0.0f;

    const double attack = msToSeconds (settings.attackMs);
    const double decay = msToSeconds (settings.decayMs);
    const double release = msToSeconds (settings.releaseMs);
    const float sustain = juce::jlimit (0.0f, 1.0f, settings.sustain);

    float heldGain = sustain;
    if (secondsFromStart < attack)
    {
        heldGain = (float) (secondsFromStart / attack);
    }
    else if (secondsFromStart < attack + decay)
    {
        const double decayProgress = (secondsFromStart - attack) / decay;
        heldGain = (float) (1.0 - (1.0 - sustain) * decayProgress);
    }

    if (secondsFromEnd <= 0.0)
        return heldGain;

    if (secondsFromEnd >= release)
        return 0.0f;

    return heldGain * (float) (1.0 - secondsFromEnd / release);
}

void EddieSynthAudioSource::prepareToPlay (int, double sampleRateIn)
{
    sampleRate = sampleRateIn;

    const auto pending = pendingSeekBeats.load();
    if (pending > 0.0)
    {
        setPlayheadBeats (pending);
        pendingSeekBeats.store (0.0);
    }
}

void EddieSynthAudioSource::releaseResources()
{
    sampleRate = 0.0;
}

void EddieSynthAudioSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer == nullptr)
        return;

    info.clearActiveBufferRegion();

    if (sampleRate <= 0.0)
        return;

    if (playing.load())
    {
        const auto renderSettings = getSettings();
        const auto localNotes = copyNotes();
        if (localNotes.empty())
        {
            playheadSamples.fetch_add (info.numSamples);
            renderPreviewVoices (info);
            softLimitBuffer (info);
            return;
        }

        const int64 blockStart = playheadSamples.load();
        const int64 blockEnd = blockStart + info.numSamples;
        const double secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
        const int64 releaseSamples = (int64) std::ceil (msToSeconds (renderSettings.releaseMs) * sampleRate);

        for (const auto& note : localNotes)
        {
            const int64 noteStart = (int64) std::floor (note.startBeats * secondsPerBeat * sampleRate);
            const int64 noteEnd = (int64) std::ceil ((note.startBeats + note.lengthBeats) * secondsPerBeat * sampleRate);
            const int64 tailEnd = noteEnd + releaseSamples;

            if (tailEnd <= blockStart || noteStart >= blockEnd)
                continue;

            const int renderStart = (int) juce::jmax<int64> (0, noteStart - blockStart);
            const int renderEnd = (int) juce::jmin<int64> (info.numSamples, tailEnd - blockStart);
            const int renderLength = juce::jmax (0, renderEnd - renderStart);

            EddieSynthVoiceRenderer::renderNote (note, renderSettings, sampleRate, bpm, blockStart,
                                                 *info.buffer, info.startSample + renderStart,
                                                 renderStart, renderLength);
        }

        playheadSamples.fetch_add (info.numSamples);
    }

    renderPreviewVoices (info);
    softLimitBuffer (info);
}

void EddieSynthAudioSource::setPlaying (bool shouldPlay)
{
    playing.store (shouldPlay);
}

void EddieSynthAudioSource::resetToStart()
{
    playheadSamples.store (0);
}

void EddieSynthAudioSource::setPlayheadBeats (double beats)
{
    beats = juce::jmax (0.0, beats);
    if (sampleRate <= 0.0 || bpm <= 0.0)
    {
        pendingSeekBeats.store (beats);
        return;
    }

    const double secondsPerBeat = 60.0 / bpm;
    playheadSamples.store ((int64) std::llround (beats * secondsPerBeat * sampleRate));
}

void EddieSynthAudioSource::setBpm (double bpmIn)
{
    bpm = juce::jlimit (40.0, 300.0, bpmIn);
}

void EddieSynthAudioSource::setNotes (const std::vector<MidiNote>& newNotes)
{
    const juce::ScopedLock lock (notesLock);
    notes = newNotes;
}

void EddieSynthAudioSource::setSettings (const EddieSynthSettings& newSettings)
{
    const juce::ScopedLock lock (settingsLock);
    settings = newSettings;
}

EddieSynthSettings EddieSynthAudioSource::getSettings() const
{
    const juce::ScopedLock lock (settingsLock);
    return settings;
}

void EddieSynthAudioSource::triggerPreviewNote (int pitch, int velocity)
{
    if (sampleRate <= 0.0)
        return;

    const auto previewBaseSettings = getSettings();
    const double previewSeconds = 0.32;
    const double secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);

    PreviewVoice voice;
    voice.note.pitch = juce::jlimit (0, 127, pitch);
    voice.note.velocity = juce::jlimit (1, 82, velocity);
    voice.note.startBeats = 0.0;
    voice.note.lengthBeats = previewSeconds / secondsPerBeat;
    voice.releaseEndSamples = (int64) std::ceil ((previewSeconds + msToSeconds (previewBaseSettings.releaseMs)) * sampleRate);

    const juce::ScopedLock lock (previewLock);
    if (! previewVoices.empty() && previewVoices.back().note.pitch == voice.note.pitch
        && previewVoices.back().sampleCursor < previewVoices.back().releaseEndSamples)
        return;

    if (previewVoices.size() > 2)
        previewVoices.erase (previewVoices.begin(), previewVoices.end() - 2);

    previewVoices.push_back (voice);
}

std::vector<MidiNote> EddieSynthAudioSource::copyNotes() const
{
    const juce::ScopedLock lock (notesLock);
    return notes;
}

void EddieSynthAudioSource::renderPreviewVoices (const juce::AudioSourceChannelInfo& info)
{
    const juce::ScopedLock lock (previewLock);
    if (previewVoices.empty() || info.buffer == nullptr)
        return;

    for (auto& voice : previewVoices)
    {
        EddieSynthSettings previewSettings;
        {
            const juce::ScopedLock settingsGuard (settingsLock);
            previewSettings = settings;
        }
        previewSettings.outputGain *= 0.55f;
        previewSettings.sawMix = juce::jmin (previewSettings.sawMix, 0.45f);
        previewSettings.subMix = juce::jmin (previewSettings.subMix, 0.10f);

        EddieSynthVoiceRenderer::renderNote (voice.note, previewSettings, sampleRate, bpm, voice.sampleCursor,
                                             *info.buffer, info.startSample, 0, info.numSamples);
        voice.sampleCursor += info.numSamples;
    }

    previewVoices.erase (std::remove_if (previewVoices.begin(), previewVoices.end(),
                                         [] (const PreviewVoice& voice)
                                         {
                                             return voice.sampleCursor >= voice.releaseEndSamples;
                                         }),
                         previewVoices.end());
}

} // namespace aidaw
