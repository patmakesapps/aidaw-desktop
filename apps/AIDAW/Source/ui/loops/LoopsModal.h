#pragma once
#include <JuceHeader.h>
#include "LoopsRegistry.h"

class LoopsModal  : public juce::Component,
                    public juce::DragAndDropContainer,
                    private juce::Button::Listener,
                    private juce::TextEditor::Listener,
                    public  juce::ListBoxModel,
                    private juce::KeyListener
{
public:
    struct Callbacks
    {
        std::function<void(uint32)> onOpenLoop;
        std::function<void(uint32)> onDelete;
        std::function<void(uint32)> onCreate;
        std::function<void(uint32)> onAddToComposer;
    };

    class GreenCloseLNF : public juce::LookAndFeel_V4
    {
    public:
        GreenCloseLNF() : juce::LookAndFeel_V4(juce::LookAndFeel_V4::getDarkColourScheme()) {}
        juce::Button* createDocumentWindowButton(int buttonType) override
        {
            auto* b = LookAndFeel_V4::createDocumentWindowButton(buttonType);
            if (buttonType == juce::DocumentWindow::closeButton)
            {
                const auto green = juce::Colour(0xFF22C55E);
                b->setColour(juce::TextButton::buttonColourId,    green.darker(0.6f));
                b->setColour(juce::TextButton::buttonOnColourId,  green);
                b->setColour(juce::TextButton::textColourOffId,   juce::Colours::white);
                b->setColour(juce::TextButton::textColourOnId,    juce::Colours::white);
            }
            return b;
        }
    };

    static GreenCloseLNF& greenLNF() { static GreenCloseLNF lnf; return lnf; }

    static void show(juce::Component* parentForCentre, Callbacks cb = {})
    {
        auto* c = new LoopsModal(std::move(cb));

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned(c);
        o.dialogTitle                  = "Loops";
        o.dialogBackgroundColour       = juce::Colour(0xFF121212);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar            = false;
        o.resizable                    = false;
        o.componentToCentreAround      = parentForCentre;

        if (auto* dw = o.launchAsync())
            dw->setLookAndFeel(&greenLNF());
    }

    int getNumRows() override { return filteredIndexes.size(); }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (auto id = loopIdForFilteredRow(row); id != 0 && callbacks.onOpenLoop)
            callbacks.onOpenLoop(id);
    }

    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override {}

    juce::Component* refreshComponentForRow(int row, bool rowIsSelected, juce::Component* existing) override
    {
        auto* comp = dynamic_cast<LoopRow*>(existing);
        if (comp == nullptr)
            comp = new LoopRow(*this);

        comp->update(row, rowIsSelected, loopIdForFilteredRow(row));
        return comp;
    }

    LoopsModal(Callbacks cb) : callbacks(std::move(cb))
    {
        setSize(560, 440);

        title.setText("Session Loops", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centredLeft);
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        title.setFont(juce::Font(18.f, juce::Font::bold));
        addAndMakeVisible(title);

        search.setTextToShowWhenEmpty("Filter by name...", juce::Colour(0x55FFFFFF));
        search.addListener(this);
        addAndMakeVisible(search);

        list.setModel(this);
        list.setRowHeight(70);
        list.addKeyListener(this);
        addAndMakeVisible(list);

        btnNew.setButtonText("New Loop");
        btnOpen.setButtonText("Open");
        btnAdd.setButtonText("Add");
        btnDelete.setButtonText("Delete");
        btnClose.setButtonText("Close");
        for (auto* b : { &btnNew, &btnOpen, &btnAdd, &btnDelete, &btnClose })
        { b->addListener(this); addAndMakeVisible(b); }

        refreshFilter();
    }

    void openLoop(uint32 id)
    {
        if (id != 0 && callbacks.onOpenLoop)
            callbacks.onOpenLoop(id);
    }

    void beginDrag(uint32 id, juce::Component* source)
    {
        if (id == 0 || source == nullptr)
            return;

        startDragging("aidaw-loop:" + juce::String(id), source);
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
        list.setBounds(r.removeFromTop(getHeight() - 144));

        auto row = r.removeFromBottom(36);
        btnNew.setBounds   (row.removeFromLeft(110)); row.removeFromLeft(8);
        btnOpen.setBounds  (row.removeFromLeft(82));  row.removeFromLeft(8);
        btnAdd.setBounds   (row.removeFromLeft(82));  row.removeFromLeft(8);
        btnDelete.setBounds(row.removeFromLeft(82));
        btnClose.setBounds (r.removeFromBottom(32).removeFromRight(80));

        if (renameEditor && renamingRow >= 0)
            positionRenameEditor(renamingRow);
    }

