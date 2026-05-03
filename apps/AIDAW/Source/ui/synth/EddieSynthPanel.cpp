#include "EddieSynthPanel.h"

#include "../shared/Theme.h"
#include "../shared/ThemeManager.h"
#include "BinaryData.h"

namespace aidaw
{

namespace
{
constexpr uint32 panelBg = 0xFF10151C;
constexpr uint32 controlBg = 0xFF151D26;
constexpr uint32 controlLine = 0x5538E0FF;
constexpr uint32 vintageCream = 0xFFEFD9A5;
constexpr uint32 vintageOrange = 0xFFFF7A1A;
constexpr uint32 vintageRed = 0xFFD02816;
constexpr uint32 vintageAmber = 0xFFFFB84A;

class EddieKnobLook final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                        .reduced (7.0f, 5.0f);
        bounds.removeFromBottom (24.0f);

        const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto knob = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
        const auto centre = knob.getCentre();
        const auto radius = knob.getWidth() * 0.5f;
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour (juce::Colour (0xAA000000));
        g.fillEllipse (knob.translated (0.0f, 4.0f));

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius + 4.0f, radius + 4.0f, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (vintageCream).withAlpha (0.26f));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius + 4.0f, radius + 4.0f, 0.0f,
                                rotaryStartAngle, angle, true);
        auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        g.setColour (accent.withAlpha (0.95f));
        g.strokePath (valueArc, juce::PathStrokeType (3.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::ColourGradient face (juce::Colour (0xFF403B32), centre.x - radius, centre.y - radius,
                                   juce::Colour (0xFF050506), centre.x + radius, centre.y + radius, true);
        face.addColour (0.48, juce::Colour (0xFF171512));
        g.setGradientFill (face);
        g.fillEllipse (knob);

        g.setColour (juce::Colour (vintageCream).withAlpha (0.16f));
        g.drawEllipse (knob.reduced (1.0f), 1.4f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (knob.reduced (5.0f), 1.0f);

        const auto pointerLength = radius * 0.34f;
        const auto pointerThickness = juce::jmax (2.5f, radius * 0.07f);
        juce::Path pointer;
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -radius + 8.0f,
                                     pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour (accent);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

        g.setColour (accent.withAlpha (0.14f));
        g.fillEllipse (knob.reduced (radius * 0.34f));
    }
};

int waveformToId (EddieWaveform waveform)
{
    return (int) waveform + 1;
}

EddieWaveform waveformFromId (int id)
{
    switch (id)
    {
        case 1: return EddieWaveform::sine;
        case 2: return EddieWaveform::saw;
        case 3: return EddieWaveform::square;
        case 4: return EddieWaveform::triangle;
        default: return EddieWaveform::saw;
    }
}
}

void EddieSynthPanel::MiniPreviewKeyboard::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xDD07090D));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    auto keyArea = getLocalBounds().reduced (12, 12);
    const int whiteCount = 15;
    const float whiteW = keyArea.getWidth() / (float) whiteCount;
    static constexpr int whiteNotes[] = { 48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72 };

    auto whiteIndexForNote = [] (int note)
    {
        const int semitone = note % 12;
        const int octave = (note - 48) / 12;
        int indexInOctave = 0;
        switch (semitone)
        {
            case 0: indexInOctave = 0; break;
            case 2: indexInOctave = 1; break;
            case 4: indexInOctave = 2; break;
            case 5: indexInOctave = 3; break;
            case 7: indexInOctave = 4; break;
            case 9: indexInOctave = 5; break;
            case 11: indexInOctave = 6; break;
            default: return -1;
        }
        return octave * 7 + indexInOctave;
    };

    for (int i = 0; i < whiteCount; ++i)
    {
        auto key = juce::Rectangle<float> (keyArea.getX() + i * whiteW, (float) keyArea.getY(),
                                           whiteW - 1.0f, (float) keyArea.getHeight());
        const int note = whiteNotes[i];
        const bool active = note == activeNote;
        juce::ColourGradient fill (active ? juce::Colour (0xFFFFD58C) : juce::Colour (0xFFE8E0CF),
                                   key.getCentreX(), key.getY(),
                                   active ? juce::Colour (0xFFFF8A2A) : juce::Colour (0xFF837B6E),
                                   key.getCentreX(), key.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (key, 3.0f);
        g.setColour (juce::Colour (0xFF111111).withAlpha (0.52f));
        g.drawRoundedRectangle (key, 3.0f, 1.0f);
    }

    for (int note = whiteNotes[0]; note <= whiteNotes[whiteCount - 1]; ++note)
    {
        const int semitone = note % 12;
        if (semitone != 1 && semitone != 3 && semitone != 6 && semitone != 8 && semitone != 10)
            continue;

        const int previousWhite = whiteIndexForNote (note - 1);
        if (previousWhite < 0 || previousWhite >= whiteCount - 1)
            continue;

        auto key = juce::Rectangle<float> (keyArea.getX() + (previousWhite + 0.64f) * whiteW,
                                           (float) keyArea.getY(),
                                           whiteW * 0.58f,
                                           keyArea.getHeight() * 0.62f);
        const bool active = note == activeNote;
        g.setColour (active ? juce::Colour (vintageOrange) : juce::Colour (0xFF11161D));
        g.fillRoundedRectangle (key, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.74f));
        g.drawRoundedRectangle (key, 3.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (active ? 0.24f : 0.08f));
        g.fillRoundedRectangle (key.reduced (4.0f, 3.0f).removeFromTop (key.getHeight() * 0.28f), 2.0f);
    }
}

