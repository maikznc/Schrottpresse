#pragma once

#include "PluginProcessor.h"

class MainPanel;
class SchrottLnf;

class SchrottEditor : public juce::AudioProcessorEditor
{
public:
    explicit SchrottEditor (SchrottProcessor&);
    ~SchrottEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override {}

    static constexpr int baseW = 1240, baseH = 840;

private:
    std::unique_ptr<SchrottLnf> lnf;
    std::unique_ptr<MainPanel> panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SchrottEditor)
};
