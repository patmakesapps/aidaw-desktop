#pragma once
#include <JuceHeader.h>

// Session-scoped storage for loops (temporary, in-memory)
struct LoopInfo
{
    uint32      id { 0 };
    juce::String name;
    juce::Time   created;
};

class LoopsRegistry
{
public:
    static LoopsRegistry& instance()
    {
        static LoopsRegistry reg;
        return reg;
    }

    const juce::Array<LoopInfo>& list() const { return loops; }

    uint32 createLoop(const juce::String& optName = {})
    {
        LoopInfo li;
        li.id      = nextId++;
        li.name    = optName.isNotEmpty() ? optName : juce::String("Loop ") + juce::String(li.id);
        li.created = juce::Time::getCurrentTime();
        loops.add(li);
        return li.id;
    }

    bool removeLoop(uint32 id)
    {
        for (int i = 0; i < loops.size(); ++i)
            if (loops.getReference(i).id == id) { loops.remove(i); return true; }
        return false;
    }

    const LoopInfo* get(uint32 id) const
    {
        for (const auto& l : loops) if (l.id == id) return &l;
        return nullptr;
    }

private:
    LoopsRegistry() = default;
    juce::Array<LoopInfo> loops;
    uint32 nextId { 1 };
};
