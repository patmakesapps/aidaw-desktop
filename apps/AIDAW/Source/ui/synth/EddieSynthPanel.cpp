#include "EddieSynthPanel.h"

#include "../shared/Theme.h"
#include "../shared/ThemeManager.h"
#include "BinaryData.h"

namespace aidaw
{

namespace
{
constexpr uint32 vintageCream = 0xFFEFD9A5;
constexpr uint32 vintageOrange = 0xFFFF7A1A;
constexpr uint32 vintageAmber = 0xFFFFB84A;

class EddieKnobLook final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                        .reduced (5.0f, 4.0f);
        bounds.removeFromBottom (20.0f);

        const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto knob = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
        const auto centre = knob.getCentre();
        const auto radius = knob.getWidth() * 0.5f;
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour (juce::Colour (0xAA000000));
        g.fillEllipse (knob.translated (0.0f, 3.0f));

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius + 3.0f, radius + 3.0f, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (vintageCream).withAlpha (0.26f));
        g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius + 3.0f, radius + 3.0f, 0.0f,
                                rotaryStartAngle, angle, true);
        auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        g.setColour (accent.withAlpha (0.95f));
        g.strokePath (valueArc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::ColourGradient face (juce::Colour (0xFF403B32), centre.x - radius, centre.y - radius,
                                   juce::Colour (0xFF050506), centre.x + radius, centre.y + radius, true);
        face.addColour (0.48, juce::Colour (0xFF171512));
        g.setGradientFill (face);
        g.fillEllipse (knob);

        g.setColour (juce::Colour (vintageCream).withAlpha (0.16f));
        g.drawEllipse (knob.reduced (1.0f), 1.2f);

        const auto pointerLength = radius * 0.34f;
        const auto pointerThickness = juce::jmax (2.2f, radius * 0.07f);
        juce::Path pointer;
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -radius + 6.0f,
                                     pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour (accent);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }
};

int waveformToId (EddieWaveform w) { return (int) w + 1; }
EddieWaveform waveformFromId (int id)
{
    switch (id)
    {
        case 1: return EddieWaveform::sine;
        case 2: return EddieWaveform::saw;
        case 3: return EddieWaveform::square;
        case 4: return EddieWaveform::triangle;
    }
    return EddieWaveform::saw;
}

int filterModeToId (EddieFilterMode m) { return (int) m + 1; }
EddieFilterMode filterModeFromId (int id)
{
    switch (id)
    {
        case 1: return EddieFilterMode::off;
        case 2: return EddieFilterMode::lowPass;
        case 3: return EddieFilterMode::bandPass;
        case 4: return EddieFilterMode::highPass;
    }
    return EddieFilterMode::lowPass;
}

int lfoShapeToId (EddieLfoShape s) { return (int) s + 1; }
EddieLfoShape lfoShapeFromId (int id)
{
    switch (id)
    {
        case 1: return EddieLfoShape::sine;
        case 2: return EddieLfoShape::triangle;
        case 3: return EddieLfoShape::saw;
        case 4: return EddieLfoShape::square;
        case 5: return EddieLfoShape::sampleAndHold;
    }
    return EddieLfoShape::sine;
}

int lfoDestToId (EddieLfoDest d) { return (int) d + 1; }
EddieLfoDest lfoDestFromId (int id)
{
    switch (id)
    {
        case 1: return EddieLfoDest::off;
        case 2: return EddieLfoDest::filterCutoff;
        case 3: return EddieLfoDest::filterResonance;
        case 4: return EddieLfoDest::amp;
        case 5: return EddieLfoDest::pan;
    }
    return EddieLfoDest::off;
}

int noiseTypeToId (EddieNoiseType n) { return (int) n + 1; }
EddieNoiseType noiseTypeFromId (int id)
{
    switch (id)
    {
        case 1: return EddieNoiseType::white;
        case 2: return EddieNoiseType::pink;
        case 3: return EddieNoiseType::brown;
        case 4: return EddieNoiseType::digital;
    }
    return EddieNoiseType::white;
}

int divToId (EddieSyncDiv d) { return (int) d + 1; }
EddieSyncDiv divFromId (int id)
{
    if (id < 1) id = 1;
    if (id > 12) id = 12;
    return (EddieSyncDiv) (id - 1);
}

const std::array<EddieSyncDiv, 12> kAllDivs {
    EddieSyncDiv::d1_32, EddieSyncDiv::d1_16T, EddieSyncDiv::d1_16, EddieSyncDiv::d1_16D,
    EddieSyncDiv::d1_8T, EddieSyncDiv::d1_8, EddieSyncDiv::d1_8D,
    EddieSyncDiv::d1_4T, EddieSyncDiv::d1_4, EddieSyncDiv::d1_4D,
    EddieSyncDiv::d1_2, EddieSyncDiv::d1_1
};

void fillDivCombo (juce::ComboBox& cb)
{
    for (auto d : kAllDivs)
        cb.addItem (syncDivLabel (d), divToId (d));
}
} // namespace

void EddieSynthPanel::MiniPreviewKeyboard::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xDD07090D));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    auto keyArea = getLocalBounds().reduced (10, 8);
    const int whiteCount = 21; // ~3 octaves
    const float whiteW = keyArea.getWidth() / (float) whiteCount;
    static constexpr int firstNote = 36; // C2

    auto whiteIndexForNote = [] (int note) -> int
    {
        const int semitone = note % 12;
        const int octave = (note - firstNote) / 12;
        int idx = 0;
        switch (semitone) { case 0:idx=0;break; case 2:idx=1;break; case 4:idx=2;break;
            case 5:idx=3;break; case 7:idx=4;break; case 9:idx=5;break; case 11:idx=6;break;
            default: return -1; }
        return octave * 7 + idx;
    };

    // White keys
    for (int i = 0; i < whiteCount; ++i)
    {
        // map index -> note
        const int oct = i / 7;
        const int inOct = i % 7;
        static constexpr int wPC[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int note = firstNote + oct * 12 + wPC[inOct];

        auto key = juce::Rectangle<float> (keyArea.getX() + i * whiteW, (float) keyArea.getY(),
                                           whiteW - 1.0f, (float) keyArea.getHeight());
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

    const int lastWhiteNote = firstNote + (whiteCount / 7) * 12 + 11;
    for (int note = firstNote; note <= lastWhiteNote; ++note)
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
    triggerAt (event.getPosition(), true);
}

void EddieSynthPanel::MiniPreviewKeyboard::mouseDrag (const juce::MouseEvent& event)
{
    triggerAt (event.getPosition(), false);
}

void EddieSynthPanel::MiniPreviewKeyboard::mouseUp (const juce::MouseEvent&)
{
    activeNote = -1;
    repaint();
}

int EddieSynthPanel::MiniPreviewKeyboard::noteForPosition (juce::Point<int> position) const
{
    auto keyArea = getLocalBounds().reduced (10, 8);
    if (! keyArea.contains (position))
        return -1;

    constexpr int firstNote = 36;
    constexpr int whiteCount = 21;
    const float whiteW = keyArea.getWidth() / (float) whiteCount;

    auto whiteIndexForNote = [] (int note) -> int
    {
        const int semitone = note % 12;
        const int octave = (note - firstNote) / 12;
        int idx = 0;
        switch (semitone) { case 0:idx=0;break; case 2:idx=1;break; case 4:idx=2;break;
            case 5:idx=3;break; case 7:idx=4;break; case 9:idx=5;break; case 11:idx=6;break;
            default: return -1; }
        return octave * 7 + idx;
    };

    const int lastWhiteNote = firstNote + (whiteCount / 7) * 12 + 11;
    for (int note = firstNote; note <= lastWhiteNote; ++note)
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
    const int oct = whiteIndex / 7;
    const int inOct = whiteIndex % 7;
    static constexpr int wPC[] = { 0, 2, 4, 5, 7, 9, 11 };
    return firstNote + oct * 12 + wPC[inOct];
}

void EddieSynthPanel::MiniPreviewKeyboard::triggerAt (juce::Point<int> position, bool forceRetrigger)
{
    const int note = noteForPosition (position);
    if (note < 0 || (! forceRetrigger && note == activeNote))
        return;
    activeNote = note;
    if (onPreviewNote)
        onPreviewNote (note, 104);
    repaint();
}

void EddieSynthPanel::styleCombo (juce::ComboBox& combo)
{
    combo.addListener (this);
    combo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xDD0B0A08));
    combo.setColour (juce::ComboBox::textColourId, juce::Colour (vintageCream));
    combo.setColour (juce::ComboBox::outlineColourId, juce::Colour (vintageCream).withAlpha (0.30f));
    combo.setColour (juce::ComboBox::arrowColourId, juce::Colour (vintageOrange));
    addAndMakeVisible (combo);
}

