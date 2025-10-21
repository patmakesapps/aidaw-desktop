#pragma once
#include <JuceHeader.h>

// ---------- Minimal MIDI preview ----------
struct MidiPreviewNote
{
    int    pitch { 60 };          // 0..127
    double startBeats { 0.0 };
    double lengthBeats { 1.0 };
    uint8  velocity { 100 };
};

// ---------- Session-scoped storage ----------
struct LoopInfo
{
    uint32      id { 0 };
    juce::String name;
    juce::Time   created;

    // Optional: small set of notes used only for the UI preview
    juce::Array<MidiPreviewNote> previewNotes;
};

class LoopsRegistry
{
public:
    static LoopsRegistry& instance() { static LoopsRegistry reg; return reg; }

    const juce::Array<LoopInfo>& list() const { return loops; }

    uint32 createLoop(const juce::String& optName = {})
    {
        LoopInfo li;
        li.id   = nextId++;
        li.name = optName.isNotEmpty() ? optName
                                       : juce::String("Loop ") + juce::String(li.id);
        li.created = juce::Time::getCurrentTime();

        // optional: seed a tiny preview so the UI isn’t empty
        // (C–E–G triad one bar long). Call setPreview(...) later to replace.
        li.previewNotes.add({ 60, 0.0, 1.0, 100 });
        li.previewNotes.add({ 64, 0.5, 0.5, 100 });
        li.previewNotes.add({ 67, 1.0, 1.0, 100 });

        loops.add(li);
        return li.id;
    }

    bool removeLoop(uint32 id)
    {
        for (int i = 0; i < loops.size(); ++i)
            if (loops.getReference(i).id == id)
            { loops.remove(i); return true; }
        return false;
    }

    bool renameLoop(uint32 id, const juce::String& newName)
    {
        if (newName.trim().isEmpty()) return false;
        for (auto& l : loops)
            if (l.id == id) { l.name = newName.trim(); return true; }
        return false;
    }

    bool setPreview(uint32 id, const juce::Array<MidiPreviewNote>& notes)
    {
        for (auto& l : loops)
            if (l.id == id) { l.previewNotes = notes; return true; }
        return false;
    }

    const LoopInfo* get(uint32 id) const
    {
        for (const auto& l : loops)
            if (l.id == id) return &l;
        return nullptr;
    }

private:
    LoopsRegistry() = default;

    juce::Array<LoopInfo> loops;
    uint32 nextId { 1 };
};
