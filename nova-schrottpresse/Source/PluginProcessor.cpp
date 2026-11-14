#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace sp;

static juce::String U (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

SchrottProcessor::SchrottProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Eingang", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Ausgang", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SCHROTTPRESSE", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        params[(size_t) i] = apvts.getParameter (def (i).id);
        raw[(size_t) i] = apvts.getRawParameterValue (def (i).id);
        jassert (params[(size_t) i] != nullptr);
    }
    applyValues (presetValues (0));
    startTimerHz (15);
}

SchrottProcessor::~SchrottProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout SchrottProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int id = 0; id < kNumParams; ++id)
    {
        const auto& d = def (id);
        const juce::ParameterID pid { d.id, 1 };
        if (d.kind == Kind::Float)
        {
            juce::NormalisableRange<float> range (d.min, d.max,
                [id] (float, float, float n) { return fromNorm (id, n); },
                [id] (float, float, float v) { return toNorm (id, v); },
                [id] (float, float, float v) { return clampf (v, def (id).min, def (id).max); });
            auto attrs = juce::AudioParameterFloatAttributes()
                             .withStringFromValueFunction ([id] (float v, int) { return U (formatValue (id, v)); })
                             .withValueFromStringFunction ([id] (const juce::String& t) { return parseValue (id, t.toStdString()); });
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, U (d.name), range, d.def, attrs));
        }
        else if (d.kind == Kind::Choice)
        {
            juce::StringArray c;
            for (const auto& s : d.choices) c.add (U (s));
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, U (d.name), c, (int) d.def));
        }
        else layout.add (std::make_unique<juce::AudioParameterBool> (pid, U (d.name), d.def >= 0.5f));
    }
    return layout;
}

void SchrottProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.updateConvolutions (values());
    setLatencySamples (engine.getLatency());
    stereo.setSize (2, juce::jmax (1, samplesPerBlock));
}

bool SchrottProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    return in.size() <= out.size();
}

Values SchrottProcessor::values() const
{
    Values v;
    for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = raw[(size_t) i]->load (std::memory_order_relaxed);
    return v;
}

void SchrottProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    for (int c = inCh; c < outCh; ++c) buffer.clear (c, 0, n);
    const auto v = values();
    const bool cmp = compare.load();

    if (outCh >= 2 && buffer.getNumChannels() >= 2)
    {
        if (inCh == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
        engine.process (buffer, v, cmp);
    }
    else
    {
        stereo.setSize (2, n, false, false, true);
        stereo.copyFrom (0, 0, buffer, 0, 0, n);
        stereo.copyFrom (1, 0, buffer, 0, 0, n);
        engine.process (stereo, v, cmp);
        buffer.copyFrom (0, 0, stereo.getReadPointer (0), n, 0.5f);
        buffer.addFrom (0, 0, stereo, 1, 0, n, 0.5f);
    }
}

juce::AudioProcessorEditor* SchrottProcessor::createEditor() { return new SchrottEditor (*this); }

const juce::String SchrottProcessor::getProgramName (int index)
{
    if (index < 0 || index >= numPresets()) return {};
    return U (presets()[(size_t) index].name);
}

void SchrottProcessor::setValue (int id, float v)
{
    auto* p = params[(size_t) id];
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (v));
    p->endChangeGesture();
}

void SchrottProcessor::setValueInGesture (int id, float v)
{
    auto* p = params[(size_t) id];
    const float nv = p->convertTo0to1 (v);
    if (std::abs (p->getValue() - nv) > 1.0e-6f) p->setValueNotifyingHost (nv);
}

void SchrottProcessor::applyValues (const Values& v)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        auto* p = params[(size_t) i];
        const float nv = p->convertTo0to1 (v[(size_t) i]);
        if (std::abs (p->getValue() - nv) < 1.0e-6f) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost (nv);
        p->endChangeGesture();
    }
}

void SchrottProcessor::loadPreset (int index)
{
    currentPreset = (index % numPresets() + numPresets()) % numPresets();
    applyValues (presetValues (currentPreset));
    sendChangeMessage();
}

void SchrottProcessor::randomPreset()
{
    if (numPresets() < 2) return;
    int i;
    do { i = juce::Random::getSystemRandom().nextInt (numPresets()); } while (i == currentPreset);
    loadPreset (i);
}

void SchrottProcessor::randomTypes (int band)
{
    for (int c = 0; c < 4; ++c) setValue (bp (band, bT0 + c), (float) juce::Random::getSystemRandom().nextInt (numShapes()));
}

void SchrottProcessor::randomIRs (int band)
{
    for (int c = 0; c < 4; ++c) setValue (bp (band, bI0 + c), (float) juce::Random::getSystemRandom().nextInt (numIRs()));
    setValue (bp (band, bFOn), 1.0f);
}

bool SchrottProcessor::isModified() const
{
    const auto p = presetValues (currentPreset);
    const auto v = values();
    for (int i = 0; i < kNumParams; ++i)
        if (std::abs (toNorm (i, p[(size_t) i]) - toNorm (i, v[(size_t) i])) > 0.002f) return true;
    return false;
}

void SchrottProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SchrottProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    currentPreset = juce::jlimit (0, numPresets() - 1, (int) tree.getProperty ("preset", 0));
    apvts.replaceState (tree);
    sendChangeMessage();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SchrottProcessor(); }