void EddieSynthPanel::styleButton (juce::Button& b)
{
    b.addListener (this);
    b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xE00B0A08));
    b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF24160C));
    b.setColour (juce::TextButton::textColourOffId, juce::Colour (vintageCream));
    b.setColour (juce::TextButton::textColourOnId, juce::Colour (vintageAmber));
    addAndMakeVisible (b);
}

EddieSynthPanel::EddieSynthPanel()
{
    setOpaque (false);
    ThemeManager::get().addChangeListener (this);
    knobLook = std::make_unique<EddieKnobLook>();
    faceplateImage = juce::ImageFileFormat::loadFrom (BinaryData::eddie_full_plugin_frame_png,
                                                      BinaryData::eddie_full_plugin_frame_pngSize);

    auto setupSectionLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, juce::Colour (vintageAmber));
        l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };

    setupSectionLabel (osc1Header,    "OSC 1");
    setupSectionLabel (osc2Header,    "OSC 2");
    setupSectionLabel (unisonHeader,  "UNISON");
    setupSectionLabel (filterHeader,  "FILTER");
    setupSectionLabel (fEnvHeader,    "FILTER ENV");
    setupSectionLabel (ampEnvHeader,  "AMP ENV");
    setupSectionLabel (lfoHeader,     "LFO");
    setupSectionLabel (fxHeader,      "FX");
    setupSectionLabel (masterHeader,  "MASTER");

    title.setText ("EDDIE", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setColour (juce::Label::textColourId, juce::Colour (vintageCream));
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    addAndMakeVisible (title);

    subtitle.setText ("wave synth / sound design", juce::dontSendNotification);
    subtitle.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.58f));
    subtitle.setFont (juce::FontOptions (11.0f, juce::Font::plain));
    addAndMakeVisible (subtitle);

    styleCombo (presetMenu);

    presetName.setText ("New preset", juce::dontSendNotification);
    presetName.setSelectAllWhenFocused (true);
    presetName.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xCC0B0A08));
    presetName.setColour (juce::TextEditor::textColourId, juce::Colour (vintageCream));
    presetName.setColour (juce::TextEditor::outlineColourId, juce::Colour (vintageCream).withAlpha (0.28f));
    addAndMakeVisible (presetName);

    savePreset.setButtonText ("Save");
    closeButton.setButtonText ("X");
    monoToggle.setButtonText ("POLY");
    monoToggle.setClickingTogglesState (true);
    styleButton (savePreset);
    styleButton (closeButton);
    styleButton (monoToggle);

    // OSC1
    osc1WaveLabel.setText ("Wave", juce::dontSendNotification);
    osc1WaveLabel.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (osc1WaveLabel);
    osc1WaveMenu.addItem ("Sine", 1); osc1WaveMenu.addItem ("Saw", 2);
    osc1WaveMenu.addItem ("Square", 3); osc1WaveMenu.addItem ("Triangle", 4);
    styleCombo (osc1WaveMenu);
    noiseTypeLabel.setText ("Noise", juce::dontSendNotification);
    noiseTypeLabel.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (noiseTypeLabel);
    noiseTypeMenu.addItem ("White", 1);
    noiseTypeMenu.addItem ("Pink", 2);
    noiseTypeMenu.addItem ("Brown", 3);
    noiseTypeMenu.addItem ("Digital", 4);
    styleCombo (noiseTypeMenu);
    configureSlider (saw,        sawLabel,        "PW",    0.05, 0.95, 0.01);
    configureSlider (sub,        subLabel,        "Sub",   0.0, 1.0, 0.01);
    configureSlider (noise,      noiseLabel,      "Noise Amt", 0.0, 1.0, 0.01);
    configureSlider (osc1Level,  osc1LevelLabel,  "Level", 0.0, 1.0, 0.01);

    // OSC2
    osc2WaveLabel.setText ("Wave", juce::dontSendNotification);
    osc2WaveLabel.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (osc2WaveLabel);
    osc2WaveMenu.addItem ("Sine", 1); osc2WaveMenu.addItem ("Saw", 2);
    osc2WaveMenu.addItem ("Square", 3); osc2WaveMenu.addItem ("Triangle", 4);
    styleCombo (osc2WaveMenu);
    osc2EnabledBtn.setButtonText ("OFF");
    osc2EnabledBtn.setClickingTogglesState (true);
    styleButton (osc2EnabledBtn);
    configureSlider (osc2Semis,  osc2SemisLabel,  "Semi",  -24.0, 24.0, 1.0);
    configureSlider (osc2Cents,  osc2CentsLabel,  "Cent",  -100.0, 100.0, 1.0);
    configureSlider (osc2Level,  osc2LevelLabel,  "Level", 0.0, 1.0, 0.01);

    // Unison
    configureSlider (unisonVoices, unisonVoicesLabel, "Voices", 1.0, 7.0, 1.0);
    configureSlider (unisonDetune, unisonDetuneLabel, "Detune", 0.0, 50.0, 0.5);
    configureSlider (unisonSpread, unisonSpreadLabel, "Spread", 0.0, 1.0, 0.01);

    // Filter
    filterModeLabel.setText ("Mode", juce::dontSendNotification);
    filterModeLabel.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (filterModeLabel);
    filterModeMenu.addItem ("Off", 1); filterModeMenu.addItem ("LP", 2);
    filterModeMenu.addItem ("BP", 3);  filterModeMenu.addItem ("HP", 4);
    styleCombo (filterModeMenu);
    configureSlider (cutoff, cutoffLabel, "Cutoff", 30.0, 20000.0, 1.0);
    cutoff.setSkewFactorFromMidPoint (1000.0);
    configureSlider (reso,   resoLabel,   "Reso",  0.0, 1.0, 0.01);
    configureSlider (fDrive, fDriveLabel, "Drive", 0.0, 1.0, 0.01);
    configureSlider (kTrk,   kTrkLabel,   "KeyTrk",0.0, 1.0, 0.01);

    // Filter env
    configureSlider (fAttack,  fAttackLabel,  "A",   1.0, 500.0, 1.0);
    configureSlider (fDecay,   fDecayLabel,   "D",   1.0, 1500.0, 1.0);
    configureSlider (fSustain, fSustainLabel, "S",   0.0, 1.0, 0.01);
    configureSlider (fRelease, fReleaseLabel, "R",   1.0, 2000.0, 1.0);
    configureSlider (fAmount,  fAmountLabel,  "Amt",-1.0, 1.0, 0.01);

    // Amp env
    configureSlider (attack,  attackLabel,  "A", 1.0, 250.0, 1.0);
    configureSlider (decay,   decayLabel,   "D", 1.0, 500.0, 1.0);
    configureSlider (sustain, sustainLabel, "S", 0.0, 1.0, 0.01);
    configureSlider (release, releaseLabel, "R", 5.0, 2000.0, 1.0);

    // LFO
    lfoShapeLabel.setText ("Shape", juce::dontSendNotification);
    lfoShapeLabel.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (lfoShapeLabel);
    lfoShapeMenu.addItem ("Sine", 1); lfoShapeMenu.addItem ("Tri", 2);
    lfoShapeMenu.addItem ("Saw", 3);  lfoShapeMenu.addItem ("Sq", 4);
    lfoShapeMenu.addItem ("S&H", 5);
    styleCombo (lfoShapeMenu);

    lfoDestLabel.setText ("Dest", juce::dontSendNotification);
    lfoDestLabel.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (lfoDestLabel);
    lfoDestMenu.addItem ("Off", 1); lfoDestMenu.addItem ("Cutoff", 2);
    lfoDestMenu.addItem ("Reso", 3); lfoDestMenu.addItem ("Amp", 4);
    lfoDestMenu.addItem ("Pan", 5);
    styleCombo (lfoDestMenu);

    lfoSyncBtn.setButtonText ("FREE");
    lfoSyncBtn.setClickingTogglesState (true);
    styleButton (lfoSyncBtn);

    lfoDivLabel.setText ("Div", juce::dontSendNotification);
    lfoDivLabel.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (lfoDivLabel);
    fillDivCombo (lfoDivMenu);
    styleCombo (lfoDivMenu);

    configureSlider (lfoRate,  lfoRateLabel,  "Rate", 0.05, 20.0, 0.01);
    configureSlider (lfoDepth, lfoDepthLabel, "Depth", 0.0, 1.0, 0.01);

    // FX: Drive
    configureSlider (drive, driveLabel, "Drive", 0.0, 1.0, 0.01);

    // FX: Delay
    delayPowerBtn.setButtonText ("DLY OFF");
    delayPowerBtn.setClickingTogglesState (true);
    styleButton (delayPowerBtn);
    delaySyncBtn.setButtonText ("SYNC");
    delaySyncBtn.setClickingTogglesState (true);
    styleButton (delaySyncBtn);
    delayPingPongBtn.setButtonText ("PP");
    delayPingPongBtn.setClickingTogglesState (true);
    styleButton (delayPingPongBtn);
    delayDivLabel.setText ("Div", juce::dontSendNotification);
    delayDivLabel.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    addAndMakeVisible (delayDivLabel);
    fillDivCombo (delayDivMenu);
    styleCombo (delayDivMenu);
    configureSlider (delayMix,      delayMixLabel,    "Mix",   0.0, 1.0, 0.01);
    configureSlider (delayTime,     delayTimeLabel,   "Time",  20.0, 1500.0, 1.0);
    configureSlider (delayFeedback, delayFbLabel,     "Fdbk",  0.0, 0.92, 0.01);
    configureSlider (delayHiCut,    delayHiCutLabel,  "HiCut", 500.0, 18000.0, 1.0);
    delayHiCut.setSkewFactorFromMidPoint (3000.0);

    // FX: Chorus
    configureSlider (chorusMix,   chorusMixLabel,   "Mix",   0.0, 1.0, 0.01);
    configureSlider (chorusRate,  chorusRateLabel,  "Rate",  0.05, 6.0, 0.01);
    configureSlider (chorusDepth, chorusDepthLabel, "Depth", 0.0, 1.0, 0.01);

    // FX: Reverb
    configureSlider (reverbMix,     reverbMixLabel,     "Rev Mix", 0.0, 1.0, 0.01);
    configureSlider (reverbSize,    reverbSizeLabel,    "Size",    0.0, 1.0, 0.01);
    configureSlider (reverbDamping, reverbDampingLabel, "Damp",    0.0, 1.0, 0.01);

    // Master
    configureSlider (gain,      gainLabel, "Gain", 0.02, 1.0, 0.01);
    configureSlider (polyphony, polyLabel, "Poly", 1.0, 16.0, 1.0);

    previewKeyboard.onPreviewNote = [this] (int pitch, int velocity)
    {
        if (onPreviewNote)
            onPreviewNote (pitch, velocity);
    };
    addAndMakeVisible (previewKeyboard);

    // Tab buttons
    for (auto* b : { &tabOsc, &tabFilter, &tabMod, &tabFx })
    {
        styleButton (*b);
        b->setRadioGroupId (101);
        b->setClickingTogglesState (true);
    }
    tabOsc.setButtonText    ("OSC");
    tabFilter.setButtonText ("FILTER");
    tabMod.setButtonText    ("MOD");
    tabFx.setButtonText     ("FX");

    loadPresets();
    if (presets.empty())
    {
        addPreset ("Eddie Init", currentSettings);

        EddieSynthSettings soft = currentSettings;
        soft.sawMix = 0.24f; soft.waveform = EddieWaveform::triangle;
        soft.subMix = 0.10f; soft.attackMs = 20.0f; soft.releaseMs = 380.0f;
        soft.filterCutoffHz = 4500.0f; soft.filterResonance = 0.05f;
        soft.reverbMix = 0.22f; soft.reverbSize = 0.62f;
        addPreset ("Soft Keys", soft);

        EddieSynthSettings bass = currentSettings;
        bass.outputGain = 0.55f; bass.waveform = EddieWaveform::square;
        bass.mono = true; bass.polyphony = 1; bass.sawMix = 0.5f; bass.subMix = 0.5f;
        bass.attackMs = 4.0f; bass.decayMs = 120.0f; bass.sustain = 0.55f; bass.releaseMs = 90.0f;
        bass.filterCutoffHz = 600.0f; bass.filterResonance = 0.25f;
        bass.fEnvAmount = 0.35f; bass.fEnvDecayMs = 180.0f; bass.fEnvSustain = 0.0f;
        bass.drive = 0.22f;
        addPreset ("Sub Bass", bass);

        EddieSynthSettings supersaw = currentSettings;
        supersaw.waveform = EddieWaveform::saw; supersaw.sawMix = 1.0f;
        supersaw.osc2Enabled = true; supersaw.osc2Waveform = EddieWaveform::saw;
        supersaw.osc2Semis = 0; supersaw.osc2Cents = 7.0f; supersaw.osc2Level = 0.7f;
        supersaw.unisonVoices = 7; supersaw.unisonDetuneCents = 22.0f; supersaw.unisonSpread = 0.85f;
        supersaw.attackMs = 10.0f; supersaw.releaseMs = 480.0f;
        supersaw.filterCutoffHz = 8000.0f;
        supersaw.delayMix = 0.18f; supersaw.delaySync = true; supersaw.delayDiv = EddieSyncDiv::d1_8D;
        supersaw.delayPingPong = true; supersaw.delayFeedback = 0.35f;
        supersaw.reverbMix = 0.32f; supersaw.reverbSize = 0.78f;
        supersaw.outputGain = 0.32f;
        addPreset ("Supersaw", supersaw);

        EddieSynthSettings pluck = currentSettings;
        pluck.waveform = EddieWaveform::triangle; pluck.sawMix = 0.7f;
        pluck.attackMs = 1.0f; pluck.decayMs = 220.0f; pluck.sustain = 0.0f; pluck.releaseMs = 220.0f;
        pluck.filterCutoffHz = 12000.0f; pluck.fEnvAmount = -0.4f; pluck.fEnvDecayMs = 250.0f;
        pluck.delayMix = 0.28f; pluck.delaySync = true; pluck.delayDiv = EddieSyncDiv::d1_4D;
        pluck.delayFeedback = 0.45f; pluck.delayPingPong = true;
        pluck.reverbMix = 0.25f;
        addPreset ("Tempo Pluck", pluck);

        EddieSynthSettings sweep = currentSettings;
        sweep.waveform = EddieWaveform::saw; sweep.unisonVoices = 5; sweep.unisonDetuneCents = 14.0f;
        sweep.attackMs = 80.0f; sweep.releaseMs = 800.0f;
        sweep.filterCutoffHz = 600.0f; sweep.filterResonance = 0.4f;
        sweep.lfoSync = true; sweep.lfoSyncDiv = EddieSyncDiv::d1_2;
        sweep.lfoDest = EddieLfoDest::filterCutoff; sweep.lfoDepth = 0.55f;
        sweep.reverbMix = 0.4f; sweep.reverbSize = 0.85f;
        sweep.chorusMix = 0.3f;
        addPreset ("LFO Sweep Pad", sweep);
    }

    refreshPresetMenu();
    updateSlidersFromSettings();
    setActiveTab (Tab::Osc);
}

