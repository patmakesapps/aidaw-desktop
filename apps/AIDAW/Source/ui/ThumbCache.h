#pragma once
#include <JuceHeader.h>

struct ThumbPersistence
{
    static uint64_t fnv1a64(const void* data, size_t len)
    {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        uint64_t h = 1469598103934665603ull, prime = 1099511628211ull;
        for (size_t i=0;i<len;++i){ h ^= p[i]; h *= prime; }
        return h;
    }
    static juce::String hexFromU64(uint64_t v){ return juce::String::toHexString(&v, sizeof(v), 0); }

    static juce::File cacheDir()
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("AIDAW").getChildFile("ThumbCache");
        base.createDirectory(); return base;
    }
    static juce::File cacheFileFor(const juce::File& audio)
    {
        const juce::String key = audio.getFullPathName() + "|" +
                                 juce::String(audio.getLastModificationTime().toMilliseconds()) + "|" +
                                 juce::String((juce::int64)audio.getSize());
        const char* bytes = key.toRawUTF8();
        const size_t len  = (size_t) key.getNumBytesAsUTF8() - 1;
        const uint64_t h  = fnv1a64(bytes, len);
        return cacheDir().getChildFile(hexFromU64(h) + ".athumb");
    }
    static bool load(juce::AudioThumbnail& thumb, const juce::File& audio)
    {
        auto f = cacheFileFor(audio);
        if (!f.existsAsFile()) return false;
        if (auto in = f.createInputStream()) return thumb.loadFrom(*in);
        return false;
    }
    static void save(const juce::AudioThumbnail& thumb, const juce::File& audio)
    {
        auto f = cacheFileFor(audio);
        if (auto out = f.createOutputStream()) thumb.saveTo(*out);
    }
};
