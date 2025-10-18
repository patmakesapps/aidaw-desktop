#pragma once
#include <JuceHeader.h>
#include <functional>

// ---------- AIDAW Look & Feel ----------
class AIDAWLook : public juce::LookAndFeel_V4
{
public:
    AIDAWLook()
    {
        setColour(juce::TextButton::buttonColourId,         juce::Colour(0xFF2A2A2A));
        setColour(juce::TextButton::buttonOnColourId,       juce::Colour(0xFF3A3A3A));
        setColour(juce::TextButton::textColourOnId,         juce::Colours::white);
        setColour(juce::TextButton::textColourOffId,        juce::Colours::white);
        setColour(juce::Label::textColourId,                juce::Colours::white);

        setColour(juce::TextEditor::backgroundColourId,     juce::Colour(0xFF202020));
        setColour(juce::TextEditor::textColourId,           juce::Colours::white);
        setColour(juce::TextEditor::outlineColourId,        juce::Colours::transparentBlack);
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::CaretComponent::caretColourId,      juce::Colour(0x88FFFFFF));
        setColour(juce::TextEditor::highlightedTextColourId,juce::Colours::black);
        setColour(juce::TextEditor::highlightColourId,      juce::Colour(0x66FFFFFF));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
                              const juce::Colour& base, bool hi, bool down) override
    {
        auto r  = b.getLocalBounds().toFloat();
        auto bg = base.brighter(hi ? 0.10f : 0.0f).darker(down ? 0.20f : 0.0f);
        g.setColour(bg); g.fillRoundedRectangle(r, 10.0f);
        g.setColour(juce::Colour(0x22FFFFFF)); g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);
    }
};

