#pragma once

#include <JuceHeader.h>

#include "SamplePreviewSource.h"

namespace aidaw
{

struct SampleItem
{
    juce::String packId;
    juce::String packName;
    juce::String category;
    juce::String name;
    juce::File file;
    int bpm { 0 };
    juce::String key;
};

class SampleBrowser final : public juce::Component,
                            private juce::ListBoxModel,
                            private juce::TextEditor::Listener,
                            private juce::ComboBox::Listener,
                            private juce::Button::Listener,
                            private juce::ChangeListener
{
public:
    explicit SampleBrowser(SamplePreviewSource& previewSource);
    ~SampleBrowser() override;

    void rescan();
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class Row;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    juce::Component* refreshComponentForRow(int row, bool selected, juce::Component* existing) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void textEditorTextChanged(juce::TextEditor&) override;
    void comboBoxChanged(juce::ComboBox*) override;
    void buttonClicked(juce::Button*) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    void rebuildCatalog();
    void rebuildFilters();
    void applyFilter();
    void previewRow(int row);
    void beginSampleDrag(int row, juce::Component* source);

    static bool isAudioFile(const juce::File& file);
    static int parseBpm(const juce::String& text);
    static juce::String parseKey(const juce::String& text);
    static juce::String makePackId(const juce::String& name);
    static juce::Array<juce::File> sampleRoots();

    SamplePreviewSource& preview;
    juce::OwnedArray<SampleItem> samples;
    juce::Array<int> filteredRows;

    juce::Label title;
    juce::TextButton closeButton { "Close" };
    juce::TextButton refreshButton { "Refresh" };
    juce::TextEditor search;
    juce::ComboBox packFilter;
    juce::ComboBox categoryFilter;
    juce::ListBox list;
    juce::Label emptyState;

    juce::StringArray packNames;
    juce::StringArray categoryNames;
};

} // namespace aidaw