void EddieSynthPanel::setActiveTab (Tab t)
{
    activeTab = t;
    tabOsc.setToggleState    (t == Tab::Osc,    juce::dontSendNotification);
    tabFilter.setToggleState (t == Tab::Filter, juce::dontSendNotification);
    tabMod.setToggleState    (t == Tab::Mod,    juce::dontSendNotification);
    tabFx.setToggleState     (t == Tab::Fx,     juce::dontSendNotification);
    applyTabVisibility();
    resized();
    repaint();
}

void EddieSynthPanel::applyTabVisibility()
{
    auto setVis = [] (juce::Component& c, bool v) { c.setVisible (v); };

    const bool osc = activeTab == Tab::Osc;
    const bool flt = activeTab == Tab::Filter;
    const bool mod = activeTab == Tab::Mod;
    const bool fx  = activeTab == Tab::Fx;

    // OSC tab: OSC1 + OSC2 + Sub + Noise + Unison
    for (auto* c : std::initializer_list<juce::Component*>
                    { &osc1WaveLabel, &osc1WaveMenu, &noiseTypeLabel, &noiseTypeMenu,
                      &sawLabel, &saw,
                      &subLabel, &sub, &noiseLabel, &noise,
                      &osc1LevelLabel, &osc1Level,
                      &osc2WaveLabel, &osc2WaveMenu, &osc2EnabledBtn,
                      &osc2SemisLabel, &osc2Semis, &osc2CentsLabel, &osc2Cents,
                      &osc2LevelLabel, &osc2Level,
                      &unisonVoicesLabel, &unisonVoices,
                      &unisonDetuneLabel, &unisonDetune,
                      &unisonSpreadLabel, &unisonSpread,
                      &osc1Header, &osc2Header, &unisonHeader })
        setVis (*c, osc);

    // FILTER tab: Filter + Filter env + Amp env (envelopes live here too)
    for (auto* c : std::initializer_list<juce::Component*>
                    { &filterModeLabel, &filterModeMenu, &cutoffLabel, &cutoff,
                      &resoLabel, &reso, &fDriveLabel, &fDrive, &kTrkLabel, &kTrk,
                      &fAttackLabel, &fAttack, &fDecayLabel, &fDecay,
                      &fSustainLabel, &fSustain, &fReleaseLabel, &fRelease,
                      &fAmountLabel, &fAmount,
                      &attackLabel, &attack, &decayLabel, &decay,
                      &sustainLabel, &sustain, &releaseLabel, &release,
                      &filterHeader, &fEnvHeader, &ampEnvHeader })
        setVis (*c, flt);

    // MOD tab: LFO
    for (auto* c : std::initializer_list<juce::Component*>
                    { &lfoShapeLabel, &lfoShapeMenu, &lfoSyncBtn,
                      &lfoDivLabel, &lfoDivMenu, &lfoRateLabel, &lfoRate,
                      &lfoDepthLabel, &lfoDepth, &lfoDestLabel, &lfoDestMenu,
                      &lfoHeader })
        setVis (*c, mod);

    // FX tab: Drive + Delay + Chorus + Reverb
    for (auto* c : std::initializer_list<juce::Component*>
                    { &driveLabel, &drive,
                      &delayPowerBtn, &delaySyncBtn, &delayDivLabel, &delayDivMenu, &delayPingPongBtn,
                      &delayMixLabel, &delayMix, &delayTimeLabel, &delayTime,
                      &delayFbLabel, &delayFeedback, &delayHiCutLabel, &delayHiCut,
                      &chorusMixLabel, &chorusMix, &chorusRateLabel, &chorusRate,
                      &chorusDepthLabel, &chorusDepth,
                      &reverbMixLabel, &reverbMix, &reverbSizeLabel, &reverbSize,
                      &reverbDampingLabel, &reverbDamping,
                      &fxHeader })
        setVis (*c, fx);
}