// ---------- Top Bar ----------
class TopBar : public juce::Component,
               private juce::Button::Listener,
               private juce::TextEditor::Listener
{
public:
    enum class AppMode { Composer, Midi, Mixer };

    TopBar()
    {
        setLookAndFeel(&look);

        // Transport
        playPause.setButtonText("Play");
        playPause.addListener(this);
        playPause.setTooltip("Space = Play/Pause");
        addAndMakeVisible(playPause);

        stop.setButtonText("Stop");
        stop.addListener(this);
        stop.setTooltip("Stop and return to start");
        addAndMakeVisible(stop);

        record.setButtonText("Rec");
        record.addListener(this);
        record.setTooltip("Arm recording");
        addAndMakeVisible(record);

        // Mode buttons
        modeComposer.setButtonText("Composer");
        modeMidi.setButtonText("MIDI Editor");
        modeMixer.setButtonText("Mixer");
        for (auto* b : { &modeComposer, &modeMidi, &modeMixer })
        {
            b->setClickingTogglesState(true);
            b->addListener(this);
            addAndMakeVisible(b);
        }
        setMode(AppMode::Composer, false);

        // Title
        title.setText("AIDAW", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setColour(juce::Label::textColourId, juce::Colour(0x88FFFFFF));
        title.setFont(juce::Font(18.0f, juce::Font::bold));
        title.setEditable(true, true, false);
        title.onTextChange = [this](){ if (onTitleChanged) onTitleChanged(title.getText()); };
        addAndMakeVisible(title);

        // BPM pill
        bpmLabelLeft.setText("BPM", juce::dontSendNotification);
        bpmLabelLeft.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(bpmLabelLeft);

        minusBtn.addListener(this); minusBtn.setButtonText("-"); minusBtn.setTooltip("Tempo -1");
        addAndMakeVisible(minusBtn);

        bpmEdit.setText(juce::String(currentBPM, 2), juce::dontSendNotification);
        bpmEdit.setInputRestrictions(6, "0123456789.");
        bpmEdit.setJustification(juce::Justification::centred);
        bpmEdit.setScrollbarsShown(false);
        bpmEdit.addListener(this);
        bpmEdit.setTooltip("Type tempo and press Enter");
        addAndMakeVisible(bpmEdit);

        bpmLabelRight.setText("bpm", juce::dontSendNotification);
        bpmLabelRight.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(bpmLabelRight);

        plusBtn.addListener(this); plusBtn.setButtonText("+"); plusBtn.setTooltip("Tempo +1");
        addAndMakeVisible(plusBtn);

        clickToggle.setButtonText("Click");
        clickToggle.setToggleState(clickEnabled, juce::dontSendNotification);
        clickToggle.addListener(this);
        clickToggle.setTooltip("Metronome on/off (also toggles grid snap)");
        addAndMakeVisible(clickToggle);

        // Window controls
        btnMin.setButtonText("-"); btnMin.addListener(this); btnMin.setTooltip("Minimize"); addAndMakeVisible(btnMin);
        btnClose.setButtonText("X"); btnClose.addListener(this); btnClose.setTooltip("Close"); addAndMakeVisible(btnClose);
    }

    ~TopBar() override { setLookAndFeel(nullptr); }

    // Hooks
    std::function<void(bool)>   onPlayPause;
    std::function<void()>       onStop;
    std::function<void(bool)>   onRecordArm;
    std::function<void(double)> onTempoChanged;
    std::function<void(bool)>   onClickToggled;
    std::function<void()>       onMinimize;
    std::function<void()>       onClose;
    std::function<void(const juce::String&)> onTitleChanged;
    std::function<void(AppMode)> onModeChanged;

    // State
    void setPlaying(bool shouldPlay)
    {
        isPlaying = shouldPlay;
        const bool showPause = isPlaying && !recordArmed;
        playPause.setButtonText(showPause ? "Pause" : "Play");
        playPause.setColour(juce::TextButton::buttonColourId,
                            isPlaying ? juce::Colour(0xFF3B82F6) : juce::Colour(0xFF2A2A2A));
        repaint();
    }

    void setRecordArmed(bool armed)
    {
        recordArmed = armed;
        record.setColour(juce::TextButton::buttonColourId,
                         armed ? juce::Colour(0xFFEF4444) : juce::Colour(0xFF2A2A2A));
        if (isPlaying) setPlaying(true);
        repaint();
    }

    double getBPM() const { return currentBPM; }
    void   setBPM(double bpm)
    {
        currentBPM = juce::jlimit(40.0, 300.0, bpm);
        bpmEdit.setText(juce::String(currentBPM, 2), juce::dontSendNotification);
        if (onTempoChanged) onTempoChanged(currentBPM);
    }

    void setClickEnabled(bool enabled)
    {
        clickEnabled = enabled;
        clickToggle.setToggleState(clickEnabled, juce::dontSendNotification);
    }

    void setMode(AppMode m, bool fireCallback = true)
    {
        currentMode = m;
        modeComposer.setToggleState(m == AppMode::Composer, juce::dontSendNotification);
        modeMidi.setToggleState(m == AppMode::Midi, juce::dontSendNotification);
        modeMixer.setToggleState(m == AppMode::Mixer, juce::dontSendNotification);
        if (fireCallback && onModeChanged) onModeChanged(m);
        repaint();
    }

    AppMode getMode() const { return currentMode; }

    void paint(juce::Graphics& g) override
    {
        juce::Colour top  = juce::Colour(0xFF0E0E0E);
        juce::Colour bot  = juce::Colour(0xFF151515);
        g.setGradientFill(juce::ColourGradient(top, 0, 0, bot, 0, (float)getHeight(), false));
        g.fillAll();

        g.setColour(juce::Colour(0x33FFFFFF));
        g.fillRect(getLocalBounds().removeFromBottom(1));

        // BPM pill background
        auto pill = bpmPillBounds.toFloat();
        g.setColour(juce::Colour(0xFF1C1C1C));
        g.fillRoundedRectangle(pill, 12.0f);
        g.setColour(juce::Colour(0x22FFFFFF));
        g.drawRoundedRectangle(pill, 12.0f, 1.0f);
    }

    void resized() override
    {
        const int sidePad   = 12;
        const int vpad      = 8;
        const int gap       = 8;
        const int btnH      = 32;

        // Buttons: enforce same width for all 6 (fits long “MIDI Editor” label).
        const int buttonW   = 126;        // <= tweak if you want tighter fit
        const int clusterGap= 24;         // visual spacer between transport and modes

        const int winBtnW   = 48;
        const int pillW     = 320;
        const int clickW    = 76;

        auto r = getLocalBounds().reduced(sidePad, vpad);

        // Window controls (far right)
        auto win = r.removeFromRight(winBtnW * 2 + gap);
        btnClose.setBounds(win.removeFromRight(winBtnW).reduced(4));
        win.removeFromRight(gap);
        btnMin.setBounds(win.removeFromRight(winBtnW).reduced(4));

        // Right-side BPM + click
        r.removeFromRight(12);
        auto pillBlock = r.removeFromRight(pillW + gap + clickW);
        bpmPillBounds  = pillBlock.removeFromLeft(pillW);

        auto inside = bpmPillBounds.reduced(12, 8);
        int totalW = inside.getWidth();
        int smallBtnW = 28;
        int labelW  = 42;
        int editW   = totalW - (smallBtnW * 2 + labelW * 2 + 16);

        bpmLabelLeft.setBounds(inside.removeFromLeft(labelW));
        minusBtn.setBounds(inside.removeFromLeft(smallBtnW).reduced(2, 2));
        inside.removeFromLeft(4);
        bpmEdit.setBounds(inside.removeFromLeft(editW).reduced(2, 0));
        inside.removeFromLeft(4);
        bpmLabelRight.setBounds(inside.removeFromLeft(labelW));
        plusBtn.setBounds(inside.removeFromLeft(smallBtnW).reduced(2, 2));

        pillBlock.removeFromLeft(gap);
        clickToggle.setBounds(pillBlock.removeFromLeft(clickW));

        // Left side: transport + spacer + modes, each with identical size
        // Transport cluster
        auto leftCluster = r.removeFromLeft(buttonW * 3 + gap * 2 + clusterGap + buttonW * 3 + gap * 2);
        auto transport = leftCluster.removeFromLeft(buttonW * 3 + gap * 2);
        playPause.setBounds(transport.removeFromLeft(buttonW).withHeight(btnH));
        transport.removeFromLeft(gap);
        stop.setBounds(transport.removeFromLeft(buttonW).withHeight(btnH));
        transport.removeFromLeft(gap);
        record.setBounds(transport.removeFromLeft(buttonW).withHeight(btnH));

        // Spacer between clusters
        leftCluster.removeFromLeft(clusterGap);

        // Modes cluster
        auto modes = leftCluster.removeFromLeft(buttonW * 3 + gap * 2);
        modeComposer.setBounds(modes.removeFromLeft(buttonW).withHeight(btnH));
        modes.removeFromLeft(gap);
        modeMidi.setBounds(modes.removeFromLeft(buttonW).withHeight(btnH));
        modes.removeFromLeft(gap);
        modeMixer.setBounds(modes.removeFromLeft(buttonW).withHeight(btnH));

        // Title centered (independent of clusters)
        const int titleW = 200, titleH = btnH;
        const int cx     = getWidth() / 2;
        title.setBounds(cx - titleW/2, vpad, titleW, titleH);
    }

private:
    void buttonClicked(juce::Button* b) override
    {
        if (b == &playPause)
        {
            if (recordArmed && isPlaying) return;
            if (!isPlaying) { setPlaying(true);  if (onPlayPause) onPlayPause(true);  }
            else            { setPlaying(false); if (onPlayPause) onPlayPause(false); }
        }
        else if (b == &stop)
        {
            if (onStop) onStop();
            setPlaying(false);
        }
        else if (b == &record)
        {
            recordArmed = !recordArmed;
            setRecordArmed(recordArmed);
            if (onRecordArm) onRecordArm(recordArmed);
        }
        else if (b == &clickToggle)
        {
            clickEnabled = clickToggle.getToggleState();
            if (onClickToggled) onClickToggled(clickEnabled);
        }
        else if (b == &minusBtn) { setBPM(currentBPM - 1.0); }
        else if (b == &plusBtn)  { setBPM(currentBPM + 1.0); }
        else if (b == &btnMin)   { if (onMinimize) onMinimize(); }
        else if (b == &btnClose) { if (onClose) onClose(); }
        else if (b == &modeComposer) setMode(AppMode::Composer);
        else if (b == &modeMidi)     setMode(AppMode::Midi);
        else if (b == &modeMixer)    setMode(AppMode::Mixer);
    }

    void textEditorReturnKeyPressed(juce::TextEditor& te) override { applyBpmFrom(te); }
    void textEditorFocusLost(juce::TextEditor& te) override        { applyBpmFrom(te); }

    void applyBpmFrom(juce::TextEditor& te)
    {
        auto val = te.getText().trim().getDoubleValue();
        val = juce::jlimit(40.0, 300.0, val);
        currentBPM = val;
        te.setText(juce::String(val, 2), juce::dontSendNotification);
        if (onTempoChanged) onTempoChanged(val);
    }

    AIDAWLook        look;

    juce::TextButton playPause, stop, record;
    juce::TextButton modeComposer, modeMidi, modeMixer;
    juce::Label      title;

    juce::Rectangle<int> bpmPillBounds;
    juce::Label      bpmLabelLeft, bpmLabelRight;
    juce::TextButton minusBtn, plusBtn;
    juce::TextEditor bpmEdit;
    juce::ToggleButton clickToggle;

    juce::TextButton btnMin, btnClose;

    bool   isPlaying    { false };
    bool   recordArmed  { false };
    double currentBPM   { 120.0 };
    bool   clickEnabled { true };
    AppMode currentMode { AppMode::Composer };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};
