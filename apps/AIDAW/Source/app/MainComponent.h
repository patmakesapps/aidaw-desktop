#pragma once

#include <JuceHeader.h>

#include "../audio/EddieSynth.h"
#include "../audio/MetronomeSource.h"
#include "../audio/TimelineAudioSource.h"
#include "../ui/arranger/Arranger.h"
#include "../ui/midi/MidiEditor.h"
#include "../ui/mixer/MixerComponent.h"
#include "../ui/shared/AIDAWLook.h"
#include "../ui/shell/TopBar.h"
#include "../ui/synth/EddieSynthPanel.h"

namespace aidaw
{

class MainComponent : public juce::Component,
                      public juce::ChangeListener,
                      private juce::Timer
{
public:
    MainComponent (juce::MixerAudioSource& mix, MetronomeSource& metro);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    void timerCallback() override;
    void setTransportPlaying (bool shouldPlay);
    void stopTransport();
    void openLoopsModal();
    void openEddiePanel();
    void openLoopInMidiEditor(uint32 loopId);
    void addLoopToComposer(uint32 loopId);
    void rebuildEddiePlaybackNotes();
    void saveProjectAs();
    void openProjectFile();
    bool saveProjectToFile(const juce::File& file);
    bool loadProjectFromFile(const juce::File& file);

    AIDAWLook look;
    TopBar topBar;
    Arranger arranger;
    MidiEditor midi;
    MixerComponent mixerUI;
    EddieSynthPanel eddiePanel;

    juce::MixerAudioSource& mixer;
    MetronomeSource& metronome;
    TimelineAudioSource timeline;
    EddieSynthAudioSource eddie;

    bool playing { false };
    bool recordArmed { false };
    double currentBpm { 120.0 };

    bool loopEnabled { false };
    double loopStartBeats { 0.0 };
    double loopLengthBeats { 0.0 };
    uint32 activeLoopId { 0 };
    TopBar::PlaybackMode playbackMode { TopBar::PlaybackMode::Song };
    juce::File currentProjectFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TooltipWindow tooltip { this, 350 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace aidaw
