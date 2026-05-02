#include <JuceHeader.h>

#include "app/MainWindow.h"
#include "audio/MetronomeSource.h"

class AIDAWApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "AIDAW"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
       #if JUCE_WINDOWS
        juce::LookAndFeel::getDefaultLookAndFeel()
            .setDefaultSansSerifTypefaceName ("Segoe UI Emoji");
       #endif

        deviceManager.initialise (0, 2, nullptr, true);
        player.setSource (&mixer);
        deviceManager.addAudioCallback (&player);
        mainWindow = std::make_unique<aidaw::MainWindow> (mixer, metronome);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        player.setSource (nullptr);
        deviceManager.removeAudioCallback (&player);
        deviceManager.closeAudioDevice();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer player;
    juce::MixerAudioSource mixer;
    aidaw::MetronomeSource metronome;

    std::unique_ptr<aidaw::MainWindow> mainWindow;
};

START_JUCE_APPLICATION (AIDAWApplication)
