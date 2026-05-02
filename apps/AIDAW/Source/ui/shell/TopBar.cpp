#include "TopBar.h"
#include "../shared/Theme.h"
#include "../shared/ThemeManager.h"

TopBar::TopBar()
{
    // Transport
    for (auto* b : { &playPause, &stop, &record })
    {
        b->addListener(this);
        b->setIconScale(0.42f);
        addAndMakeVisible(*b);
    }
    record.setAccentTint(false);

    // Segmented mode pills
    for (auto* b : { &modeComposer, &modeMidi, &modeMixer })
    {
        b->setClickingTogglesState(true);
        b->setRadioGroupId(0xA1DA, juce::dontSendNotification);
        b->addListener(this);
        addAndMakeVisible(*b);
    }
    setMode(AppMode::Composer, false);

    for (auto* b : { &playbackPattern, &playbackSong })
    {
        b->setClickingTogglesState(true);
        b->setRadioGroupId(0xA1DB, juce::dontSendNotification);
        b->addListener(this);
        addAndMakeVisible(*b);
    }
    setPlaybackMode(PlaybackMode::Song, false);

    for (auto* b : { &openProject, &saveProject })
    {
        b->addListener(this);
        b->setWantsKeyboardFocus(false);
        addAndMakeVisible(*b);
    }

    // Title
    title.setText("AIDAW", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centred);
    title.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    title.setEditable(false, true, false);
    title.onTextChange = [this]
    {
        if (onTitleChanged)
            onTitleChanged(title.getText());
    };
    addAndMakeVisible(title);

    // Tempo pill
    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    bpmLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
    addAndMakeVisible(bpmLabel);

    minusBtn.addListener(this); minusBtn.setIconScale(0.55f); addAndMakeVisible(minusBtn);
    plusBtn .addListener(this); plusBtn .setIconScale(0.55f); addAndMakeVisible(plusBtn);

    bpmEdit.setText(juce::String(currentBPM, 2), juce::dontSendNotification);
    bpmEdit.setInputRestrictions(6, "0123456789.");
    bpmEdit.setJustification(juce::Justification::centred);
    bpmEdit.setScrollbarsShown(false);
    bpmEdit.setBorder(juce::BorderSize<int>(0));
    bpmEdit.addListener(this);
    bpmEdit.setTooltip("Type tempo and press Enter");
    bpmEdit.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    addAndMakeVisible(bpmEdit);

    // Click toggle (icon-style button that toggles)
    clickToggle.setClickingTogglesState(true);
    clickToggle.setToggleState(clickEnabled, juce::dontSendNotification);
    clickToggle.setAccentTint(true);
    clickToggle.setIconScale(0.55f);
    clickToggle.addListener(this);
    addAndMakeVisible(clickToggle);

    // Theme switcher
    btnTheme.setIconScale(0.55f);
    btnTheme.addListener(this);
    addAndMakeVisible(btnTheme);

    // Window
    btnMin  .setIconScale(0.55f); btnMin  .addListener(this); addAndMakeVisible(btnMin);
    btnClose.setIconScale(0.45f); btnClose.addListener(this); addAndMakeVisible(btnClose);

    ThemeManager::get().addChangeListener(this);
    refreshColours();
}

TopBar::~TopBar()
{
    ThemeManager::get().removeChangeListener(this);
}

void TopBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshColours();
    repaint();
}

void TopBar::refreshColours()
{
    title.setColour(juce::Label::textColourId, juce::Colour(Theme::colTextDim));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(Theme::colTextDim));

    bpmEdit.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    bpmEdit.setColour(juce::TextEditor::textColourId,       juce::Colour(Theme::colText));
    bpmEdit.setColour(juce::TextEditor::outlineColourId,    juce::Colours::transparentBlack);
    bpmEdit.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    bpmEdit.applyColourToAllText(juce::Colour(Theme::colText));

    auto styleMode = [](juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId,   juce::Colour(Theme::colBtnIdle));
        b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Theme::colBtnActive));
        b.setColour(juce::TextButton::textColourOffId,  juce::Colour(Theme::colTextDim));
        b.setColour(juce::TextButton::textColourOnId,   juce::Colour(Theme::colAccent));
    };
    styleMode(modeComposer);
    styleMode(modeMidi);
    styleMode(modeMixer);
    styleMode(playbackPattern);
    styleMode(playbackSong);
    styleMode(openProject);
    styleMode(saveProject);
}

void TopBar::setPlaying (bool shouldPlay)
{
    isPlaying = shouldPlay;
    const bool showPause = isPlaying && ! recordArmed;
    playPause.setIcon(showPause ? Icons::pause() : Icons::play());
    playPause.setAccentTint(isPlaying);
    repaint();
}

void TopBar::setRecordArmed (bool armed)
{
    recordArmed = armed;
    record.setToggleState(armed, juce::dontSendNotification);
    if (isPlaying) setPlaying(true);
    repaint();
}

double TopBar::getBPM() const                { return currentBPM; }