void EddieSynthPanel::MiniPreviewKeyboard::mouseDown (const juce::MouseEvent& event)
{
    triggerAt (event.getPosition());
}

void EddieSynthPanel::MiniPreviewKeyboard::mouseDrag (const juce::MouseEvent& event)
{
    triggerAt (event.getPosition());
}

void EddieSynthPanel::MiniPreviewKeyboard::mouseUp (const juce::MouseEvent&)
{
    activeNote = -1;
    repaint();
}

int EddieSynthPanel::MiniPreviewKeyboard::noteForPosition (juce::Point<int> position) const
{
    auto keyArea = getLocalBounds().reduced (12, 12);
    if (! keyArea.contains (position))
        return -1;

    constexpr int startNote = 48;
    constexpr int endNote = 72;
    constexpr int whiteCount = 15;
    const float whiteW = keyArea.getWidth() / (float) whiteCount;

    auto whiteIndexForNote = [] (int note)
    {
        const int semitone = note % 12;
        const int octave = (note - 48) / 12;
        int indexInOctave = 0;
        switch (semitone)
        {
            case 0: indexInOctave = 0; break;
            case 2: indexInOctave = 1; break;
            case 4: indexInOctave = 2; break;
            case 5: indexInOctave = 3; break;
            case 7: indexInOctave = 4; break;
            case 9: indexInOctave = 5; break;
            case 11: indexInOctave = 6; break;
            default: return -1;
        }
        return octave * 7 + indexInOctave;
    };

    for (int note = startNote; note <= endNote; ++note)
    {
        const int semitone = note % 12;
        if (semitone != 1 && semitone != 3 && semitone != 6 && semitone != 8 && semitone != 10)
            continue;

        const int previousWhite = whiteIndexForNote (note - 1);
        if (previousWhite < 0 || previousWhite >= whiteCount - 1)
            continue;

        auto key = juce::Rectangle<float> (keyArea.getX() + (previousWhite + 0.64f) * whiteW,
                                           (float) keyArea.getY(),
                                           whiteW * 0.58f,
                                           keyArea.getHeight() * 0.62f);
        if (key.contains (position.toFloat()))
            return note;
    }

    const int whiteIndex = juce::jlimit (0, whiteCount - 1, (int) ((position.x - keyArea.getX()) / whiteW));
    static constexpr int whiteNotes[] = { 48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72 };
    return whiteNotes[whiteIndex];
}

void EddieSynthPanel::MiniPreviewKeyboard::triggerAt (juce::Point<int> position)
{
    const int note = noteForPosition (position);
    if (note < 0 || note == activeNote)
        return;

    activeNote = note;
    if (onPreviewNote)
        onPreviewNote (note, 104);
    repaint();
}

