// Selbsttest für NOVA Schrottpresse
// - Latenzausgleich (trockener und bearbeiteter Weg müssen deckungsgleich sein)
// - alle Presets: Pegel, Spitzen, ungültige Werte
// - Rechenzeit, Speichern/Laden
// Aufruf:  SchrottTest [ausgabeordner] [--screenshot datei.png]
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

using namespace sp;

static juce::AudioBuffer<float> schranzLoop (double sr, double seconds)
{
    const int n = (int) (sr * seconds);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    const double beat = 60.0 / 155.0;
    juce::Random rnd (3);
    for (double t0 = 0; t0 < seconds; t0 += beat)
    {
        const int s0 = (int) (t0 * sr);
        double ph = 0;
        for (int i = 0; i < (int) (0.5 * sr) && s0 + i < n; ++i)
        {
            const double t = i / sr, f = 47 + 163 * std::exp (-t / 0.03);
            ph += 2 * juce::MathConstants<double>::pi * f / sr;
            const float v = (float) (std::sin (ph) * std::exp (-t / 0.16) * 0.75);
            b.addSample (0, s0 + i, v); b.addSample (1, s0 + i, v);
        }
        const int h0 = s0 + (int) (beat / 2 * sr);
        float lp = 0;
        for (int i = 0; i < (int) (0.12 * sr) && h0 + i < n; ++i)
        {
            const float x = rnd.nextFloat() * 2 - 1; lp += 0.3f * (x - lp);
            const float v = (x - lp) * 0.22f * std::exp (-(float) i / (float) (0.035 * sr));
            b.addSample (0, h0 + i, v); b.addSample (1, h0 + i, v * 0.9f);
        }
    }
    return b;
}

static void setAll (SchrottProcessor& p, const Values& v)
{
    for (int i = 0; i < kNumParams; ++i) p.param (i)->setValueNotifyingHost (p.param (i)->convertTo0to1 (v[(size_t) i]));
}

static juce::AudioBuffer<float> run (SchrottProcessor& p, const juce::AudioBuffer<float>& in, int block = 256)
{
    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        const int m = juce::jmin (block, out.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, m);
        p.processBlock (view, midi);
    }
    return out;
}

static double rms (const juce::AudioBuffer<float>& b, int from, int to)
{
    double a = 0;
    for (int c = 0; c < 2; ++c) for (int i = from; i < to; ++i) a += (double) b.getSample (c, i) * b.getSample (c, i);
    return std::sqrt (a / (2.0 * (to - from)));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::String outDir = argc > 1 && ! juce::String (argv[1]).startsWith ("--") ? juce::String (argv[1]) : juce::String();
    juce::String shot;
    for (int i = 1; i < argc - 1; ++i) if (juce::String (argv[i]) == "--screenshot") shot = argv[i + 1];
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "OK    " : "FEHLER", what); if (! ok) ++fails; };

    const double sr = 48000;
    SchrottProcessor p;
    p.setPlayConfigDetails (2, 2, sr, 512);
    p.prepareToPlay (sr, 512);
    const int L = p.getLatencySamples();
    std::printf ("Parameter: %d, Presets: %d, Latenz: %d Samples\n", (int) p.getParameters().size(), p.getNumPrograms(), L);
    const auto loop = schranzLoop (sr, 4.0);

    // 1) Latenzausgleich: globaler Mix 50 %, Presse-Mix 0 -> Ausgang = Eingang um L verzögert
    {
        auto v = presetValues (numPresets() - 1);
        v[gMix] = 0.5f; v[gAutoGain] = 0; v[gLimiter] = 0; v[(size_t) bp (0, bPMix)] = 0;
        setAll (p, v); p.engine.reset();
        auto out = run (p, loop);
        double err = 0, ref = 0;
        for (int i = 24000; i < loop.getNumSamples(); ++i) { const double e = out.getSample (0, i) - loop.getSample (0, i - L); err += e * e; ref += (double) loop.getSample (0, i - L) * loop.getSample (0, i - L); }
        const double db = 10 * std::log10 (err / ref + 1e-30);
        std::printf ("Null-Test: Restfehler %.1f dB\n", db);
        check (db < -60, "Trockener und bearbeiteter Weg sind deckungsgleich");
    }

    // 2) Alle Presets
    juce::WavAudioFormat wav;
    for (int i = 0; i < p.getNumPrograms(); ++i)
    {
        p.loadPreset (i); p.engine.reset(); p.updateIRsNow();
        auto out = run (p, loop);
        const int a = (int) (1.5 * sr), b = loop.getNumSamples();
        const double ri = rms (loop, a, b), ro = rms (out, a, b);
        float pk = 0, pkSteady = 0; bool bad = false;
        for (int c = 0; c < 2; ++c) for (int k = 0; k < out.getNumSamples(); ++k)
        { const float x = out.getSample (c, k); if (! std::isfinite (x)) bad = true; pk = juce::jmax (pk, std::abs (x)); if (k >= a) pkSteady = juce::jmax (pkSteady, std::abs (x)); }
        const auto name = p.getProgramName (i);
        std::printf ("  %-24s ein %.3f  aus %.3f  Spitze %.3f (eingeschwungen %.3f)%s\n", name.toRawUTF8(), ri, ro, pk, pkSteady, bad ? "  UNGUELTIG" : "");
        if (bad || ro < 0.02 || pk > 1.0f) { ++fails; std::printf ("FEHLER  %s\n", name.toRawUTF8()); }
        if (outDir.isNotEmpty())
        {
            juce::File f = juce::File (outDir).getChildFile (name.replaceCharacter (' ', '_') + ".wav");
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            if (os != nullptr)
                if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24)))
                    w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
        }
    }

    // 3) Rechenzeit des schwersten Presets
    {
        p.loadPreset (numPresets() - 2); p.engine.reset(); p.updateIRsNow();
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int k = 0; k < 3; ++k) run (p, loop, 128);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const double rt = 12000.0 / ms;
        std::printf ("Rechenzeit: 12 s Audio in %.0f ms (%.0fx Echtzeit)\n", ms, rt);
        check (rt > 4, "schnell genug für Echtzeit");
    }

    // 4) Speichern und Laden
    {
        p.loadPreset (3);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        const float drive = p.value (bp (0, bDrive));
        p.loadPreset (0);
        p.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (p.value (bp (0, bDrive)) - drive) < 1e-3f && p.getCurrentProgram() == 3, "Song-Zustand wird wiederhergestellt");
        check (p.param (bp (0, bDrive))->getText (p.param (bp (0, bDrive))->getValue(), 0).isNotEmpty(), "Parametertexte");
    }

    // 5) Oberfläche
    if (shot.isNotEmpty())
    {
        p.loadPreset (numPresets() - 2); p.updateIRsNow();
        run (p, loop);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (SchrottEditor::baseW, SchrottEditor::baseH);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (shot); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat png; png.writeImageToStream (img, os);
        std::printf ("Screenshot: %s\n", shot.toRawUTF8());
    }

    std::printf ("\n%s\n", fails ? "FEHLER gefunden" : "Alle Prüfungen bestanden");
    return fails ? 1 : 0;
}
