#include "Engine.h"

namespace sp {

static inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
static inline float softClip (float x)
{
    if (! std::isfinite (x)) return 0.0f;
    const float a = std::fabs (x);
    if (a <= 0.9f) return x;
    const float y = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
    return x < 0 ? -y : y;
}

Engine::Engine()
{
    jassert ((int) params().size() == kNumParams);
    (void) ShapeTables::get();   // Tabellen einmal vorab berechnen
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (16, juce::jmin (maxBlockSize, 2048));
    const float fsr = (float) sr;
    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };

    for (auto& b : bandBuf) b.setSize (2, maxBlock);
    dryTmp.setSize (2, maxBlock); fw.setSize (2, maxBlock); globalDry.setSize (2, maxBlock);
    envArr.assign ((size_t) maxBlock, 0); dArr.assign ((size_t) maxBlock, 0); wetArr.assign ((size_t) maxBlock, 0); dryArr.assign ((size_t) maxBlock, 0);
    for (auto& w : wArr) w.assign ((size_t) maxBlock, 0);

    for (auto& b : bands)
    {
        b.os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        b.os->initProcessing ((size_t) maxBlock);
        latency = (int) std::lround (b.os->getLatencyInSamples());
        for (auto& r : b.dryRing) r.assign ((size_t) std::max (1, latency), 0.0f);
        b.dryPos = 0;
        b.conv.prepare (spec);
        b.pre.prepare (spec);
        b.pre.setMaximumDelayInSamples ((int) (0.25 * sr) + 8);
        for (auto* s : { &b.drive, &b.envRatio, &b.wet, &b.dry, &b.fWet, &b.fDry, &b.width, &b.gain })
            s->setTau (0.02f, fsr);
        for (int i = 0; i < 4; ++i) { b.w0[i].setTau (0.02f, fsr); b.wd[i].setTau (0.02f, fsr); }
        b.preDelay.setTau (0.05f, fsr);
        b.irKey.clear(); b.convLoaded = false;
        for (auto& f : b.dc) f.setHP (18.0f, 0.7071f, fsr);
        b.tiltDb = 1000;
    }
    for (auto& r : globalRing) r.assign ((size_t) std::max (1, latency), 0.0f);
    globalPos = 0;

    for (auto* s : { &fOnMix, &inGain, &wetMix, &dryMix, &outGain }) s->setTau (0.02f, fsr);
    agApplied.setTau (0.15f, fsr);
    agWindow = (int) (0.12 * sr);
    limAtt = std::exp (-1.0f / (0.0005f * fsr));
    limRel = std::exp (-1.0f / (0.08f * fsr));
    envSmoothSet = -1;
    reset();
}

void Engine::reset()
{
    for (auto& b : bands)
    {
        for (int c = 0; c < 2; ++c) { b.tiltLo[c].reset(); b.tiltHi[c].reset(); b.dc[c].reset(); std::fill (b.dryRing[c].begin(), b.dryRing[c].end(), 0.0f); }
        if (b.os) b.os->reset();
        b.conv.reset(); b.pre.reset();
        b.wet.init (0); b.dry.init (1); b.fWet.init (0); b.fDry.init (1); b.gain.init (1);
        b.drive.init (dbToGain (12)); b.envRatio.init (0); b.width.init (1); b.preDelay.init (0);
        for (int i = 0; i < 4; ++i) { b.w0[i].init (i == 0 ? 1.0f : 0.0f); b.wd[i].init (0); }
    }
    for (auto& r : globalRing) std::fill (r.begin(), r.end(), 0.0f);
    for (int c = 0; c < 2; ++c)
    {
        for (int k = 0; k < 2; ++k) { lp1[c][k].reset(); hp1[c][k].reset(); lp2[c][k].reset(); hp2[c][k].reset(); }
        envLP[c].reset(); svf1[c] = svf2[c] = 0;
    }
    fOnMix.init (0); inGain.init (1); wetMix.init (1); dryMix.init (0); outGain.init (1); agApplied.init (1);
    ag = 1; agIn = agWet = 0; agCount = 0; grSm = 0; envNow = 0;
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Values& v, bool compare)
{
    jassert (buffer.getNumChannels() >= 2);
    const int n = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    float pin = 0, pout = 0;
    for (int i = 0; i < n; ++i) pin = std::max (pin, std::max (std::fabs (L[i]), std::fabs (R[i])));
    for (int pos = 0; pos < n; pos += maxBlock)
    {
        const int m = std::min (maxBlock, n - pos);
        inFifo.push (L + pos, R + pos, m);
        processChunk (L + pos, R + pos, m, v, compare);
        outFifo.push (L + pos, R + pos, m);
    }
    for (int i = 0; i < n; ++i) pout = std::max (pout, std::max (std::fabs (L[i]), std::fabs (R[i])));
    uiIn.store (pin); uiOut.store (pout); uiEnv.store (envNow);
}