EddieSynthPanel::EddieSynthPanel()
{
    setOpaque (false);
    ThemeManager::get().addChangeListener(this);
    knobLook = std::make_unique<EddieKnobLook>();
    faceplateImage = juce::ImageFileFormat::loadFrom (BinaryData::eddie_full_plugin_frame_png,
                                                      BinaryData::eddie_full_plugin_frame_pngSize);

    title.setText ("EDDIE", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setColour (juce::Label::textColourId, juce::Colour (vintageCream));
    title.setFont (juce::FontOptions (25.0f, juce::Font::bold));
    addAndMakeVisible (title);
    title.setVisible (false);

    subtitle.setText ("wave synth / space box", juce::dontSendNotification);
    subtitle.setJustificationType (juce::Justification::centredLeft);
    subtitle.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.58f));
    subtitle.setFont (juce::FontOptions (12.0f, juce::Font::plain));
    addAndMakeVisible (subtitle);
    subtitle.setVisible (false);

    presetMenu.addListener (this);
    presetMenu.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xDD0B0A08));
    presetMenu.setColour (juce::ComboBox::textColourId, juce::Colour (vintageCream));
    presetMenu.setColour (juce::ComboBox::outlineColourId, juce::Colour (vintageCream).withAlpha (0.30f));
    presetMenu.setColour (juce::ComboBox::arrowColourId, juce::Colour (vintageOrange));
    addAndMakeVisible (presetMenu);

    waveformLabel.setText ("Wave", juce::dontSendNotification);
    waveformLabel.setJustificationType (juce::Justification::centredLeft);
    waveformLabel.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (waveformLabel);

    waveformMenu.addItem ("Sine", waveformToId (EddieWaveform::sine));
    waveformMenu.addItem ("Saw", waveformToId (EddieWaveform::saw));
    waveformMenu.addItem ("Square", waveformToId (EddieWaveform::square));
    waveformMenu.addItem ("Triangle", waveformToId (EddieWaveform::triangle));
    waveformMenu.addListener (this);
    waveformMenu.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xDD0B0A08));
    waveformMenu.setColour (juce::ComboBox::textColourId, juce::Colour (vintageCream));
    waveformMenu.setColour (juce::ComboBox::outlineColourId, juce::Colour (vintageCream).withAlpha (0.30f));
    waveformMenu.setColour (juce::ComboBox::arrowColourId, juce::Colour (vintageOrange));
    addAndMakeVisible (waveformMenu);

    presetName.setText ("New preset", juce::dontSendNotification);
    presetName.setSelectAllWhenFocused (true);
    presetName.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xCC0B0A08));
    presetName.setColour (juce::TextEditor::textColourId, juce::Colour (vintageCream));
    presetName.setColour (juce::TextEditor::outlineColourId, juce::Colour (vintageCream).withAlpha (0.28f));
    addAndMakeVisible (presetName);

    for (auto* button : { &savePreset, &closeButton })
    {
        button->addListener (this);
        addAndMakeVisible (button);
    }

    savePreset.setButtonText ("Save");
    closeButton.setButtonText ("X");
    for (auto* button : { &savePreset, &closeButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xE00B0A08));
        button->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF24160C));
        button->setColour (juce::TextButton::textColourOffId, juce::Colour (vintageCream));
        button->setColour (juce::TextButton::textColourOnId, juce::Colour (vintageAmber));
    }
    previewKeyboard.onPreviewNote = [this] (int pitch, int velocity)
    {
        if (onPreviewNote)
            onPreviewNote (pitch, velocity);
    };
    addAndMakeVisible (previewKeyboard);

    configureSlider (gain, gainLabel, "Gain", 0.02, 0.60, 0.01);
    configureSlider (saw, sawLabel, "Shape", 0.0, 1.0, 0.01);
    configureSlider (sub, subLabel, "Sub", 0.0, 0.60, 0.01);
    configureSlider (attack, attackLabel, "Attack", 1.0, 250.0, 1.0);
    configureSlider (decay, decayLabel, "Decay", 1.0, 500.0, 1.0);
    configureSlider (sustain, sustainLabel, "Sustain", 0.0, 1.0, 0.01);
    configureSlider (release, releaseLabel, "Release", 5.0, 1000.0, 1.0);
    configureSlider (drive, driveLabel, "Drive", 0.0, 1.0, 0.01);
    configureSlider (delayMix, delayMixLabel, "Dly Mix", 0.0, 1.0, 0.01);
    configureSlider (delayTime, delayTimeLabel, "Time", 40.0, 900.0, 1.0);
    configureSlider (delayFeedback, delayFeedbackLabel, "Fdbk", 0.0, 0.92, 0.01);
    configureSlider (reverbMix, reverbMixLabel, "Rev Mix", 0.0, 1.0, 0.01);
    configureSlider (reverbSize, reverbSizeLabel, "Size", 0.0, 1.0, 0.01);
    configureSlider (reverbDamping, reverbDampingLabel, "Damp", 0.0, 1.0, 0.01);

    loadPresets();
    if (presets.empty())
    {
        addPreset ("Eddie Init", currentSettings);
        auto soft = currentSettings;
        soft.sawMix = 0.24f;
        soft.waveform = EddieWaveform::triangle;
        soft.subMix = 0.08f;
        soft.attackMs = 18.0f;
        soft.releaseMs = 220.0f;
        soft.reverbMix = 0.18f;
        soft.reverbSize = 0.58f;
        addPreset ("Soft Keys", soft);

        auto bass = currentSettings;
        bass.outputGain = 0.22f;
        bass.waveform = EddieWaveform::square;
        bass.sawMix = 0.38f;
        bass.subMix = 0.42f;
        bass.attackMs = 4.0f;
        bass.decayMs = 120.0f;
        bass.sustain = 0.58f;
        bass.releaseMs = 90.0f;
        bass.drive = 0.18f;
        addPreset ("Sub Bass", bass);

        auto space = currentSettings;
        space.waveform = EddieWaveform::saw;
        space.sawMix = 0.64f;
        space.subMix = 0.14f;
        space.attackMs = 12.0f;
        space.releaseMs = 340.0f;
        space.drive = 0.12f;
        space.delayMix = 0.24f;
        space.delayMs = 330.0f;
        space.delayFeedback = 0.36f;
        space.reverbMix = 0.28f;
        space.reverbSize = 0.68f;
        space.reverbDamping = 0.38f;
        addPreset ("Space Eddie", space);
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
    label.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (label);

    slider.setRange (min, max, interval);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f),
                                juce::degreesToRadians (495.0f),
                                true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 22);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (vintageOrange));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (vintageCream).withAlpha (0.22f));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (vintageCream));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xCC080807));
    slider.setLookAndFeel (knobLook.get());
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
    currentSettings.drive = (float) drive.getValue();
    currentSettings.delayMix = (float) delayMix.getValue();
    currentSettings.delayMs = (float) delayTime.getValue();
    currentSettings.delayFeedback = (float) delayFeedback.getValue();
    currentSettings.reverbMix = (float) reverbMix.getValue();
    currentSettings.reverbSize = (float) reverbSize.getValue();
    currentSettings.reverbDamping = (float) reverbDamping.getValue();

    if (onSettingsChanged)
        onSettingsChanged (currentSettings);
}

