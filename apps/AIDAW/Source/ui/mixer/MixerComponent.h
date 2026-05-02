#pragma once
#include <JuceHeader.h>
#include "../shared/Theme.h"
#include "../shared/ThemeManager.h"

class MixerComponent : public juce::Component,
                       public juce::ChangeListener
{
public:
    MixerComponent()
    {
        setOpaque(true);
        ThemeManager::get().addChangeListener(this);
    }

    ~MixerComponent() override
    {
        ThemeManager::get().removeChangeListener(this);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(Theme::colBgMain));
        g.setColour(juce::Colour(Theme::colTextDim));
        g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::plain)));
        g.drawText("Mixer (placeholder)", getLocalBounds(), juce::Justification::centred);
    }
};