void Engine::processChunk (float* L, float* R, int n, const Values& v, bool compare)
{
    const float fsr = (float) sr;
    const int nb = std::clamp ((int) v[gBands] + 1, 1, 3);

    // Eingang
    inGain.target = dbToGain (v[gInput]);
    for (int i = 0; i < n; ++i) { const float g = inGain.next(); L[i] *= g; R[i] *= g; }

    // Trockenes Signal mit Latenzausgleich
    for (int c = 0; c < 2; ++c)
    {
        const float* src = c == 0 ? L : R;
        float* dst = globalDry.getWritePointer (c);
        auto& ring = globalRing[(size_t) c];
        int p = globalPos;
        for (int i = 0; i < n; ++i) { dst[i] = ring[(size_t) p]; ring[(size_t) p] = src[i]; if (++p >= (int) ring.size()) p = 0; }
        if (c == 1) globalPos = p;
    }

    // Hüllkurvenfolger
    if (std::fabs (envSmoothSet - v[gEnvSmooth]) > 1e-6f)
    {
        envSmoothSet = v[gEnvSmooth];
        for (auto& f : envLP) f.setLP (1.0f / (2 * kPi * envSmoothSet), 0.7071f, fsr);
    }
    const float eg = dbToGain (v[gEnvSens]) * 0.5f;
    for (int i = 0; i < n; ++i)
    {
        const float ul = clampf (L[i] * eg, -1, 1), ur = clampf (R[i] * eg, -1, 1);
        const float el = clampf (envLP[0].tick (std::min (1.0f, 2 * std::fabs (ul))), 0, 1);
        const float er = clampf (envLP[1].tick (std::min (1.0f, 2 * std::fabs (ur))), 0, 1);
        envArr[(size_t) i] = 0.5f * (el + er);
    }
    envNow = envArr[(size_t) n - 1];

    // Bänder aufteilen
    const float tf1 = clampf (v[gX1], 30, fsr * 0.45f);
    const float tf2 = clampf (std::max (v[gX2], tf1 * 1.5f), 30, fsr * 0.45f);
    const float k = 1.0f - std::exp (-(float) n / (0.02f * fsr));
    f1 += (tf1 - f1) * k; f2 += (tf2 - f2) * k;
    for (int c = 0; c < 2; ++c)
        for (int s = 0; s < 2; ++s)
        {
            lp1[c][s].setLP (f1, 0.7071f, fsr); hp1[c][s].setHP (f1, 0.7071f, fsr);
            lp2[c][s].setLP (f2, 0.7071f, fsr); hp2[c][s].setHP (f2, 0.7071f, fsr);
        }
    for (int c = 0; c < 2; ++c)
    {
        const float* src = c == 0 ? L : R;
        float* b0 = bandBuf[0].getWritePointer (c);
        float* b1 = bandBuf[1].getWritePointer (c);
        float* b2 = bandBuf[2].getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            const float x = src[i];
            if (nb == 1) { b0[i] = x; continue; }
            b0[i] = lp1[c][1].tick (lp1[c][0].tick (x));
            const float h = hp1[c][1].tick (hp1[c][0].tick (x));
            if (nb == 2) { b1[i] = h; continue; }
            b1[i] = lp2[c][1].tick (lp2[c][0].tick (h));
            b2[i] = hp2[c][1].tick (hp2[c][0].tick (h));
        }
    }

    // Bänder bearbeiten und summieren
    bool anySolo = false;
    for (int b = 0; b < nb; ++b) anySolo = anySolo || v[(size_t) bp (b, bSolo)] >= 0.5f;
    std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
    for (int b = 0; b < nb; ++b)
    {
        float* const ch[2] = { bandBuf[b].getWritePointer (0), bandBuf[b].getWritePointer (1) };
        if ((int) v[(size_t) bp (b, bOrder)] == 0) { presse (b, ch, n, v); faltung (b, ch, n, v); }
        else                                         { faltung (b, ch, n, v); presse (b, ch, n, v); }
        auto& band = bands[(size_t) b];
        const bool silent = v[(size_t) bp (b, bMute)] >= 0.5f || (anySolo && v[(size_t) bp (b, bSolo)] < 0.5f);
        band.gain.target = silent ? 0.0f : dbToGain (v[(size_t) bp (b, bLevel)]);
        for (int i = 0; i < n; ++i) { const float g = band.gain.next(); L[i] += ch[0][i] * g; R[i] += ch[1][i] * g; }
    }

    // Kreisch-Filter
    fOnMix.target = v[gFOn] >= 0.5f ? 1.0f : 0.0f;
    if (fOnMix.target > 0 || fOnMix.cur > 1e-5f)
    {
        const int type = (int) v[gFType];
        const float res = v[gFRes];
        const float q = type == 1 ? 0.7f + res * 25.0f : std::pow (10.0f, (-3.0f + res * 33.0f) / 20.0f);
        const float kq = 1.0f / q;
        const float pre = 1.0f + v[gFScream] * 14.0f, post = 1.0f / std::sqrt (pre) * (type == 1 ? 1.6f : 1.0f);
        float a1 = 1, a2 = 0, a3 = 0;
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0)
            {
                const float fc = clampf (v[gFCut] * std::exp2 (envArr[(size_t) i] * v[gFEnv] * 4.0f), 20.0f, fsr * 0.45f);
                const float g = std::tan (kPi * fc / fsr);
                a1 = 1.0f / (1.0f + g * (g + kq)); a2 = g * a1; a3 = g * a2;
            }
            const float mix = fOnMix.next();
            float* io[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                const float v3 = x - svf2[c];
                const float v1 = a1 * svf1[c] + a2 * v3;
                const float v2 = svf2[c] + a2 * svf1[c] + a3 * v3;
                svf1[c] = 2 * v1 - svf1[c]; svf2[c] = 2 * v2 - svf2[c];
                float y = type == 0 ? v2 : (type == 2 ? x - kq * v1 - v2 : kq * v1);
                y = std::tanh (clampf (y * pre, -4.0f, 4.0f)) * post;
                io[c][i] = x * (1 - mix) + y * mix;
            }
        }
    }

    // Auto-Gain messen
    const bool agOn = v[gAutoGain] >= 0.5f;
    for (int i = 0; i < n; ++i)
    {
        const float* gd0 = globalDry.getReadPointer (0); const float* gd1 = globalDry.getReadPointer (1);
        agIn += 0.5 * ((double) gd0[i] * gd0[i] + (double) gd1[i] * gd1[i]);
        agWet += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]);
        if (++agCount >= agWindow)
        {
            const double inR = std::sqrt (agIn / agCount), wR = std::sqrt (agWet / agCount);
            if (agOn && ! compare && inR > 1e-4 && wR > 1e-5)
                ag = ag * 0.75f + (float) clampf ((float) (inR / wR), 0.05f, 4.0f) * 0.25f;
            agIn = agWet = 0; agCount = 0;
        }
    }
    agApplied.target = agOn ? ag : 1.0f;

    // Mix, Ausgang, Limiter
    wetMix.target = compare ? 0.0f : v[gMix];
    dryMix.target = compare ? 1.0f : 1.0f - v[gMix];
    outGain.target = compare ? 1.0f : dbToGain (v[gOutput]);
    const bool lim = v[gLimiter] >= 0.5f;
    const float* d0 = globalDry.getReadPointer (0); const float* d1 = globalDry.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float a = agApplied.next(), w = wetMix.next(), d = dryMix.next(), o = outGain.next();
        float l = (L[i] * a * w + d0[i] * d) * o, r = (R[i] * a * w + d1[i] * d) * o;
        if (lim)
        {
            const float pk = std::max (std::fabs (l), std::fabs (r));
            const float over = 20.0f * std::log10 (pk + 1e-9f) + 1.0f;
            const float target = over > 0 ? -over * (1.0f - 1.0f / 20.0f) : 0.0f;
            grSm = target < grSm ? limAtt * grSm + (1 - limAtt) * target : limRel * grSm + (1 - limRel) * target;
            const float g = dbToGain (grSm);
            l *= g; r *= g;
        }
        L[i] = softClip (l); R[i] = softClip (r);
    }
}