void EddieSynthPanel::updateSettingsFromWaveform()
{
    currentSettings.waveform = waveformFromId (waveformMenu.getSelectedId());

    if (onSettingsChanged)
        onSettingsChanged (currentSettings);
}

void EddieSynthPanel::updateSlidersFromSettings()
{
    gain.setValue (currentSettings.outputGain, juce::dontSendNotification);
    waveformMenu.setSelectedId (waveformToId (currentSettings.waveform), juce::dontSendNotification);
    saw.setValue (currentSettings.sawMix, juce::dontSendNotification);
    sub.setValue (currentSettings.subMix, juce::dontSendNotification);
    attack.setValue (currentSettings.attackMs, juce::dontSendNotification);
    decay.setValue (currentSettings.decayMs, juce::dontSendNotification);
    sustain.setValue (currentSettings.sustain, juce::dontSendNotification);
    release.setValue (currentSettings.releaseMs, juce::dontSendNotification);
    drive.setValue (currentSettings.drive, juce::dontSendNotification);
    delayMix.setValue (currentSettings.delayMix, juce::dontSendNotification);
    delayTime.setValue (currentSettings.delayMs, juce::dontSendNotification);
    delayFeedback.setValue (currentSettings.delayFeedback, juce::dontSendNotification);
    reverbMix.setValue (currentSettings.reverbMix, juce::dontSendNotification);
    reverbSize.setValue (currentSettings.reverbSize, juce::dontSendNotification);
    reverbDamping.setValue (currentSettings.reverbDamping, juce::dontSendNotification);
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
        preset.settings.waveform = waveformFromId (child->getIntAttribute ("waveform", waveformToId (preset.settings.waveform)));
        preset.settings.sawMix = (float) child->getDoubleAttribute ("saw", preset.settings.sawMix);
        preset.settings.subMix = (float) child->getDoubleAttribute ("sub", preset.settings.subMix);
        preset.settings.attackMs = (float) child->getDoubleAttribute ("attack", preset.settings.attackMs);
        preset.settings.decayMs = (float) child->getDoubleAttribute ("decay", preset.settings.decayMs);
        preset.settings.sustain = (float) child->getDoubleAttribute ("sustain", preset.settings.sustain);
        preset.settings.releaseMs = (float) child->getDoubleAttribute ("release", preset.settings.releaseMs);
        preset.settings.drive = (float) child->getDoubleAttribute ("drive", preset.settings.drive);
        preset.settings.delayMix = (float) child->getDoubleAttribute ("delayMix", preset.settings.delayMix);
        preset.settings.delayMs = (float) child->getDoubleAttribute ("delayMs", preset.settings.delayMs);
        preset.settings.delayFeedback = (float) child->getDoubleAttribute ("delayFeedback", preset.settings.delayFeedback);
        preset.settings.reverbMix = (float) child->getDoubleAttribute ("reverbMix", preset.settings.reverbMix);
        preset.settings.reverbSize = (float) child->getDoubleAttribute ("reverbSize", preset.settings.reverbSize);
        preset.settings.reverbDamping = (float) child->getDoubleAttribute ("reverbDamping", preset.settings.reverbDamping);
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
        item->setAttribute ("waveform", waveformToId (preset.settings.waveform));
        item->setAttribute ("saw", preset.settings.sawMix);
        item->setAttribute ("sub", preset.settings.subMix);
        item->setAttribute ("attack", preset.settings.attackMs);
        item->setAttribute ("decay", preset.settings.decayMs);
        item->setAttribute ("sustain", preset.settings.sustain);
        item->setAttribute ("release", preset.settings.releaseMs);
        item->setAttribute ("drive", preset.settings.drive);
        item->setAttribute ("delayMix", preset.settings.delayMix);
        item->setAttribute ("delayMs", preset.settings.delayMs);
        item->setAttribute ("delayFeedback", preset.settings.delayFeedback);
        item->setAttribute ("reverbMix", preset.settings.reverbMix);
        item->setAttribute ("reverbSize", preset.settings.reverbSize);
        item->setAttribute ("reverbDamping", preset.settings.reverbDamping);
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

}