void EddieSynthPanel::setSettings (const EddieSynthSettings& s)
{
    currentSettings = s;
    updateSlidersFromSettings();
}

EddieSynthSettings EddieSynthPanel::getSettings() const { return currentSettings; }

void EddieSynthPanel::configureSlider (juce::Slider& slider, juce::Label& label,
                                       const juce::String& labelText,
                                       double min, double max, double interval)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (vintageCream).withAlpha (0.86f));
    label.setFont (juce::FontOptions (10.5f, juce::Font::plain));
    addAndMakeVisible (label);

    slider.setRange (min, max, interval);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f),
                                juce::degreesToRadians (495.0f), true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (vintageOrange));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (vintageCream).withAlpha (0.22f));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (vintageCream));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xCC080807));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setLookAndFeel (knobLook.get());
    slider.onValueChange = [this] { updateSettingsFromSliders(); };
    addAndMakeVisible (slider);
}

void EddieSynthPanel::updateSettingsFromSliders()
{
    currentSettings.outputGain = (float) gain.getValue();
    currentSettings.polyphony = (int) polyphony.getValue();
    currentSettings.mono = monoToggle.getToggleState();
    currentSettings.sawMix = (float) saw.getValue();
    currentSettings.subMix = (float) sub.getValue();
    currentSettings.noiseLevel = (float) noise.getValue();
    currentSettings.osc1Level = (float) osc1Level.getValue();
    currentSettings.osc2Enabled = osc2EnabledBtn.getToggleState();
    currentSettings.osc2Semis = (int) osc2Semis.getValue();
    currentSettings.osc2Cents = (float) osc2Cents.getValue();
    currentSettings.osc2Level = (float) osc2Level.getValue();
    currentSettings.unisonVoices = (int) unisonVoices.getValue();
    currentSettings.unisonDetuneCents = (float) unisonDetune.getValue();
    currentSettings.unisonSpread = (float) unisonSpread.getValue();

    currentSettings.attackMs = (float) attack.getValue();
    currentSettings.decayMs = (float) decay.getValue();
    currentSettings.sustain = (float) sustain.getValue();
    currentSettings.releaseMs = (float) release.getValue();

    currentSettings.filterCutoffHz = (float) cutoff.getValue();
    currentSettings.filterResonance = (float) reso.getValue();
    currentSettings.filterDrive = (float) fDrive.getValue();
    currentSettings.filterKeyTrack = (float) kTrk.getValue();

    currentSettings.fEnvAttackMs = (float) fAttack.getValue();
    currentSettings.fEnvDecayMs = (float) fDecay.getValue();
    currentSettings.fEnvSustain = (float) fSustain.getValue();
    currentSettings.fEnvReleaseMs = (float) fRelease.getValue();
    currentSettings.fEnvAmount = (float) fAmount.getValue();

    currentSettings.lfoRateHz = (float) lfoRate.getValue();
    currentSettings.lfoDepth = (float) lfoDepth.getValue();
    currentSettings.lfoSync = lfoSyncBtn.getToggleState();

    currentSettings.drive = (float) drive.getValue();

    currentSettings.delayMix = (float) delayMix.getValue();
    currentSettings.delayMs = (float) delayTime.getValue();
    currentSettings.delayFeedback = (float) delayFeedback.getValue();
    currentSettings.delayHiCutHz = (float) delayHiCut.getValue();
    currentSettings.delaySync = delaySyncBtn.getToggleState();
    currentSettings.delayPingPong = delayPingPongBtn.getToggleState();
    delayPowerBtn.setToggleState (currentSettings.delayMix > 0.001f, juce::dontSendNotification);
    delayPowerBtn.setButtonText (currentSettings.delayMix > 0.001f ? "DLY ON" : "DLY OFF");

    currentSettings.chorusMix = (float) chorusMix.getValue();
    currentSettings.chorusRateHz = (float) chorusRate.getValue();
    currentSettings.chorusDepth = (float) chorusDepth.getValue();

    currentSettings.reverbMix = (float) reverbMix.getValue();
    currentSettings.reverbSize = (float) reverbSize.getValue();
    currentSettings.reverbDamping = (float) reverbDamping.getValue();

    if (onSettingsChanged)
        onSettingsChanged (currentSettings);
}

