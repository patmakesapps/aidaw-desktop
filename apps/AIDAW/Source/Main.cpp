#include <JuceHeader.h>
#include <cmath>
#include "ui/TopBar.h"
#include "ui/Arranger.h"


/* ---------------- Simple timeline playback that mixes all clips ----------------
   - No time-stretching; clips play at natural speed, positioned by startBeats.
   - If an audio file's sample-rate differs from the device, we resample on the fly.
------------------------------------------------------------------------------- */
class TimelineAudioSource : public juce::AudioSource
{
public:
    TimelineAudioSource(std::vector<TrackModel>& tracksRef, double& bpmRefIn)
        : tracks(tracksRef), bpmRef(bpmRefIn)
    {
        fmt.registerBasicFormats();
    }

    void setPlaying(bool on)      { playing = on; }
    void resetToStart()           { playheadSamples = 0; }
    void requestRebuild()         { needsRebuild = true; }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRateIn) override
    {
        juce::ignoreUnused(samplesPerBlockExpected);
        deviceSampleRate = sampleRateIn;
        buildReaders();
    }

    void releaseResources() override
    {
        resamplers.clear();
        sources.clear();
        clipModels.clear();
        resampleRatios.clear();
        scratch.setSize(0,0);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();
        if (!playing || deviceSampleRate <= 0.0)
        {
            playheadSamples += info.numSamples;
            return;
        }

        if (needsRebuild) { buildReaders(); needsRebuild = false; }

        if (scratch.getNumChannels() < juce::jmax(2, info.buffer->getNumChannels()) || scratch.getNumSamples() < info.numSamples)
            scratch.setSize(juce::jmax(2, info.buffer->getNumChannels()), info.numSamples, false, false, true);

        const int64 blockStart = playheadSamples;
        const int64 blockEnd   = blockStart + info.numSamples;

        for (size_t i = 0; i < sources.size(); ++i)
        {
            auto* model = clipModels[i];
            const double secPerBeat = 60.0 / juce::jmax(1.0, bpmRef);
            const int64 clipStart  = (int64) std::llround(model->startBeats * secPerBeat * deviceSampleRate);
            const int64 clipLen    = (int64) std::llround(model->lengthBeats * secPerBeat * deviceSampleRate);
            const int64 clipEnd    = clipStart + clipLen;

            // overlap with current block?
            const int64 segStart = std::max(blockStart, clipStart);
            const int64 segEnd   = std::min(blockEnd,   clipEnd);
            const int   segLen   = (int) std::max<int64>(0, segEnd - segStart);
            if (segLen <= 0) continue;

            // where to put it in output buffer
            const int outOffset = (int) (segStart - blockStart);

            // position inside the source (in the reader's sample-rate domain)
            const double ratio = resampleRatios[i]; // inSamples per outSample
            const int64 clipPosOutSamples = segStart - clipStart;
            const int64 clipPosInSamples  = (int64) std::llround((double)clipPosOutSamples * ratio);

            sources[i]->setNextReadPosition(clipPosInSamples);

            scratch.clear();
            juce::AudioSourceChannelInfo segInfo(&scratch, 0, segLen);
            resamplers[i]->getNextAudioBlock(segInfo);

            // mix into destination
            for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
            {
                const float* src = scratch.getReadPointer(juce::jmin(ch, scratch.getNumChannels()-1));
                float* dst = info.buffer->getWritePointer(ch, info.startSample + outOffset);
                juce::FloatVectorOperations::add(dst, src, segLen);
            }
        }

        playheadSamples += info.numSamples;
    }

