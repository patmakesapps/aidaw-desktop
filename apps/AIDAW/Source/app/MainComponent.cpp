#include "MainComponent.h"

#include "../ui/loops/LoopsModal.h"
#include "../ui/loops/LoopsRegistry.h"
#include "../ui/shared/Theme.h"
#include "../ui/shared/ThemeManager.h"

#include <cmath>

namespace aidaw
{

MainComponent::MainComponent (juce::MixerAudioSource& mix, MetronomeSource& metro)
    : sampleBrowser (samplePreview),
      mixer (mix), metronome (metro), timeline (arranger.tracks, currentBpm)
{
    setOpaque (true);
    juce::LookAndFeel::setDefaultLookAndFeel(&look);
    ThemeManager::get().addChangeListener(this);

    addAndMakeVisible (topBar);
    addAndMakeVisible (arranger);
    addAndMakeVisible (midi);
    addAndMakeVisible (mixerUI);
    addChildComponent (sampleBrowser);
    addChildComponent (eddiePanel);
    midi.setVisible (false);
    mixerUI.setVisible (false);

    (void) tooltip;

    mixer.addInputSource (&metronome, false);
    mixer.addInputSource (&timeline, false);
    mixer.addInputSource (&eddie, false);
    mixer.addInputSource (&samplePreview, false);

    arranger.onProjectChanged = [this]
    {
        timeline.requestRebuild();
        rebuildEddiePlaybackNotes();
    };
    midi.onNotesChanged = [this] (const std::vector<MidiNote>& notes)
    {
        if (activeLoopId != 0)
        {
            double length = 4.0;
            for (const auto& n : notes)
                length = juce::jmax(length, n.startBeats + n.lengthBeats);
            length = juce::jmax(4.0, std::ceil(length / 4.0) * 4.0);
            LoopsRegistry::instance().setNotes(activeLoopId, notes, length);
            midi.setLoopRegion(0.0, length);
            loopStartBeats = 0.0;
            loopLengthBeats = length;
        }

        rebuildEddiePlaybackNotes();
    };
    midi.onPreviewNote = [this] (int pitch, int velocity)
    {
        eddie.triggerPreviewNote (pitch, velocity);
    };
    eddie.setNotes (midi.getNotes());
    eddiePanel.setSettings (eddie.getSettings());
    eddiePanel.onSettingsChanged = [this] (const EddieSynthSettings& settings)
    {
        eddie.setSettings (settings);
    };
    eddiePanel.onPreviewNote = [this] (int pitch, int velocity)
    {
        eddie.triggerPreviewNote (pitch, velocity);
    };
    eddiePanel.onClose = [this]
    {
        eddiePanel.setVisible (false);
    };

    arranger.onPlayheadSet = [this] (double beats)
    {
        timeline.setPlayheadBeats (beats);
        eddie.setPlayheadBeats (beats);
        metronome.setPlayheadBeats (beats);
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
        if (mode == TopBar::AppMode::Composer && playbackMode != TopBar::PlaybackMode::Song)
        {
            playbackMode = TopBar::PlaybackMode::Song;
            topBar.setPlaybackMode(playbackMode, false);
            rebuildEddiePlaybackNotes();
        }

    arranger.setVisible (mode == TopBar::AppMode::Composer);
    midi.setVisible (mode == TopBar::AppMode::Midi);
    mixerUI.setVisible (mode == TopBar::AppMode::Mixer);

    if (mode == TopBar::AppMode::Composer)
        arranger.setTool(ArrangerTool::Pointer);
    else if (mode == TopBar::AppMode::Midi)
        midi.setTool(MidiEditor::Tool::Draw);

    resized();
};

    topBar.onPlaybackModeChanged = [this] (TopBar::PlaybackMode mode)
    {
        playbackMode = mode;
        rebuildEddiePlaybackNotes();
    };

    topBar.onSaveProject = [this] { saveProjectAs(); };
    topBar.onOpenProject = [this] { openProjectFile(); };

    topBar.onMinimize = [this]
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setMinimised (true);
    };

