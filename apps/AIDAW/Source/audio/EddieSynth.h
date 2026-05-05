#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>

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

enum class EddieFilterMode
{
    off = 0,
    lowPass,
    bandPass,
    highPass
};

// Tempo-synced note divisions used by both the delay and the LFO sync.
// Order matters — IDs persisted in presets and used by combo boxes.
enum class EddieSyncDiv
{
    d1_32 = 0,
    d1_16T,
    d1_16,
    d1_16D,
    d1_8T,
    d1_8,
    d1_8D,
    d1_4T,
    d1_4,
    d1_4D,
    d1_2,
    d1_1
};

enum class EddieLfoShape
{
    sine = 0,
    triangle,
    saw,
    square,
    sampleAndHold
};

enum class EddieLfoDest
{
    off = 0,
    filterCutoff,
    filterResonance,
    amp,
    pan
};

enum class EddieNoiseType
{
    white = 0,
    pink,
    brown,
    digital
};

struct EddieSynthSettings
{
    // Master
    float outputGain { 0.45f };       // louder default — sells the synth
    bool  mono { false };
    int   polyphony { 16 };           // simultaneous notes cap

    // OSC1
    EddieWaveform waveform { EddieWaveform::saw };
    float sawMix     { 0.5f };        // legacy name — now PULSE WIDTH for square (0.05..0.95)
    float subMix     { 0.30f };       // sub osc (1 octave below, sine)
    float osc1Level  { 1.0f };
    float noiseLevel { 0.0f };        // noise mixed into OSC1 path
    EddieNoiseType noiseType { EddieNoiseType::white };

    // OSC2 (new)
    bool  osc2Enabled { false };
    EddieWaveform osc2Waveform { EddieWaveform::saw };
    int   osc2Semis   { 0 };          // -24..+24
    float osc2Cents   { 0.0f };       // -100..+100
    float osc2Level   { 0.5f };

    // Unison (true voices, Serum-style — per-note detuned osc stack)
    int   unisonVoices { 1 };         // 1..7
    float unisonDetuneCents { 18.0f };// 0..50
    float unisonSpread { 0.6f };      // 0..1 stereo spread

    // Amp ADSR
    float attackMs   { 8.0f };
    float decayMs    { 80.0f };
    float sustain    { 0.70f };
    float releaseMs  { 120.0f };

    // Filter (post-mix global filter — see EddieSynth.cpp note)
    EddieFilterMode filterMode { EddieFilterMode::lowPass };
    float filterCutoffHz   { 18000.0f }; // 30..20000
    float filterResonance  { 0.10f };    // 0..1
    float filterDrive      { 0.0f };     // 0..1
    float filterKeyTrack   { 0.0f };     // 0..1

    // Filter envelope (retriggered per note-on detected during playback)
    float fEnvAttackMs  { 6.0f };
    float fEnvDecayMs   { 200.0f };
    float fEnvSustain   { 0.0f };
    float fEnvReleaseMs { 200.0f };
    float fEnvAmount    { 0.0f };        // -1..+1, scaled to ~5 octaves

    // LFO
    EddieLfoShape lfoShape { EddieLfoShape::sine };
    bool  lfoSync { false };
    float lfoRateHz { 4.0f };
    EddieSyncDiv lfoSyncDiv { EddieSyncDiv::d1_4 };
    float lfoDepth { 0.0f };
    EddieLfoDest lfoDest { EddieLfoDest::off };

    // FX: Drive
    float drive { 0.0f };

    // FX: Delay
    float delayMix      { 0.0f };
    bool  delaySync     { true };
    EddieSyncDiv delayDiv { EddieSyncDiv::d1_4 };
    float delayMs       { 260.0f };       // free-mode time
    float delayFeedback { 0.24f };
    bool  delayPingPong { false };
    float delayHiCutHz  { 8000.0f };

    // FX: Chorus
    float chorusMix    { 0.0f };
    float chorusRateHz { 0.6f };
    float chorusDepth  { 0.4f };

    // FX: Reverb
    float reverbMix     { 0.0f };
    float reverbSize    { 0.36f };
    float reverbDamping { 0.42f };
};

// Convert a sync division to seconds at a given BPM.
double syncDivToSeconds (EddieSyncDiv div, double bpm);
const char* syncDivLabel (EddieSyncDiv div);

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

    EddieSynthAudioSource();

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

    // State-variable filter (TPT topology) — stereo.
    struct SvfState
    {
        float ic1eq[2] { 0.0f, 0.0f };
        float ic2eq[2] { 0.0f, 0.0f };
        void reset() { ic1eq[0] = ic1eq[1] = 0.0f; ic2eq[0] = ic2eq[1] = 0.0f; }
    };

    std::vector<MidiNote> copyNotes() const;
    void renderPreviewVoices (const juce::AudioSourceChannelInfo& info);
    void processPostEffects (const juce::AudioSourceChannelInfo& info,
                             const EddieSynthSettings& renderSettings,
                             int activeKeyForKeyTrack);
    void processFilter (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                        const EddieSynthSettings& s, int activeKey);
    void processLfo (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                     const EddieSynthSettings& s, double bpmNow,
                     float& outCutoffMod, float& outResMod);
    float advanceLfo (float phaseInc, EddieLfoShape shape);
    void updateNoteEvents (const std::vector<MidiNote>& notes,
                           int64 blockStart, int64 blockEnd);

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

    // Delay (stereo, ping-pong support)
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition { 0 };
    float delayHiCutZ[2] { 0.0f, 0.0f };

    // Filter state
    SvfState svf;

    // Filter envelope state (retriggered per note-on)
    double fEnvNoteOnSample { -1.0e18 };
    double fEnvNoteOffSample { -1.0e18 };
    int    activeKey { 60 };
    bool   keyHeld { false };

    // LFO phase (0..1)
    float lfoPhase { 0.0f };
    float lfoSampleHoldVal { 0.0f };
    int   lfoSampleHoldCounter { 0 };

    // Chorus
    juce::dsp::Chorus<float> chorus;

    juce::dsp::Reverb reverb;
};

} // namespace aidaw