void Engine::presse (int b, float* const* ch, int n, const Values& v)
{
    auto& band = bands[(size_t) b];
    const float fsr = (float) sr;
    const bool on = v[(size_t) bp (b, bPOn)] >= 0.5f;
    const float mix = v[(size_t) bp (b, bPMix)];
    band.wet.target = on ? mix : 0.0f;
    band.dry.target = on ? 1.0f - mix : 1.0f;

    // Trockenes Signal um die Oversampling-Latenz verzögern
    for (int c = 0; c < 2; ++c)
    {
        auto& ring = band.dryRing[c];
        float* dst = dryTmp.getWritePointer (c);
        int p = band.dryPos;
        for (int i = 0; i < n; ++i) { dst[i] = ring[(size_t) p]; ring[(size_t) p] = ch[c][i]; if (++p >= (int) ring.size()) p = 0; }
        if (c == 1) band.dryPos = p;
    }

    const bool active = on || band.wet.cur > 1e-4f;
    if (active)
    {
        // Tilt vor der Verzerrung
        const float tilt = v[(size_t) bp (b, bTilt)];
        if (std::fabs (tilt - band.tiltDb) > 0.01f)
        {
            band.tiltDb = tilt;
            for (int c = 0; c < 2; ++c) { band.tiltLo[c].setLowShelf (700, -tilt, fsr); band.tiltHi[c].setHighShelf (700, tilt, fsr); }
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i) ch[c][i] = band.tiltHi[c].tick (band.tiltLo[c].tick (ch[c][i]));

        // Modulationswerte pro Sample
        const float driveDb = v[(size_t) bp (b, bDrive)];
        band.drive.target = dbToGain (driveDb);
        band.envRatio.target = dbToGain (v[(size_t) bp (b, bEnvDrive)]) - 1.0f;
        const auto w0 = weights (v[(size_t) bp (b, bX)], v[(size_t) bp (b, bY)]);
        const auto w1 = weights (v[(size_t) bp (b, bX2)], v[(size_t) bp (b, bY2)]);
        const float exy = v[(size_t) bp (b, bEnvXY)];
        for (int c = 0; c < 4; ++c) { band.w0[c].target = w0[(size_t) c]; band.wd[c].target = exy * (w1[(size_t) c] - w0[(size_t) c]); }
        float wmax[4] { 0, 0, 0, 0 };
        for (int i = 0; i < n; ++i)
        {
            const float e = envArr[(size_t) i];
            dArr[(size_t) i] = band.drive.next() * std::max (0.0f, 1.0f + e * band.envRatio.next());
            for (int c = 0; c < 4; ++c)
            {
                const float w = band.w0[c].next() + e * band.wd[c].next();
                wArr[c][(size_t) i] = w; wmax[c] = std::max (wmax[c], std::fabs (w));
            }
        }

        // Kennlinien für diesen Block
        const auto& T = ShapeTables::get();
        const float* tA[4]; const float* tB[4]; float tg[4];
        for (int c = 0; c < 4; ++c)
        {
            const int type = std::clamp ((int) v[(size_t) bp (b, bT0 + c)], 0, numShapes() - 1);
            band.style[c] += (v[(size_t) bp (b, bS0 + c)] - band.style[c]) * std::min (1.0f, (float) n / (0.03f * fsr));
            const float spos = clampf (band.style[c], 0, 1) * (kStyles - 1);
            const int j = std::min (kStyles - 2, (int) spos);
            tA[c] = T.table (type, j); tB[c] = T.table (type, j + 1); tg[c] = spos - (float) j;
        }

        // 2x Oversampling um die Verzerrung
        juce::dsp::AudioBlock<float> blk (ch, 2, (size_t) n);
        auto up = band.os->processSamplesUp (blk);
        const int un = (int) up.getNumSamples();
        for (int c2 = 0; c2 < 2; ++c2)
        {
            float* x = up.getChannelPointer ((size_t) c2);
            for (int i = 0; i < un; ++i)
            {
                const size_t k = (size_t) (i >> 1);
                const float in = x[i] * dArr[k];
                float y = 0;
                for (int c = 0; c < 4; ++c)
                {
                    if (wmax[c] < 1e-5f) continue;
                    float s = ShapeTables::lookup (tA[c], in);
                    if (tg[c] > 0.001f) s += (ShapeTables::lookup (tB[c], in) - s) * tg[c];
                    y += wArr[c][k] * s;
                }
                x[i] = y;
            }
        }
        band.os->processSamplesDown (blk);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) ch[c][i] = band.dc[c].tick (ch[c][i]);
    }

    const float* d0 = dryTmp.getReadPointer (0); const float* d1 = dryTmp.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float w = band.wet.next(), d = band.dry.next();
        ch[0][i] = (active ? ch[0][i] * w : 0.0f) + d0[i] * d;
        ch[1][i] = (active ? ch[1][i] * w : 0.0f) + d1[i] * d;
    }
}

