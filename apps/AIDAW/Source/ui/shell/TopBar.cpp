#include "TopBar.h"

AIDAWLook::AIDAWLook()
{
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2A2A));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF3A3A3A));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    setColour (juce::Label::textColourId, juce::Colours::white);

    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF202020));
    setColour (juce::TextEditor::textColourId, juce::Colours::white);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, juce::Colour (0x88FFFFFF));
    setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextEditor::highlightColourId, juce::Colour (0x66FFFFFF));
}

void AIDAWLook::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                      const juce::Colour& base,
                                      bool highlighted,
                                      bool down)
{
    auto r = b.getLocalBounds().toFloat();
    auto bg = base.brighter (highlighted ? 0.10f : 0.0f).darker (down ? 0.20f : 0.0f);
    g.setColour (bg);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (juce::Colour (0x22FFFFFF));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
}

TopBar::TopBar()
{
    setLookAndFeel (&look);

    playPause.setButtonText ("Play");
    playPause.addListener (this);
    playPause.setTooltip ("Space = Play/Pause");
    stop.setButtonText ("Stop");
    stop.addListener (this);
    stop.setTooltip ("Stop and return to start");
    record.setButtonText ("Rec");
    record.addListener (this);
    record.setTooltip ("Arm recording");
    addAndMakeVisible (playPause);
    addAndMakeVisible (stop);
    addAndMakeVisible (record);

    modeComposer.setButtonText ("Composer");
    modeMidi.setButtonText ("MIDI Editor");
    modeMixer.setButtonText ("Mixer");
    for (auto* b : { &modeComposer, &modeMidi, &modeMixer })
    {
        b->setClickingTogglesState (true);
        b->addListener (this);
        addAndMakeVisible (b);
    }
    setMode (AppMode::Composer, false);

    clusterDot.setText (".", juce::dontSendNotification);
    clusterDot.setJustificationType (juce::Justification::centred);
    clusterDot.setColour (juce::Label::textColourId, juce::Colour (0x66FFFFFF));
    clusterDot.setFont (juce::Font (20.0f, juce::Font::plain));
    addAndMakeVisible (clusterDot);

    title.setText ("AIDAW", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, juce::Colour (0x88FFFFFF));
    title.setFont (juce::Font (18.0f, juce::Font::bold));
    title.setEditable (true, true, false);
    title.onTextChange = [this]
    {
        if (onTitleChanged)
            onTitleChanged (title.getText());
    };
    addAndMakeVisible (title);

    bpmLabelLeft.setText ("BPM", juce::dontSendNotification);
    bpmLabelLeft.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bpmLabelLeft);

    minusBtn.addListener (this);
    minusBtn.setButtonText ("-");
    minusBtn.setTooltip ("Tempo -1");
    addAndMakeVisible (minusBtn);

    bpmEdit.setText (juce::String (currentBPM, 2), juce::dontSendNotification);
    bpmEdit.setInputRestrictions (6, "0123456789.");
    bpmEdit.setJustification (juce::Justification::centred);
    bpmEdit.setScrollbarsShown (false);
    bpmEdit.addListener (this);
    bpmEdit.setTooltip ("Type tempo and press Enter");
    addAndMakeVisible (bpmEdit);

    bpmLabelRight.setText ("bpm", juce::dontSendNotification);
    bpmLabelRight.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bpmLabelRight);

    plusBtn.addListener (this);
    plusBtn.setButtonText ("+");
    plusBtn.setTooltip ("Tempo +1");
    addAndMakeVisible (plusBtn);

    clickToggle.setButtonText ("Click");
    clickToggle.setToggleState (clickEnabled, juce::dontSendNotification);
    clickToggle.addListener (this);
    clickToggle.setTooltip ("Metronome on/off (also toggles grid snap)");
    addAndMakeVisible (clickToggle);

    btnMin.setButtonText ("-");
    btnMin.addListener (this);
    btnMin.setTooltip ("Minimize");
    addAndMakeVisible (btnMin);

    btnClose.setButtonText ("X");
    btnClose.addListener (this);
    btnClose.setTooltip ("Close");
    addAndMakeVisible (btnClose);
}

