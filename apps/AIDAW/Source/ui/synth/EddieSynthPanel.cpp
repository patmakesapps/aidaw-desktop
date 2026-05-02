#include "EddieSynthPanel.h"

#include "../shared/Theme.h"
#include "../shared/ThemeManager.h"

namespace aidaw
{

namespace
{
constexpr uint32 panelBg = 0xFF10151C;
constexpr uint32 controlBg = 0xFF151D26;
constexpr uint32 controlLine = 0x5538E0FF;
}

EddieSynthPanel::EddieSynthPanel()
{
    setOpaque (false);
    ThemeManager::get().addChangeListener(this);

    title.setText ("Eddie", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setColour (juce::Label::textColourId, juce::Colours::white);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    addAndMakeVisible (title);

    presetMenu.addListener (this);
    addAndMakeVisible (presetMenu);

    presetName.setText ("New preset", juce::dontSendNotification);
    presetName.setSelectAllWhenFocused (true);
    presetName.setColour (juce::TextEditor::backgroundColourId, juce::Colour (controlBg));
    presetName.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    presetName.setColour (juce::TextEditor::outlineColourId, juce::Colour (0x334A6074));
    addAndMakeVisible (presetName);

    for (auto* button : { &savePreset, &closeButton, &previewC, &previewA })
    {
        button->addListener (this);
        addAndMakeVisible (button);
    }

    savePreset.setButtonText ("Save");
    closeButton.setButtonText ("X");
    previewC.setButtonText ("C4");
    previewA.setButtonText ("A4");

    configureSlider (gain, gainLabel, "Gain", 0.02, 0.60, 0.01);
    configureSlider (saw, sawLabel, "Saw", 0.0, 1.0, 0.01);
    configureSlider (sub, subLabel, "Sub", 0.0, 0.60, 0.01);
    configureSlider (attack, attackLabel, "Attack", 1.0, 250.0, 1.0);
    configureSlider (decay, decayLabel, "Decay", 1.0, 500.0, 1.0);
    configureSlider (sustain, sustainLabel, "Sustain", 0.0, 1.0, 0.01);
    configureSlider (release, releaseLabel, "Release", 5.0, 1000.0, 1.0);

    loadPresets();
    if (presets.empty())
    {
        addPreset ("Eddie Init", currentSettings);
        auto soft = currentSettings;
        soft.sawMix = 0.24f;
        soft.subMix = 0.08f;
        soft.attackMs = 18.0f;
        soft.releaseMs = 220.0f;
        addPreset ("Soft Keys", soft);

        auto bass = currentSettings;
        bass.outputGain = 0.22f;
        bass.sawMix = 0.38f;
        bass.subMix = 0.42f;
        bass.attackMs = 4.0f;
        bass.decayMs = 120.0f;
        bass.sustain = 0.58f;
        bass.releaseMs = 90.0f;
        addPreset ("Sub Bass", bass);
    }

    refreshPresetMenu();
    updateSlidersFromSettings();
}

void EddieSynthPanel::setSettings (const EddieSynthSettings& settings)
{
    currentSettings = settings;
    updateSlidersFromSettings();
}

EddieSynthSettings EddieSynthPanel::getSettings() const
{
    return currentSettings;
}

void EddieSynthPanel::configureSlider (juce::Slider& slider,
                                       juce::Label& label,
                                       const juce::String& labelText,
                                       double min,
                                       double max,
                                       double interval)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.82f));
    addAndMakeVisible (label);

    slider.setRange (min, max, interval);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 22);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (Theme::colNoteA));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0x334A6074));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (controlBg));
    slider.onValueChange = [this] { updateSettingsFromSliders(); };
    addAndMakeVisible (slider);
}

void EddieSynthPanel::updateSettingsFromSliders()
{
    currentSettings.outputGain = (float) gain.getValue();
    currentSettings.sawMix = (float) saw.getValue();
    currentSettings.subMix = (float) sub.getValue();
    currentSettings.attackMs = (float) attack.getValue();
    currentSettings.decayMs = (float) decay.getValue();
    currentSettings.sustain = (float) sustain.getValue();
    currentSettings.releaseMs = (float) release.getValue();

    if (onSettingsChanged)
        onSettingsChanged (currentSettings);
}

void EddieSynthPanel::updateSlidersFromSettings()
{
    gain.setValue (currentSettings.outputGain, juce::dontSendNotification);
    saw.setValue (currentSettings.sawMix, juce::dontSendNotification);
    sub.setValue (currentSettings.subMix, juce::dontSendNotification);
    attack.setValue (currentSettings.attackMs, juce::dontSendNotification);
    decay.setValue (currentSettings.decayMs, juce::dontSendNotification);
    sustain.setValue (currentSettings.sustain, juce::dontSendNotification);
    release.setValue (currentSettings.releaseMs, juce::dontSendNotification);
}

juce::File EddieSynthPanel::presetFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("AIDAW");
    return dir.getChildFile ("eddie_presets.xml");
}