    topBar.onClose = []
    {
        juce::JUCEApplicationBase::getInstance()->systemRequestedQuit();
    };

    midi.setLoopEnabled (false);
    midi.setLoopRegion (loopStartBeats, loopLengthBeats);
    midi.onLoopChanged = [this] (double start, double len)
    {
        loopStartBeats = juce::jmax (0.0, start);
        loopLengthBeats = juce::jmax (0.0, len);
        loopEnabled = (loopLengthBeats > 0.0);
        if (activeLoopId != 0)
            LoopsRegistry::instance().setLength(activeLoopId, loopLengthBeats);
    };

    midi.onShowLoops = [this] { openLoopsModal(); };
    midi.onOpenSynth = [this] { openEddiePanel(); };
    arranger.onLoopsClicked = [this] { openLoopsModal(); };
    arranger.onEddieClicked = [this] { openEddiePanel(); };
    arranger.onSamplesClicked = [this] { openSampleBrowser(); };
    arranger.onOpenMidiLoop = [this] (uint32 loopId) { openLoopInMidiEditor(loopId); };

    setWantsKeyboardFocus (true);
    setSize (1200, 720);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    ThemeManager::get().removeChangeListener(this);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    mixer.removeInputSource (&eddie);
    mixer.removeInputSource (&samplePreview);
    mixer.removeInputSource (&timeline);
    mixer.removeInputSource (&metronome);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour(Theme::colBgMain));
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    topBar.setBounds (area.removeFromTop (56));

    auto body = area.reduced (8, 8);
    arranger.setBounds (body);
    midi.setBounds (body);
    mixerUI.setBounds (body);
    sampleBrowser.setBounds (body.removeFromRight (juce::jmin (420, juce::jmax (320, getWidth() / 3))));
    eddiePanel.setBounds (body.reduced (80, 42));
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
    if (playbackMode == TopBar::PlaybackMode::Pattern)
        playhead = eddie.getPlayheadBeats();

    if (playing && playbackMode == TopBar::PlaybackMode::Pattern && loopLengthBeats > 0.0)
    {
        const double loopEnd = loopStartBeats + loopLengthBeats;
        if (playhead >= loopEnd)
        {
            timeline.setPlayheadBeats (loopStartBeats);
            eddie.setPlayheadBeats (loopStartBeats);
            metronome.setPlayheadBeats (loopStartBeats);
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
    timeline.setPlaying (playing && playbackMode == TopBar::PlaybackMode::Song);
    rebuildEddiePlaybackNotes();
    eddie.setPlaying (playing);

    if (playing)
    {
        const auto playhead = playbackMode == TopBar::PlaybackMode::Pattern ? loopStartBeats
                                                                            : arranger.getPlayheadBeats();
        timeline.setPlayheadBeats (playhead);
        eddie.setPlayheadBeats (playhead);
        metronome.setPlayheadBeats (playhead);
    }

    topBar.setPlaying (playing);
}

void MainComponent::stopTransport()
{
    playing = false;
    samplePreview.stop();
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
    callbacks.onCreate = [this] (uint32 loopId)
    {
        juce::Logger::writeToLog ("Created loop id=" + juce::String (loopId));
    };
    callbacks.onOpenLoop = [this] (uint32 loopId)
    {
        openLoopInMidiEditor(loopId);
    };
    callbacks.onAddToComposer = [this] (uint32 loopId)
    {
        addLoopToComposer(loopId);
    };
    callbacks.onDelete = [this] (uint32 loopId)
    {
        juce::Logger::writeToLog ("Deleted loop id=" + juce::String (loopId));
        arranger.removeClipsForLoop(loopId);
        if (activeLoopId == loopId)
        {
            activeLoopId = 0;
            midi.setNotes({});
        }
        rebuildEddiePlaybackNotes();
    };

    LoopsModal::show (this, callbacks);
}

void MainComponent::openLoopInMidiEditor(uint32 loopId)
{
    const auto* loop = LoopsRegistry::instance().get(loopId);
    if (loop == nullptr)
        return;

    activeLoopId = loopId;
    playbackMode = TopBar::PlaybackMode::Pattern;
    topBar.setPlaybackMode(playbackMode, false);

    loopStartBeats = 0.0;
    loopLengthBeats = juce::jmax(1.0, loop->lengthBeats);
    loopEnabled = true;

    midi.setNotes(loop->notes);
    midi.setLoopEnabled(true);
    midi.setLoopRegion(loopStartBeats, loopLengthBeats);
    midi.frameAllView();
    topBar.setMode(TopBar::AppMode::Midi);
    rebuildEddiePlaybackNotes();
}

void MainComponent::addLoopToComposer(uint32 loopId)
{
    arranger.addMidiLoopClip(loopId, arranger.getPlayheadBeats(), 0);
    playbackMode = TopBar::PlaybackMode::Song;
    topBar.setPlaybackMode(playbackMode, false);
    topBar.setMode(TopBar::AppMode::Composer);
    rebuildEddiePlaybackNotes();
}

void MainComponent::saveProjectAs()
{
    if (currentProjectFile.existsAsFile())
    {
        saveProjectToFile(currentProjectFile);
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser>("Save AIDAW project",
                                                       juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                       "*.aidaw");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode
                           | juce::FileBrowserComponent::canSelectFiles
                           | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file == juce::File{})
                return;

            if (! file.hasFileExtension(".aidaw"))
                file = file.withFileExtension(".aidaw");

            saveProjectToFile(file);
        });
}