void Engine::faltung (int b, float* const* ch, int n, const Values& v)
{
    auto& band = bands[(size_t) b];
    const bool on = v[(size_t) bp (b, bFOn)] >= 0.5f && band.convLoaded;
    const float mix = v[(size_t) bp (b, bFMix)];
    band.fWet.target = on ? mix : 0.0f;
    band.fDry.target = on ? 1.0f - mix : 1.0f;
    band.width.target = v[(size_t) bp (b, bWidth)];
    band.preDelay.target = v[(size_t) bp (b, bPre)] * (float) sr;

    if (! on && band.fWet.cur < 1e-5f)
    {
        for (int i = 0; i < n; ++i) { band.fWet.next(); band.fDry.next(); band.width.next(); band.preDelay.next(); }
        return;
    }

    float* w0 = fw.getWritePointer (0); float* w1 = fw.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        band.pre.setDelay (std::max (0.0f, band.preDelay.next()));
        band.pre.pushSample (0, ch[0][i]); band.pre.pushSample (1, ch[1][i]);
        w0[i] = band.pre.popSample (0); w1[i] = band.pre.popSample (1);
    }
    float* wp[2] = { w0, w1 };
    juce::dsp::AudioBlock<float> blk (wp, 2, (size_t) n);
    band.conv.process (juce::dsp::ProcessContextReplacing<float> (blk));
    for (int i = 0; i < n; ++i)
    {
        const float wd = band.width.next(), a = 0.5f + 0.5f * wd, c = 0.5f - 0.5f * wd;
        const float l = w0[i], r = w1[i];
        const float wl = a * l + c * r, wr = c * l + a * r;
        const float wet = band.fWet.next(), dry = band.fDry.next();
        ch[0][i] = ch[0][i] * dry + wl * wet;
        ch[1][i] = ch[1][i] * dry + wr * wet;
    }
}

