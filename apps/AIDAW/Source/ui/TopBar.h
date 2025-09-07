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
        g.setColour(bg);
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(juce::Colour(0x22FFFFFF));
        g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                          bool hi, bool down) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();

        auto base = on ? juce::Colour(0xFF22C55E) : juce::Colour(0xFF2A2A2A);
        if (hi)   base = base.brighter(0.1f);
        if (down) base = base.darker(0.2f);

        g.setColour(base);
        g.fillRoundedRectangle(r, 12.0f);
        g.setColour(on ? juce::Colour(0xFF0F5D2E) : juce::Colour(0x22FFFFFF));
        g.drawRoundedRectangle(r.reduced(0.5f), 12.0f, 1.0f);

        g.setColour(on ? juce::Colours::black : juce::Colours::white);
        g.setFont(juce::Font(14.0f));
        g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
    }
};

// ---------- Top Bar ----------
class TopBar : public juce::Component,
               private juce::Button::Listener,
               private juce::TextEditor::Listener
{
public:
    TopBar()
    {
        setLookAndFeel(&look);

        // Transport
        playPause.setButtonText("Play");
        playPause.addListener(this);
        playPause.setTooltip("Space = Play/Stop");
        addAndMakeVisible(playPause);

        stop.setButtonText("Stop");
        stop.addListener(this);
        stop.setTooltip("Stop (Return to start)");
        addAndMakeVisible(stop);

        record.setButtonText("Rec");
        record.addListener(this);
        record.setTooltip("Arm recording");
        addAndMakeVisible(record);

        // Title (absolute center) — editable on single click
        title.setText("AIDAW", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setColour(juce::Label::textColourId, juce::Colour(0x88FFFFFF));
        title.setFont(juce::Font(18.0f, juce::Font::bold));
        title.setEditable(true, true, false);
        title.onTextChange = [this]()
        {
            if (onTitleChanged) onTitleChanged(title.getText());
        };
        addAndMakeVisible(title);

        // BPM tight pill
        bpmLabelLeft.setText("BPM", juce::dontSendNotification);
        bpmLabelLeft.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(bpmLabelLeft);

        minusBtn.addListener(this); minusBtn.setButtonText("-");
        minusBtn.setTooltip("Tempo -1");
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

        plusBtn.addListener(this); plusBtn.setButtonText("+");
        plusBtn.setTooltip("Tempo +1");
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

    // State
    void setPlaying(bool shouldPlay)
    {
        isPlaying = shouldPlay;

        const bool showPause = isPlaying && !recordArmed; // no pause while armed
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
        const int transW    = 84;
        const int winBtnW   = 48;

        const int pillW     = 320; // tight pill
        const int clickW    = 76;

        auto r = getLocalBounds().reduced(sidePad, vpad);

        // Right window controls
        auto win = r.removeFromRight(winBtnW * 2 + gap);
        btnClose.setBounds(win.removeFromRight(winBtnW).reduced(4));
        win.removeFromRight(gap);
        btnMin.setBounds(win.removeFromRight(winBtnW).reduced(4));

        // Left transport
        const int transportW = transW * 3 + gap * 2;
        auto left = r.removeFromLeft(transportW);
        playPause.setBounds(left.removeFromLeft(transW).withHeight(btnH));
        left.removeFromLeft(gap);
        stop.setBounds(left.removeFromLeft(transW).withHeight(btnH));
        left.removeFromLeft(gap);
        record.setBounds(left.removeFromLeft(transW).withHeight(btnH));

        // Reserve BPM + Click on right
        r.removeFromRight(12);
        auto pillBlock = r.removeFromRight(pillW + gap + clickW);
        bpmPillBounds  = pillBlock.removeFromLeft(pillW);

        // Title ABS centered
        const int titleW = 200, titleH = btnH;
        const int cx     = getWidth() / 2;
        title.setBounds(cx - titleW/2, vpad, titleW, titleH);

        // Sleeker BPM pill layout
        auto inside = bpmPillBounds.reduced(12, 8);
        int totalW = inside.getWidth();
        int btnW   = 28;
        int labelW = 42;
        int editW  = totalW - (btnW * 2 + labelW * 2 + 16);

        bpmLabelLeft.setBounds(inside.removeFromLeft(labelW));
        minusBtn.setBounds(inside.removeFromLeft(btnW).reduced(2, 2));
        inside.removeFromLeft(4);
        bpmEdit.setBounds(inside.removeFromLeft(editW).reduced(2, 0));
        inside.removeFromLeft(4);
        bpmLabelRight.setBounds(inside.removeFromLeft(labelW));
        plusBtn.setBounds(inside.removeFromLeft(btnW).reduced(2, 2));

        pillBlock.removeFromLeft(gap);
        clickToggle.setBounds(pillBlock.removeFromLeft(clickW));
    }

private:
    // UI callbacks
    void buttonClicked(juce::Button* b) override
    {
        if (b == &playPause)
        {
            if (recordArmed && isPlaying) return; // no pause while armed
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

    // Transport
    juce::TextButton playPause, stop, record;

    // Title
    juce::Label      title;

    // BPM pill
    juce::Rectangle<int> bpmPillBounds;
    juce::Label      bpmLabelLeft, bpmLabelRight;
    juce::TextButton minusBtn, plusBtn;
    juce::TextEditor bpmEdit;
    juce::ToggleButton clickToggle;

    // Window controls
    juce::TextButton btnMin, btnClose;

    bool   isPlaying    { false };
    bool   recordArmed  { false };
    double currentBPM   { 120.0 };
    bool   clickEnabled { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};
