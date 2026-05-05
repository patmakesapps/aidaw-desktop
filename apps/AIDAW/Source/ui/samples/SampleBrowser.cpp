#include "SampleBrowser.h"

#include "../shared/Theme.h"
#include "../shared/ThemeManager.h"

#include <algorithm>

namespace aidaw
{

class SampleBrowser::Row final : public juce::Component
{
public:
    explicit Row(SampleBrowser& ownerIn) : owner(ownerIn) {}

    void update(int rowIn, bool selectedIn, const SampleItem* itemIn)
    {
        row = rowIn;
        selected = selectedIn;
        item = itemIn;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (item == nullptr)
            return;

        auto r = getLocalBounds().reduced(4);
        const auto rowColour = selected
            ? juce::Colour(Theme::colBtnActive).interpolatedWith(juce::Colour(Theme::colAccent), 0.16f)
            : juce::Colour(Theme::colRowEven);
        g.setColour(rowColour);
        g.fillRoundedRectangle(r.toFloat(), 7.0f);

        g.setColour(juce::Colour(Theme::colText));
        g.setFont(14.5f);
        g.drawText(item->name, r.removeFromTop(22).reduced(10, 0), juce::Justification::centredLeft, true);

        juce::String meta = item->category;
        if (item->bpm > 0)
            meta << "  |  " << item->bpm << " BPM";
        if (item->key.isNotEmpty())
            meta << "  |  " << item->key;

        g.setColour(juce::Colour(Theme::colTextDim));
        g.setFont(12.0f);
        g.drawText(meta, 14, 28, getWidth() - 28, 18, juce::Justification::centredLeft, true);

        g.setColour(juce::Colour(selected ? Theme::colSelectBd : Theme::colBtnStroke));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4.5f), 7.0f, 1.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStart = e.getPosition();
        owner.list.selectRow(row);
        owner.previewRow(row);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.getPosition().getDistanceFrom(dragStart) > 6)
            owner.beginSampleDrag(row, this);
    }

private:
    SampleBrowser& owner;
    const SampleItem* item { nullptr };
    int row { -1 };
    bool selected { false };
    juce::Point<int> dragStart { 0, 0 };
};

SampleBrowser::SampleBrowser(SamplePreviewSource& previewSource)
    : preview(previewSource)
{
    title.setText("Samples", juce::dontSendNotification);
    title.setColour(juce::Label::textColourId, juce::Colour(Theme::colText));
    title.setFont(juce::Font(18.0f, juce::Font::bold));
    addAndMakeVisible(title);

    search.setTextToShowWhenEmpty("Search samples...", juce::Colour(Theme::colTextDim));
    search.addListener(this);
    addAndMakeVisible(search);

    packFilter.addListener(this);
    categoryFilter.addListener(this);
    addAndMakeVisible(packFilter);
    addAndMakeVisible(categoryFilter);

    closeButton.addListener(this);
    refreshButton.addListener(this);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(refreshButton);

    list.setModel(this);
    list.setRowHeight(56);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(Theme::colBgPanel));
    list.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(list);

    emptyState.setText("No samples found. Put packs in Samples or Documents/AIDAW/Samples.", juce::dontSendNotification);
    emptyState.setColour(juce::Label::textColourId, juce::Colour(Theme::colTextDim));
    emptyState.setJustificationType(juce::Justification::centred);
    addChildComponent(emptyState);

    rescan();
    ThemeManager::get().addChangeListener(this);
}

SampleBrowser::~SampleBrowser()
{
    search.removeListener(this);
    packFilter.removeListener(this);
    categoryFilter.removeListener(this);
    closeButton.removeListener(this);
    refreshButton.removeListener(this);
    list.setModel(nullptr);
    ThemeManager::get().removeChangeListener(this);
}

void SampleBrowser::rescan()
{
    rebuildCatalog();
    rebuildFilters();
    applyFilter();
}

void SampleBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::colBgPanel));
    g.setColour(juce::Colour(Theme::colHeaderDiv));
    g.drawRect(getLocalBounds(), 1);
}

