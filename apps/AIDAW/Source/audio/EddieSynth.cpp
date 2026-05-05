#include "EddieSynth.h"

#include <algorithm>
#include <cmath>

namespace aidaw
{

namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;

double midiNoteToHz (double midiNote)
{
    return 440.0 * std::pow (2.0, (midiNote - 69.0) / 12.0);
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

std::vector<MidiNote> applyVoiceMode (const std::vector<MidiNote>& source,
                                      const EddieSynthSettings& settings,
                                      double bpm)
{
    if (source.empty())
        return source;

    auto notes = source;
    std::stable_sort (notes.begin(), notes.end(), [] (const MidiNote& a, const MidiNote& b)
    {
        if (a.startBeats == b.startBeats)
            return a.pitch < b.pitch;
        return a.startBeats < b.startBeats;
    });

    if (settings.mono)
    {
        for (size_t i = 0; i + 1 < notes.size(); ++i)
        {
            const double nextStart = notes[i + 1].startBeats;
            if (notes[i].startBeats + notes[i].lengthBeats > nextStart)
                notes[i].lengthBeats = juce::jmax (0.0, nextStart - notes[i].startBeats);
        }

        notes.erase (std::remove_if (notes.begin(), notes.end(), [] (const MidiNote& note)
        {
            return note.lengthBeats <= 0.0;
        }), notes.end());
        return notes;
    }

    const int maxVoices = juce::jlimit (1, 16, settings.polyphony);
    if (maxVoices >= 16)
        return notes;

    const double releaseBeats = settings.releaseMs * 0.001 * juce::jmax (1.0, bpm) / 60.0;
    std::vector<double> voiceEnds;
    std::vector<MidiNote> accepted;
    accepted.reserve (notes.size());

    for (const auto& note : notes)
    {
        voiceEnds.erase (std::remove_if (voiceEnds.begin(), voiceEnds.end(), [&note] (double endBeat)
        {
            return endBeat <= note.startBeats;
        }), voiceEnds.end());

        if ((int) voiceEnds.size() >= maxVoices)
            continue;

        voiceEnds.push_back (note.startBeats + note.lengthBeats + releaseBeats);
        accepted.push_back (note);
    }

    return accepted;
}

float waveformAt (double phaseCycles, EddieWaveform waveform, float pulseWidth = 0.5f)
{
    const double wrapped = phaseCycles - std::floor (phaseCycles);
    switch (waveform)
    {
        case EddieWaveform::sine:
            return (float) std::sin (wrapped * twoPi);
        case EddieWaveform::saw:
            // Rising sawtooth, -1..+1, no DC offset
            return (float) (2.0 * wrapped - 1.0);
        case EddieWaveform::square:
        {
            const float pw = juce::jlimit (0.05f, 0.95f, pulseWidth);
            return wrapped < (double) pw ? 1.0f : -1.0f;
        }
        case EddieWaveform::triangle:
        {
            // Symmetric triangle peaking at +1 mid-cycle, -1 at the boundaries
            const double t = wrapped < 0.5 ? wrapped * 2.0 : 2.0 - wrapped * 2.0;
            return (float) (2.0 * t - 1.0);
        }
    }
    return 0.0f;
}
} // namespace

double syncDivToSeconds (EddieSyncDiv div, double bpm)
{
    const double beat = 60.0 / juce::jmax (1.0, bpm);
    switch (div)
    {
        case EddieSyncDiv::d1_32:  return beat * 0.125;
        case EddieSyncDiv::d1_16T: return beat * 0.25 * (2.0 / 3.0);
        case EddieSyncDiv::d1_16:  return beat * 0.25;
        case EddieSyncDiv::d1_16D: return beat * 0.25 * 1.5;
        case EddieSyncDiv::d1_8T:  return beat * 0.5  * (2.0 / 3.0);
        case EddieSyncDiv::d1_8:   return beat * 0.5;
        case EddieSyncDiv::d1_8D:  return beat * 0.5  * 1.5;
        case EddieSyncDiv::d1_4T:  return beat * 1.0  * (2.0 / 3.0);
        case EddieSyncDiv::d1_4:   return beat * 1.0;
        case EddieSyncDiv::d1_4D:  return beat * 1.0  * 1.5;
        case EddieSyncDiv::d1_2:   return beat * 2.0;
        case EddieSyncDiv::d1_1:   return beat * 4.0;
    }
    return beat;
}

const char* syncDivLabel (EddieSyncDiv div)
{
    switch (div)
    {
        case EddieSyncDiv::d1_32:  return "1/32";
        case EddieSyncDiv::d1_16T: return "1/16T";
        case EddieSyncDiv::d1_16:  return "1/16";
        case EddieSyncDiv::d1_16D: return "1/16D";
        case EddieSyncDiv::d1_8T:  return "1/8T";
        case EddieSyncDiv::d1_8:   return "1/8";
        case EddieSyncDiv::d1_8D:  return "1/8D";
        case EddieSyncDiv::d1_4T:  return "1/4T";
        case EddieSyncDiv::d1_4:   return "1/4";
        case EddieSyncDiv::d1_4D:  return "1/4D";
        case EddieSyncDiv::d1_2:   return "1/2";
        case EddieSyncDiv::d1_1:   return "1/1";
    }
    return "?";
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
    if (sampleRate <= 0.0 || bpm <= 0.0 || note.lengthBeats <= 0.0)
        return;

    const double secondsPerBeat = 60.0 / bpm;
    const double noteStartSeconds = note.startBeats * secondsPerBeat;
    const double noteEndSeconds = (note.startBeats + note.lengthBeats) * secondsPerBeat;
    const double noteStartSample = noteStartSeconds * sampleRate;
    const double noteEndSample = noteEndSeconds * sampleRate;

    const double freq1 = midiNoteToHz ((double) note.pitch);
    const double osc2Pitch = (double) note.pitch + (double) settings.osc2Semis + (double) settings.osc2Cents * 0.01;
    const double freq2 = midiNoteToHz (osc2Pitch);
    const float velocityGain = juce::jlimit (1, 127, note.velocity) / 127.0f;
    const int channels = buffer.getNumChannels();

    const int unison = juce::jlimit (1, 7, settings.unisonVoices);
    const float detuneCents = juce::jlimit (0.0f, 50.0f, settings.unisonDetuneCents);
    const float spread = juce::jlimit (0.0f, 1.0f, settings.unisonSpread);
    const float unisonNorm = 1.0f / std::sqrt ((float) unison);

    // Precompute per-unison detune ratios and pan positions (-1..+1).
    std::array<double, 7> detuneRatio {};
    std::array<float, 7>  panL {};
    std::array<float, 7>  panR {};
    for (int u = 0; u < unison; ++u)
    {
        const float t = unison == 1 ? 0.0f : ((float) u / (float) (unison - 1)) * 2.0f - 1.0f; // -1..+1
        const double cents = (double) t * (double) detuneCents;
        detuneRatio[(size_t) u] = std::pow (2.0, cents / 1200.0);
        // equal-power pan
        const float pan = t * spread;
        const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // 0..pi/2
        panL[(size_t) u] = std::cos (angle);
        panR[(size_t) u] = std::sin (angle);
    }

    const float pw    = juce::jlimit (0.05f, 0.95f, settings.sawMix); // legacy field = pulse width
    const float sub   = juce::jlimit (0.0f, 1.0f, settings.subMix);
    const float o1Lev = juce::jlimit (0.0f, 1.0f, settings.osc1Level);
    const float o2Lev = settings.osc2Enabled ? juce::jlimit (0.0f, 1.0f, settings.osc2Level) : 0.0f;
    const float noise = juce::jlimit (0.0f, 1.0f, settings.noiseLevel);

    auto& rng = juce::Random::getSystemRandom();

    for (int i = 0; i < numSamples; ++i)
    {
        const int bufferIndex = bufferStartSample + i;
        const double absoluteSample = (double) blockStartSample + blockOffsetSample + i;
        const double secondsFromStart = (absoluteSample - noteStartSample) / sampleRate;
        const double secondsFromEnd = (absoluteSample - noteEndSample) / sampleRate;

        const float env = envelopeGain (secondsFromStart, secondsFromEnd, settings);
        if (env <= 0.0f)
            continue;

        // Noise & sub are mono — sum once per sample, then split equal-power into stereo.
        const float noiseS = noise > 0.0f ? (rng.nextFloat() * 2.0f - 1.0f) * noise : 0.0f;
        const double subPhase = secondsFromStart * freq1 * 0.5; // -1 octave
        const float subS = (float) std::sin ((subPhase - std::floor (subPhase)) * twoPi) * sub;
        const float monoExtras = noiseS + subS;

        float left = 0.0f, right = 0.0f;

        for (int u = 0; u < unison; ++u)
        {
            const double phase1 = secondsFromStart * freq1 * detuneRatio[(size_t) u];
            const double phase2 = secondsFromStart * freq2 * detuneRatio[(size_t) u];

            const float v1 = waveformAt (phase1, settings.waveform, pw);
            const float v2 = (o2Lev > 0.0f)
                ? waveformAt (phase2, settings.osc2Waveform, pw)
                : 0.0f;

            const float oscMix = v1 * o1Lev + v2 * o2Lev;
            left  += oscMix * panL[(size_t) u];
            right += oscMix * panR[(size_t) u];
        }

        // Add mono extras (sub + noise) to both channels equally.
        left  += monoExtras;
        right += monoExtras;

        const float gainScale = env * velocityGain * settings.outputGain * unisonNorm;
        left  *= gainScale;
        right *= gainScale;

        if (channels >= 2)
        {
            buffer.addSample (0, bufferIndex, left);
            buffer.addSample (1, bufferIndex, right);
        }
        else
        {
            buffer.addSample (0, bufferIndex, (left + right) * 0.5f);
        }
    }
}

float EddieSynthVoiceRenderer::oscillatorSample (double phaseCycles,
                                                 EddieWaveform waveform,
                                                 float pulseWidth,
                                                 float subMix)
{
    const float main = waveformAt (phaseCycles, waveform, pulseWidth);
    const double subWrapped = phaseCycles * 0.5 - std::floor (phaseCycles * 0.5);
    const float sub = (float) std::sin (subWrapped * twoPi);
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

EddieSynthAudioSource::EddieSynthAudioSource() = default;

void EddieSynthAudioSource::prepareToPlay (int samplesPerBlockExpected, double sampleRateIn)
{
    sampleRate = sampleRateIn;
    delayBuffer.setSize (2, (int) std::ceil (juce::jmax (1.0, sampleRate) * 3.0));
    delayBuffer.clear();
    delayWritePosition = 0;
    delayHiCutZ[0] = delayHiCutZ[1] = 0.0f;

    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) juce::jmax (32, samplesPerBlockExpected),
                                  2 };
    reverb.prepare (spec);
    reverb.reset();
    chorus.prepare (spec);
    chorus.reset();

