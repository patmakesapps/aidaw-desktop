#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../shared/Icons.h"

class TopBar : public juce::Component,
               public juce::ChangeListener,
               private juce::Button::Listener,
               private juce::TextEditor::Listener
{
public:
    enum class AppMode { Composer, Midi, Mixer };

    TopBar();
    ~TopBar() override;

    std::function<void(bool)> onPlayPause;
    std::function<void()> onStop;
    std::function<void(bool)> onRecordArm;
    std::function<void(double)> onTempoChanged;
    std::function<void(bool)> onClickToggled;
    std::function<void()> onMinimize;
    std::function<void()> onClose;
    std::function<void(const juce::String&)> onTitleChanged;
    std::function<void(AppMode)> onModeChanged;

    void setPlaying (bool shouldPlay);
    void setRecordArmed (bool armed);
    double getBPM() const;
    void setBPM (double bpm);
    void setClickEnabled (bool enabled);
    void setMode (AppMode mode, bool fireCallback = true);
    AppMode getMode() const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    void buttonClicked (juce::Button* button) override;
    void textEditorReturnKeyPressed (juce::TextEditor& editor) override;
    void textEditorFocusLost (juce::TextEditor& editor) override;
    void applyBpmFrom (juce::TextEditor& editor);
    void refreshColours();
    void showThemeMenu();

    // Transport (icon buttons)
    IconButton playPause { "Play (Space)",  Icons::play(),    IconButton::Style::Filled };
    IconButton stop      { "Stop",          Icons::stop(),    IconButton::Style::Filled };
    IconButton record    { "Arm record",    Icons::record(),  IconButton::Style::Filled };

    // Segmented mode switch (three buttons rendered as connected pills)
    juce::TextButton modeComposer { "Composer" };
    juce::TextButton modeMidi     { "MIDI" };
    juce::TextButton modeMixer    { "Mixer" };

    juce::Label title;

    // Tempo pill
    juce::Rectangle<int> bpmPillBounds;
    juce::Label bpmLabel;
    IconButton minusBtn { "Tempo -1", Icons::minus(), IconButton::Style::Stroked };
    IconButton plusBtn  { "Tempo +1", Icons::plus(),  IconButton::Style::Stroked };
    juce::TextEditor bpmEdit;

    // Click / metronome toggle
    IconButton clickToggle { "Metronome on/off", Icons::metronome(), IconButton::Style::Stroked };

    // Theme + window
    IconButton btnTheme { "Switch theme",  Icons::palette(),  IconButton::Style::Stroked };
    IconButton btnMin   { "Minimize",      Icons::minimize(), IconButton::Style::Stroked };
    IconButton btnClose { "Close",         Icons::closeX(),   IconButton::Style::Stroked };

    bool isPlaying { false };
    bool recordArmed { false };
    double currentBPM { 120.0 };
    bool clickEnabled { true };
    AppMode currentMode { AppMode::Composer };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
};