void MainComponent::openProjectFile()
{
    fileChooser = std::make_unique<juce::FileChooser>("Open AIDAW project",
                                                       juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                       "*.aidaw");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file.existsAsFile())
                loadProjectFromFile(file);
        });
}

bool MainComponent::saveProjectToFile(const juce::File& file)
{
    juce::XmlElement root("AIDAWProject");
    root.setAttribute("version", 1);
    root.setAttribute("bpm", currentBpm);
    root.setAttribute("activeLoopId", (int) activeLoopId);
    root.setAttribute("playbackMode", playbackMode == TopBar::PlaybackMode::Pattern ? "pattern" : "song");

    auto* loopsXml = root.createNewChildElement("Loops");
    for (const auto& loop : LoopsRegistry::instance().list())
    {
        auto* loopXml = loopsXml->createNewChildElement("Loop");
        loopXml->setAttribute("id", (int) loop.id);
        loopXml->setAttribute("name", loop.name);
        loopXml->setAttribute("createdMs", juce::String(loop.created.toMilliseconds()));
        loopXml->setAttribute("lengthBeats", loop.lengthBeats);

        for (const auto& note : loop.notes)
        {
            auto* noteXml = loopXml->createNewChildElement("Note");
            noteXml->setAttribute("pitch", note.pitch);
            noteXml->setAttribute("startBeats", note.startBeats);
            noteXml->setAttribute("lengthBeats", note.lengthBeats);
            noteXml->setAttribute("velocity", note.velocity);
            noteXml->setAttribute("uid", (int) note.uid);
        }
    }

    auto* tracksXml = root.createNewChildElement("Tracks");
    for (const auto& track : arranger.tracks)
    {
        auto* trackXml = tracksXml->createNewChildElement("Track");
        trackXml->setAttribute("id", track.id);
        trackXml->setAttribute("name", track.name);

        for (const auto& clip : track.clips)
        {
            auto* clipXml = trackXml->createNewChildElement("Clip");
            clipXml->setAttribute("id", clip.id);
            clipXml->setAttribute("kind", clip.kind == ClipModel::Kind::MidiLoop ? "midiLoop" : "audio");
            clipXml->setAttribute("startBeats", clip.startBeats);
            clipXml->setAttribute("lengthBeats", clip.lengthBeats);
            clipXml->setAttribute("offsetBeats", clip.offsetBeats);
            clipXml->setAttribute("sourceBpm", clip.sourceBpm);
            clipXml->setAttribute("pitchSemitones", clip.pitchSemitones);
            clipXml->setAttribute("gainDb", clip.gainDb);
            clipXml->setAttribute("stretchMode", clip.stretchMode == ClipModel::StretchMode::Resample ? "resample" : "stretch");
            clipXml->setAttribute("normalize", clip.normalize);
            clipXml->setAttribute("muted", clip.muted);
            clipXml->setAttribute("file", clip.file.getFullPathName());
            clipXml->setAttribute("loopId", (int) clip.loopId);
            clipXml->setAttribute("label", clip.label);
        }
    }

    const bool ok = root.writeTo(file);
    if (ok)
        currentProjectFile = file;

    return ok;
}

