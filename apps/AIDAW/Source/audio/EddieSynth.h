#pragma once

#include <JuceHeader.h>
#include <vector>

#include "../ui/midi/MidiNote.h"

namespace aidaw
{

enum class EddieWaveform
{
    sine = 0,
    saw,
    square,
    triangle
};

struct EddieSynthSettings
{
    float outputGain { 0.18f };
    EddieWaveform waveform { EddieWaveform::saw };
    float sawMix     { 0.72f };
    float subMix     { 0.20f };
    float attackMs   { 8.0f };
    float decayMs    { 80.0f };
    float sustain    { 0.70f };
    float releaseMs  { 120.0f };
    float drive      { 0.0f };
    float delayMix   { 0.0f };
    float delayMs    { 260.0f };
    float delayFeedback { 0.24f };
    float reverbMix  { 0.0f };
    float reverbSize { 0.36f };
    float reverbDamping { 0.42f };
};

class EddieSynthVoiceRenderer
{
public:
    static void renderNote (const MidiNote& note,
                            const EddieSynthSettings& settings,
                            double sampleRate,
                            double bpm,
                            int64 blockStartSample,
                            juce::AudioBuffer<float>& buffer,
                            int bufferStartSample,
                            int blockOffsetSample,
                            int numSamples);

private:
    static float oscillatorSample (double phaseCycles,
                                   EddieWaveform waveform,
                                   float shapeAmount,
                                   float subMix);
    static float envelopeGain (double secondsFromStart,
                               double secondsFromEnd,
                               const EddieSynthSettings& settings);
};

class EddieSynthAudioSource : public juce::AudioSource
{
public:
    static constexpr const char* instrumentName = "Eddie";

    void prepareToPlay (int samplesPerBlockExpected, double sampleRateIn) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;

    void setPlaying (bool shouldPlay);
    void resetToStart();
    void setPlayheadBeats (double beats);
    double getPlayheadBeats() const;
    void setBpm (double bpmIn);
    void setNotes (const std::vector<MidiNote>& newNotes);
    void setSettings (const EddieSynthSettings& newSettings);
    EddieSynthSettings getSettings() const;
    void triggerPreviewNote (int pitch, int velocity = 100);

private:
    struct PreviewVoice
    {
        MidiNote note;
        int64 sampleCursor { 0 };
        int64 releaseEndSamples { 0 };
    };

    std::vector<MidiNote> copyNotes() const;
    void renderPreviewVoices (const juce::AudioSourceChannelInfo& info);
    void processPostEffects (const juce::AudioSourceChannelInfo& info, const EddieSynthSettings& renderSettings);

    mutable juce::CriticalSection notesLock;
    std::vector<MidiNote> notes;
    mutable juce::CriticalSection previewLock;
    std::vector<PreviewVoice> previewVoices;

    EddieSynthSettings settings;
    mutable juce::CriticalSection settingsLock;
    std::atomic<bool> playing { false };
    std::atomic<int64> playheadSamples { 0 };
    std::atomic<double> pendingSeekBeats { 0.0 };
    double sampleRate { 0.0 };
    double bpm { 120.0 };

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition { 0 };
    juce::dsp::Reverb reverb;
};

} // namespace aidaw
