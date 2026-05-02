#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Runtime theme switcher. Holds 4 palettes; on setTheme() it overwrites
// every Theme::colX field and broadcasts a change so all listeners repaint.
class ThemeManager : public juce::ChangeBroadcaster
{
public:
    enum class ThemeId { Aurora = 0, Carbon = 1, Sunset = 2 };

    struct Palette
    {
        juce::String name;
        bool isLight { false };

        juce::uint32 colBgMain, colBgPanel, colBgRuler;
        juce::uint32 colHeaderDiv, colGridBar, colGridBeat, colGridSub, colOctave;
        juce::uint32 colBarTick, colBarLabel;
        juce::uint32 colRowEven, colRowOdd;
        juce::uint32 colKeyWhite, colKeyBlack, colKeySep;
        juce::uint32 colText, colTextDim;
        juce::uint32 colAccent, colAccent2;
        juce::uint32 colPlayhead, colLoop, colLoopFill;
        juce::uint32 colSelect, colSelectBd;
        juce::uint32 colNoteA, colNoteB;
        float        noteBorderGain;
        juce::uint32 colVelLane;
        juce::uint32 colBtnIdle, colBtnHover, colBtnActive, colBtnText, colBtnStroke;
        juce::uint32 colChromeTop, colChromeBot, colPillBg;
        juce::uint32 colDanger;
    };

    static ThemeManager& get()
    {
        static ThemeManager inst;
        return inst;
    }

    const std::array<Palette, 3>& palettes() const { return all; }
    ThemeId currentId() const { return current; }
    const Palette& currentPalette() const { return all[(size_t)current]; }

    void setTheme(ThemeId id)
    {
        current = id;
        apply(all[(size_t)id]);
        save();
        sendChangeMessage();
    }

    void cycleNext()
    {
        const int n = (int) all.size();
        setTheme(static_cast<ThemeId>(((int)current + 1) % n));
    }

private:
    ThemeManager()
    {
        buildPalettes();
        load();
        apply(all[(size_t)current]);
    }

    void apply(const Palette& p)
    {
        Theme::colBgMain    = p.colBgMain;
        Theme::colBgPanel   = p.colBgPanel;
        Theme::colBgRuler   = p.colBgRuler;
        Theme::colHeaderDiv = p.colHeaderDiv;
        Theme::colGridBar   = p.colGridBar;
        Theme::colGridBeat  = p.colGridBeat;
        Theme::colGridSub   = p.colGridSub;
        Theme::colOctave    = p.colOctave;
        Theme::colBarTick   = p.colBarTick;
        Theme::colBarLabel  = p.colBarLabel;
        Theme::colRowEven   = p.colRowEven;
        Theme::colRowOdd    = p.colRowOdd;
        Theme::colKeyWhite  = p.colKeyWhite;
        Theme::colKeyBlack  = p.colKeyBlack;
        Theme::colKeySep    = p.colKeySep;
        Theme::colText      = p.colText;
        Theme::colTextDim   = p.colTextDim;
        Theme::colAccent    = p.colAccent;
        Theme::colAccent2   = p.colAccent2;
        Theme::colPlayhead  = p.colPlayhead;
        Theme::colLoop      = p.colLoop;
        Theme::colLoopFill  = p.colLoopFill;
        Theme::colSelect    = p.colSelect;
        Theme::colSelectBd  = p.colSelectBd;
        Theme::colNoteA     = p.colNoteA;
        Theme::colNoteB     = p.colNoteB;
        Theme::noteBorderGain = p.noteBorderGain;
        Theme::colVelLane   = p.colVelLane;
        Theme::colBtnIdle   = p.colBtnIdle;
        Theme::colBtnHover  = p.colBtnHover;
        Theme::colBtnActive = p.colBtnActive;
        Theme::colBtnText   = p.colBtnText;
        Theme::colBtnStroke = p.colBtnStroke;
        Theme::colChromeTop = p.colChromeTop;
        Theme::colChromeBot = p.colChromeBot;
        Theme::colPillBg    = p.colPillBg;
        Theme::colDanger    = p.colDanger;
    }