bool MainComponent::loadProjectFromFile(const juce::File& file)
{
    auto root = juce::XmlDocument::parse(file);
    if (root == nullptr || ! root->hasTagName("AIDAWProject"))
        return false;

    stopTransport();

    currentBpm = root->getDoubleAttribute("bpm", currentBpm);
    topBar.setBPM(currentBpm);
    metronome.setBpm(currentBpm);
    eddie.setBpm(currentBpm);
    arranger.setBPM(currentBpm);
    midi.setBPM(currentBpm);

    LoopsRegistry::instance().clear();
    if (auto* loopsXml = root->getChildByName("Loops"))
    {
        for (auto* loopXml : loopsXml->getChildWithTagNameIterator("Loop"))
        {
            std::vector<MidiNote> notes;
            for (auto* noteXml : loopXml->getChildWithTagNameIterator("Note"))
            {
                MidiNote note;
                note.pitch = noteXml->getIntAttribute("pitch", 60);
                note.startBeats = noteXml->getDoubleAttribute("startBeats", 0.0);
                note.lengthBeats = noteXml->getDoubleAttribute("lengthBeats", 1.0);
                note.velocity = noteXml->getIntAttribute("velocity", 100);
                note.uid = (uint32) noteXml->getIntAttribute("uid", 0);
                notes.push_back(note);
            }

            const auto created = juce::Time(loopXml->getStringAttribute("createdMs").getLargeIntValue());
            LoopsRegistry::instance().addLoadedLoop((uint32) loopXml->getIntAttribute("id", 0),
                                                    loopXml->getStringAttribute("name"),
                                                    created,
                                                    loopXml->getDoubleAttribute("lengthBeats", 4.0),
                                                    notes);
        }
    }

    arranger.tracks.clear();
    if (auto* tracksXml = root->getChildByName("Tracks"))
    {
        for (auto* trackXml : tracksXml->getChildWithTagNameIterator("Track"))
        {
            TrackModel track;
            track.id = trackXml->getStringAttribute("id", juce::Uuid().toString());
            track.name = trackXml->getStringAttribute("name", "Track");

            for (auto* clipXml : trackXml->getChildWithTagNameIterator("Clip"))
            {
                ClipModel clip;
                clip.id = clipXml->getStringAttribute("id", juce::Uuid().toString());
                clip.kind = clipXml->getStringAttribute("kind") == "midiLoop"
                          ? ClipModel::Kind::MidiLoop
                          : ClipModel::Kind::Audio;
                clip.startBeats = clipXml->getDoubleAttribute("startBeats", 0.0);
                clip.lengthBeats = clipXml->getDoubleAttribute("lengthBeats", 4.0);
                clip.offsetBeats = clipXml->getDoubleAttribute("offsetBeats", 0.0);
                clip.sourceBpm = clipXml->getDoubleAttribute("sourceBpm", 0.0);
                clip.pitchSemitones = clipXml->getDoubleAttribute("pitchSemitones", 0.0);
                clip.gainDb = clipXml->getDoubleAttribute("gainDb", 0.0);
                clip.stretchMode = clipXml->getStringAttribute("stretchMode") == "resample"
                                 ? ClipModel::StretchMode::Resample
                                 : ClipModel::StretchMode::Stretch;
                clip.normalize = clipXml->getBoolAttribute("normalize", false);
                clip.muted = clipXml->getBoolAttribute("muted", false);
                clip.file = juce::File(clipXml->getStringAttribute("file"));
                clip.loopId = (uint32) clipXml->getIntAttribute("loopId", 0);
                clip.label = clipXml->getStringAttribute("label");
                track.clips.push_back(std::move(clip));
            }

            arranger.tracks.push_back(std::move(track));
        }
    }

    currentProjectFile = file;
    activeLoopId = (uint32) root->getIntAttribute("activeLoopId", 0);
    playbackMode = root->getStringAttribute("playbackMode") == "pattern"
                 ? TopBar::PlaybackMode::Pattern
                 : TopBar::PlaybackMode::Song;
    topBar.setPlaybackMode(playbackMode, false);

    if (const auto* loop = LoopsRegistry::instance().get(activeLoopId))
    {
        midi.setNotes(loop->notes);
        midi.setLoopEnabled(true);
        midi.setLoopRegion(0.0, loop->lengthBeats);
        loopLengthBeats = loop->lengthBeats;
    }
    else
    {
        midi.setNotes({});
        midi.setLoopEnabled(false);
        loopLengthBeats = 0.0;
    }

    timeline.requestRebuild();
    arranger.refreshFromModel();
    arranger.frameAll();
    rebuildEddiePlaybackNotes();
    repaint();
    return true;
}

