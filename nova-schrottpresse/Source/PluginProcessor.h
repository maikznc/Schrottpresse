#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class SchrottProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster,
                         private juce::Timer
{
public:
    SchrottProcessor();
    ~SchrottProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Schrottpresse"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return sp::numPresets(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ----- für die Oberfläche -----
    juce::RangedAudioParameter* param (int id) const { return params[(size_t) id]; }
    float value (int id) const { return raw[(size_t) id]->load(); }
    sp::Values values() const;
    void setValue (int id, float v);              // mit Geste, für Klicks
    void setValueInGesture (int id, float v);     // innerhalb begin/end
    void loadPreset (int index);
    void randomPreset();
    void randomTypes (int band);
    void randomIRs (int band);
    bool isModified() const;
    void updateIRsNow() { engine.updateConvolutions (values()); }

    juce::AudioProcessorValueTreeState apvts;
    sp::Engine engine;
    std::atomic<bool> compare { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override { engine.updateConvolutions (values()); }
    void applyValues (const sp::Values& v);

    std::array<juce::RangedAudioParameter*, sp::kNumParams> params {};
    std::array<std::atomic<float>*, sp::kNumParams> raw {};
    juce::AudioBuffer<float> stereo;
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SchrottProcessor)
};