void TopBar::setBPM (double bpm)
{
    currentBPM = juce::jlimit(40.0, 300.0, bpm);
    bpmEdit.setText(juce::String(currentBPM, 2), juce::dontSendNotification);
    if (onTempoChanged) onTempoChanged(currentBPM);
}

void TopBar::setClickEnabled (bool enabled)
{
    clickEnabled = enabled;
    clickToggle.setToggleState(clickEnabled, juce::dontSendNotification);
}

void TopBar::setMode (AppMode mode, bool fireCallback)
{
    currentMode = mode;
    modeComposer.setToggleState(mode == AppMode::Composer, juce::dontSendNotification);
    modeMidi    .setToggleState(mode == AppMode::Midi,     juce::dontSendNotification);
    modeMixer   .setToggleState(mode == AppMode::Mixer,    juce::dontSendNotification);
    playbackPattern.setVisible(mode == AppMode::Midi);
    playbackSong.setVisible(mode == AppMode::Midi);
    title.setVisible(mode != AppMode::Midi);
    resized();
    if (fireCallback && onModeChanged) onModeChanged(mode);
    repaint();
}

TopBar::AppMode TopBar::getMode() const      { return currentMode; }

void TopBar::setPlaybackMode (PlaybackMode mode, bool fireCallback)
{
    currentPlaybackMode = mode;
    playbackPattern.setToggleState(mode == PlaybackMode::Pattern, juce::dontSendNotification);
    playbackSong   .setToggleState(mode == PlaybackMode::Song,    juce::dontSendNotification);
    if (fireCallback && onPlaybackModeChanged) onPlaybackModeChanged(mode);
    repaint();
}

TopBar::PlaybackMode TopBar::getPlaybackMode() const { return currentPlaybackMode; }

void TopBar::paint (juce::Graphics& g)
{
    // chrome gradient
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(Theme::colChromeTop), 0, 0,
        juce::Colour(Theme::colChromeBot), 0, (float) getHeight(), false));
    g.fillAll();

    // bottom hairline
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.fillRect(getLocalBounds().removeFromBottom(1));

    // BPM pill
    auto pill = bpmPillBounds.toFloat();
    g.setColour(juce::Colour(Theme::colPillBg));
    g.fillRoundedRectangle(pill, 10.0f);
    g.setColour(juce::Colour(Theme::colBtnStroke));
    g.drawRoundedRectangle(pill, 10.0f, 1.0f);

    // Connected pill background behind segmented switcher
    if (modeComposer.getWidth() > 0)
    {
        auto seg = juce::Rectangle<int>(modeComposer.getX(),
                                        modeComposer.getY(),
                                        modeMixer.getRight() - modeComposer.getX(),
                                        modeComposer.getHeight()).toFloat();
        g.setColour(juce::Colour(Theme::colPillBg));
        g.fillRoundedRectangle(seg, 10.0f);
        g.setColour(juce::Colour(Theme::colBtnStroke));
        g.drawRoundedRectangle(seg, 10.0f, 1.0f);
    }

    if (playbackPattern.isVisible() && playbackPattern.getWidth() > 0)
    {
        auto seg = juce::Rectangle<int>(playbackPattern.getX(),
                                        playbackPattern.getY(),
                                        playbackSong.getRight() - playbackPattern.getX(),
                                        playbackPattern.getHeight()).toFloat();
        g.setColour(juce::Colour(Theme::colPillBg));
        g.fillRoundedRectangle(seg, 10.0f);
        g.setColour(juce::Colour(Theme::colBtnStroke));
        g.drawRoundedRectangle(seg, 10.0f, 1.0f);
    }
}

