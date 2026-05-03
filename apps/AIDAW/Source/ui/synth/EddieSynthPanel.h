#pragma once

#include <JuceHeader.h>

#include "../../audio/EddieSynth.h"

namespace aidaw
{

class EddieSynthPanel : public juce::Component,
                        public juce::ChangeListener,
                        private juce::Button::Listener,
                        private juce::ComboBox::Listener
{
public:
    EddieSynthPanel();
    ~EddieSynthPanel() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void setSettings (const EddieSynthSettings& settings);
    EddieSynthSettings getSettings() const;

    std::function<void(const EddieSynthSettings&)> onSettingsChanged;
    std::function<void(int, int)> onPreviewNote;
    std::function<void()> onClose;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class MiniPreviewKeyboard : public juce::Component
    {
    public:
        std::function<void(int, int)> onPreviewNote;

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        int noteForPosition (juce::Point<int> position) const;
        void triggerAt (juce::Point<int> position, bool forceRetrigger);

        int activeNote { -1 };
    };

    struct Preset
    {
        juce::String name;
        EddieSynthSettings settings;
    };

    void configureSlider (juce::Slider& slider,
                          juce::Label& label,
                          const juce::String& labelText,
                          double min,
                          double max,
                          double interval);

    void updateSettingsFromSliders();
    void updateSettingsFromWaveform();
    void updateSlidersFromSettings();
    void loadPresets();
    void savePresets() const;
    void refreshPresetMenu();
    void addPreset (const juce::String& name, const EddieSynthSettings& settings);
    juce::File presetFile() const;

    void buttonClicked (juce::Button* button) override;
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;

    EddieSynthSettings currentSettings;
    std::vector<Preset> presets;

    juce::Label title;
    juce::Label subtitle;
    juce::ComboBox presetMenu;
    juce::Label waveformLabel;
    juce::ComboBox waveformMenu;
    juce::TextEditor presetName;
    juce::TextButton savePreset, closeButton, monoToggle;
    MiniPreviewKeyboard previewKeyboard;

    juce::Label gainLabel, sawLabel, subLabel, attackLabel, decayLabel, sustainLabel, releaseLabel;
    juce::Label voicesLabel;
    juce::Label driveLabel, delayMixLabel, delayTimeLabel, delayFeedbackLabel;
    juce::Label reverbMixLabel, reverbSizeLabel, reverbDampingLabel;
    juce::Slider gain, saw, sub, attack, decay, sustain, release;
    juce::Slider voices;
    juce::Slider drive, delayMix, delayTime, delayFeedback, reverbMix, reverbSize, reverbDamping;

    juce::Image faceplateImage;
    std::unique_ptr<juce::LookAndFeel_V4> knobLook;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EddieSynthPanel)
};

} // namespace aidaw