    svf.reset();
    lfoPhase = 0.0f;
    lfoSampleHoldVal = 0.0f;
    lfoSampleHoldCounter = 0;

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
    delayBuffer.setSize (0, 0);
    delayWritePosition = 0;
    reverb.reset();
    chorus.reset();
    svf.reset();
}

void EddieSynthAudioSource::updateNoteEvents (const std::vector<MidiNote>& localNotes,
                                              int64 blockStart, int64 blockEnd)
{
    if (sampleRate <= 0.0 || bpm <= 0.0)
        return;

    const double secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
    int64 newestOnSample = std::numeric_limits<int64>::min();
    int   newestOnPitch = activeKey;
    int64 newestOffSample = std::numeric_limits<int64>::min();

    bool anyHeld = false;

    for (const auto& note : localNotes)
    {
        const int64 noteStart = (int64) std::floor (note.startBeats * secondsPerBeat * sampleRate);
        const int64 noteEnd   = (int64) std::ceil ((note.startBeats + note.lengthBeats) * secondsPerBeat * sampleRate);

        if (noteStart >= blockStart && noteStart < blockEnd && noteStart > newestOnSample)
        {
            newestOnSample = noteStart;
            newestOnPitch = note.pitch;
        }
        if (noteEnd >= blockStart && noteEnd < blockEnd && noteEnd > newestOffSample)
        {
            newestOffSample = noteEnd;
        }
        if (noteStart < blockEnd && noteEnd > blockStart)
            anyHeld = true;
    }

    if (newestOnSample != std::numeric_limits<int64>::min())
    {
        fEnvNoteOnSample = (double) newestOnSample;
        fEnvNoteOffSample = -1.0e18;
        activeKey = newestOnPitch;
        keyHeld = true;
    }
    if (newestOffSample != std::numeric_limits<int64>::min() && ! anyHeld)
    {
        fEnvNoteOffSample = (double) newestOffSample;
        keyHeld = false;
    }
}

void EddieSynthAudioSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer == nullptr)
        return;

    info.clearActiveBufferRegion();

    if (sampleRate <= 0.0)
        return;

    const auto renderSettings = getSettings();
    const int64 blockStart = playheadSamples.load();
    const int64 blockEnd   = blockStart + info.numSamples;

    if (playing.load())
    {
        const auto localNotes = applyVoiceMode (copyNotes(), renderSettings, bpm);

        updateNoteEvents (localNotes, blockStart, blockEnd);

        const double secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
        const int64 releaseSamples = (int64) std::ceil (msToSeconds (renderSettings.releaseMs) * sampleRate);

        for (const auto& note : localNotes)
        {
            const int64 noteStart = (int64) std::floor (note.startBeats * secondsPerBeat * sampleRate);
            const int64 noteEnd   = (int64) std::ceil ((note.startBeats + note.lengthBeats) * secondsPerBeat * sampleRate);
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
    processPostEffects (info, renderSettings, activeKey);
    softLimitBuffer (info);
}

void EddieSynthAudioSource::setPlaying (bool shouldPlay)         { playing.store (shouldPlay); }
void EddieSynthAudioSource::resetToStart()                        { playheadSamples.store (0); }

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

double EddieSynthAudioSource::getPlayheadBeats() const
{
    if (sampleRate <= 0.0 || bpm <= 0.0)
        return pendingSeekBeats.load();
    const double secondsPerBeat = 60.0 / bpm;
    return (double) playheadSamples.load() / sampleRate / secondsPerBeat;
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
    voice.note.velocity = juce::jlimit (1, 100, velocity);
    voice.note.startBeats = 0.0;
    voice.note.lengthBeats = previewSeconds / secondsPerBeat;
    voice.releaseEndSamples = (int64) std::ceil ((previewSeconds + msToSeconds (previewBaseSettings.releaseMs)) * sampleRate);

    const juce::ScopedLock lock (previewLock);
    if (previewBaseSettings.mono)
        previewVoices.clear();

    const int maxPreviewVoices = juce::jlimit (1, 16, previewBaseSettings.mono ? 1 : previewBaseSettings.polyphony);
    while ((int) previewVoices.size() >= maxPreviewVoices)
        previewVoices.erase (previewVoices.begin());

    previewVoices.push_back (voice);

    // Trigger filter envelope on preview too.
    fEnvNoteOnSample = (double) playheadSamples.load();
    fEnvNoteOffSample = -1.0e18;
    activeKey = voice.note.pitch;
    keyHeld = true;
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

        EddieSynthVoiceRenderer::renderNote (voice.note, previewSettings, sampleRate, bpm, voice.sampleCursor,
                                             *info.buffer, info.startSample, 0, info.numSamples);
        voice.sampleCursor += info.numSamples;

        // Decide if preview voice has reached its note-off this block, mark for fEnv release.
        const double secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
        const int64 endSamples = (int64) std::ceil (voice.note.lengthBeats * secondsPerBeat * sampleRate);
        if (voice.sampleCursor >= endSamples && fEnvNoteOffSample < -1.0e17 && keyHeld)
        {
            fEnvNoteOffSample = (double) playheadSamples.load();
            keyHeld = false;
        }
    }

    previewVoices.erase (std::remove_if (previewVoices.begin(), previewVoices.end(),
                                         [] (const PreviewVoice& voice)
                                         {
                                             return voice.sampleCursor >= voice.releaseEndSamples;
                                         }),
                         previewVoices.end());
}