void SampleBrowser::resized()
{
    auto r = getLocalBounds().reduced(12);
    auto header = r.removeFromTop(30);
    title.setBounds(header.removeFromLeft(140));
    refreshButton.setBounds(header.removeFromRight(76));
    header.removeFromRight(8);
    closeButton.setBounds(header.removeFromRight(72));

    r.removeFromTop(10);
    search.setBounds(r.removeFromTop(28));
    r.removeFromTop(8);

    auto filters = r.removeFromTop(28);
    packFilter.setBounds(filters.removeFromLeft((filters.getWidth() - 8) / 2));
    filters.removeFromLeft(8);
    categoryFilter.setBounds(filters);

    r.removeFromTop(10);
    list.setBounds(r);
    emptyState.setBounds(r);
}

int SampleBrowser::getNumRows()
{
    return filteredRows.size();
}

void SampleBrowser::paintListBoxItem(int, juce::Graphics&, int, int, bool) {}

juce::Component* SampleBrowser::refreshComponentForRow(int row, bool selected, juce::Component* existing)
{
    auto* comp = dynamic_cast<Row*>(existing);
    if (comp == nullptr)
        comp = new Row(*this);

    const auto real = juce::isPositiveAndBelow(row, filteredRows.size()) ? filteredRows[row] : -1;
    comp->update(row, selected, juce::isPositiveAndBelow(real, samples.size()) ? samples[real] : nullptr);
    return comp;
}

void SampleBrowser::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    previewRow(row);
}

void SampleBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    previewRow(row);
}

void SampleBrowser::textEditorTextChanged(juce::TextEditor&)
{
    applyFilter();
}

void SampleBrowser::comboBoxChanged(juce::ComboBox*)
{
    applyFilter();
}

void SampleBrowser::buttonClicked(juce::Button* b)
{
    if (b == &refreshButton)
        rescan();
    else if (b == &closeButton)
    {
        preview.stop();
        setVisible(false);
    }
}

void SampleBrowser::changeListenerCallback(juce::ChangeBroadcaster*)
{
    title.setColour(juce::Label::textColourId, juce::Colour(Theme::colText));
    search.setTextToShowWhenEmpty("Search samples...", juce::Colour(Theme::colTextDim));
    emptyState.setColour(juce::Label::textColourId, juce::Colour(Theme::colTextDim));
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(Theme::colBgPanel));
    list.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    repaint();
    list.repaint();
}

void SampleBrowser::rebuildCatalog()
{
    samples.clear();

    for (const auto& root : sampleRoots())
    {
        if (! root.isDirectory())
            continue;

        juce::Array<juce::File> packs;
        root.findChildFiles(packs, juce::File::findDirectories, false);

        if (packs.isEmpty())
            packs.add(root);

        for (const auto& packRoot : packs)
        {
            juce::Array<juce::File> files;
            packRoot.findChildFiles(files, juce::File::findFiles, true);

            for (const auto& file : files)
            {
                if (! isAudioFile(file))
                    continue;

                auto item = std::make_unique<SampleItem>();
                item->packName = packRoot.getFileName();
                item->packId = makePackId(item->packName);
                item->file = file;
                item->name = file.getFileNameWithoutExtension();
                item->bpm = parseBpm(item->name);
                item->key = parseKey(item->name);

                const auto rel = file.getRelativePathFrom(packRoot);
                const int slash = rel.indexOfChar(juce::File::getSeparatorChar());
                item->category = slash > 0 ? rel.substring(0, slash) : "Samples";

                samples.add(item.release());
            }
        }
    }

    std::sort(samples.begin(), samples.end(), [](const SampleItem* a, const SampleItem* b)
    {
        const auto left = a->packName + a->category + a->name;
        const auto right = b->packName + b->category + b->name;
        return left.compareIgnoreCase(right) < 0;
    });
}

void SampleBrowser::rebuildFilters()
{
    const auto selectedPack = packFilter.getText();
    const auto selectedCategory = categoryFilter.getText();

    packNames.clear();
    categoryNames.clear();
    for (auto* item : samples)
    {
        packNames.addIfNotAlreadyThere(item->packName);
        categoryNames.addIfNotAlreadyThere(item->category);
    }
    packNames.sort(true);
    categoryNames.sort(true);

    packFilter.clear(juce::dontSendNotification);
    categoryFilter.clear(juce::dontSendNotification);
    packFilter.addItem("All Packs", 1);
    categoryFilter.addItem("All Types", 1);
    for (int i = 0; i < packNames.size(); ++i)
        packFilter.addItem(packNames[i], i + 2);
    for (int i = 0; i < categoryNames.size(); ++i)
        categoryFilter.addItem(categoryNames[i], i + 2);

    packFilter.setSelectedId(packNames.contains(selectedPack) ? packNames.indexOf(selectedPack) + 2 : 1,
                             juce::dontSendNotification);
    categoryFilter.setSelectedId(categoryNames.contains(selectedCategory) ? categoryNames.indexOf(selectedCategory) + 2 : 1,
                                 juce::dontSendNotification);
}