void EddieSynthPanel::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &waveformMenu)
    {
        updateSettingsFromWaveform();
        return;
    }

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
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillRect (getLocalBounds());

    auto panelBounds = getLocalBounds().reduced (18);
    auto panel = panelBounds.toFloat();
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (panel, 10.0f);
    if (faceplateImage.isValid())
    {
        g.setOpacity (0.74f);
        g.drawImageWithin (faceplateImage,
                           panelBounds.getX(), panelBounds.getY(), panelBounds.getWidth(), panelBounds.getHeight(),
                           juce::RectanglePlacement::stretchToFit);
        g.setOpacity (1.0f);
    }

    g.setColour (juce::Colour (vintageCream).withAlpha (0.36f));
    g.drawRoundedRectangle (panel.reduced (1.0f), 10.0f, 1.0f);
}

EddieSynthPanel::~EddieSynthPanel()
{
    for (auto* s : { &gain, &saw, &sub, &attack, &decay, &sustain, &release,
                     &drive, &delayMix, &delayTime, &delayFeedback,
                     &reverbMix, &reverbSize, &reverbDamping })
        s->setLookAndFeel (nullptr);

    ThemeManager::get().removeChangeListener(this);
}

void EddieSynthPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto* s : { &gain, &saw, &sub, &attack, &decay, &sustain, &release,
                     &drive, &delayMix, &delayTime, &delayFeedback,
                     &reverbMix, &reverbSize, &reverbDamping })
    {
        s->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(vintageOrange));
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xCC080807));
    }
    presetName.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xCC0B0A08));
    presetName.setColour(juce::TextEditor::textColourId,       juce::Colour(vintageCream));
    repaint();
}

