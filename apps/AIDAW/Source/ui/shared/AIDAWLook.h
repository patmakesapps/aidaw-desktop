#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "ThemeManager.h"

// Single LookAndFeel for the whole app. Pulls colors from runtime Theme.
// Listens to ThemeManager and refreshes its registered colors on change.
class AIDAWLook : public juce::LookAndFeel_V4,
                 public juce::ChangeListener
{
public:
    AIDAWLook()
    {
        applyColours();
        ThemeManager::get().addChangeListener(this);
    }

    ~AIDAWLook() override
    {
        ThemeManager::get().removeChangeListener(this);
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        applyColours();
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
                              const juce::Colour& base, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool toggled = b.getToggleState();

        juce::Colour bg = base;
        if (toggled)            bg = juce::Colour(Theme::colBtnActive);
        else if (down)          bg = base.darker(0.15f);
        else if (over)          bg = juce::Colour(Theme::colBtnHover);

        g.setColour(bg);
        g.fillRoundedRectangle(r, 8.0f);

        juce::Colour stroke = toggled
            ? juce::Colour(Theme::colAccent).withAlpha(0.55f)
            : juce::Colour(Theme::colBtnStroke);
        g.setColour(stroke);
        g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto font = getTextButtonFont(b, b.getHeight());
        g.setFont(font);

        const auto col = b.getToggleState() ? juce::Colour(Theme::colAccent)
                                            : juce::Colour(Theme::colBtnText);
        g.setColour(col.withAlpha(b.isEnabled() ? 1.0f : 0.4f));

        auto area = b.getLocalBounds().reduced(2, 0);
        g.drawFittedText(b.getButtonText(), area, juce::Justification::centred, 1);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int /*h*/) override
    {
        return juce::Font(juce::FontOptions(13.5f, juce::Font::plain));
    }

    void drawTickBox(juce::Graphics& g, juce::Component& /*c*/,
                     float x, float y, float w, float h,
                     bool ticked, bool /*enabled*/, bool over, bool /*down*/) override
    {
        juce::Rectangle<float> box(x, y, w, h);
        box = box.withSizeKeepingCentre(juce::jmin(w, h) * 0.85f, juce::jmin(w, h) * 0.85f);

        g.setColour(juce::Colour(ticked ? Theme::colBtnActive : Theme::colBtnIdle));
        g.fillRoundedRectangle(box, 4.0f);

        g.setColour(ticked ? juce::Colour(Theme::colAccent)
                           : juce::Colour(Theme::colBtnStroke));
        g.drawRoundedRectangle(box.reduced(0.5f), 4.0f, 1.0f);

        if (ticked)
        {
            juce::Path tick;
            tick.startNewSubPath(box.getX() + box.getWidth() * 0.22f,
                                 box.getY() + box.getHeight() * 0.55f);
            tick.lineTo(box.getX() + box.getWidth() * 0.42f,
                        box.getY() + box.getHeight() * 0.74f);
            tick.lineTo(box.getX() + box.getWidth() * 0.78f,
                        box.getY() + box.getHeight() * 0.30f);
            g.setColour(juce::Colour(Theme::colAccent));
            g.strokePath(tick, juce::PathStrokeType(1.6f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }

        if (over)
        {
            g.setColour(juce::Colour(Theme::colAccent).withAlpha(0.18f));
            g.drawRoundedRectangle(box.reduced(-1.0f), 5.0f, 1.0f);
        }
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float pos, float startAng, float endAng,
                          juce::Slider& s) override
    {
        auto area = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(4.0f);
        const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
        auto centre = area.getCentre();

        // track
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius - 4.0f, radius - 4.0f,
                            0.0f, startAng, endAng, true);
        g.setColour(juce::Colour(Theme::colBtnStroke));
        g.strokePath(track, juce::PathStrokeType(2.0f));

        // value
        const float ang = startAng + pos * (endAng - startAng);
        juce::Path val;
        val.addCentredArc(centre.x, centre.y, radius - 4.0f, radius - 4.0f,
                          0.0f, startAng, ang, true);
        g.setColour(s.findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(val, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

        // knob centre
        const float knobR = radius - 9.0f;
        g.setColour(juce::Colour(Theme::colBtnIdle));
        g.fillEllipse(centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour(juce::Colour(Theme::colBtnStroke));
        g.drawEllipse(centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

        // pointer
        const float px = centre.x + std::cos(ang - juce::MathConstants<float>::halfPi) * (knobR - 2.0f);
        const float py = centre.y + std::sin(ang - juce::MathConstants<float>::halfPi) * (knobR - 2.0f);
        g.setColour(juce::Colour(Theme::colAccent));
        g.fillEllipse(px - 2.0f, py - 2.0f, 4.0f, 4.0f);
    }

private:
    void applyColours()
    {
        setColour(juce::TextButton::buttonColourId,    juce::Colour(Theme::colBtnIdle));
        setColour(juce::TextButton::buttonOnColourId,  juce::Colour(Theme::colBtnActive));
        setColour(juce::TextButton::textColourOnId,    juce::Colour(Theme::colAccent));
        setColour(juce::TextButton::textColourOffId,   juce::Colour(Theme::colBtnText));
        setColour(juce::Label::textColourId,           juce::Colour(Theme::colBtnText));

        setColour(juce::TextEditor::backgroundColourId,       juce::Colour(Theme::colPillBg));
        setColour(juce::TextEditor::textColourId,             juce::Colour(Theme::colBtnText));
        setColour(juce::TextEditor::outlineColourId,          juce::Colours::transparentBlack);
        setColour(juce::TextEditor::focusedOutlineColourId,   juce::Colour(Theme::colAccent).withAlpha(0.5f));
        setColour(juce::CaretComponent::caretColourId,        juce::Colour(Theme::colAccent));
        setColour(juce::TextEditor::highlightedTextColourId,  juce::Colour(Theme::colBgMain));
        setColour(juce::TextEditor::highlightColourId,        juce::Colour(Theme::colAccent).withAlpha(0.4f));

        setColour(juce::ToggleButton::textColourId,           juce::Colour(Theme::colBtnText));
        setColour(juce::ToggleButton::tickColourId,           juce::Colour(Theme::colAccent));
        setColour(juce::ToggleButton::tickDisabledColourId,   juce::Colour(Theme::colBtnStroke));

        setColour(juce::ComboBox::backgroundColourId,         juce::Colour(Theme::colBtnIdle));
        setColour(juce::ComboBox::textColourId,               juce::Colour(Theme::colBtnText));
        setColour(juce::ComboBox::outlineColourId,            juce::Colour(Theme::colBtnStroke));
        setColour(juce::ComboBox::arrowColourId,              juce::Colour(Theme::colTextDim));
        setColour(juce::ComboBox::buttonColourId,             juce::Colour(Theme::colBtnIdle));

        setColour(juce::PopupMenu::backgroundColourId,        juce::Colour(Theme::colBgPanel));
        setColour(juce::PopupMenu::textColourId,              juce::Colour(Theme::colBtnText));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(Theme::colBtnHover));
        setColour(juce::PopupMenu::highlightedTextColourId,   juce::Colour(Theme::colAccent));

        setColour(juce::Slider::rotarySliderFillColourId,     juce::Colour(Theme::colAccent));
        setColour(juce::Slider::rotarySliderOutlineColourId,  juce::Colour(Theme::colBtnStroke));
        setColour(juce::Slider::textBoxTextColourId,          juce::Colour(Theme::colBtnText));
        setColour(juce::Slider::textBoxBackgroundColourId,    juce::Colour(Theme::colPillBg));
        setColour(juce::Slider::textBoxOutlineColourId,       juce::Colours::transparentBlack);
    }
};