TopBar::~TopBar()
{
    setLookAndFeel (nullptr);
}

void TopBar::setPlaying (bool shouldPlay)
{
    isPlaying = shouldPlay;
    const bool showPause = isPlaying && ! recordArmed;
    playPause.setButtonText (showPause ? "Pause" : "Play");
    playPause.setColour (juce::TextButton::buttonColourId,
                         isPlaying ? juce::Colour (0xFF3B82F6) : juce::Colour (0xFF2A2A2A));
    repaint();
}

void TopBar::setRecordArmed (bool armed)
{
    recordArmed = armed;
    record.setColour (juce::TextButton::buttonColourId,
                      armed ? juce::Colour (0xFFEF4444) : juce::Colour (0xFF2A2A2A));
    if (isPlaying)
        setPlaying (true);

    repaint();
}

double TopBar::getBPM() const
{
    return currentBPM;
}

void TopBar::setBPM (double bpm)
{
    currentBPM = juce::jlimit (40.0, 300.0, bpm);
    bpmEdit.setText (juce::String (currentBPM, 2), juce::dontSendNotification);
    if (onTempoChanged)
        onTempoChanged (currentBPM);
}

void TopBar::setClickEnabled (bool enabled)
{
    clickEnabled = enabled;
    clickToggle.setToggleState (clickEnabled, juce::dontSendNotification);
}

void TopBar::setMode (AppMode mode, bool fireCallback)
{
    currentMode = mode;
    modeComposer.setToggleState (mode == AppMode::Composer, juce::dontSendNotification);
    modeMidi.setToggleState (mode == AppMode::Midi, juce::dontSendNotification);
    modeMixer.setToggleState (mode == AppMode::Mixer, juce::dontSendNotification);
    if (fireCallback && onModeChanged)
        onModeChanged (mode);

    repaint();
}

TopBar::AppMode TopBar::getMode() const
{
    return currentMode;
}

void TopBar::paint (juce::Graphics& g)
{
    juce::Colour top = juce::Colour (0xFF0E0E0E);
    juce::Colour bot = juce::Colour (0xFF151515);
    g.setGradientFill (juce::ColourGradient (top, 0, 0, bot, 0, (float) getHeight(), false));
    g.fillAll();

    g.setColour (juce::Colour (0x33FFFFFF));
    g.fillRect (getLocalBounds().removeFromBottom (1));

    auto pill = bpmPillBounds.toFloat();
    g.setColour (juce::Colour (0xFF1C1C1C));
    g.fillRoundedRectangle (pill, 12.0f);
    g.setColour (juce::Colour (0x22FFFFFF));
    g.drawRoundedRectangle (pill, 12.0f, 1.0f);
}

