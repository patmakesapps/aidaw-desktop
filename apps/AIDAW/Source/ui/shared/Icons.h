#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Vector icons drawn with juce::Path. Coordinates are in a 24x24 box;
// IconButton scales to fit. Stroke weight is consistent (line-icon style).
namespace Icons
{
    inline juce::Path play()
    {
        juce::Path p;
        p.startNewSubPath(7.0f, 5.0f);
        p.lineTo(19.0f, 12.0f);
        p.lineTo(7.0f, 19.0f);
        p.closeSubPath();
        return p;
    }

    inline juce::Path pause()
    {
        juce::Path p;
        p.addRectangle(7.0f, 5.0f, 3.5f, 14.0f);
        p.addRectangle(13.5f, 5.0f, 3.5f, 14.0f);
        return p;
    }

    inline juce::Path stop()
    {
        juce::Path p;
        p.addRectangle(6.5f, 6.5f, 11.0f, 11.0f);
        return p;
    }

    inline juce::Path record()
    {
        juce::Path p;
        p.addEllipse(6.5f, 6.5f, 11.0f, 11.0f);
        return p;
    }

    inline juce::Path metronome()
    {
        juce::Path p;
        p.startNewSubPath(8.5f, 20.0f);
        p.lineTo(7.0f, 20.0f);
        p.lineTo(10.0f, 4.5f);
        p.lineTo(14.0f, 4.5f);
        p.lineTo(17.0f, 20.0f);
        p.lineTo(15.5f, 20.0f);
        p.closeSubPath();
        // pendulum
        p.startNewSubPath(15.5f, 8.5f);
        p.lineTo(9.0f, 16.5f);
        return p;
    }

    inline juce::Path pointer()
    {
        juce::Path p;
        p.startNewSubPath(5.0f, 3.5f);
        p.lineTo(5.0f, 18.0f);
        p.lineTo(9.0f, 14.5f);
        p.lineTo(11.5f, 20.0f);
        p.lineTo(13.7f, 19.0f);
        p.lineTo(11.2f, 13.7f);
        p.lineTo(16.0f, 13.0f);
        p.closeSubPath();
        return p;
    }

    inline juce::Path scissors()
    {
        juce::Path p;
        p.addEllipse(4.0f, 4.0f, 5.0f, 5.0f);
        p.addEllipse(4.0f, 15.0f, 5.0f, 5.0f);
        p.startNewSubPath(8.5f, 8.0f);
        p.lineTo(20.0f, 17.5f);
        p.startNewSubPath(8.5f, 16.0f);
        p.lineTo(20.0f, 6.5f);
        return p;
    }

    inline juce::Path resizeH()
    {
        juce::Path p;
        // left arrow
        p.startNewSubPath(3.0f, 12.0f);
        p.lineTo(7.0f, 8.0f);
        p.startNewSubPath(3.0f, 12.0f);
        p.lineTo(7.0f, 16.0f);
        // right arrow
        p.startNewSubPath(21.0f, 12.0f);
        p.lineTo(17.0f, 8.0f);
        p.startNewSubPath(21.0f, 12.0f);
        p.lineTo(17.0f, 16.0f);
        // shaft
        p.startNewSubPath(3.0f, 12.0f);
        p.lineTo(21.0f, 12.0f);
        return p;
    }

    inline juce::Path magnifier()
    {
        juce::Path p;
        p.addEllipse(4.0f, 4.0f, 12.0f, 12.0f);
        p.startNewSubPath(14.5f, 14.5f);
        p.lineTo(20.5f, 20.5f);
        return p;
    }

    inline juce::Path frame()
    {
        juce::Path p;
        const float c = 4.0f;
        // four corner brackets
        p.startNewSubPath(3.0f, 3.0f + c);
        p.lineTo(3.0f, 3.0f);
        p.lineTo(3.0f + c, 3.0f);
        p.startNewSubPath(21.0f - c, 3.0f);
        p.lineTo(21.0f, 3.0f);
        p.lineTo(21.0f, 3.0f + c);
        p.startNewSubPath(3.0f, 21.0f - c);
        p.lineTo(3.0f, 21.0f);
        p.lineTo(3.0f + c, 21.0f);
        p.startNewSubPath(21.0f - c, 21.0f);
        p.lineTo(21.0f, 21.0f);
        p.lineTo(21.0f, 21.0f - c);
        return p;
    }

    inline juce::Path magnet() // snap
    {
        juce::Path p;
        // U-shape
        p.startNewSubPath(5.0f, 5.0f);
        p.lineTo(5.0f, 13.0f);
        p.cubicTo(5.0f, 17.0f, 8.0f, 20.0f, 12.0f, 20.0f);
        p.cubicTo(16.0f, 20.0f, 19.0f, 17.0f, 19.0f, 13.0f);
        p.lineTo(19.0f, 5.0f);
        // inner gap
        p.startNewSubPath(8.5f, 5.0f);
        p.lineTo(8.5f, 13.0f);
        p.cubicTo(8.5f, 15.0f, 10.0f, 16.5f, 12.0f, 16.5f);
        p.cubicTo(14.0f, 16.5f, 15.5f, 15.0f, 15.5f, 13.0f);
        p.lineTo(15.5f, 5.0f);
        // tip caps
        p.startNewSubPath(5.0f, 5.0f);
        p.lineTo(8.5f, 5.0f);
        p.startNewSubPath(15.5f, 5.0f);
        p.lineTo(19.0f, 5.0f);
        return p;
    }

    inline juce::Path plus()
    {
        juce::Path p;
        p.startNewSubPath(12.0f, 5.0f);
        p.lineTo(12.0f, 19.0f);
        p.startNewSubPath(5.0f, 12.0f);
        p.lineTo(19.0f, 12.0f);
        return p;
    }

