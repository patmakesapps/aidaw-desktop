#include "ArrangerFileUtils.h"

#include <cmath>
#include <memory>

namespace ArrangerFileUtils
{

juce::String extractTrackNameFromAudio (juce::AudioFormatManager& formatManager,
                                        const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr)
    {
        auto& meta = reader->metadataValues;
        for (auto key : { "title", "artist", "name" })
            if (meta.containsKey (key))
                return meta[key];
    }

    return {};
}

double estimateBeatsFromFile (juce::AudioFormatManager& formatManager,
                              const juce::File& file,
                              double bpm)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr && reader->sampleRate > 0.0)
    {
        const double secs = (double) reader->lengthInSamples / reader->sampleRate;
        const double beats = (secs * bpm) / 60.0;
        const double bars = std::max (1.0, std::round (beats / 4.0));
        return bars * 4.0;
    }

    return 16.0;
}

double detectLoopBpm (juce::AudioFormatManager& formatManager,
                      const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr)
    {
        auto& meta = reader->metadataValues;
        for (auto key : { "tempo", "bpm", "BPM", "Tempo" })
        {
            if (meta.containsKey (key))
            {
                auto v = meta[key].retainCharacters ("0123456789.");
                auto d = v.getDoubleValue();
                if (d > 20.0 && d < 300.0)
                    return d;
            }
        }
    }

    auto name = file.getFileNameWithoutExtension().toLowerCase();
    juce::String digits;
    for (int i = 0; i < name.length(); ++i)
    {
        const juce_wchar ch = name[i];
        if (juce::CharacterFunctions::isDigit (ch) || ch == '.')
        {
            digits << juce::String::charToString (ch);
        }
        else
        {
            if (digits.length() >= 2)
            {
                const double val = digits.getDoubleValue();
                if (val > 20.0 && val < 300.0)
                    return val;
            }

            digits.clear();
        }
    }

    if (digits.isNotEmpty())
    {
        const double val = digits.getDoubleValue();
        if (val > 20.0 && val < 300.0)
            return val;
    }

    return 0.0;
}

double secondsOfFile (juce::AudioFormatManager& formatManager,
                      const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr && reader->sampleRate > 0.0)
        return (double) reader->lengthInSamples / reader->sampleRate;

    return 0.0;
}

} // namespace ArrangerFileUtils
