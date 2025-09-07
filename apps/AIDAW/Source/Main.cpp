#include <JuceHeader.h>
#include "ui/TopBar.h"

// ---------------- Metronome audio source ----------------
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
    void reset()                   { samplesUntilTick = 0; burstSamplesRemaining = 0; /* TODO: reset song marker to 0 */ }

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

// ---------------- Main UI ----------------
class MainComponent : public juce::Component
{
public:
    MainComponent(juce::AudioDeviceManager& dm, MetronomeSource& metro)
        : deviceManager(dm), metronome(metro)
    {
        setOpaque(true);

        addAndMakeVisible(topBar);

        // Transport & metronome
        topBar.onPlayPause = [this](bool isPlaying)
        {
            // TopBar already blocks pause while armed
            if (recordArmed && playing) return;
            playing = isPlaying;
            metronome.setPlaying(playing);
        };

        topBar.onStop = [this]()
        {
            // STOP: fully stop + reset (and later reset marker to 0)
            playing = false;
            metronome.setPlaying(false);
            metronome.reset();
            topBar.setPlaying(false);
        };

        topBar.onRecordArm = [this](bool armed)
        {
            recordArmed = armed;
            if (playing) topBar.setPlaying(true);
        };

        topBar.onTempoChanged = [this](double bpm) { metronome.setBpm(bpm); };
        topBar.onClickToggled = [this](bool on)    { metronome.setClickEnabled(on); };

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

        // Keyboard focus for spacebar
        setWantsKeyboardFocus(true);

        setSize(1200, 720);
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }

    void resized() override
    {
        auto area = getLocalBounds();
        topBar.setBounds(area.removeFromTop(56));
        // content area: 'area'
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        // SPACE = START/STOP ONLY (never pause)
        if (key.getTextCharacter() == ' ')
        {
            if (playing)
            {
                // STOP
                playing = false;
                metronome.setPlaying(false);
                metronome.reset();       // also where the future song marker resets to 0
                topBar.setPlaying(false);
            }
            else
            {
                // START (if record armed, still just plays—no pause state anywhere for space)
                playing = true;
                metronome.setPlaying(true);
                topBar.setPlaying(true);
            }
            return true;
        }
        return false;
    }

private:
    TopBar topBar;

    juce::AudioDeviceManager& deviceManager;
    MetronomeSource&          metronome;

    bool playing     { false };
    bool recordArmed { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

// ---------------- Main window / app ----------------
class MainWindow : public juce::DocumentWindow,
                   private juce::KeyListener
{
public:
    MainWindow(juce::AudioDeviceManager& dm, MetronomeSource& metro)
        : juce::DocumentWindow("AIDAW", juce::Colours::black, 0 /* no OS buttons */)
    {
        setUsingNativeTitleBar(false);   // custom TopBar
        setTitleBarHeight(0);
        setResizable(true, true);

        setContentOwned(new MainComponent(dm, metro), true);

        setBounds(juce::Desktop::getInstance().getDisplays()
                      .getPrimaryDisplay()->userArea);
        setVisible(true);

        addKeyListener(this); // for F11/Esc
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
        deviceManager.initialise(0, 2, nullptr, true);

        player.setSource(&metronome);
        deviceManager.addAudioCallback(&player);

        mainWindow = std::make_unique<MainWindow>(deviceManager, metronome);
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
    MetronomeSource          metronome;

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (AIDAWApplication)
