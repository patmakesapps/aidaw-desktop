#include <JuceHeader.h>

class MainComponent : public juce::Component
{
public:
    MainComponent()
    {
        setSize(720, 480);
        setOpaque(true);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
       g.setColour(juce::Colours::white);
g.setFont(juce::Font("Arial", 24.0f, juce::Font::plain));
g.drawFittedText("AIDAW - engine check", getLocalBounds(), juce::Justification::centred, 1);


    }
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow()
        : juce::DocumentWindow("AIDAW", juce::Colours::black, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override { juce::JUCEApplicationBase::quit(); }
};

class AIDAWApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "AIDAW"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        // basic audio init (0 inputs, 2 outputs)
        deviceManager_.initialise(0, 2, nullptr, true);
        mainWindow_ = std::make_unique<MainWindow>();
    }

    void shutdown() override
{
    mainWindow_.reset();
    deviceManager_.closeAudioDevice(); 

}


    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<MainWindow> mainWindow_;
    juce::AudioDeviceManager deviceManager_;
};

START_JUCE_APPLICATION (AIDAWApplication)