float EddieSynthAudioSource::advanceLfo (float phaseInc, EddieLfoShape shape)
{
    lfoPhase += phaseInc;
    if (lfoPhase >= 1.0f)
        lfoPhase -= std::floor (lfoPhase);

    switch (shape)
    {
        case EddieLfoShape::sine:
            return std::sin (lfoPhase * juce::MathConstants<float>::twoPi);
        case EddieLfoShape::triangle:
            return 4.0f * std::abs (lfoPhase - 0.5f) - 1.0f;
        case EddieLfoShape::saw:
            return 2.0f * lfoPhase - 1.0f;
        case EddieLfoShape::square:
            return lfoPhase < 0.5f ? 1.0f : -1.0f;
        case EddieLfoShape::sampleAndHold:
        {
            // Re-sample once per cycle.
            if (lfoSampleHoldCounter <= 0)
            {
                lfoSampleHoldVal = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                lfoSampleHoldCounter = 1;
            }
            // Decrement marker by checking phase wrap.
            if (lfoPhase < phaseInc * 1.5f)
            {
                lfoSampleHoldVal = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
            }
            return lfoSampleHoldVal;
        }
    }
    return 0.0f;
}

void EddieSynthAudioSource::processFilter (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                                           const EddieSynthSettings& s, int activeKeyForKt)
{
    if (s.filterMode == EddieFilterMode::off || sampleRate <= 0.0)
        return;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    if (channels < 1)
        return;

    // LFO modulation tap (sample once per block — cheap)
    float lfoCutoffMod = 0.0f;
    float lfoResMod = 0.0f;
    if (s.lfoDest != EddieLfoDest::off && s.lfoDepth > 0.001f)
    {
        const double rate = s.lfoSync ? (1.0 / juce::jmax (1.0e-6, syncDivToSeconds (s.lfoSyncDiv, bpm)))
                                       : (double) s.lfoRateHz;
        const float phaseInc = (float) (rate / juce::jmax (1.0, sampleRate));
        // Advance LFO across the block by numSamples steps but only sample at midpoint
        for (int i = 0; i < numSamples; ++i)
            (void) advanceLfo (phaseInc, s.lfoShape);
        const float lfoVal = advanceLfo (0.0f, s.lfoShape); // read current
        if (s.lfoDest == EddieLfoDest::filterCutoff)
            lfoCutoffMod = lfoVal * s.lfoDepth;
        else if (s.lfoDest == EddieLfoDest::filterResonance)
            lfoResMod = lfoVal * s.lfoDepth * 0.6f;
    }

    // Filter envelope: ADSR retriggered per note-on.
    float envOctaves = 0.0f;
    if (std::abs (s.fEnvAmount) > 0.001f)
    {
        const double curSample = (double) playheadSamples.load();
        const double t = (curSample - fEnvNoteOnSample) / juce::jmax (1.0, sampleRate);
        const double tOff = (curSample - fEnvNoteOffSample) / juce::jmax (1.0, sampleRate);

        const double a = msToSeconds (s.fEnvAttackMs);
        const double d = msToSeconds (s.fEnvDecayMs);
        const double r = msToSeconds (s.fEnvReleaseMs);
        const float  sus = juce::jlimit (0.0f, 1.0f, s.fEnvSustain);

        float envVal = 0.0f;
        if (t >= 0.0)
        {
            if (t < a)              envVal = (float) (t / a);
            else if (t < a + d)     envVal = (float) (1.0 - (1.0 - sus) * ((t - a) / d));
            else                    envVal = sus;

            if (! keyHeld && fEnvNoteOffSample > -1.0e17)
            {
                if (tOff >= 0.0 && tOff < r)
                    envVal *= (float) (1.0 - tOff / r);
                else if (tOff >= r)
                    envVal = 0.0f;
            }
        }
        envOctaves = envVal * s.fEnvAmount * 5.0f; // up to ~5 octaves
    }

    // Key tracking: 0 at C3 (60), full at +/- octaves
    const float kt = juce::jlimit (0.0f, 1.0f, s.filterKeyTrack);
    const float ktOctaves = ((float) activeKeyForKt - 60.0f) / 12.0f * kt;

    float cutoffHz = s.filterCutoffHz * std::pow (2.0f, envOctaves + ktOctaves + lfoCutoffMod * 4.0f);
    cutoffHz = juce::jlimit (20.0f, (float) (sampleRate * 0.45), cutoffHz);

    const float resonance = juce::jlimit (0.0f, 1.0f, s.filterResonance + lfoResMod);
    const float q = juce::jmap (resonance, 0.0f, 1.0f, 0.5f, 12.0f);

    // TPT SVF coefficients
    const float g = std::tan (juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);
    const float k = 1.0f / juce::jmax (0.05f, q);
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;

    const float drive = 1.0f + juce::jlimit (0.0f, 1.0f, s.filterDrive) * 4.0f;
    const float driveTrim = 1.0f / std::sqrt (drive);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch, startSample);
        float ic1 = svf.ic1eq[ch];
        float ic2 = svf.ic2eq[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float in = std::tanh (data[i] * drive) * driveTrim;
            const float v3 = in - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;

            float out = 0.0f;
            switch (s.filterMode)
            {
                case EddieFilterMode::lowPass:  out = v2; break;
                case EddieFilterMode::bandPass: out = v1; break;
                case EddieFilterMode::highPass: out = in - k * v1 - v2; break;
                case EddieFilterMode::off:      out = data[i]; break;
            }
            data[i] = out;
        }

        svf.ic1eq[ch] = ic1;
        svf.ic2eq[ch] = ic2;
    }
}

