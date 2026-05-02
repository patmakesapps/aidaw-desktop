#include "MainWindow.h"

#include "MainComponent.h"

namespace aidaw
{

MainWindow::MainWindow (juce::MixerAudioSource& mix, MetronomeSource& metro)
    : juce::DocumentWindow ("AIDAW", juce::Colours::black, 0)
{
    setUsingNativeTitleBar (false);
    setTitleBarHeight (0);
    setResizable (true, true);
    setContentOwned (new MainComponent (mix, metro), true);

    setBounds (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea);
    setVisible (true);
    addKeyListener (this);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplicationBase::getInstance()->systemRequestedQuit();
}

bool MainWindow::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (key.getKeyCode() == juce::KeyPress::F11Key)
    {
        setFullScreen (! isFullScreen());
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::escapeKey && isFullScreen())
    {
        setFullScreen (false);
        return true;
    }

    return false;
}

} // namespace aidaw
