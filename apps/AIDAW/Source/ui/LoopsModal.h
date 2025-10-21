#pragma once
#include <JuceHeader.h>

// ---------- Session-scoped storage ----------
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

// ---------- Modal UI ----------
class LoopsModal  : public juce::Component,
                    private juce::Button::Listener,
                    private juce::TextEditor::Listener,
                    public juce::ListBoxModel
{
public:
    struct Callbacks
    {
        std::function<void(uint32 /*loopId*/)> onOpenLoop;
        std::function<void(uint32 /*loopId*/)> onDelete;
        std::function<void(uint32 /*loopId*/)> onCreate;
    };

    static void show(juce::Component* parentForCentre, Callbacks cb = {})
    {
        auto* c = new LoopsModal(std::move(cb));

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned(c);
        o.dialogTitle = "Loops";
        o.dialogBackgroundColour = juce::Colour(0xFF121212);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = false;
        o.componentToCentreAround = parentForCentre;

        // Older JUCE versions didn’t expose runModal() here; use async
        // (it still behaves modally to the user)
        o.launchAsync();
    }

    // --------- JUCE ---------
    int getNumRows() override { return filteredIndexes.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool rowIsSelected) override
    {
        const auto& arr = LoopsRegistry::instance().list();
        if (!juce::isPositiveAndBelow(row, filteredIndexes.size())) return;
        const int realIndex = filteredIndexes[(size_t)row];
        if (!juce::isPositiveAndBelow(realIndex, arr.size())) return;

        g.setColour(rowIsSelected ? juce::Colour(0xFF1F2937) : juce::Colour(0xFF111111));
        g.fillRoundedRectangle(juce::Rectangle<float>(2.f, 2.f, (float)w-4.f, (float)h-4.f), 8.f);

        const auto& li = arr.getReference(realIndex);
        g.setColour(juce::Colours::white);
        g.setFont(15.f);
        g.drawText(li.name, 12, 6, w-24, 20, juce::Justification::left);

        g.setColour(juce::Colour(0x77FFFFFF));
        g.setFont(12.f);
        g.drawText(li.created.formatted("%Y-%m-%d  %H:%M"), 12, 24, w-24, h-24, juce::Justification::left);
    }

    LoopsModal(Callbacks cb) : callbacks(std::move(cb))
    {
        setSize(420, 380);

        title.setText("Session Loops", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centredLeft);
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        title.setFont(juce::Font(18.f, juce::Font::bold));
        addAndMakeVisible(title);

        search.setTextToShowWhenEmpty("Filter by name…", juce::Colour(0x55FFFFFF));
        search.addListener(this);
        addAndMakeVisible(search);

        list.setModel(this);
        list.setRowHeight(44);
        addAndMakeVisible(list);

        btnNew.setButtonText("New Loop");
        btnNew.addListener(this);
        addAndMakeVisible(btnNew);

        btnOpen.setButtonText("Open");
        btnOpen.addListener(this);
        addAndMakeVisible(btnOpen);

        btnDelete.setButtonText("Delete");
        btnDelete.addListener(this);
        addAndMakeVisible(btnDelete);

        btnClose.setButtonText("Close");
        btnClose.addListener(this);
        addAndMakeVisible(btnClose);

        refreshFilter();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF0E0E0E));
        g.setColour(juce::Colour(0x33FFFFFF));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 12.f, 1.f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(14);
        title.setBounds(r.removeFromTop(28));
        r.removeFromTop(8);
        search.setBounds(r.removeFromTop(28));
        r.removeFromTop(8);

        list.setBounds(r.removeFromTop(getHeight()-140));

        auto row = r.removeFromBottom(36);
        btnNew.setBounds(row.removeFromLeft(110));
        row.removeFromLeft(8);
        btnOpen.setBounds(row.removeFromLeft(82));
        row.removeFromLeft(8);
        btnDelete.setBounds(row.removeFromLeft(82));
        btnClose.setBounds(r.removeFromBottom(32).removeFromRight(80));
    }

private:
    // --- actions ---
    void buttonClicked(juce::Button* b) override
    {
        if (b == &btnNew)
        {
            auto base = search.getText().trim();
            uint32 id = LoopsRegistry::instance().createLoop(base);
            refreshFilter();
            if (callbacks.onCreate) callbacks.onCreate(id);
        }
        else if (b == &btnOpen)
        {
            auto id = selectedLoopId();
            if (id != 0 && callbacks.onOpenLoop) callbacks.onOpenLoop(id);
        }
        else if (b == &btnDelete)
        {
            auto id = selectedLoopId();
            if (id != 0)
            {
                LoopsRegistry::instance().removeLoop(id);
                refreshFilter();
                if (callbacks.onDelete) callbacks.onDelete(id);
            }
        }
        else if (b == &btnClose)
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->closeButtonPressed();
        }
    }

    void textEditorTextChanged(juce::TextEditor&) override { refreshFilter(); }

    // --- helpers ---
    uint32 selectedLoopId() const
    {
        auto row = list.getSelectedRow();
        if (row < 0 || row >= filteredIndexes.size()) return 0;
        const int realIndex = filteredIndexes[(size_t)row];
        const auto& arr = LoopsRegistry::instance().list();
        return juce::isPositiveAndBelow(realIndex, arr.size()) ? arr[realIndex].id : 0;
    }

    void refreshFilter()
    {
        filteredIndexes.clearQuick();
        const auto q = search.getText().trim().toLowerCase();
        auto& arr = LoopsRegistry::instance().list();

        for (int i = 0; i < arr.size(); ++i)
        {
            const bool keep = q.isEmpty() || arr[i].name.toLowerCase().contains(q);
            if (keep) filteredIndexes.add(i);
        }
        list.updateContent();

        // Select first if any, otherwise clear selection
        if (filteredIndexes.size() > 0) list.selectRow(0);
        else                             list.deselectAllRows();
        list.repaint();
    }

    Callbacks callbacks;

    juce::Label       title;
    juce::TextEditor  search;
    juce::ListBox     list;

    juce::TextButton  btnNew, btnOpen, btnDelete, btnClose;

    // mapping filtered rows -> real indices in registry
    juce::Array<int>  filteredIndexes;
};
