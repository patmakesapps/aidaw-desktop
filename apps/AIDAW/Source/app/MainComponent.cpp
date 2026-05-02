#include "MainComponent.h"

#include "../ui/loops/LoopsModal.h"

namespace aidaw
{

MainComponent::MainComponent (juce::MixerAudioSource& mix, MetronomeSource& metro)
    : mixer (mix), metronome (metro), timeline (arranger.tracks, currentBpm)
{
    setOpaque (true);

    addAndMakeVisible (topBar);
    addAndMakeVisible (arranger);
    addAndMakeVisible (midi);
    addAndMakeVisible (mixerUI);
    midi.setVisible (false);
    mixerUI.setVisible (false);

    (void) tooltip;

    mixer.addInputSource (&metronome, false);
    mixer.addInputSource (&timeline, false);
    mixer.addInputSource (&eddie, false);

    arranger.onProjectChanged = [this] { timeline.requestRebuild(); };
    midi.onNotesChanged = [this] (const std::vector<MidiNote>& notes)
    {
        eddie.setNotes (notes);
    };
    midi.onPreviewNote = [this] (int pitch, int velocity)
    {
        eddie.triggerPreviewNote (pitch, velocity);
    };
    eddie.setNotes (midi.getNotes());

    arranger.onPlayheadSet = [this] (double beats)
    {
        timeline.setPlayheadBeats (beats);
        eddie.setPlayheadBeats (beats);
    };
    arranger.isPlayingQuery = [this] { return playing; };

    topBar.onPlayPause = [this] (bool isPlaying)
    {
        if (recordArmed && playing)
            return;

        setTransportPlaying (isPlaying);
    };

    topBar.onStop = [this]
    {
        stopTransport();
    };

    topBar.onRecordArm = [this] (bool armed)
    {
        recordArmed = armed;
        if (playing)
            topBar.setPlaying (true);
    };

    topBar.onTempoChanged = [this] (double bpm)
    {
        currentBpm = bpm;
        metronome.setBpm (bpm);
        eddie.setBpm (bpm);
        arranger.setBPM (bpm);
        midi.setBPM (bpm);
    };

    topBar.onClickToggled = [this] (bool on)
    {
        metronome.setClickEnabled (on);
        arranger.setSnap (on);
        midi.setSnap (on);
    };

    topBar.onModeChanged = [this] (TopBar::AppMode mode)
    {
        arranger.setVisible (mode == TopBar::AppMode::Composer);
        midi.setVisible (mode == TopBar::AppMode::Midi);
        mixerUI.setVisible (mode == TopBar::AppMode::Mixer);
        resized();
    };

    midi.setLoopEnabled (true);
    midi.setLoopRegion (loopStartBeats, loopLengthBeats);
    midi.onLoopChanged = [this] (double start, double len)
    {
        loopStartBeats = juce::jmax (0.0, start);
        loopLengthBeats = juce::jmax (0.0, len);
        loopEnabled = (loopLengthBeats > 0.0);
    };

    midi.onShowLoops = [this] { openLoopsModal(); };
    arranger.onLoopsClicked = [this] { openLoopsModal(); };

    setWantsKeyboardFocus (true);
    setSize (1200, 720);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    mixer.removeInputSource (&eddie);
    mixer.removeInputSource (&timeline);
    mixer.removeInputSource (&metronome);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    topBar.setBounds (area.removeFromTop (56));

    auto body = area.reduced (8, 8);
    arranger.setBounds (body);
    midi.setBounds (body);
    mixerUI.setBounds (body);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key.getTextCharacter() == ' ')
    {
        setTransportPlaying (! playing);
        return true;
    }

    return false;
}

void MainComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        if (arranger.isVisible())
            arranger.zoomDeltaFromWheel (wheel.deltaY, e.getScreenPosition().getX());

        if (midi.isVisible())
            midi.zoomDeltaFromWheel (wheel.deltaY, e.getScreenPosition().getX());

        return;
    }

    juce::Component::mouseWheelMove (e, wheel);
}

void MainComponent::timerCallback()
{
    double playhead = timeline.getPlayheadBeats();

    if (playing && loopEnabled && loopLengthBeats > 0.0)
    {
        const double loopEnd = loopStartBeats + loopLengthBeats;
        if (playhead >= loopEnd)
        {
            timeline.setPlayheadBeats (loopStartBeats);
            eddie.setPlayheadBeats (loopStartBeats);
            playhead = loopStartBeats;
        }
    }

    arranger.setPlayheadBeats (playhead);
    midi.setPlayheadBeats (playhead);
}

void MainComponent::setTransportPlaying (bool shouldPlay)
{
    playing = shouldPlay;
    metronome.setPlaying (playing);
    timeline.setPlaying (playing);
    eddie.setPlaying (playing);

    if (playing)
    {
        const auto playhead = arranger.getPlayheadBeats();
        timeline.setPlayheadBeats (playhead);
        eddie.setPlayheadBeats (playhead);
    }

    topBar.setPlaying (playing);
}

void MainComponent::stopTransport()
{
    playing = false;
    metronome.setPlaying (false);
    metronome.reset();
    timeline.setPlaying (false);
    timeline.resetToStart();
    eddie.setPlaying (false);
    eddie.resetToStart();
    arranger.setPlayheadBeats (0.0);
    midi.setPlayheadBeats (0.0);
    topBar.setPlaying (false);
}

void MainComponent::openLoopsModal()
{
    LoopsModal::Callbacks callbacks;
    callbacks.onCreate = [] (uint32 loopId)
    {
        juce::Logger::writeToLog ("Created loop id=" + juce::String (loopId));
    };
    callbacks.onOpenLoop = [] (uint32 loopId)
    {
        juce::Logger::writeToLog ("Open loop id=" + juce::String (loopId));
    };
    callbacks.onDelete = [] (uint32 loopId)
    {
        juce::Logger::writeToLog ("Deleted loop id=" + juce::String (loopId));
    };

    LoopsModal::show (this, callbacks);
}

} // namespace aidaw