void EddieSynthAudioSource::processPostEffects (const juce::AudioSourceChannelInfo& info,
                                                const EddieSynthSettings& renderSettings,
                                                int activeKeyForKt)
{
    if (info.buffer == nullptr || sampleRate <= 0.0 || info.numSamples <= 0)
        return;

    const int numChannels = info.buffer->getNumChannels();

    // 1) Filter (with LFO + env mod)
    processFilter (*info.buffer, info.startSample, info.numSamples, renderSettings, activeKeyForKt);

    // 2) Drive
    const float driveAmount = juce::jlimit (0.0f, 1.0f, renderSettings.drive);
    if (driveAmount > 0.001f)
    {
        const float inputGain = 1.0f + driveAmount * 7.0f;
        const float outputTrim = 1.0f / (1.0f + driveAmount * 1.8f);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = info.buffer->getWritePointer (ch, info.startSample);
            for (int i = 0; i < info.numSamples; ++i)
                data[i] = std::tanh (data[i] * inputGain) * outputTrim;
        }
    }

    // 3) LFO -> amp / pan (applied here, post filter)
    if (renderSettings.lfoDepth > 0.001f
        && (renderSettings.lfoDest == EddieLfoDest::amp || renderSettings.lfoDest == EddieLfoDest::pan)
        && numChannels > 0)
    {
        const double rate = renderSettings.lfoSync
            ? (1.0 / juce::jmax (1.0e-6, syncDivToSeconds (renderSettings.lfoSyncDiv, bpm)))
            : (double) renderSettings.lfoRateHz;
        const float phaseInc = (float) (rate / juce::jmax (1.0, sampleRate));

        for (int i = 0; i < info.numSamples; ++i)
        {
            const float lfo = advanceLfo (phaseInc, renderSettings.lfoShape);
            if (renderSettings.lfoDest == EddieLfoDest::amp)
            {
                const float gain = juce::jlimit (0.0f, 2.0f, 1.0f + lfo * renderSettings.lfoDepth);
                for (int ch = 0; ch < numChannels; ++ch)
                    info.buffer->getWritePointer (ch, info.startSample)[i] *= gain;
            }
            else if (numChannels >= 2)
            {
                const float pan = lfo * renderSettings.lfoDepth;
                const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                const float lG = std::cos (angle) * juce::MathConstants<float>::sqrt2;
                const float rG = std::sin (angle) * juce::MathConstants<float>::sqrt2;
                auto* L = info.buffer->getWritePointer (0, info.startSample);
                auto* R = info.buffer->getWritePointer (1, info.startSample);
                const float mid = (L[i] + R[i]) * 0.5f;
                L[i] = mid * lG;
                R[i] = mid * rG;
            }
        }
    }

    // 4) Chorus
    if (renderSettings.chorusMix > 0.001f && numChannels <= 2)
    {
        chorus.setMix (juce::jlimit (0.0f, 1.0f, renderSettings.chorusMix));
        chorus.setRate (juce::jlimit (0.05f, 8.0f, renderSettings.chorusRateHz));
        chorus.setDepth (juce::jlimit (0.0f, 1.0f, renderSettings.chorusDepth));
        chorus.setCentreDelay (12.0f);
        chorus.setFeedback (0.18f);

        juce::dsp::AudioBlock<float> block (*info.buffer);
        auto active = block.getSubBlock ((size_t) info.startSample, (size_t) info.numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (active);
        chorus.process (ctx);
    }

    // 5) Delay (tempo-sync, ping-pong, hi-cut on feedback)
    const float delayMix = juce::jlimit (0.0f, 1.0f, renderSettings.delayMix);
    if (delayMix > 0.001f && delayBuffer.getNumSamples() > 0)
    {
        const int delayChannels = delayBuffer.getNumChannels();
        const int delayLength = delayBuffer.getNumSamples();
        const double timeSec = renderSettings.delaySync
            ? syncDivToSeconds (renderSettings.delayDiv, bpm)
            : (double) renderSettings.delayMs * 0.001;
        const int delaySamples = juce::jlimit (1, delayLength - 1, (int) std::round (timeSec * sampleRate));
        const float feedback = juce::jlimit (0.0f, 0.92f, renderSettings.delayFeedback);

        // Hi-cut one-pole coefficient
        const float fc = juce::jlimit (200.0f, 18000.0f, renderSettings.delayHiCutHz);
        const float rc = 1.0f / (juce::MathConstants<float>::twoPi * fc);
        const float dt = 1.0f / (float) sampleRate;
        const float alpha = dt / (rc + dt);

        const bool pingPong = renderSettings.delayPingPong && numChannels >= 2 && delayChannels >= 2;

        for (int i = 0; i < info.numSamples; ++i)
        {
            const int readPosition = (delayWritePosition + delayLength - delaySamples) % delayLength;

            if (pingPong)
            {
                auto* L = info.buffer->getWritePointer (0, info.startSample);
                auto* R = info.buffer->getWritePointer (1, info.startSample);
                const float dryL = L[i];
                const float dryR = R[i];
                const float delL = delayBuffer.getSample (0, readPosition);
                const float delR = delayBuffer.getSample (1, readPosition);

                L[i] = dryL + delL * delayMix;
                R[i] = dryR + delR * delayMix;

                // Apply hi-cut to feedback path then cross-feed
                const float fbL = dryR + delR * feedback;
                const float fbR = dryL + delL * feedback;
                delayHiCutZ[0] += alpha * (fbL - delayHiCutZ[0]);
                delayHiCutZ[1] += alpha * (fbR - delayHiCutZ[1]);
                delayBuffer.setSample (0, delayWritePosition, delayHiCutZ[0]);
                delayBuffer.setSample (1, delayWritePosition, delayHiCutZ[1]);
            }
            else
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* data = info.buffer->getWritePointer (ch, info.startSample);
                    const int delayChannel = ch % delayChannels;
                    const float dry = data[i];
                    const float delayed = delayBuffer.getSample (delayChannel, readPosition);
                    data[i] = dry + delayed * delayMix;
                    const float fb = dry + delayed * feedback;
                    const int z = juce::jlimit (0, 1, delayChannel);
                    delayHiCutZ[z] += alpha * (fb - delayHiCutZ[z]);
                    delayBuffer.setSample (delayChannel, delayWritePosition, delayHiCutZ[z]);
                }
            }

            delayWritePosition = (delayWritePosition + 1) % delayLength;
        }
    }

    // 6) Reverb
    const float reverbMix = juce::jlimit (0.0f, 1.0f, renderSettings.reverbMix);
    if (reverbMix > 0.001f)
    {
        juce::dsp::Reverb::Parameters params;
        params.roomSize = juce::jlimit (0.0f, 1.0f, renderSettings.reverbSize);
        params.damping = juce::jlimit (0.0f, 1.0f, renderSettings.reverbDamping);
        params.width = 0.86f;
        params.wetLevel = reverbMix;
        params.dryLevel = 1.0f - reverbMix * 0.28f;
        params.freezeMode = 0.0f;
        reverb.setParameters (params);

        if (numChannels <= 2)
        {
            juce::dsp::AudioBlock<float> block (*info.buffer);
            auto activeBlock = block.getSubBlock ((size_t) info.startSample, (size_t) info.numSamples);
            juce::dsp::ProcessContextReplacing<float> context (activeBlock);
            reverb.process (context);
        }
    }
}

} // namespace aidaw