void EddieSynthPanel::resized()
{
    auto frame = getLocalBounds().reduced (18);
    auto sx = [frame] (float x) { return frame.getX() + (int) std::round (frame.getWidth() * x); };
    auto sy = [frame] (float y) { return frame.getY() + (int) std::round (frame.getHeight() * y); };
    auto sw = [frame] (float w) { return (int) std::round (frame.getWidth() * w); };
    auto sh = [frame] (float h) { return (int) std::round (frame.getHeight() * h); };
    auto rect = [&] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> (sx (x), sy (y), sw (w), sh (h));
    };

    title.setBounds ({});
    subtitle.setBounds ({});
    closeButton.setBounds (rect (0.953f, 0.034f, 0.026f, 0.060f));

    auto presetRow = rect (0.070f, 0.326f, 0.585f, 0.048f);
    presetMenu.setBounds (presetRow.removeFromLeft (sw (0.165f)));
    presetRow.removeFromLeft (sw (0.018f));
    waveformLabel.setBounds (presetRow.removeFromLeft (sw (0.034f)));
    waveformMenu.setBounds (presetRow.removeFromLeft (sw (0.096f)));
    presetRow.removeFromLeft (sw (0.014f));
    presetName.setBounds (presetRow.removeFromLeft (sw (0.164f)));
    presetRow.removeFromLeft (sw (0.014f));
    savePreset.setBounds (presetRow.removeFromLeft (sw (0.066f)));

    previewKeyboard.setBounds (rect (0.060f, 0.846f, 0.880f, 0.094f));

    const int labelH = 20;
    const int knobW = juce::jlimit (74, 104, sw (0.054f));
    const int knobH = juce::jlimit (96, 122, sh (0.132f));

    auto placeAt = [knobW, knobH, labelH] (juce::Rectangle<int> section,
                                           float centerX,
                                           float topY,
                                           juce::Label& label,
                                           juce::Slider& slider)
    {
        const int x = section.getX() + (int) std::round (section.getWidth() * centerX) - knobW / 2;
        const int y = section.getY() + (int) std::round (section.getHeight() * topY);
        label.setBounds (x, y, knobW, labelH);
        slider.setBounds (x, y + labelH + 4, knobW, knobH);
    };

    auto leftTop = rect (0.060f, 0.390f, 0.445f, 0.250f);
    auto rightTop = rect (0.505f, 0.390f, 0.435f, 0.250f);
    auto bottom = rect (0.060f, 0.640f, 0.880f, 0.190f);

    placeAt (leftTop, 0.105f, 0.135f, sawLabel, saw);
    placeAt (leftTop, 0.255f, 0.135f, subLabel, sub);
    placeAt (leftTop, 0.405f, 0.135f, gainLabel, gain);

    placeAt (rightTop, 0.140f, 0.135f, attackLabel, attack);
    placeAt (rightTop, 0.295f, 0.135f, decayLabel, decay);
    placeAt (rightTop, 0.450f, 0.135f, sustainLabel, sustain);
    placeAt (rightTop, 0.605f, 0.135f, releaseLabel, release);

    placeAt (bottom, 0.055f, 0.085f, delayMixLabel, delayMix);
    placeAt (bottom, 0.135f, 0.085f, delayTimeLabel, delayTime);
    placeAt (bottom, 0.215f, 0.085f, delayFeedbackLabel, delayFeedback);
    placeAt (bottom, 0.430f, 0.085f, reverbMixLabel, reverbMix);
    placeAt (bottom, 0.510f, 0.085f, reverbSizeLabel, reverbSize);
    placeAt (bottom, 0.590f, 0.085f, reverbDampingLabel, reverbDamping);
    placeAt (bottom, 0.805f, 0.085f, driveLabel, drive);
}

} // namespace aidaw
