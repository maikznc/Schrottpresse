// NOVA Schrottpresse: Audio-Engine
// Multiband (1-3 Bänder) -> pro Band Presse (4 Verzerrer im XY-Pad, 2x Oversampling) und Faltung (4 IRs im XY-Pad)
// -> Kreisch-Filter -> Auto-Gain, Mix, Limiter. Hüllkurvenfolger moduliert Drive, XY-Ziel und Cutoff.
#pragma once

#include <juce_dsp/juce_dsp.h>
#include "dsp/Presets.h"

namespace sp {

// Ringpuffer für Spektrum und Anzeige (Audio-Thread schreibt, Oberfläche liest)
class ScopeFifo
{
public:
    static constexpr int size = 8192, mask = size - 1;
    void push (const float* l, const float* r, int n)
    {
        int w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) data[(size_t) ((w + i) & mask)].store (0.5f * (l[i] + r[i]), std::memory_order_relaxed);
        writePos.store ((w + n) & mask, std::memory_order_release);
    }
    void read (float* out, int n) const
    {
        const int w = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i) out[i] = data[(size_t) ((w - n + i) & mask)].load (std::memory_order_relaxed);
    }
private:
    std::array<std::atomic<float>, size> data {};
    std::atomic<int> writePos { 0 };
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    void copyCoefs (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }
    void setLP (float f, float q, float sr) { rbj (0, f, q, 0, sr); }
    void setHP (float f, float q, float sr) { rbj (1, f, q, 0, sr); }
    void setLowShelf (float f, float db, float sr) { rbj (2, f, 0.7071f, db, sr); }
    void setHighShelf (float f, float db, float sr) { rbj (3, f, 0.7071f, db, sr); }
    void rbj (int type, float f, float q, float db, float sr)
    {
        const float w = 2 * kPi * clampf (f, 5.0f, sr * 0.49f) / sr, cs = std::cos (w), sn = std::sin (w);
        float B0, B1, B2, A0, A1, A2;
        if (type < 2)
        {
            const float al = sn / (2 * q);
            if (type == 0) { B0 = (1 - cs) / 2; B1 = 1 - cs; B2 = B0; }
            else           { B0 = (1 + cs) / 2; B1 = -(1 + cs); B2 = B0; }
            A0 = 1 + al; A1 = -2 * cs; A2 = 1 - al;
        }
        else
        {
            const float A = std::pow (10.0f, db / 40), al = sn / 2 * std::sqrt (2.0f), sa = 2 * std::sqrt (A) * al;
            if (type == 2)
            {
                B0 = A * ((A + 1) - (A - 1) * cs + sa); B1 = 2 * A * ((A - 1) - (A + 1) * cs); B2 = A * ((A + 1) - (A - 1) * cs - sa);
                A0 = (A + 1) + (A - 1) * cs + sa; A1 = -2 * ((A - 1) + (A + 1) * cs); A2 = (A + 1) + (A - 1) * cs - sa;
            }
            else
            {
                B0 = A * ((A + 1) + (A - 1) * cs + sa); B1 = -2 * A * ((A - 1) + (A + 1) * cs); B2 = A * ((A + 1) + (A - 1) * cs - sa);
                A0 = (A + 1) - (A - 1) * cs + sa; A1 = 2 * ((A - 1) - (A + 1) * cs); A2 = (A + 1) - (A - 1) * cs - sa;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
};

struct Smooth
{
    float cur = 0, target = 0, coef = 0.001f;
    void init (float v) { cur = target = v; }
    void setTau (float tau, float sr) { coef = 1.0f - std::exp (-1.0f / (std::max (tau, 1.0e-4f) * sr)); }
    inline float next() { cur += (target - cur) * coef; return cur; }
};

class Engine
{
public:
    Engine();
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    int getLatency() const { return latency; }

    // Audio-Thread: Puffer mit 2 Kanälen, Werte aller Parameter
    void process (juce::AudioBuffer<float>& buffer, const Values& v, bool compare);

    // Message-Thread: Impulsantworten neu mischen, wenn sich Räume oder XY geändert haben
    void updateConvolutions (const Values& v);
    const std::vector<float>& irPreview (int band) const { return preview[(size_t) band]; }

    std::atomic<float> uiEnv { 0 }, uiIn { 0 }, uiOut { 0 };
    ScopeFifo inFifo, outFifo;

private:
    struct Band
    {
        // Presse
        Biquad tiltLo[2], tiltHi[2], dc[2];
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        std::vector<float> dryRing[2]; int dryPos = 0;
        Smooth drive, envRatio, wet, dry, w0[4], wd[4];
        float style[4] { 0.5f, 0.5f, 0.5f, 0.5f };
        float tiltDb = 1000;
        // Faltung
        juce::dsp::Convolution conv { juce::dsp::Convolution::NonUniform { 512 } };
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> pre { 48000 };
        Smooth fWet, fDry, width, preDelay;
        std::string irKey;
        std::atomic<bool> convLoaded { false };
        // Band
        Smooth gain;
    };

    void processChunk (float* L, float* R, int n, const Values& v, bool compare);
    void presse (int b, float* const* ch, int n, const Values& v);
    void faltung (int b, float* const* ch, int n, const Values& v);

    double sr = 48000;
    int maxBlock = 512, latency = 0;
    std::array<Band, kBands> bands;
    juce::AudioBuffer<float> bandBuf[kBands], dryTmp, fw, globalDry;
    std::vector<float> envArr, dArr, wArr[4], wetArr, dryArr;
    std::vector<float> globalRing[2]; int globalPos = 0;

    // Crossover (LR4) pro Kanal
    Biquad lp1[2][2], hp1[2][2], lp2[2][2], hp2[2][2];
    float f1 = 150, f2 = 2500;

    // Hüllkurve
    Biquad envLP[2]; float envSmoothSet = -1, envNow = 0;

    // Kreisch-Filter (TPT-SVF)
    float svf1[2] {}, svf2[2] {};
    Smooth fOnMix;

    // Ausgang
    Smooth inGain, wetMix, dryMix, outGain, agApplied;
    float ag = 1; double agIn = 0, agWet = 0; int agCount = 0, agWindow = 5760;
    float grSm = 0, limAtt = 0.9f, limRel = 0.999f;
    std::array<std::vector<float>, kBands> preview;
};

} // namespace sp
