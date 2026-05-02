#pragma once

#include <JuceHeader.h>
#include <vector>

#include "../ui/midi/MidiNote.h"

namespace aidaw
{

struct EddieSynthSettings
{
    float outputGain { 0.18f };
    float sawMix     { 0.72f };
    float subMix     { 0.20f };
    float attackMs   { 8.0f };
    float decayMs    { 80.0f };
    float sustain    { 0.70f };
    float releaseMs  { 120.0f };
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
    static float oscillatorSample (double phaseCycles, float sawMix, float subMix);
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
};

} // namespace aidaw