void EddieSynthPanel::loadPresets()
{
    presets.clear();
    const auto file = presetFile();
    if (! file.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse (file);
    if (! xml || ! xml->hasTagName ("EddiePresets"))
        return;

    for (auto* child : xml->getChildIterator())
    {
        if (! child->hasTagName ("Preset"))
            continue;

        Preset preset;
        preset.name = child->getStringAttribute ("name", "Preset");
        preset.settings.outputGain = (float) child->getDoubleAttribute ("gain", preset.settings.outputGain);
        preset.settings.sawMix = (float) child->getDoubleAttribute ("saw", preset.settings.sawMix);
        preset.settings.subMix = (float) child->getDoubleAttribute ("sub", preset.settings.subMix);
        preset.settings.attackMs = (float) child->getDoubleAttribute ("attack", preset.settings.attackMs);
        preset.settings.decayMs = (float) child->getDoubleAttribute ("decay", preset.settings.decayMs);
        preset.settings.sustain = (float) child->getDoubleAttribute ("sustain", preset.settings.sustain);
        preset.settings.releaseMs = (float) child->getDoubleAttribute ("release", preset.settings.releaseMs);
        presets.push_back (preset);
    }
}

void EddieSynthPanel::savePresets() const
{
    auto file = presetFile();
    file.getParentDirectory().createDirectory();

    juce::XmlElement root ("EddiePresets");
    for (const auto& preset : presets)
    {
        auto* item = root.createNewChildElement ("Preset");
        item->setAttribute ("name", preset.name);
        item->setAttribute ("gain", preset.settings.outputGain);
        item->setAttribute ("saw", preset.settings.sawMix);
        item->setAttribute ("sub", preset.settings.subMix);
        item->setAttribute ("attack", preset.settings.attackMs);
        item->setAttribute ("decay", preset.settings.decayMs);
        item->setAttribute ("sustain", preset.settings.sustain);
        item->setAttribute ("release", preset.settings.releaseMs);
    }

    root.writeTo (file);
}

void EddieSynthPanel::refreshPresetMenu()
{
    presetMenu.clear (juce::dontSendNotification);
    for (int i = 0; i < (int) presets.size(); ++i)
        presetMenu.addItem (presets[(size_t) i].name, i + 1);

    if (! presets.empty())
        presetMenu.setSelectedId (1, juce::dontSendNotification);
}

void EddieSynthPanel::addPreset (const juce::String& name, const EddieSynthSettings& settings)
{
    Preset preset { name.trim().isNotEmpty() ? name.trim() : "Preset", settings };
    presets.push_back (preset);
}

void EddieSynthPanel::buttonClicked (juce::Button* button)
{
    if (button == &closeButton)
    {
        if (onClose)
            onClose();
        return;
    }

    if (button == &savePreset)
    {
        addPreset (presetName.getText(), currentSettings);
        savePresets();
        refreshPresetMenu();
        presetMenu.setSelectedId ((int) presets.size(), juce::dontSendNotification);
        return;
    }

    if (button == &previewC && onPreviewNote)
        onPreviewNote (60, 100);
    else if (button == &previewA && onPreviewNote)
        onPreviewNote (69, 100);
}

void EddieSynthPanel::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged != &presetMenu)
        return;

    const int index = presetMenu.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentSettings = presets[(size_t) index].settings;
    presetName.setText (presets[(size_t) index].name, juce::dontSendNotification);
    updateSlidersFromSettings();
    if (onSettingsChanged)
        onSettingsChanged (currentSettings);
}

void EddieSynthPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillRect (getLocalBounds());

    auto panel = getLocalBounds().reduced (18).toFloat();
    g.setColour (juce::Colour (Theme::colBgPanel));
    g.fillRoundedRectangle (panel, 10.0f);
    g.setColour (juce::Colour (Theme::colAccent).withAlpha (0.35f));
    g.drawRoundedRectangle (panel, 10.0f, 1.4f);
}

EddieSynthPanel::~EddieSynthPanel()
{
    ThemeManager::get().removeChangeListener(this);
}

void EddieSynthPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto* s : { &gain, &saw, &sub, &attack, &decay, &sustain, &release })
    {
        s->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(Theme::colAccent));
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(Theme::colBgPanel));
    }
    presetName.setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::colBgPanel));
    presetName.setColour(juce::TextEditor::textColourId,       juce::Colour(Theme::colText));
    repaint();
}

void EddieSynthPanel::resized()
{
    auto area = getLocalBounds().reduced (34);
    auto header = area.removeFromTop (44);
    title.setBounds (header.removeFromLeft (160));
    closeButton.setBounds (header.removeFromRight (38).reduced (2));
    header.removeFromRight (8);
    previewA.setBounds (header.removeFromRight (56).reduced (2));
    previewC.setBounds (header.removeFromRight (56).reduced (2));

    area.removeFromTop (12);
    auto presetRow = area.removeFromTop (36);
    presetMenu.setBounds (presetRow.removeFromLeft (220));
    presetRow.removeFromLeft (10);
    presetName.setBounds (presetRow.removeFromLeft (220));
    presetRow.removeFromLeft (10);
    savePreset.setBounds (presetRow.removeFromLeft (80));

    area.removeFromTop (18);
    const int labelH = 20;
    const int knobW = 104;
    const int knobH = 126;
    auto row1 = area.removeFromTop (labelH + knobH);
    auto row2 = area.removeFromTop (labelH + knobH);

    auto place = [] (juce::Rectangle<int>& row, juce::Label& label, juce::Slider& slider, int w, int labelHeight, int knobHeight)
    {
        auto cell = row.removeFromLeft (w);
        label.setBounds (cell.removeFromTop (labelHeight));
        slider.setBounds (cell.removeFromTop (knobHeight));
        row.removeFromLeft (10);
    };

    place (row1, gainLabel, gain, knobW, labelH, knobH);
    place (row1, sawLabel, saw, knobW, labelH, knobH);
    place (row1, subLabel, sub, knobW, labelH, knobH);
    place (row1, sustainLabel, sustain, knobW, labelH, knobH);

    place (row2, attackLabel, attack, knobW, labelH, knobH);
    place (row2, decayLabel, decay, knobW, labelH, knobH);
    place (row2, releaseLabel, release, knobW, labelH, knobH);
}

} // namespace aidaw
