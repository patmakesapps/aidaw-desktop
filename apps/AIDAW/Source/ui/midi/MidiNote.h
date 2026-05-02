#pragma once

#include <JuceHeader.h>

#ifndef AIDAW_MIDI_NOTE_DEFINED
#define AIDAW_MIDI_NOTE_DEFINED 1
struct MidiNote
{
    int pitch { 60 };
    double startBeats { 0.0 };
    double lengthBeats { 1.0 };
    int velocity { 100 };
    uint32 uid { 0 };
};
#endif
