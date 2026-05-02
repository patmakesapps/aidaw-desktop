#pragma once

#include <JuceHeader.h>
#include <functional>

class AIDAWLook : public juce::LookAndFeel_V4
{
public:
    AIDAWLook();

    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour& base,
                               bool highlighted,
                               bool down) override;
};

class TopBar : public juce::Component,
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

private:
    void buttonClicked (juce::Button* button) override;
    void textEditorReturnKeyPressed (juce::TextEditor& editor) override;
    void textEditorFocusLost (juce::TextEditor& editor) override;
    void applyBpmFrom (juce::TextEditor& editor);

    AIDAWLook look;

    juce::TextButton playPause, stop, record;
    juce::TextButton modeComposer, modeMidi, modeMixer;
    juce::Label clusterDot;
    juce::Label title;

    juce::Rectangle<int> bpmPillBounds;
    juce::Label bpmLabelLeft, bpmLabelRight;
    juce::TextButton minusBtn, plusBtn;
    juce::TextEditor bpmEdit;
    juce::ToggleButton clickToggle;

    juce::TextButton btnMin, btnClose;

    bool isPlaying { false };
    bool recordArmed { false };
    double currentBPM { 120.0 };
    bool clickEnabled { true };
    AppMode currentMode { AppMode::Composer };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
};
