#pragma once
#include <JuceHeader.h>
#include "../midi/MidiNote.h"

struct MidiPreviewNote
{
    int    pitch { 60 };
    double startBeats { 0.0 };
    double lengthBeats { 1.0 };
    uint8  velocity { 100 };
};

struct LoopInfo
{
    uint32       id { 0 };
    juce::String name;
    juce::Time   created;
    double       lengthBeats { 4.0 };
    std::vector<MidiNote> notes;
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
        li.lengthBeats = 4.0;
        li.notes.push_back({ 60, 0.0, 1.0, 100, nextNoteUid++ });
        li.notes.push_back({ 64, 0.5, 0.5, 100, nextNoteUid++ });
        li.notes.push_back({ 67, 1.0, 1.0, 100, nextNoteUid++ });
        refreshPreview(li);

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

    bool setNotes(uint32 id, const std::vector<MidiNote>& notes, double lengthBeats = 0.0)
    {
        for (auto& l : loops)
        {
            if (l.id != id)
                continue;

            l.notes = notes;
            for (auto& n : l.notes)
                if (n.uid == 0)
                    n.uid = nextNoteUid++;

            l.lengthBeats = lengthBeats > 0.0 ? juce::jmax(1.0, lengthBeats)
                                              : inferLengthBeats(l.notes);
            refreshPreview(l);
            return true;
        }
        return false;
    }

    bool setLength(uint32 id, double lengthBeats)
    {
        for (auto& l : loops)
            if (l.id == id)
            {
                l.lengthBeats = juce::jmax(1.0, lengthBeats);
                refreshPreview(l);
                return true;
            }
        return false;
    }

    const LoopInfo* get(uint32 id) const
    {
        for (const auto& l : loops)
            if (l.id == id) return &l;
        return nullptr;
    }

    LoopInfo* getMutable(uint32 id)
    {
        for (auto& l : loops)
            if (l.id == id) return &l;
        return nullptr;
    }

    void clear()
    {
        loops.clear();
        nextId = 1;
        nextNoteUid = 1;
    }

    uint32 addLoadedLoop(uint32 id, const juce::String& name, juce::Time created,
                         double lengthBeats, const std::vector<MidiNote>& notes)
    {
        LoopInfo li;
        li.id = id == 0 ? nextId++ : id;
        li.name = name.isNotEmpty() ? name : juce::String("Loop ") + juce::String(li.id);
        li.created = created.toMilliseconds() > 0 ? created : juce::Time::getCurrentTime();
        li.lengthBeats = juce::jmax(1.0, lengthBeats);
        li.notes = notes;

        for (auto& n : li.notes)
        {
            if (n.uid == 0)
                n.uid = nextNoteUid++;
            else
                nextNoteUid = juce::jmax(nextNoteUid, n.uid + 1);
        }

        refreshPreview(li);
        loops.add(li);
        nextId = juce::jmax(nextId, li.id + 1);
        return li.id;
    }

private:
    LoopsRegistry() = default;

    static double inferLengthBeats(const std::vector<MidiNote>& notes)
    {
        double end = 4.0;
        for (const auto& n : notes)
            end = juce::jmax(end, n.startBeats + n.lengthBeats);

        return juce::jmax(4.0, std::ceil(end / 4.0) * 4.0);
    }

    static void refreshPreview(LoopInfo& loop)
    {
        loop.previewNotes.clearQuick();
        for (const auto& n : loop.notes)
            loop.previewNotes.add({ n.pitch, n.startBeats, n.lengthBeats,
                                    (uint8) juce::jlimit(1, 127, n.velocity) });
    }

    juce::Array<LoopInfo> loops;
    uint32 nextId { 1 };
    uint32 nextNoteUid { 1 };
};
