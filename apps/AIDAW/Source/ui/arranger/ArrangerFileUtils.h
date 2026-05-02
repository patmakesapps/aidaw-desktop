#pragma once

#include <JuceHeader.h>

namespace ArrangerFileUtils
{
juce::String extractTrackNameFromAudio (juce::AudioFormatManager& formatManager,
                                        const juce::File& file);
double estimateBeatsFromFile (juce::AudioFormatManager& formatManager,
                              const juce::File& file,
                              double bpm);
double detectLoopBpm (juce::AudioFormatManager& formatManager,
                      const juce::File& file);
double secondsOfFile (juce::AudioFormatManager& formatManager,
                      const juce::File& file);
}
