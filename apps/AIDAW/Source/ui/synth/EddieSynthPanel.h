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

    void styleCombo (juce::ComboBox& combo);
    void styleButton (juce::Button& b);

    void updateSettingsFromSliders();
    void updateSettingsFromCombos();
    void updateSlidersFromSettings();
    void loadPresets();
    void savePresets() const;
    void refreshPresetMenu();
    void addPreset (const juce::String& name, const EddieSynthSettings& settings);
    juce::File presetFile() const;

    void buttonClicked (juce::Button* button) override;
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;

    enum class Tab { Osc, Filter, Mod, Fx };
    void setActiveTab (Tab t);
    void applyTabVisibility();

    Tab activeTab { Tab::Osc };

    juce::TextButton tabOsc, tabFilter, tabMod, tabFx;

    EddieSynthSettings currentSettings;
    std::vector<Preset> presets;

    juce::Label title, subtitle;
    juce::ComboBox presetMenu;
    juce::TextEditor presetName;
    juce::TextButton savePreset, closeButton, monoToggle;

    // Section labels
    juce::Label osc1Header, osc2Header, unisonHeader, filterHeader, fEnvHeader,
                ampEnvHeader, lfoHeader, fxHeader, masterHeader;

    // OSC1
    juce::Label osc1WaveLabel;
    juce::ComboBox osc1WaveMenu;
    juce::Label sawLabel, subLabel, noiseLabel, osc1LevelLabel;
    juce::Slider saw, sub, noise, osc1Level;

    // OSC2
    juce::Label osc2WaveLabel;
    juce::ComboBox osc2WaveMenu;
    juce::TextButton osc2EnabledBtn;
    juce::Label osc2SemisLabel, osc2CentsLabel, osc2LevelLabel;
    juce::Slider osc2Semis, osc2Cents, osc2Level;

    // Unison
    juce::Label unisonVoicesLabel, unisonDetuneLabel, unisonSpreadLabel;
    juce::Slider unisonVoices, unisonDetune, unisonSpread;

    // Filter
    juce::Label filterModeLabel;
    juce::ComboBox filterModeMenu;
    juce::Label cutoffLabel, resoLabel, fDriveLabel, kTrkLabel;
    juce::Slider cutoff, reso, fDrive, kTrk;

    // Filter env
    juce::Label fAttackLabel, fDecayLabel, fSustainLabel, fReleaseLabel, fAmountLabel;
    juce::Slider fAttack, fDecay, fSustain, fRelease, fAmount;

    // Amp env
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    juce::Slider attack, decay, sustain, release;

    // LFO
    juce::Label lfoShapeLabel, lfoDestLabel, lfoRateLabel, lfoDepthLabel, lfoDivLabel;
    juce::ComboBox lfoShapeMenu, lfoDestMenu, lfoDivMenu;
    juce::TextButton lfoSyncBtn;
    juce::Slider lfoRate, lfoDepth;

    // FX: Drive
    juce::Label driveLabel;
    juce::Slider drive;

    // FX: Delay
    juce::Label delayMixLabel, delayTimeLabel, delayDivLabel, delayFbLabel, delayHiCutLabel;
    juce::Slider delayMix, delayTime, delayFeedback, delayHiCut;
    juce::ComboBox delayDivMenu;
    juce::TextButton delaySyncBtn, delayPingPongBtn;

    // FX: Chorus
    juce::Label chorusMixLabel, chorusRateLabel, chorusDepthLabel;
    juce::Slider chorusMix, chorusRate, chorusDepth;

    // FX: Reverb
    juce::Label reverbMixLabel, reverbSizeLabel, reverbDampingLabel;
    juce::Slider reverbMix, reverbSize, reverbDamping;

    // Master
    juce::Label gainLabel, polyLabel;
    juce::Slider gain, polyphony;

    MiniPreviewKeyboard previewKeyboard;

    juce::Image faceplateImage;
    std::unique_ptr<juce::LookAndFeel_V4> knobLook;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EddieSynthPanel)
};

} // namespace aidaw