void SampleBrowser::applyFilter()
{
    const auto q = search.getText().trim().toLowerCase();
    const auto pack = packFilter.getSelectedId() > 1 ? packFilter.getText() : juce::String();
    const auto category = categoryFilter.getSelectedId() > 1 ? categoryFilter.getText() : juce::String();

    filteredRows.clearQuick();
    for (int i = 0; i < samples.size(); ++i)
    {
        const auto* item = samples[i];
        if (pack.isNotEmpty() && item->packName != pack)
            continue;
        if (category.isNotEmpty() && item->category != category)
            continue;

        const auto haystack = (item->packName + " " + item->category + " " + item->name + " " + item->key).toLowerCase();
        if (q.isNotEmpty() && ! haystack.contains(q))
            continue;

        filteredRows.add(i);
    }

    list.updateContent();
    emptyState.setVisible(filteredRows.isEmpty());
    list.repaint();
}

void SampleBrowser::previewRow(int row)
{
    const auto real = juce::isPositiveAndBelow(row, filteredRows.size()) ? filteredRows[row] : -1;
    if (! juce::isPositiveAndBelow(real, samples.size()))
        return;

    preview.previewFile(samples[real]->file);
}

void SampleBrowser::beginSampleDrag(int row, juce::Component* source)
{
    const auto real = juce::isPositiveAndBelow(row, filteredRows.size()) ? filteredRows[row] : -1;
    if (! juce::isPositiveAndBelow(real, samples.size()) || source == nullptr)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        container->startDragging("aidaw-sample:" + samples[real]->file.getFullPathName(), source);
}

bool SampleBrowser::isAudioFile(const juce::File& file)
{
    return file.hasFileExtension("wav;aif;aiff;mp3;flac;ogg");
}

int SampleBrowser::parseBpm(const juce::String& text)
{
    const auto lower = text.toLowerCase();
    const int bpmPos = lower.indexOf("bpm");
    if (bpmPos < 0)
        return 0;

    int start = bpmPos - 1;
    while (start >= 0 && juce::CharacterFunctions::isWhitespace(lower[start]))
        --start;
    int end = start;
    while (start >= 0 && juce::CharacterFunctions::isDigit(lower[start]))
        --start;

    const auto value = lower.substring(start + 1, end + 1).getIntValue();
    return juce::isPositiveAndBelow(value, 301) ? value : 0;
}

juce::String SampleBrowser::parseKey(const juce::String& text)
{
    const auto lower = text.toLowerCase();
    const int keyPos = lower.indexOf("key of ");
    if (keyPos < 0)
        return {};

    auto key = text.substring(keyPos + 7).upToFirstOccurrenceOf(" ", false, false).trim();
    return key.length() <= 4 ? key : juce::String();
}

juce::String SampleBrowser::makePackId(const juce::String& name)
{
    auto id = name.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789 ");
    return id.replace(" ", "-");
}

juce::Array<juce::File> SampleBrowser::sampleRoots()
{
    juce::Array<juce::File> roots;

    auto addRoot = [&roots](const juce::File& root)
    {
        for (const auto& existing : roots)
            if (existing == root)
                return;
        roots.add(root);
    };

    auto addUpwardSamples = [&addRoot](juce::File start)
    {
        for (int i = 0; i < 5 && start != juce::File{}; ++i)
        {
            addRoot(start.getChildFile("Samples"));
            const auto parent = start.getParentDirectory();
            if (parent == start)
                break;
            start = parent;
        }
    };

    addUpwardSamples(juce::File::getCurrentWorkingDirectory());
    addUpwardSamples(juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory());
    addRoot(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile("AIDAW").getChildFile("Samples"));
    addRoot(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("AIDAW").getChildFile("Samples"));
    return roots;
}

} // namespace aidaw