    inline juce::Path minus()
    {
        juce::Path p;
        p.startNewSubPath(5.0f, 12.0f);
        p.lineTo(19.0f, 12.0f);
        return p;
    }

    inline juce::Path loopGlyph()
    {
        juce::Path p;
        // arrow loop
        p.startNewSubPath(5.0f, 9.0f);
        p.cubicTo(5.0f, 6.0f, 7.5f, 4.5f, 11.0f, 4.5f);
        p.lineTo(17.5f, 4.5f);
        p.startNewSubPath(15.5f, 2.5f);
        p.lineTo(17.5f, 4.5f);
        p.lineTo(15.5f, 6.5f);
        p.startNewSubPath(19.0f, 15.0f);
        p.cubicTo(19.0f, 18.0f, 16.5f, 19.5f, 13.0f, 19.5f);
        p.lineTo(6.5f, 19.5f);
        p.startNewSubPath(8.5f, 17.5f);
        p.lineTo(6.5f, 19.5f);
        p.lineTo(8.5f, 21.5f);
        return p;
    }

    inline juce::Path pencil()
    {
        juce::Path p;
        // body
        p.startNewSubPath(4.0f, 20.0f);
        p.lineTo(4.0f, 16.5f);
        p.lineTo(15.0f, 5.5f);
        p.lineTo(18.5f, 9.0f);
        p.lineTo(7.5f, 20.0f);
        p.closeSubPath();
        // tip line
        p.startNewSubPath(13.0f, 7.5f);
        p.lineTo(16.5f, 11.0f);
        return p;
    }

    inline juce::Path closeX()
    {
        juce::Path p;
        p.startNewSubPath(6.0f, 6.0f);
        p.lineTo(18.0f, 18.0f);
        p.startNewSubPath(18.0f, 6.0f);
        p.lineTo(6.0f, 18.0f);
        return p;
    }

    inline juce::Path minimize()
    {
        juce::Path p;
        p.startNewSubPath(6.0f, 17.0f);
        p.lineTo(18.0f, 17.0f);
        return p;
    }

    inline juce::Path palette()
    {
        juce::Path p;
        p.startNewSubPath(12.0f, 4.0f);
        p.cubicTo(7.0f, 4.0f, 3.5f, 8.0f, 3.5f, 12.5f);
        p.cubicTo(3.5f, 17.0f, 7.0f, 20.0f, 11.0f, 20.0f);
        p.cubicTo(13.0f, 20.0f, 13.0f, 18.0f, 14.0f, 17.5f);
        p.cubicTo(15.5f, 17.0f, 20.5f, 17.5f, 20.5f, 13.0f);
        p.cubicTo(20.5f, 8.0f, 17.0f, 4.0f, 12.0f, 4.0f);
        p.closeSubPath();
        // dots (palette wells)
        p.addEllipse(7.0f, 9.5f, 1.6f, 1.6f);
        p.addEllipse(11.0f, 7.0f, 1.6f, 1.6f);
        p.addEllipse(15.0f, 9.0f, 1.6f, 1.6f);
        p.addEllipse(16.5f, 13.0f, 1.6f, 1.6f);
        return p;
    }
}

// Lightweight icon button. Filled-glyph or stroked-glyph styles.
class IconButton : public juce::Button
{
public:
    enum class Style { Stroked, Filled };

    IconButton(const juce::String& tip, juce::Path glyph, Style s = Style::Stroked)
        : juce::Button(tip), path(std::move(glyph)), style(s)
    {
        setTooltip(tip);
        setClickingTogglesState(false);
    }

    void setIcon(juce::Path glyph) { path = std::move(glyph); repaint(); }
    void setStyle(Style s)         { style = s; repaint(); }
    void setAccentTint(bool on)    { accentTint = on; repaint(); }
    void setIconScale(float s)     { iconScale = juce::jlimit(0.4f, 1.0f, s); repaint(); }

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        const bool toggled = getToggleState();

        // background
        juce::Colour bg = juce::Colour(toggled ? Theme::colBtnActive
                                              : (over ? Theme::colBtnHover : Theme::colBtnIdle));
        if (down) bg = bg.darker(0.15f);
        g.setColour(bg);
        g.fillRoundedRectangle(r, 8.0f);

        // stroke
        juce::Colour stroke = toggled ? juce::Colour(Theme::colAccent).withAlpha(0.55f)
                                      : juce::Colour(Theme::colBtnStroke);
        g.setColour(stroke);
        g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);

        // glyph: scale from the fixed 24x24 design frame so paths with
        // zero-height/width bounds (like a horizontal "-") still center.
        const float box = juce::jmin(r.getWidth(), r.getHeight()) * iconScale;
        auto target = juce::Rectangle<float>(box, box).withCentre(r.getCentre());
        const float s = box / 24.0f;
        auto t = juce::AffineTransform::scale(s).translated(target.getX(), target.getY());

        juce::Colour glyphCol = toggled
            ? juce::Colour(Theme::colAccent)
            : (accentTint ? juce::Colour(Theme::colAccent).withAlpha(0.95f)
                          : juce::Colour(Theme::colBtnText));
        if (! isEnabled()) glyphCol = glyphCol.withAlpha(0.35f);

        g.setColour(glyphCol);
        if (style == Style::Stroked)
        {
            g.strokePath(path, juce::PathStrokeType(1.6f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded), t);
        }
        else
        {
            g.fillPath(path, t);
        }
    }

private:
    juce::Path path;
    Style style { Style::Stroked };
    bool accentTint { false };
    float iconScale { 0.55f };
};