    juce::PropertiesFile* props()
    {
        if (!propsFile)
        {
            juce::PropertiesFile::Options opt;
            opt.applicationName = "AIDAW";
            opt.filenameSuffix = ".settings";
            opt.osxLibrarySubFolder = "Application Support";
            opt.folderName = "AIDAW";
            propsFile = std::make_unique<juce::PropertiesFile>(opt);
        }
        return propsFile.get();
    }

    void load()
    {
        const int v = props()->getIntValue("themeId", (int)ThemeId::Aurora);
        if (v >= 0 && v < (int)all.size())
            current = (ThemeId)v;
    }

    void save() { props()->setValue("themeId", (int)current); props()->saveIfNeeded(); }

    void buildPalettes()
    {
        // ---------- 0 : Aurora (deep teal/cyan) ----------
        {
            Palette p;
            p.name = "Aurora";
            p.isLight       = false;
            p.colBgMain     = 0xFF0C1016;
            p.colBgPanel    = 0xFF0E141C;
            p.colBgRuler    = 0xFF0A0F15;
            p.colHeaderDiv  = 0x22344A62;
            p.colGridBar    = 0x5538E0FF;
            p.colGridBeat   = 0x1A38E0FF;
            p.colGridSub    = 0x0D38E0FF;
            p.colOctave     = 0x2038E0FF;
            p.colBarTick    = 0x66FFFFFF;
            p.colBarLabel   = 0xD0EAF2FF;
            p.colRowEven    = 0xFF0F151D;
            p.colRowOdd     = 0xFF0B1118;
            p.colKeyWhite   = 0xFF121925;
            p.colKeyBlack   = 0xFF0A0F15;
            p.colKeySep     = 0x29324352;
            p.colText       = 0xE6FFFFFF;
            p.colTextDim    = 0x88FFFFFF;
            p.colAccent     = 0xFF3CE0FF;
            p.colAccent2    = 0xFF59F3C3;
            p.colPlayhead   = 0xFF3CE0FF;
            p.colLoop       = 0xFF3CE0FF;
            p.colLoopFill   = 0x143CE0FF;
            p.colSelect     = 0x3329FFC1;
            p.colSelectBd   = 0x8829FFC1;
            p.colNoteA      = 0xFF59F3C3;
            p.colNoteB      = 0xFF7AA7FF;
            p.noteBorderGain= 0.40f;
            p.colVelLane    = 0xFF0B1118;
            p.colBtnIdle    = 0xFF111923;
            p.colBtnHover   = 0xFF182331;
            p.colBtnActive  = 0xFF0F2A38;
            p.colBtnText    = 0xE6FFFFFF;
            p.colBtnStroke  = 0x222D7A8E;
            p.colChromeTop  = 0xFF0B1016;
            p.colChromeBot  = 0xFF131A24;
            p.colPillBg     = 0xFF0F1620;
            p.colDanger     = 0xFFEF4444;
            all[0] = p;
        }

        // ---------- 1 : Carbon (neutral graphite, warm-white accent) ----------
        {
            Palette p;
            p.name = "Carbon";
            p.isLight       = false;
            p.colBgMain     = 0xFF131313;
            p.colBgPanel    = 0xFF181818;
            p.colBgRuler    = 0xFF101010;
            p.colHeaderDiv  = 0x22A0A0A0;
            p.colGridBar    = 0x55C8C2B6;
            p.colGridBeat   = 0x1AC8C2B6;
            p.colGridSub    = 0x0DC8C2B6;
            p.colOctave     = 0x14E8E2D6;
            p.colBarTick    = 0x66FFFFFF;
            p.colBarLabel   = 0xD0F2EEE4;
            p.colRowEven    = 0xFF1A1A1A;
            p.colRowOdd     = 0xFF161616;
            p.colKeyWhite   = 0xFF1F1F1F;
            p.colKeyBlack   = 0xFF101010;
            p.colKeySep     = 0x29555555;
            p.colText       = 0xE6F6F1E7;
            p.colTextDim    = 0x88F6F1E7;
            p.colAccent     = 0xFFE8DDC0;
            p.colAccent2    = 0xFFC8B98F;
            p.colPlayhead   = 0xFFE8DDC0;
            p.colLoop       = 0xFFE8DDC0;
            p.colLoopFill   = 0x14E8DDC0;
            p.colSelect     = 0x33E8DDC0;
            p.colSelectBd   = 0x88E8DDC0;
            p.colNoteA      = 0xFFE8DDC0;
            p.colNoteB      = 0xFFB7A878;
            p.noteBorderGain= 0.30f;
            p.colVelLane    = 0xFF161616;
            p.colBtnIdle    = 0xFF1E1E1E;
            p.colBtnHover   = 0xFF272727;
            p.colBtnActive  = 0xFF2F2A1E;
            p.colBtnText    = 0xE6F6F1E7;
            p.colBtnStroke  = 0x22B7A878;
            p.colChromeTop  = 0xFF101010;
            p.colChromeBot  = 0xFF1A1A1A;
            p.colPillBg     = 0xFF1A1A1A;
            p.colDanger     = 0xFFD4554B;
            all[1] = p;
        }

        // ---------- 2 : Sunset (deep plum, amber + magenta accents) ----------
        {
            Palette p;
            p.name = "Sunset";
            p.isLight       = false;
            p.colBgMain     = 0xFF1A0E1C;
            p.colBgPanel    = 0xFF221327;
            p.colBgRuler    = 0xFF150A18;
            p.colHeaderDiv  = 0x22FF8AB6;
            p.colGridBar    = 0x55FFB347;
            p.colGridBeat   = 0x1AFFB347;
            p.colGridSub    = 0x0DFFB347;
            p.colOctave     = 0x18FF6FB6;
            p.colBarTick    = 0x66FFE0C0;
            p.colBarLabel   = 0xD0FFE0C8;
            p.colRowEven    = 0xFF22132A;
            p.colRowOdd     = 0xFF1B0E22;
            p.colKeyWhite   = 0xFF2A1A33;
            p.colKeyBlack   = 0xFF150A1A;
            p.colKeySep     = 0x29562C4D;
            p.colText       = 0xE6FFEDD6;
            p.colTextDim    = 0x88FFEDD6;
            p.colAccent     = 0xFFFFB347; // amber
            p.colAccent2    = 0xFFFF6FB6; // magenta
            p.colPlayhead   = 0xFFFFB347;
            p.colLoop       = 0xFFFF6FB6;
            p.colLoopFill   = 0x14FF6FB6;
            p.colSelect     = 0x33FFB347;
            p.colSelectBd   = 0x88FFB347;
            p.colNoteA      = 0xFFFFB347;
            p.colNoteB      = 0xFFFF6FB6;
            p.noteBorderGain= 0.45f;
            p.colVelLane    = 0xFF1B0E22;
            p.colBtnIdle    = 0xFF2A1A33;
            p.colBtnHover   = 0xFF3A2444;
            p.colBtnActive  = 0xFF4A2A3A;
            p.colBtnText    = 0xE6FFEDD6;
            p.colBtnStroke  = 0x22FF8AB6;
            p.colChromeTop  = 0xFF180A1C;
            p.colChromeBot  = 0xFF24122C;
            p.colPillBg     = 0xFF22132A;
            p.colDanger     = 0xFFFF5A6F;
            all[2] = p;
        }

    }

    std::array<Palette, 3> all;
    ThemeId current { ThemeId::Aurora };
    std::unique_ptr<juce::PropertiesFile> propsFile;
};