void EddieSynthPanel::updateSettingsFromCombos()
{
    currentSettings.waveform = waveformFromId (osc1WaveMenu.getSelectedId());
    saw.setEnabled (currentSettings.waveform == EddieWaveform::square);
    currentSettings.noiseType = noiseTypeFromId (noiseTypeMenu.getSelectedId());
    currentSettings.osc2Waveform = waveformFromId (osc2WaveMenu.getSelectedId());
    currentSettings.filterMode = filterModeFromId (filterModeMenu.getSelectedId());
    currentSettings.lfoShape = lfoShapeFromId (lfoShapeMenu.getSelectedId());
    currentSettings.lfoDest = lfoDestFromId (lfoDestMenu.getSelectedId());
    currentSettings.lfoSyncDiv = divFromId (lfoDivMenu.getSelectedId());
    currentSettings.delayDiv = divFromId (delayDivMenu.getSelectedId());

    if (onSettingsChanged)
        onSettingsChanged (currentSettings);
}

void EddieSynthPanel::updateSlidersFromSettings()
{
    auto setS = [] (juce::Slider& s, double v) { s.setValue (v, juce::dontSendNotification); };

    setS (gain, currentSettings.outputGain);
    setS (polyphony, currentSettings.polyphony);
    monoToggle.setToggleState (currentSettings.mono, juce::dontSendNotification);
    monoToggle.setButtonText (currentSettings.mono ? "MONO" : "POLY");
    polyphony.setEnabled (! currentSettings.mono);

    osc1WaveMenu.setSelectedId (waveformToId (currentSettings.waveform), juce::dontSendNotification);
    noiseTypeMenu.setSelectedId (noiseTypeToId (currentSettings.noiseType), juce::dontSendNotification);
    setS (saw, currentSettings.sawMix);
    setS (sub, currentSettings.subMix);
    setS (noise, currentSettings.noiseLevel);
    setS (osc1Level, currentSettings.osc1Level);
    saw.setEnabled (currentSettings.waveform == EddieWaveform::square);

    osc2WaveMenu.setSelectedId (waveformToId (currentSettings.osc2Waveform), juce::dontSendNotification);
    osc2EnabledBtn.setToggleState (currentSettings.osc2Enabled, juce::dontSendNotification);
    osc2EnabledBtn.setButtonText (currentSettings.osc2Enabled ? "ON" : "OFF");
    setS (osc2Semis, currentSettings.osc2Semis);
    setS (osc2Cents, currentSettings.osc2Cents);
    setS (osc2Level, currentSettings.osc2Level);

    setS (unisonVoices, currentSettings.unisonVoices);
    setS (unisonDetune, currentSettings.unisonDetuneCents);
    setS (unisonSpread, currentSettings.unisonSpread);

    setS (attack, currentSettings.attackMs);
    setS (decay, currentSettings.decayMs);
    setS (sustain, currentSettings.sustain);
    setS (release, currentSettings.releaseMs);

    filterModeMenu.setSelectedId (filterModeToId (currentSettings.filterMode), juce::dontSendNotification);
    setS (cutoff, currentSettings.filterCutoffHz);
    setS (reso, currentSettings.filterResonance);
    setS (fDrive, currentSettings.filterDrive);
    setS (kTrk, currentSettings.filterKeyTrack);

    setS (fAttack, currentSettings.fEnvAttackMs);
    setS (fDecay, currentSettings.fEnvDecayMs);
    setS (fSustain, currentSettings.fEnvSustain);
    setS (fRelease, currentSettings.fEnvReleaseMs);
    setS (fAmount, currentSettings.fEnvAmount);

    lfoShapeMenu.setSelectedId (lfoShapeToId (currentSettings.lfoShape), juce::dontSendNotification);
    lfoDestMenu.setSelectedId (lfoDestToId (currentSettings.lfoDest), juce::dontSendNotification);
    lfoDivMenu.setSelectedId (divToId (currentSettings.lfoSyncDiv), juce::dontSendNotification);
    lfoSyncBtn.setToggleState (currentSettings.lfoSync, juce::dontSendNotification);
    lfoSyncBtn.setButtonText (currentSettings.lfoSync ? "SYNC" : "FREE");
    lfoRate.setEnabled (! currentSettings.lfoSync);
    lfoDivMenu.setEnabled (currentSettings.lfoSync);
    setS (lfoRate, currentSettings.lfoRateHz);
    setS (lfoDepth, currentSettings.lfoDepth);

    setS (drive, currentSettings.drive);

    delaySyncBtn.setToggleState (currentSettings.delaySync, juce::dontSendNotification);
    delayPowerBtn.setToggleState (currentSettings.delayMix > 0.001f, juce::dontSendNotification);
    delayPowerBtn.setButtonText (currentSettings.delayMix > 0.001f ? "DLY ON" : "DLY OFF");
    delaySyncBtn.setButtonText (currentSettings.delaySync ? "SYNC" : "MS");
    delayPingPongBtn.setToggleState (currentSettings.delayPingPong, juce::dontSendNotification);
    delayPingPongBtn.setButtonText (currentSettings.delayPingPong ? "PP" : "ST");
    delayDivMenu.setSelectedId (divToId (currentSettings.delayDiv), juce::dontSendNotification);
    delayTime.setEnabled (! currentSettings.delaySync);
    delayDivMenu.setEnabled (currentSettings.delaySync);
    setS (delayMix, currentSettings.delayMix);
    setS (delayTime, currentSettings.delayMs);
    setS (delayFeedback, currentSettings.delayFeedback);
    setS (delayHiCut, currentSettings.delayHiCutHz);

    setS (chorusMix, currentSettings.chorusMix);
    setS (chorusRate, currentSettings.chorusRateHz);
    setS (chorusDepth, currentSettings.chorusDepth);

    setS (reverbMix, currentSettings.reverbMix);
    setS (reverbSize, currentSettings.reverbSize);
    setS (reverbDamping, currentSettings.reverbDamping);
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
        EddieSynthSettings& s = preset.settings;
        preset.name = child->getStringAttribute ("name", "Preset");
        s.outputGain = (float) child->getDoubleAttribute ("gain", s.outputGain);
        s.mono = child->getBoolAttribute ("mono", s.mono);
        s.polyphony = child->getIntAttribute ("poly", s.polyphony);
        s.waveform = waveformFromId (child->getIntAttribute ("waveform", waveformToId (s.waveform)));
        s.sawMix = (float) child->getDoubleAttribute ("saw", s.sawMix);
        s.subMix = (float) child->getDoubleAttribute ("sub", s.subMix);
        s.osc1Level = (float) child->getDoubleAttribute ("o1Lvl", s.osc1Level);
        s.noiseLevel = (float) child->getDoubleAttribute ("noise", s.noiseLevel);
        s.noiseType = noiseTypeFromId (child->getIntAttribute ("noiseType", noiseTypeToId (s.noiseType)));
        s.osc2Enabled = child->getBoolAttribute ("o2On", s.osc2Enabled);
        s.osc2Waveform = waveformFromId (child->getIntAttribute ("o2Wav", waveformToId (s.osc2Waveform)));
        s.osc2Semis = child->getIntAttribute ("o2Semi", s.osc2Semis);
        s.osc2Cents = (float) child->getDoubleAttribute ("o2Cent", s.osc2Cents);
        s.osc2Level = (float) child->getDoubleAttribute ("o2Lvl", s.osc2Level);
        s.unisonVoices = child->getIntAttribute ("uV", s.unisonVoices);
        s.unisonDetuneCents = (float) child->getDoubleAttribute ("uD", s.unisonDetuneCents);
        s.unisonSpread = (float) child->getDoubleAttribute ("uS", s.unisonSpread);
        s.attackMs = (float) child->getDoubleAttribute ("attack", s.attackMs);
        s.decayMs = (float) child->getDoubleAttribute ("decay", s.decayMs);
        s.sustain = (float) child->getDoubleAttribute ("sustain", s.sustain);
        s.releaseMs = (float) child->getDoubleAttribute ("release", s.releaseMs);
        s.filterMode = filterModeFromId (child->getIntAttribute ("fMode", filterModeToId (s.filterMode)));
        s.filterCutoffHz = (float) child->getDoubleAttribute ("fCut", s.filterCutoffHz);
        s.filterResonance = (float) child->getDoubleAttribute ("fRes", s.filterResonance);
        s.filterDrive = (float) child->getDoubleAttribute ("fDrv", s.filterDrive);
        s.filterKeyTrack = (float) child->getDoubleAttribute ("fKt", s.filterKeyTrack);
        s.fEnvAttackMs = (float) child->getDoubleAttribute ("fEA", s.fEnvAttackMs);
        s.fEnvDecayMs = (float) child->getDoubleAttribute ("fED", s.fEnvDecayMs);
        s.fEnvSustain = (float) child->getDoubleAttribute ("fES", s.fEnvSustain);
        s.fEnvReleaseMs = (float) child->getDoubleAttribute ("fER", s.fEnvReleaseMs);
        s.fEnvAmount = (float) child->getDoubleAttribute ("fEAmt", s.fEnvAmount);
        s.lfoShape = lfoShapeFromId (child->getIntAttribute ("lfoSh", lfoShapeToId (s.lfoShape)));
        s.lfoSync = child->getBoolAttribute ("lfoSync", s.lfoSync);
        s.lfoRateHz = (float) child->getDoubleAttribute ("lfoR", s.lfoRateHz);
        s.lfoSyncDiv = divFromId (child->getIntAttribute ("lfoDv", divToId (s.lfoSyncDiv)));
        s.lfoDepth = (float) child->getDoubleAttribute ("lfoDp", s.lfoDepth);
        s.lfoDest = lfoDestFromId (child->getIntAttribute ("lfoDst", lfoDestToId (s.lfoDest)));
        s.drive = (float) child->getDoubleAttribute ("drive", s.drive);
        s.delayMix = (float) child->getDoubleAttribute ("delayMix", s.delayMix);
        s.delaySync = child->getBoolAttribute ("delaySync", s.delaySync);
        s.delayDiv = divFromId (child->getIntAttribute ("delayDiv", divToId (s.delayDiv)));
        s.delayMs = (float) child->getDoubleAttribute ("delayMs", s.delayMs);
        s.delayFeedback = (float) child->getDoubleAttribute ("delayFb", s.delayFeedback);
        s.delayPingPong = child->getBoolAttribute ("delayPP", s.delayPingPong);
        s.delayHiCutHz = (float) child->getDoubleAttribute ("delayHC", s.delayHiCutHz);
        s.chorusMix = (float) child->getDoubleAttribute ("chMix", s.chorusMix);
        s.chorusRateHz = (float) child->getDoubleAttribute ("chRate", s.chorusRateHz);
        s.chorusDepth = (float) child->getDoubleAttribute ("chDep", s.chorusDepth);
        s.reverbMix = (float) child->getDoubleAttribute ("revMix", s.reverbMix);
        s.reverbSize = (float) child->getDoubleAttribute ("revSize", s.reverbSize);
        s.reverbDamping = (float) child->getDoubleAttribute ("revDamp", s.reverbDamping);
        presets.push_back (preset);
    }
}

