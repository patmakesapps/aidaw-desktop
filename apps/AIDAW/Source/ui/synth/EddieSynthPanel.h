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
    juce::ComboBox presetMenu;
    juce::TextEditor presetName;
    juce::TextButton savePreset, closeButton, previewC, previewA;

    juce::Label gainLabel, sawLabel, subLabel, attackLabel, decayLabel, sustainLabel, releaseLabel;
    juce::Slider gain, saw, sub, attack, decay, sustain, release;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EddieSynthPanel)
};

} // namespace aidaw