private:
    class LoopRow : public juce::Component
    {
    public:
        explicit LoopRow(LoopsModal& ownerIn) : owner(ownerIn) {}

        void update(int rowIn, bool selectedIn, uint32 idIn)
        {
            row = rowIn;
            selected = selectedIn;
            loopId = idIn;
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            const auto* li = LoopsRegistry::instance().get(loopId);
            if (li == nullptr)
                return;

            auto r = getLocalBounds().reduced(4, 4);
            g.setColour(selected ? juce::Colour(0xFF1F2937) : juce::Colour(0xFF111111));
            g.fillRoundedRectangle(r.toFloat(), 8.f);

            g.setColour(juce::Colours::white);
            g.setFont(16.f);
            g.drawText(li->name, 14, 8, getWidth() - 220, 22, juce::Justification::left);

            g.setColour(juce::Colour(0x77FFFFFF));
            g.setFont(12.f);
            g.drawText(juce::String(li->lengthBeats, 1) + " beats  |  " + li->created.formatted("%Y-%m-%d %H:%M"),
                       14, 32, getWidth() - 220, 18, juce::Justification::left);

            auto pr = previewRect();
            g.setColour(juce::Colour(0xFF0B0F14));
            g.fillRoundedRectangle(pr.toFloat(), 6.f);
            g.setColour(juce::Colour(0x3322C55E));
            g.drawRoundedRectangle(pr.toFloat(), 6.f, 1.0f);

            g.setColour(juce::Colour(0x22FFFFFF));
            const int divisions = juce::jmax(1, (int) std::ceil(li->lengthBeats));
            for (int i = 1; i < divisions; ++i)
            {
                const int x = pr.getX() + (int) std::round((i / (double) divisions) * pr.getWidth());
                g.fillRect(x, pr.getY(), 1, pr.getHeight());
            }

            const int pitchMin = 36;
            const int pitchMax = 84;
            for (auto& n : li->previewNotes)
            {
                const double length = juce::jmax(1.0, li->lengthBeats);
                const double x0 = juce::jlimit(0.0, length, n.startBeats);
                const double x1 = juce::jlimit(0.0, length, n.startBeats + juce::jmax(0.05, n.lengthBeats));
                const int nx = pr.getX() + (int)std::round((x0 / length) * pr.getWidth());
                const int nw = juce::jmax(3, (int)std::round(((x1 - x0) / length) * pr.getWidth()));
                const float t = (float)juce::jlimit(0.0, 1.0, (n.pitch - pitchMin) / (double)(pitchMax - pitchMin));
                const int ny = pr.getY() + pr.getHeight() - 4 - (int)std::round(t * (pr.getHeight() - 8));

                g.setColour(juce::Colour(0xFF22C55E).withAlpha(0.85f));
                g.fillRoundedRectangle(juce::Rectangle<float>((float)nx, (float)ny, (float)nw, 6.0f), 3.f);
            }
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            owner.list.selectRow(row);
            dragStart = e.getPosition();
            if (previewRect().contains(e.getPosition()))
                owner.openLoop(loopId);
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (e.getPosition().getDistanceFrom(dragStart) > 8)
                owner.beginDrag(loopId, this);
        }

    private:
        juce::Rectangle<int> previewRect() const
        {
            return { getWidth() - 190, 8, 176, getHeight() - 16 };
        }

        LoopsModal& owner;
        int row { -1 };
        uint32 loopId { 0 };
        bool selected { false };
        juce::Point<int> dragStart { 0, 0 };
    };

    bool keyPressed(const juce::KeyPress& k, juce::Component*) override
    {
        if (k.getKeyCode() == juce::KeyPress::F2Key)
        {
            const int row = list.getSelectedRow();
            if (juce::isPositiveAndBelow(row, filteredIndexes.size()))
                beginRename(row);
            return true;
        }
        return false;
    }

    void buttonClicked(juce::Button* b) override
    {
        if (b == &btnNew)
        {
            auto base = search.getText().trim();
            uint32 id = LoopsRegistry::instance().createLoop(base);
            refreshFilter();
            if (callbacks.onCreate) callbacks.onCreate(id);
            if (callbacks.onOpenLoop) callbacks.onOpenLoop(id);
        }
        else if (b == &btnOpen)
        {
            openLoop(selectedLoopId());
        }
        else if (b == &btnAdd)
        {
            auto id = selectedLoopId();
            if (id != 0 && callbacks.onAddToComposer) callbacks.onAddToComposer(id);
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
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->closeButtonPressed();
        }
    }

    void textEditorTextChanged(juce::TextEditor&) override { refreshFilter(); }
    void textEditorReturnKeyPressed(juce::TextEditor& te) override { commitRename(te.getText(), true); }
    void textEditorEscapeKeyPressed(juce::TextEditor& te) override { commitRename(te.getText(), false); }
    void textEditorFocusLost(juce::TextEditor& te) override        { commitRename(te.getText(), true); }

    uint32 selectedLoopId() const { return loopIdForFilteredRow(list.getSelectedRow()); }

    uint32 loopIdForFilteredRow(int row) const
    {
        if (!juce::isPositiveAndBelow(row, filteredIndexes.size())) return 0;
        const int realIndex = filteredIndexes[(size_t)row];
        const auto& arr = LoopsRegistry::instance().list();
        return juce::isPositiveAndBelow(realIndex, arr.size()) ? arr[realIndex].id : 0;
    }

    void refreshFilter()
    {
        const auto q = search.getText().trim().toLowerCase();
        filteredIndexes.clearQuick();

        auto& arr = LoopsRegistry::instance().list();
        for (int i = 0; i < arr.size(); ++i)
            if (q.isEmpty() || arr[i].name.toLowerCase().contains(q))
                filteredIndexes.add(i);

        list.updateContent();
        if (filteredIndexes.size() > 0)
            list.selectRow(juce::jlimit(0, filteredIndexes.size() - 1, list.getSelectedRow()));
        else
            list.deselectAllRows();

        list.repaint();
    }

    void beginRename(int filteredRow)
    {
        renamingRow = filteredRow;

        if (!renameEditor)
        {
            renameEditor = std::make_unique<juce::TextEditor>();
            renameEditor->setSelectAllWhenFocused(true);
            renameEditor->addListener(this);
            addAndMakeVisible(renameEditor.get());
        }

        const auto& arr = LoopsRegistry::instance().list();
        const int realIndex = filteredIndexes[(size_t)filteredRow];
        if (!juce::isPositiveAndBelow(realIndex, arr.size())) return;

        renameEditor->setText(arr[realIndex].name, juce::dontSendNotification);
        positionRenameEditor(filteredRow);
        renameEditor->grabKeyboardFocus();
        repaint();
    }

    void positionRenameEditor(int filteredRow)
    {
        auto rowY = list.getRowPosition(filteredRow, true).getY();
        const int h = list.getRowHeight();
        auto bounds = list.getBoundsInParent().withY(list.getY() + rowY).withHeight(h);
        bounds = bounds.reduced(16, 0);
        renameEditor->setBounds(bounds.removeFromLeft(bounds.getWidth() - 210)
                                      .withTrimmedTop(9).withHeight(24));
        renameEditor->toFront(false);
    }

    void commitRename(juce::String newText, bool accept)
    {
        if (!renameEditor) return;

        if (accept && renamingRow >= 0 && renamingRow < filteredIndexes.size())
        {
            const int realIndex = filteredIndexes[(size_t)renamingRow];
            const auto& arr = LoopsRegistry::instance().list();
            if (juce::isPositiveAndBelow(realIndex, arr.size()))
            {
                auto id = arr[realIndex].id;
                LoopsRegistry::instance().renameLoop(id, newText);
                refreshFilter();
            }
        }

        renameEditor.reset();
        renamingRow = -1;
    }

    Callbacks callbacks;
    juce::Label title;
    juce::TextEditor search;
    juce::ListBox list;
    juce::TextButton btnNew, btnOpen, btnAdd, btnDelete, btnClose;

    std::unique_ptr<juce::TextEditor> renameEditor;
    int renamingRow { -1 };

    juce::Array<int> filteredIndexes;
};