void MainComponent::rebuildEddiePlaybackNotes()
{
    std::vector<MidiNote> playbackNotes;

    if (playbackMode == TopBar::PlaybackMode::Pattern)
    {
        if (const auto* loop = LoopsRegistry::instance().get(activeLoopId))
            playbackNotes = loop->notes;
        else
            playbackNotes = midi.getNotes();

        eddie.setNotes(playbackNotes);
        timeline.setPlaying(false);
        loopStartBeats = 0.0;
        if (const auto* loop = LoopsRegistry::instance().get(activeLoopId))
            loopLengthBeats = juce::jmax(1.0, loop->lengthBeats);
        loopEnabled = loopLengthBeats > 0.0;
        return;
    }

    for (const auto& track : arranger.tracks)
    {
        for (const auto& clip : track.clips)
        {
            if (clip.kind != ClipModel::Kind::MidiLoop)
                continue;

            const auto* loop = LoopsRegistry::instance().get(clip.loopId);
            if (loop == nullptr || loop->notes.empty())
                continue;

            const double loopLen = juce::jmax(1.0, loop->lengthBeats);
            const int repeats = juce::jmax(1, (int) std::ceil(clip.lengthBeats / loopLen));

            for (int repeat = 0; repeat < repeats; ++repeat)
            {
                const double repeatStart = repeat * loopLen;
                for (const auto& src : loop->notes)
                {
                    const double localStart = repeatStart + src.startBeats;
                    if (localStart >= clip.lengthBeats)
                        continue;

                    MidiNote n = src;
                    n.startBeats = clip.startBeats + localStart;
                    n.lengthBeats = juce::jmin(src.lengthBeats, clip.lengthBeats - localStart);
                    if (n.lengthBeats > 0.0)
                        playbackNotes.push_back(n);
                }
            }
        }
    }

    eddie.setNotes(playbackNotes);
    timeline.setPlaying(playing);
}

void MainComponent::openEddiePanel()
{
    eddiePanel.setSettings (eddie.getSettings());
    eddiePanel.setVisible (true);
    eddiePanel.toFront (true);
}

void MainComponent::openSampleBrowser()
{
    sampleBrowser.rescan();
    sampleBrowser.setVisible (true);
    sampleBrowser.toFront (true);
}

} // namespace aidaw