private:
    void buildReaders()
    {
        resamplers.clear(); sources.clear(); clipModels.clear(); resampleRatios.clear();

        for (auto& t : tracks)
        {
            for (auto& c : t.clips)
            {
                if (!c.file.existsAsFile()) continue;

                if (auto* raw = fmt.createReaderFor(c.file))  // ownership passed to readerSource
                {
                    auto src = std::make_unique<juce::AudioFormatReaderSource>(raw, true /* take ownership */);
                    auto res = std::make_unique<juce::ResamplingAudioSource>(src.get(), false, 2);

                    const double ratio = raw->sampleRate / deviceSampleRate; // inSamples per outSample
                    res->setResamplingRatio(std::max(0.01, ratio));
                    res->prepareToPlay(512, deviceSampleRate);

                    clipModels.push_back(&c);
                    sources.emplace_back(std::move(src));
                    resamplers.emplace_back(std::move(res));
                    resampleRatios.push_back(ratio);
                }
            }
        }

        scratch.setSize(2, 512);
    }

    std::vector<TrackModel>& tracks;
    double& bpmRef;

    juce::AudioFormatManager fmt;   // non-copyable; we register in ctor
    double deviceSampleRate { 0.0 };
    bool   playing { false };
    bool   needsRebuild { true };
    int64  playheadSamples { 0 };

    // per-clip playback chain
    std::vector<std::unique_ptr<juce::AudioFormatReaderSource>> sources;
    std::vector<std::unique_ptr<juce::ResamplingAudioSource>>   resamplers;
    std::vector<double>                                         resampleRatios;
    std::vector<ClipModel*>                                     clipModels;

    juce::AudioBuffer<float> scratch;
};

/* ---------------- Metronome audio source ---------------- */
class MetronomeSource : public juce::AudioSource
{
public:
    void prepareToPlay (int samplesPerBlockExpected, double sampleRateIn) override
    {
        juce::ignoreUnused(samplesPerBlockExpected);
        sampleRate = sampleRateIn;
        updateTiming();
        phase = 0.0;
        reset();
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto* left  = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* right = bufferToFill.buffer->getNumChannels() > 1
                        ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample)
                        : nullptr;

        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            float sample = 0.0f;

            if (playing && clickEnabled && sampleRate > 0.0)
            {
                if (samplesUntilTick <= 0)
                {
                    // short 1kHz sine burst (~8ms)
                    burstSamplesRemaining = (int)(0.008 * sampleRate);
                    samplesUntilTick += (int)samplesPerBeat;
                }

                if (burstSamplesRemaining > 0)
                {
                    auto env   = (float)burstSamplesRemaining / (float)(0.008 * sampleRate);
                    sample     = (float)std::sin(phase) * 0.25f * env;
                    phase     += twoPi * 1000.0 / sampleRate;
                    if (phase > twoPi) phase -= twoPi;
                    --burstSamplesRemaining;
                }

                --samplesUntilTick;
            }

            left[i] = left[i] + sample;
            if (right) right[i] = right[i] + sample;
        }
    }

    // Controls
    void setBpm(double bpmIn)      { bpm = juce::jlimit(40.0, 300.0, bpmIn); updateTiming(); }
    void setPlaying(bool isOn)     { playing = isOn; }
    void setClickEnabled(bool on)  { clickEnabled = on; }
    void reset()                   { samplesUntilTick = 0; burstSamplesRemaining = 0; }

private:
    void updateTiming()
    {
        if (sampleRate > 0.0)
            samplesPerBeat = sampleRate * (60.0 / bpm);
    }

    double sampleRate { 0.0 };
    double bpm        { 120.0 };
    double samplesPerBeat { 0.0 };

    bool   playing { false };
    bool   clickEnabled { true };

    int    samplesUntilTick { 0 };
    int    burstSamplesRemaining { 0 };

    static constexpr double twoPi = juce::MathConstants<double>::twoPi;
    double phase { 0.0 };
};

/* ---------------- Main UI ---------------- */
class MainComponent : public juce::Component
{
public:
    MainComponent(juce::MixerAudioSource& mix, MetronomeSource& metro)
        : mixer(mix), metronome(metro), timeline(arranger.tracks, currentBpm)
    {
        setOpaque(true);

        addAndMakeVisible(topBar);
        addAndMakeVisible(arranger);

        // audio graph
        mixer.addInputSource(&metronome, false);
        mixer.addInputSource(&timeline,  false);

        arranger.onProjectChanged = [this] { timeline.requestRebuild(); };

        // Transport & metronome + timeline
        topBar.onPlayPause = [this](bool isPlaying)
        {
            if (recordArmed && playing) return;
            playing = isPlaying;
            metronome.setPlaying(playing);
            timeline.setPlaying(playing);
        };

        topBar.onStop = [this]()
        {
            playing = false;
            metronome.setPlaying(false);
            metronome.reset();
            timeline.setPlaying(false);
            timeline.resetToStart();
            topBar.setPlaying(false);
        };

        topBar.onRecordArm = [this](bool armed)
        {
            recordArmed = armed;
            if (playing) topBar.setPlaying(true);
        };

        // Sync BPM + snap with Arranger
        topBar.onTempoChanged = [this](double bpm)
        {
            currentBpm = bpm;
            metronome.setBpm(bpm);
            arranger.setBPM(bpm);
        };

        // For now, map Click toggle to Arranger snap
        topBar.onClickToggled = [this](bool on)
        {
            metronome.setClickEnabled(on);
            arranger.setSnap(on);
        };

        // Window controls
        topBar.onMinimize = [this]()
        {
            if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
                dw->setMinimised(true);
        };
        topBar.onClose = []()
        {
            if (auto* app = juce::JUCEApplicationBase::getInstance())
                app->systemRequestedQuit();
        };

        topBar.onTitleChanged = [](const juce::String& newTitle)
        {
            juce::Logger::writeToLog("Project title changed: " + newTitle);
        };

        if (arranger.tracks.empty())
            arranger.addTrack("Audio 1");

        setWantsKeyboardFocus(true);
        setSize(1200, 720);
    }

