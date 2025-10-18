#pragma once
#include <JuceHeader.h>

class MixerComponent : public juce::Component
{
public:
    MixerComponent() { setOpaque(true); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF0D0F13));
        g.setColour(juce::Colour(0x33FFFFFF));
        g.drawText("Mixer (placeholder)", getLocalBounds(), juce::Justification::centred);
    }
};
