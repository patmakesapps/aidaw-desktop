#include <JuceHeader.h>
#include <cmath>
#include <atomic>

#include "ui/TopBar.h"
#include "ui/Arranger.h"

/* ============================================================
   TimelineAudioSource
   ============================================================ */
class TimelineAudioSource : public juce::AudioSource
{
public:
    TimelineAudioSource(std::vector<TrackModel>& tracksRef, double& bpmRefIn)
        : tracks(tracksRef), bpmRef(bpmRefIn) { fmt.registerBasicFormats(); }

    void setPlaying(bool on)      { playing.store(on); }
    void resetToStart()           { playheadSamples.store(0); }
    void requestRebuild()         { needsRebuild.store(true); }

    double getPlayheadBeats() const
    {
        if (deviceSampleRate <= 0.0 || bpmRef <= 0.0) return 0.0;
        const double secPerBeat = 60.0 / bpmRef;
        return (double) playheadSamples.load() / deviceSampleRate / secPerBeat;
    }
    void setPlayheadBeats(double beats)
    {
        if (deviceSampleRate <= 0.0 || bpmRef <= 0.0)
        { pendingSeekBeats.store(juce::jmax(0.0, beats)); return; }

        const double secPerBeat = 60.0 / bpmRef;
        const int64 target = (int64) std::llround(juce::jmax(0.0, beats) * secPerBeat * deviceSampleRate);
        playheadSamples.store(target);
    }

    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRateIn) override
    {
        deviceSampleRate = sampleRateIn;
        buildReaders();
        if (pendingSeekBeats.load() > 0.0)
        {
            setPlayheadBeats(pendingSeekBeats.load());
            pendingSeekBeats.store(0.0);
        }
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

        if (deviceSampleRate <= 0.0)
            return;

        if (!playing.load())
            return;

        if (needsRebuild.exchange(false))
            buildReaders();

        if (scratch.getNumChannels() < juce::jmax(2, info.buffer->getNumChannels())
            || scratch.getNumSamples() < info.numSamples)
            scratch.setSize(juce::jmax(2, info.buffer->getNumChannels()), info.numSamples, false, false, true);

        const int64 blockStart = playheadSamples.load();
        const int64 blockEnd   = blockStart + info.numSamples;

        for (size_t i = 0; i < sources.size(); ++i)
        {
            auto* model = clipModels[i];
            const double secPerBeat = 60.0 / juce::jmax(1.0, bpmRef);

            const int64 clipStart = (int64) std::floor(model->startBeats  * secPerBeat * deviceSampleRate);
            const int64 clipLen   = (int64) std::ceil (model->lengthBeats * secPerBeat * deviceSampleRate);
            const int64 clipEnd   = clipStart + clipLen;

            const int64 segStart = std::max(blockStart, clipStart);
            const int64 segEnd   = std::min(blockEnd,   clipEnd);
            const int   segLen   = (int) std::max<int64>(0, segEnd - segStart);
            if (segLen <= 0) continue;

            const int outOffset = (int) (segStart - blockStart);

            const int64 offsetOutSamples =
                (int64) std::llround(model->offsetBeats * secPerBeat * deviceSampleRate);
            const int64 clipPosOutSamples = (segStart - clipStart) + offsetOutSamples;

            const double ratio = resampleRatios[i]; // inSamples per outSample
            int64 clipPosInSamples = (int64) std::llround((double)clipPosOutSamples * ratio);
            if (clipPosInSamples < 0) clipPosInSamples = 0;

            sources[i]->setNextReadPosition(clipPosInSamples);

            scratch.clear();
            juce::AudioSourceChannelInfo segInfo(&scratch, 0, segLen);
            resamplers[i]->getNextAudioBlock(segInfo);

            for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
            {
                const float* src = scratch.getReadPointer(juce::jmin(ch, scratch.getNumChannels()-1));
                float* dst = info.buffer->getWritePointer(ch, info.startSample + outOffset);
                juce::FloatVectorOperations::add(dst, src, segLen);
            }
        }

        playheadSamples.fetch_add(info.numSamples);
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
                if (auto* raw = fmt.createReaderFor(c.file))
                {
                    auto src = std::make_unique<juce::AudioFormatReaderSource>(raw, true);
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

    juce::AudioFormatManager fmt;
    double deviceSampleRate { 0.0 };

    std::atomic<bool>   playing { false };
    std::atomic<bool>   needsRebuild { true };
    std::atomic<int64>  playheadSamples { 0 };
    std::atomic<double> pendingSeekBeats { 0.0 };

    std::vector<std::unique_ptr<juce::AudioFormatReaderSource>> sources;
    std::vector<std::unique_ptr<juce::ResamplingAudioSource>>   resamplers;
    std::vector<double>                                         resampleRatios;
    std::vector<ClipModel*>                                     clipModels;

    juce::AudioBuffer<float> scratch;
};

/* ============================================================
   Metronome
   ============================================================ */
class MetronomeSource : public juce::AudioSource
{
public:
    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRateIn) override
    {
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

    void setBpm(double bpmIn)      { bpm = juce::jlimit(40.0, 300.0, bpmIn); updateTiming(); }
    void setPlaying(bool isOn)     { playing = isOn; }
    void setClickEnabled(bool on)  { clickEnabled = on; }
    void reset()                   { samplesUntilTick = 0; burstSamplesRemaining = 0; }

private:
    void updateTiming(){ if (sampleRate > 0.0) samplesPerBeat = sampleRate * (60.0 / bpm); }

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

/* ============================================================
   Main UI
   ============================================================ */
class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent(juce::MixerAudioSource& mix, MetronomeSource& metro)
        : mixer(mix), metronome(metro), timeline(arranger.tracks, currentBpm)
    {
        setOpaque(true);

        addAndMakeVisible(topBar);
        addAndMakeVisible(arranger);

        (void)tooltip;

        mixer.addInputSource(&metronome, false);
        mixer.addInputSource(&timeline,  false);

        arranger.onProjectChanged = [this] { timeline.requestRebuild(); };

        // Playhead (arranger <-> engine)
        arranger.onPlayheadSet = [this](double beats){ timeline.setPlayheadBeats(beats); };
        arranger.isPlayingQuery = [this]() { return playing; };

        topBar.onPlayPause = [this](bool isPlaying)
        {
            if (recordArmed && playing) return;
            playing = isPlaying;
            metronome.setPlaying(playing);
            timeline.setPlaying(playing);
            if (playing) // start from UI playhead
                timeline.setPlayheadBeats(arranger.getPlayheadBeats());
        };

        topBar.onStop = [this]()
        {
            playing = false;
            metronome.setPlaying(false);
            metronome.reset();
            timeline.setPlaying(false);
            timeline.resetToStart();
            arranger.setPlayheadBeats(0.0);
            topBar.setPlaying(false);
        };

        topBar.onRecordArm = [this](bool armed)
        {
            recordArmed = armed;
            if (playing) topBar.setPlaying(true);
        };

        topBar.onTempoChanged = [this](double bpm)
        {
            currentBpm = bpm;
            metronome.setBpm(bpm);
            arranger.setBPM(bpm);
        };

        topBar.onClickToggled = [this](bool on)
        {
            metronome.setClickEnabled(on);
            arranger.setSnap(on);
        };

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

        setWantsKeyboardFocus(true);
        setSize(1200, 720);

        // UI refresh for playhead at ~30Hz
        startTimerHz(30);
    }

    ~MainComponent() override
    {
        stopTimer();
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
        if (key.getTextCharacter() == ' ')
        {
            if (playing)
            {
                playing = false;
                metronome.setPlaying(false);
                metronome.reset();
                timeline.setPlaying(false);
                topBar.setPlaying(false);
            }
            else
            {
                playing = true;
                metronome.setPlaying(true);
                timeline.setPlaying(true);
                timeline.setPlayheadBeats(arranger.getPlayheadBeats());
                topBar.setPlaying(true);
            }
            return true;
        }
        return false;
    }

    // Ctrl + wheel = horizontal zoom at the mouse position (anywhere in window)
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCtrlDown())
        {
            arranger.zoomDeltaFromWheel(wheel.deltaY, e.getScreenPosition().getX());
            return;
        }
        juce::Component::mouseWheelMove(e, wheel);
    }

private:
    void timerCallback() override
    {
        // Follow engine playhead
        arranger.setPlayheadBeats(timeline.getPlayheadBeats());
    }

    TopBar topBar;
    Arranger arranger;

    juce::MixerAudioSource& mixer;
    MetronomeSource&        metronome;
    TimelineAudioSource     timeline;

    bool   playing     { false };
    bool   recordArmed { false };
    double currentBpm  { 120.0 };

    juce::TooltipWindow tooltip { this, 350 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

/* ============================================================
   Window / App
   ============================================================ */
class MainWindow : public juce::DocumentWindow,
                   private juce::KeyListener
{
public:
    MainWindow(juce::MixerAudioSource& mix, MetronomeSource& metro)
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
       #if JUCE_WINDOWS
        // Fixes emoji/glyph toolbuttons on Windows (avoids mojibake in tooltips)
        juce::LookAndFeel::getDefaultLookAndFeel()
            .setDefaultSansSerifTypefaceName("Segoe UI Emoji");
       #endif

        deviceManager.initialise(0, 2, nullptr, true);
        player.setSource(&mixer);
        deviceManager.addAudioCallback(&player);
        mainWindow = std::make_unique<MainWindow>(mixer, metronome);
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