void TopBar::resized()
{
    const int sidePad = 18;
    const int vpad = 10;
    const int gap = 8;
    const int btnH = 32;
    const int iconBtnW = 36;
    const int modeBtnW = 114;
    const int playbackBtnW = 104;
    const int fileBtnW = 72;
    const int titleW = 220;
    const int winBtnW = 36;
    const int themeBtnW = 36;
    const int pillW = 240;
    const int clickW = 36;

    auto r = getLocalBounds().reduced(sidePad, vpad);

    // ----- Right cluster: theme | min | close -----
    auto winSlot = r.removeFromRight(winBtnW * 2 + themeBtnW + gap * 2);
    btnClose.setBounds(winSlot.removeFromRight(winBtnW).withHeight(btnH));
    winSlot.removeFromRight(gap);
    btnMin  .setBounds(winSlot.removeFromRight(winBtnW).withHeight(btnH));
    winSlot.removeFromRight(gap);
    btnTheme.setBounds(winSlot.removeFromRight(themeBtnW).withHeight(btnH));

    r.removeFromRight(14);

    // ----- BPM pill + click -----
    auto fileBlock = r.removeFromRight(fileBtnW * 2 + gap);
    saveProject.setBounds(fileBlock.removeFromRight(fileBtnW).withHeight(btnH));
    fileBlock.removeFromRight(gap);
    openProject.setBounds(fileBlock.removeFromRight(fileBtnW).withHeight(btnH));
    r.removeFromRight(12);

    auto pillBlock = r.removeFromRight(pillW + gap + clickW);
    bpmPillBounds = pillBlock.removeFromLeft(pillW).withHeight(btnH);

    auto inside = bpmPillBounds.reduced(8, 4);
    const int totalW = inside.getWidth();
    const int smallBtnW = 24;
    const int labelW = 36;
    const int editW = totalW - (smallBtnW * 2 + labelW + 12);

    bpmLabel.setBounds(inside.removeFromLeft(labelW));
    minusBtn.setBounds(inside.removeFromLeft(smallBtnW).reduced(0, 2));
    inside.removeFromLeft(4);
    bpmEdit .setBounds(inside.removeFromLeft(editW));
    inside.removeFromLeft(4);
    plusBtn .setBounds(inside.removeFromLeft(smallBtnW).reduced(0, 2));

    pillBlock.removeFromLeft(gap);
    clickToggle.setBounds(pillBlock.removeFromLeft(clickW).withHeight(btnH));

    // ----- Title centred -----
    const int cx = getWidth() / 2;
    auto leftSide = r;
    title.setBounds(cx - titleW / 2, vpad, titleW, btnH);

    // ----- Left cluster: transport + segmented switcher -----
    auto t = leftSide;
    playPause.setBounds(t.removeFromLeft(iconBtnW).withHeight(btnH));
    t.removeFromLeft(gap);
    stop.setBounds(t.removeFromLeft(iconBtnW).withHeight(btnH));
    t.removeFromLeft(gap);
    record.setBounds(t.removeFromLeft(iconBtnW).withHeight(btnH));
    t.removeFromLeft(gap * 3);

    const auto composerBtn = t.removeFromLeft(modeBtnW).withHeight(btnH);
    modeComposer.setBounds(composerBtn);
    modeMidi    .setBounds(t.removeFromLeft(modeBtnW).withHeight(btnH));
    modeMixer   .setBounds(t.removeFromLeft(modeBtnW).withHeight(btnH));
    if (currentMode == AppMode::Midi)
    {
        t.removeFromLeft(gap * 3);
        playbackPattern.setBounds(t.removeFromLeft(playbackBtnW).withHeight(btnH));
        playbackSong   .setBounds(t.removeFromLeft(playbackBtnW).withHeight(btnH));
    }
    else
    {
        playbackPattern.setBounds({});
        playbackSong.setBounds({});
    }
}

void TopBar::buttonClicked (juce::Button* button)
{
    if (button == &playPause)
    {
        if (recordArmed && isPlaying) return;
        setPlaying(! isPlaying);
        if (onPlayPause) onPlayPause(isPlaying);
    }
    else if (button == &stop)
    {
        if (onStop) onStop();
        setPlaying(false);
    }
    else if (button == &record)
    {
        recordArmed = ! recordArmed;
        setRecordArmed(recordArmed);
        if (onRecordArm) onRecordArm(recordArmed);
    }
    else if (button == &clickToggle)
    {
        clickEnabled = clickToggle.getToggleState();
        if (onClickToggled) onClickToggled(clickEnabled);
    }
    else if (button == &minusBtn) { setBPM(currentBPM - 1.0); }
    else if (button == &plusBtn ) { setBPM(currentBPM + 1.0); }
    else if (button == &btnTheme){ showThemeMenu(); }
    else if (button == &btnMin)  { if (onMinimize) onMinimize(); }
    else if (button == &btnClose){ if (onClose) onClose(); }
    else if (button == &modeComposer) { setMode(AppMode::Composer); }
    else if (button == &modeMidi)     { setMode(AppMode::Midi); }
    else if (button == &modeMixer)    { setMode(AppMode::Mixer); }
    else if (button == &playbackPattern) { setPlaybackMode(PlaybackMode::Pattern); }
    else if (button == &playbackSong)    { setPlaybackMode(PlaybackMode::Song); }
    else if (button == &openProject)     { if (onOpenProject) onOpenProject(); }
    else if (button == &saveProject)     { if (onSaveProject) onSaveProject(); }
}

void TopBar::showThemeMenu()
{
    juce::PopupMenu m;
    auto& tm = ThemeManager::get();
    const auto& palettes = tm.palettes();
    const int currentIdx = (int) tm.currentId();

    for (int i = 0; i < (int) palettes.size(); ++i)
        m.addItem(i + 1, palettes[(size_t)i].name, true, currentIdx == i);

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnTheme),
        [](int chosen)
        {
            if (chosen >= 1 && chosen <= 3)
                ThemeManager::get().setTheme(static_cast<ThemeManager::ThemeId>(chosen - 1));
        });
}

void TopBar::textEditorReturnKeyPressed (juce::TextEditor& editor) { applyBpmFrom(editor); }
void TopBar::textEditorFocusLost       (juce::TextEditor& editor) { applyBpmFrom(editor); }

void TopBar::applyBpmFrom (juce::TextEditor& editor)
{
    auto val = editor.getText().trim().getDoubleValue();
    val = juce::jlimit(40.0, 300.0, val);
    currentBPM = val;
    editor.setText(juce::String(val, 2), juce::dontSendNotification);
    if (onTempoChanged) onTempoChanged(val);
}