void EddieSynthPanel::savePresets() const
{
    auto file = presetFile();
    file.getParentDirectory().createDirectory();

    juce::XmlElement root ("EddiePresets");
    for (const auto& p : presets)
    {
        const auto& s = p.settings;
        auto* item = root.createNewChildElement ("Preset");
        item->setAttribute ("name", p.name);
        item->setAttribute ("gain", s.outputGain);
        item->setAttribute ("mono", s.mono);
        item->setAttribute ("poly", s.polyphony);
        item->setAttribute ("waveform", waveformToId (s.waveform));
        item->setAttribute ("saw", s.sawMix);
        item->setAttribute ("sub", s.subMix);
        item->setAttribute ("o1Lvl", s.osc1Level);
        item->setAttribute ("noise", s.noiseLevel);
        item->setAttribute ("noiseType", noiseTypeToId (s.noiseType));
        item->setAttribute ("o2On", s.osc2Enabled);
        item->setAttribute ("o2Wav", waveformToId (s.osc2Waveform));
        item->setAttribute ("o2Semi", s.osc2Semis);
        item->setAttribute ("o2Cent", s.osc2Cents);
        item->setAttribute ("o2Lvl", s.osc2Level);
        item->setAttribute ("uV", s.unisonVoices);
        item->setAttribute ("uD", s.unisonDetuneCents);
        item->setAttribute ("uS", s.unisonSpread);
        item->setAttribute ("attack", s.attackMs);
        item->setAttribute ("decay", s.decayMs);
        item->setAttribute ("sustain", s.sustain);
        item->setAttribute ("release", s.releaseMs);
        item->setAttribute ("fMode", filterModeToId (s.filterMode));
        item->setAttribute ("fCut", s.filterCutoffHz);
        item->setAttribute ("fRes", s.filterResonance);
        item->setAttribute ("fDrv", s.filterDrive);
        item->setAttribute ("fKt", s.filterKeyTrack);
        item->setAttribute ("fEA", s.fEnvAttackMs);
        item->setAttribute ("fED", s.fEnvDecayMs);
        item->setAttribute ("fES", s.fEnvSustain);
        item->setAttribute ("fER", s.fEnvReleaseMs);
        item->setAttribute ("fEAmt", s.fEnvAmount);
        item->setAttribute ("lfoSh", lfoShapeToId (s.lfoShape));
        item->setAttribute ("lfoSync", s.lfoSync);
        item->setAttribute ("lfoR", s.lfoRateHz);
        item->setAttribute ("lfoDv", divToId (s.lfoSyncDiv));
        item->setAttribute ("lfoDp", s.lfoDepth);
        item->setAttribute ("lfoDst", lfoDestToId (s.lfoDest));
        item->setAttribute ("drive", s.drive);
        item->setAttribute ("delayMix", s.delayMix);
        item->setAttribute ("delaySync", s.delaySync);
        item->setAttribute ("delayDiv", divToId (s.delayDiv));
        item->setAttribute ("delayMs", s.delayMs);
        item->setAttribute ("delayFb", s.delayFeedback);
        item->setAttribute ("delayPP", s.delayPingPong);
        item->setAttribute ("delayHC", s.delayHiCutHz);
        item->setAttribute ("chMix", s.chorusMix);
        item->setAttribute ("chRate", s.chorusRateHz);
        item->setAttribute ("chDep", s.chorusDepth);
        item->setAttribute ("revMix", s.reverbMix);
        item->setAttribute ("revSize", s.reverbSize);
        item->setAttribute ("revDamp", s.reverbDamping);
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

void EddieSynthPanel::addPreset (const juce::String& name, const EddieSynthSettings& s)
{
    Preset preset { name.trim().isNotEmpty() ? name.trim() : "Preset", s };
    presets.push_back (preset);
}

void EddieSynthPanel::buttonClicked (juce::Button* button)
{
    if (button == &closeButton)
    {
        if (onClose) onClose();
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
    if (button == &monoToggle)
    {
        currentSettings.mono = monoToggle.getToggleState();
        monoToggle.setButtonText (currentSettings.mono ? "MONO" : "POLY");
        polyphony.setEnabled (! currentSettings.mono);
        if (onSettingsChanged) onSettingsChanged (currentSettings);
        return;
    }
    if (button == &osc2EnabledBtn)
    {
        currentSettings.osc2Enabled = osc2EnabledBtn.getToggleState();
        osc2EnabledBtn.setButtonText (currentSettings.osc2Enabled ? "ON" : "OFF");
        if (onSettingsChanged) onSettingsChanged (currentSettings);
        return;
    }
    if (button == &lfoSyncBtn)
    {
        currentSettings.lfoSync = lfoSyncBtn.getToggleState();
        lfoSyncBtn.setButtonText (currentSettings.lfoSync ? "SYNC" : "FREE");
        lfoRate.setEnabled (! currentSettings.lfoSync);
        lfoDivMenu.setEnabled (currentSettings.lfoSync);
        if (onSettingsChanged) onSettingsChanged (currentSettings);
        return;
    }
    if (button == &delayPowerBtn)
    {
        if (delayPowerBtn.getToggleState())
            delayMix.setValue (juce::jmax (0.24, delayMix.getValue()), juce::sendNotificationSync);
        else
            delayMix.setValue (0.0, juce::sendNotificationSync);

        delayPowerBtn.setButtonText (delayMix.getValue() > 0.001 ? "DLY ON" : "DLY OFF");
        return;
    }
    if (button == &delaySyncBtn)
    {
        currentSettings.delaySync = delaySyncBtn.getToggleState();
        delaySyncBtn.setButtonText (currentSettings.delaySync ? "SYNC" : "MS");
        delayTime.setEnabled (! currentSettings.delaySync);
        delayDivMenu.setEnabled (currentSettings.delaySync);
        if (onSettingsChanged) onSettingsChanged (currentSettings);
        return;
    }
    if (button == &delayPingPongBtn)
    {
        currentSettings.delayPingPong = delayPingPongBtn.getToggleState();
        delayPingPongBtn.setButtonText (currentSettings.delayPingPong ? "PP" : "ST");
        if (onSettingsChanged) onSettingsChanged (currentSettings);
        return;
    }
    if (button == &tabOsc)    { setActiveTab (Tab::Osc);    return; }
    if (button == &tabFilter) { setActiveTab (Tab::Filter); return; }
    if (button == &tabMod)    { setActiveTab (Tab::Mod);    return; }
    if (button == &tabFx)     { setActiveTab (Tab::Fx);     return; }
}

void EddieSynthPanel::comboBoxChanged (juce::ComboBox* cb)
{
    if (cb == &presetMenu)
    {
        const int index = presetMenu.getSelectedId() - 1;
        if (! juce::isPositiveAndBelow (index, (int) presets.size()))
            return;

        currentSettings = presets[(size_t) index].settings;
        presetName.setText (presets[(size_t) index].name, juce::dontSendNotification);
        updateSlidersFromSettings();
        if (onSettingsChanged) onSettingsChanged (currentSettings);
        return;
    }
    updateSettingsFromCombos();
}

void EddieSynthPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (getLocalBounds());

    auto outer = getLocalBounds().reduced (16);
    auto kbStrip = outer.removeFromBottom (96);
    juce::ignoreUnused (kbStrip); // keyboard area, drawn by child

    // The faceplate — full opacity, vintage frame is the visual identity.
    if (faceplateImage.isValid())
    {
        g.drawImageWithin (faceplateImage,
                           outer.getX(), outer.getY(),
                           outer.getWidth(), outer.getHeight(),
                           juce::RectanglePlacement::stretchToFit, false);
    }
    else
    {
        // Fallback if asset failed to load
        juce::ColourGradient bg (juce::Colour (0xFF2A1810), (float) outer.getCentreX(), (float) outer.getY(),
                                 juce::Colour (0xFF0A0807), (float) outer.getCentreX(), (float) outer.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (outer.toFloat(), 10.0f);
    }
}

EddieSynthPanel::~EddieSynthPanel()
{
    juce::Slider* allSliders[] = {
        &gain, &polyphony,
        &saw, &sub, &noise, &osc1Level,
        &osc2Semis, &osc2Cents, &osc2Level,
        &unisonVoices, &unisonDetune, &unisonSpread,
        &cutoff, &reso, &fDrive, &kTrk,
        &fAttack, &fDecay, &fSustain, &fRelease, &fAmount,
        &attack, &decay, &sustain, &release,
        &lfoRate, &lfoDepth,
        &drive, &delayMix, &delayTime, &delayFeedback, &delayHiCut,
        &chorusMix, &chorusRate, &chorusDepth,
        &reverbMix, &reverbSize, &reverbDamping
    };
    for (auto* s : allSliders)
        s->setLookAndFeel (nullptr);

    ThemeManager::get().removeChangeListener (this);
}

void EddieSynthPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void EddieSynthPanel::resized()
{
    auto outer = getLocalBounds().reduced (16);

    // Keyboard sits BELOW the faceplate so the wood frame stays uncluttered.
    auto kb = outer.removeFromBottom (96);
    previewKeyboard.setBounds (kb.reduced (4, 6));

    // The remaining `outer` rect is the faceplate area. Hide title labels —
    // the faceplate's printed logo IS the title.
    title.setBounds ({});
    subtitle.setBounds ({});

    const int W = outer.getWidth();
    const int H = outer.getHeight();
    auto px = [&] (float fx) { return outer.getX() + (int) std::round (W * fx); };
    auto py = [&] (float fy) { return outer.getY() + (int) std::round (H * fy); };
    auto rect = [&] (float fx, float fy, float fw, float fh)
    {
        return juce::Rectangle<int> (px (fx), py (fy),
                                     (int) std::round (W * fw),
                                     (int) std::round (H * fh));
    };

    // Close button — small overlay in the wood frame's top-right corner.
    closeButton.setBounds (rect (0.945f, 0.025f, 0.040f, 0.075f));

    // Faceplate insets (calibrated to the eddie_full_plugin_frame.png artwork).
    // Header band (logo) reserved 0..0.30 vertically — left untouched.
    auto leftInset   = rect (0.045f, 0.32f, 0.450f, 0.28f);
    auto rightInset  = rect (0.505f, 0.32f, 0.450f, 0.28f);
    auto bottomInset = rect (0.045f, 0.62f, 0.910f, 0.34f);

    auto placeKnob = [] (juce::Rectangle<int> r, juce::Label& lbl, juce::Slider& s)
    {
        const int labelH = 14;
        lbl.setBounds (r.getX(), r.getY(), r.getWidth(), labelH);
        s.setBounds (r.withTrimmedTop (labelH));
    };

    // ---- Top-left inset: Preset utilities ----
    {
        auto sec = leftInset.reduced (10, 8);
        auto top = sec.removeFromTop (28);
        presetMenu.setBounds (top.removeFromLeft ((int) (sec.getWidth() * 0.42)).reduced (2));
        top.removeFromLeft (4);
        presetName.setBounds (top.removeFromLeft ((int) (sec.getWidth() * 0.40)).reduced (2));
        top.removeFromLeft (4);
        savePreset.setBounds (top.reduced (2));

        // Below preset row: a hint subtitle + mono toggle on the right
        auto btnRow = sec.removeFromTop (28);
        monoToggle.setBounds (btnRow.removeFromRight (70).reduced (2));
    }

    // ---- Top-right inset: Master + tab buttons ----
    {
        auto sec = rightInset.reduced (10, 8);

        auto masterRow = sec.removeFromTop ((int) (sec.getHeight() * 0.55));
        const int kw = masterRow.getWidth() / 2;
        placeKnob (masterRow.removeFromLeft (kw), gainLabel, gain);
        placeKnob (masterRow, polyLabel, polyphony);

        // Tab buttons in a single row across the bottom of the right inset.
        sec.removeFromTop (4);
        auto tabRow = sec;
        const int tabW = tabRow.getWidth() / 4;
        tabOsc.setBounds    (tabRow.removeFromLeft (tabW).reduced (2));
        tabFilter.setBounds (tabRow.removeFromLeft (tabW).reduced (2));
        tabMod.setBounds    (tabRow.removeFromLeft (tabW).reduced (2));
        tabFx.setBounds     (tabRow.reduced (2));
    }

    // ---- Bottom inset: active tab content ----
    auto content = bottomInset.reduced (10, 8);

    auto sectionHeader = [] (juce::Label& lbl, juce::Rectangle<int> r)
    {
        lbl.setBounds (r);
    };

    if (activeTab == Tab::Osc)
    {
        const int W3 = content.getWidth();
        const int oscW = (int) (W3 * 0.36);
        const int o2W  = (int) (W3 * 0.36);
        const int uniW = W3 - oscW - o2W;

        auto oscSec = content.removeFromLeft (oscW);
        auto o2Sec  = content.removeFromLeft (o2W);
        auto uniSec = content;

        // OSC1
        sectionHeader (osc1Header, oscSec.removeFromTop (16).withTrimmedLeft (4));
        auto oscTop = oscSec.removeFromTop (24);
        osc1WaveLabel.setBounds (oscTop.removeFromLeft (34));
        osc1WaveMenu.setBounds (oscTop.removeFromLeft ((int) (oscSec.getWidth() * 0.34)).reduced (2));
        noiseTypeLabel.setBounds (oscTop.removeFromLeft (44));
        noiseTypeMenu.setBounds (oscTop.reduced (2));
        oscSec = oscSec.reduced (2, 4);
        const int kwO = oscSec.getWidth() / 4;
        placeKnob (oscSec.removeFromLeft (kwO), sawLabel,       saw);
        placeKnob (oscSec.removeFromLeft (kwO), subLabel,       sub);
        placeKnob (oscSec.removeFromLeft (kwO), noiseLabel,     noise);
        placeKnob (oscSec,                       osc1LevelLabel, osc1Level);

        // OSC2
        sectionHeader (osc2Header, o2Sec.removeFromTop (16).withTrimmedLeft (4));
        auto o2Top = o2Sec.removeFromTop (24);
        osc2WaveLabel.setBounds (o2Top.removeFromLeft (40));
        osc2WaveMenu.setBounds (o2Top.removeFromLeft ((int) (o2Sec.getWidth() * 0.45)).reduced (2));
        osc2EnabledBtn.setBounds (o2Top.reduced (2));
        o2Sec = o2Sec.reduced (2, 4);
        const int kwO2 = o2Sec.getWidth() / 3;
        placeKnob (o2Sec.removeFromLeft (kwO2), osc2SemisLabel, osc2Semis);
        placeKnob (o2Sec.removeFromLeft (kwO2), osc2CentsLabel, osc2Cents);
        placeKnob (o2Sec,                        osc2LevelLabel, osc2Level);

        // Unison
        sectionHeader (unisonHeader, uniSec.removeFromTop (16).withTrimmedLeft (4));
        uniSec.removeFromTop (24);
        uniSec = uniSec.reduced (2, 4);
        const int kwU = uniSec.getWidth() / 3;
        placeKnob (uniSec.removeFromLeft (kwU), unisonVoicesLabel, unisonVoices);
        placeKnob (uniSec.removeFromLeft (kwU), unisonDetuneLabel, unisonDetune);
        placeKnob (uniSec,                       unisonSpreadLabel, unisonSpread);
    }
    else if (activeTab == Tab::Filter)
    {
        const int W3 = content.getWidth();
        const int fW  = (int) (W3 * 0.40);
        const int feW = (int) (W3 * 0.36);
        const int aeW = W3 - fW - feW;

        auto fSec  = content.removeFromLeft (fW);
        auto feSec = content.removeFromLeft (feW);
        auto aeSec = content;

        sectionHeader (filterHeader, fSec.removeFromTop (16).withTrimmedLeft (4));
        auto fTop = fSec.removeFromTop (24);
        filterModeLabel.setBounds (fTop.removeFromLeft (40));
        filterModeMenu.setBounds (fTop.reduced (2));
        fSec = fSec.reduced (2, 4);
        const int kwF = fSec.getWidth() / 4;
        placeKnob (fSec.removeFromLeft (kwF), cutoffLabel, cutoff);
        placeKnob (fSec.removeFromLeft (kwF), resoLabel,   reso);
        placeKnob (fSec.removeFromLeft (kwF), fDriveLabel, fDrive);
        placeKnob (fSec,                       kTrkLabel,   kTrk);

        sectionHeader (fEnvHeader, feSec.removeFromTop (16).withTrimmedLeft (4));
        feSec.removeFromTop (24);
        feSec = feSec.reduced (2, 4);
        const int kwFE = feSec.getWidth() / 5;
        placeKnob (feSec.removeFromLeft (kwFE), fAttackLabel,  fAttack);
        placeKnob (feSec.removeFromLeft (kwFE), fDecayLabel,   fDecay);
        placeKnob (feSec.removeFromLeft (kwFE), fSustainLabel, fSustain);
        placeKnob (feSec.removeFromLeft (kwFE), fReleaseLabel, fRelease);
        placeKnob (feSec,                        fAmountLabel,  fAmount);

        sectionHeader (ampEnvHeader, aeSec.removeFromTop (16).withTrimmedLeft (4));
        aeSec.removeFromTop (24);
        aeSec = aeSec.reduced (2, 4);
        const int kwAE = aeSec.getWidth() / 4;
        placeKnob (aeSec.removeFromLeft (kwAE), attackLabel,  attack);
        placeKnob (aeSec.removeFromLeft (kwAE), decayLabel,   decay);
        placeKnob (aeSec.removeFromLeft (kwAE), sustainLabel, sustain);
        placeKnob (aeSec,                        releaseLabel, release);
    }
    else if (activeTab == Tab::Mod)
    {
        sectionHeader (lfoHeader, content.removeFromTop (16).withTrimmedLeft (4));
        auto top = content.removeFromTop (28);
        lfoShapeLabel.setBounds (top.removeFromLeft (44));
        lfoShapeMenu.setBounds (top.removeFromLeft (90).reduced (2));
        top.removeFromLeft (8);
        lfoSyncBtn.setBounds (top.removeFromLeft (60).reduced (2));
        lfoDivLabel.setBounds (top.removeFromLeft (28));
        lfoDivMenu.setBounds (top.removeFromLeft (90).reduced (2));
        top.removeFromLeft (12);
        lfoDestLabel.setBounds (top.removeFromLeft (44));
        lfoDestMenu.setBounds (top.reduced (2));

        // Two big knobs centered
        auto knobsRow = content.reduced (4, 6);
        const int third = knobsRow.getWidth() / 3;
        knobsRow.removeFromLeft (third);
        const int kwL = (knobsRow.getWidth()) / 2;
        placeKnob (knobsRow.removeFromLeft (kwL), lfoRateLabel, lfoRate);
        placeKnob (knobsRow,                       lfoDepthLabel, lfoDepth);
    }
    else if (activeTab == Tab::Fx)
    {
        sectionHeader (fxHeader, content.removeFromTop (16).withTrimmedLeft (4));
        auto top = content.removeFromTop (24);
        // Toggle row for delay power, sync/PP, and division selector.
        delayPowerBtn.setBounds (top.removeFromLeft (74).reduced (2));
        top.removeFromLeft (6);
        delaySyncBtn.setBounds (top.removeFromLeft (60).reduced (2));
        delayDivLabel.setBounds (top.removeFromLeft (28));
        delayDivMenu.setBounds (top.removeFromLeft (90).reduced (2));
        delayPingPongBtn.setBounds (top.removeFromLeft (50).reduced (2));

        content = content.reduced (2, 4);
        // 11 knobs across
        const int n = 11;
        const int kwx = content.getWidth() / n;
        placeKnob (content.removeFromLeft (kwx), driveLabel,           drive);
        placeKnob (content.removeFromLeft (kwx), delayMixLabel,        delayMix);
        placeKnob (content.removeFromLeft (kwx), delayTimeLabel,       delayTime);
        placeKnob (content.removeFromLeft (kwx), delayFbLabel,         delayFeedback);
        placeKnob (content.removeFromLeft (kwx), delayHiCutLabel,      delayHiCut);
        placeKnob (content.removeFromLeft (kwx), chorusMixLabel,       chorusMix);
        placeKnob (content.removeFromLeft (kwx), chorusRateLabel,      chorusRate);
        placeKnob (content.removeFromLeft (kwx), chorusDepthLabel,     chorusDepth);
        placeKnob (content.removeFromLeft (kwx), reverbMixLabel,       reverbMix);
        placeKnob (content.removeFromLeft (kwx), reverbSizeLabel,      reverbSize);
        placeKnob (content,                       reverbDampingLabel,  reverbDamping);
    }

    // The MASTER section header is no longer needed (master controls live in
    // the top-right inset and are always visible).
    masterHeader.setBounds ({});
}

} // namespace aidaw