    ~MainComponent() override
    {
        mixer.removeInputSource(&timeline);
        mixer.removeInputSource(&metronome);
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }

    void resized() override
    {
        auto area = getLocalBounds();
        topBar.setBounds(area.removeFromTop(56));
        arranger.setBounds(area.reduced(8, 8));
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        // SPACE = START/STOP ONLY
        if (key.getTextCharacter() == ' ')
        {
            if (playing)
            {
                playing = false;
                metronome.setPlaying(false);
                metronome.reset();
                timeline.setPlaying(false);
                timeline.resetToStart();
                topBar.setPlaying(false);
            }
            else
            {
                playing = true;
                metronome.setPlaying(true);
                timeline.setPlaying(true);
                topBar.setPlaying(true);
            }
            return true;
        }
        return false;
    }

    // Ctrl+wheel zooms the Arranger
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCtrlDown())
        {
            arranger.zoomDelta(wheel.deltaY);
            return;
        }
        juce::Component::mouseWheelMove(e, wheel);
    }

private:
    TopBar topBar;
    Arranger arranger;

    juce::MixerAudioSource& mixer;
    MetronomeSource&        metronome;
    TimelineAudioSource     timeline;

    bool   playing     { false };
    bool   recordArmed { false };
    double currentBpm  { 120.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

/* ---------------- Main window / app ---------------- */
class MainWindow : public juce::DocumentWindow,
                   private juce::KeyListener
{
public:
    MainWindow(juce::AudioDeviceManager& dm, juce::MixerAudioSource& mix, MetronomeSource& metro)
        : juce::DocumentWindow("AIDAW", juce::Colours::black, 0 /* no OS buttons */)
    {
        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
        setResizable(true, true);

        setContentOwned(new MainComponent(mix, metro), true);

        setBounds(juce::Desktop::getInstance().getDisplays()
                      .getPrimaryDisplay()->userArea);
        setVisible(true);

        addKeyListener(this);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplicationBase::getInstance()->systemRequestedQuit();
    }

private:
    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        if (key.getKeyCode() == juce::KeyPress::F11Key) { setFullScreen(!isFullScreen()); return true; }
        if (key.getKeyCode() == juce::KeyPress::escapeKey && isFullScreen()) { setFullScreen(false); return true; }
        return false;
    }
};

class AIDAWApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "AIDAW"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        // --- Windows emoji/UTF-8 glyph fix for toolbar icons (🔍 ✂ ↔ ⛓, etc.) ---
        #if JUCE_WINDOWS
        juce::LookAndFeel::getDefaultLookAndFeel()
            .setDefaultSansSerifTypefaceName("Segoe UI Emoji");
        #endif

        deviceManager.initialise(0, 2, nullptr, true);

        player.setSource(&mixer);
        deviceManager.addAudioCallback(&player);

        mainWindow = std::make_unique<MainWindow>(deviceManager, mixer, metronome);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        player.setSource(nullptr);
        deviceManager.removeAudioCallback(&player);
        deviceManager.closeAudioDevice();
    }

    void systemRequestedQuit() override { quit(); }

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer  player;
    juce::MixerAudioSource   mixer;
    MetronomeSource          metronome;

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (AIDAWApplication)