void TopBar::resized()
{
    const int sidePad = 12;
    const int vpad = 8;
    const int gap = 8;
    const int btnH = 32;
    const int desiredBtnW = 126;
    const int minBtnW = 96;
    const int titleW = 220;
    const int titleGutter = titleW + 32;
    const int winBtnW = 48;
    const int pillW = 320;
    const int clickW = 76;

    auto r = getLocalBounds().reduced (sidePad, vpad);

    auto win = r.removeFromRight (winBtnW * 2 + gap);
    btnClose.setBounds (win.removeFromRight (winBtnW).reduced (4));
    win.removeFromRight (gap);
    btnMin.setBounds (win.removeFromRight (winBtnW).reduced (4));

    r.removeFromRight (12);
    auto pillBlock = r.removeFromRight (pillW + gap + clickW);
    bpmPillBounds = pillBlock.removeFromLeft (pillW);

    auto inside = bpmPillBounds.reduced (12, 8);
    const int totalW = inside.getWidth();
    const int smallBtnW = 28;
    const int labelW = 42;
    const int editW = totalW - (smallBtnW * 2 + labelW * 2 + 16);

    bpmLabelLeft.setBounds (inside.removeFromLeft (labelW));
    minusBtn.setBounds (inside.removeFromLeft (smallBtnW).reduced (2, 2));
    inside.removeFromLeft (4);
    bpmEdit.setBounds (inside.removeFromLeft (editW).reduced (2, 0));
    inside.removeFromLeft (4);
    bpmLabelRight.setBounds (inside.removeFromLeft (labelW));
    plusBtn.setBounds (inside.removeFromLeft (smallBtnW).reduced (2, 2));

    pillBlock.removeFromLeft (gap);
    clickToggle.setBounds (pillBlock.removeFromLeft (clickW));

    const int cx = getWidth() / 2;
    juce::Rectangle<int> gutter (cx - titleGutter / 2, r.getY(), titleGutter, r.getHeight());
    auto leftSide = r.removeFromLeft (juce::jmax (0, gutter.getX() - r.getX()));

    const int dotW = 16;
    const int avail = leftSide.getWidth();
    int w = (avail - (gap * 5) - dotW) / 6;
    w = juce::jlimit (minBtnW, desiredBtnW, w);

    auto t = leftSide;
    playPause.setBounds (t.removeFromLeft (w).withHeight (btnH));
    t.removeFromLeft (gap);
    stop.setBounds (t.removeFromLeft (w).withHeight (btnH));
    t.removeFromLeft (gap);
    record.setBounds (t.removeFromLeft (w).withHeight (btnH));
    t.removeFromLeft (gap);

    clusterDot.setBounds (t.removeFromLeft (dotW).withHeight (btnH));
    t.removeFromLeft (gap);

    modeComposer.setBounds (t.removeFromLeft (w).withHeight (btnH));
    t.removeFromLeft (gap);
    modeMidi.setBounds (t.removeFromLeft (w).withHeight (btnH));
    t.removeFromLeft (gap);
    modeMixer.setBounds (t.removeFromLeft (w).withHeight (btnH));

    title.setBounds (cx - titleW / 2, vpad, titleW, btnH);
    title.toFront (false);
}

void TopBar::buttonClicked (juce::Button* button)
{
    if (button == &playPause)
    {
        if (recordArmed && isPlaying)
            return;

        setPlaying (! isPlaying);
        if (onPlayPause)
            onPlayPause (isPlaying);
    }
    else if (button == &stop)
    {
        if (onStop)
            onStop();
        setPlaying (false);
    }
    else if (button == &record)
    {
        recordArmed = ! recordArmed;
        setRecordArmed (recordArmed);
        if (onRecordArm)
            onRecordArm (recordArmed);
    }
    else if (button == &clickToggle)
    {
        clickEnabled = clickToggle.getToggleState();
        if (onClickToggled)
            onClickToggled (clickEnabled);
    }
    else if (button == &minusBtn)
    {
        setBPM (currentBPM - 1.0);
    }
    else if (button == &plusBtn)
    {
        setBPM (currentBPM + 1.0);
    }
    else if (button == &btnMin)
    {
        if (onMinimize)
            onMinimize();
    }
    else if (button == &btnClose)
    {
        if (onClose)
            onClose();
    }
    else if (button == &modeComposer)
    {
        setMode (AppMode::Composer);
    }
    else if (button == &modeMidi)
    {
        setMode (AppMode::Midi);
    }
    else if (button == &modeMixer)
    {
        setMode (AppMode::Mixer);
    }
}

void TopBar::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    applyBpmFrom (editor);
}

void TopBar::textEditorFocusLost (juce::TextEditor& editor)
{
    applyBpmFrom (editor);
}

void TopBar::applyBpmFrom (juce::TextEditor& editor)
{
    auto val = editor.getText().trim().getDoubleValue();
    val = juce::jlimit (40.0, 300.0, val);
    currentBPM = val;
    editor.setText (juce::String (val, 2), juce::dontSendNotification);
    if (onTempoChanged)
        onTempoChanged (val);
}