void Engine::updateConvolutions (const Values& v)
{
    const int nb = std::clamp ((int) v[gBands] + 1, 1, 3);
    for (int b = 0; b < kBands; ++b)
    {
        auto& band = bands[(size_t) b];
        if (b >= nb || v[(size_t) bp (b, bFOn)] < 0.5f) continue;
        std::array<int, 4> ids;
        std::string key;
        for (int c = 0; c < 4; ++c) { ids[(size_t) c] = std::clamp ((int) v[(size_t) bp (b, bI0 + c)], 0, numIRs() - 1); key += std::to_string (ids[(size_t) c]) + ","; }
        const float x = std::round (v[(size_t) bp (b, bFX)] * 50) / 50, y = std::round (v[(size_t) bp (b, bFY)] * 50) / 50;
        key += std::to_string ((int) (x * 50)) + "," + std::to_string ((int) (y * 50)) + "@" + std::to_string ((int) sr);
        if (key == band.irKey) continue;
        band.irKey = key;
        auto ir = IrLibrary::shared().blend (ids, x, y, (float) sr);
        juce::AudioBuffer<float> buf (2, (int) ir[0].size());
        buf.copyFrom (0, 0, ir[0].data(), (int) ir[0].size());
        buf.copyFrom (1, 0, ir[1].data(), (int) ir[1].size());
        band.conv.loadImpulseResponse (std::move (buf), sr, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        band.convLoaded = true;
        preview[(size_t) b] = std::move (ir[0]);
    }
}

} // namespace sp
