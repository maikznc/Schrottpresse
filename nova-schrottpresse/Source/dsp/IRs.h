// NOVA Schrottpresse: 16 Impulsantworten (Räume, Kisten, Lautsprecher, Effekte)
// Alle werden beim Start rechnerisch erzeugt, es werden keine Audiodateien mitgeliefert.
#pragma once

#include "Shapes.h"
#include <cstdint>
#include <functional>
#include <map>

namespace sp {

struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed ? seed : 1u) {}
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) ((double) s / 4294967296.0); }
};

using Stereo = std::array<std::vector<float>, 2>;

struct Bq { float b0, b1, b2, a1, a2; };
inline Bq bq (char type, float f, float q, float db, float sr)
{
    const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * std::min (f, sr * 0.45f) / sr;
    const float cs = std::cos (w), sn = std::sin (w), al = sn / (2 * q);
    float b0, b1, b2, a0, a1, a2;
    if (type == 'l')      { b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = b0; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; }
    else if (type == 'h') { b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = b0; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; }
    else                  { b0 = 1 + al * A; b1 = -2 * cs; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cs; a2 = 1 - al / A; }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}
inline void filt (std::vector<float>& x, const Bq& c)
{
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (auto& v : x)
    {
        const float xi = v, y = c.b0 * xi + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
        x2 = x1; x1 = xi; y2 = y1; y1 = y; v = y;
    }
}
inline Stereo stereo (size_t len) { Stereo s; s[0].assign (len, 0.0f); s[1].assign (len, 0.0f); return s; }

struct RoomOpt { uint32_t seed; float len, rt, bright, damp; int early; float erTime, hp; };
inline Stereo room (float sr, RoomOpt o)
{
    const size_t len = (size_t) (sr * o.len);
    auto out = stereo (len);
    for (int ch = 0; ch < 2; ++ch)
    {
        Rng r (o.seed + (uint32_t) ch * 7919u);
        auto& d = out[(size_t) ch];
        float lp = 0;
        for (size_t i = 0; i < len; ++i)
        {
            const float t = (float) i / sr, n = (r.next() * 2 - 1) * std::exp (-6.91f * t / o.rt);
            const float a = clampf (o.bright - o.damp * t / o.rt, 0.05f, 1.0f);
            lp += a * (n - lp); d[i] = lp;
        }
        for (int k = 0; k < o.early; ++k)
        {
            const size_t idx = (size_t) (sr * (0.003f + r.next() * o.erTime));
            if (idx < len) d[idx] += (r.next() < 0.5f ? -1.0f : 1.0f) * (0.5f + r.next() * 0.6f) * std::exp (-(float) k * 0.22f);
        }
        if (o.hp > 0) filt (d, bq ('h', o.hp, 0.7f, 0, sr));
        const size_t fi = (size_t) (sr * 0.002f);
        for (size_t i = 0; i < fi && i < len; ++i) d[i] *= (float) i / (float) fi;
    }
    return out;
}

struct Mode { float f, dec, amp; };
inline Stereo modal (float sr, uint32_t seed, float lenS, std::vector<Mode> modes, float noise = 0.6f, float burst = 0.003f, float lp = 0)
{
    const size_t len = (size_t) (sr * lenS);
    auto out = stereo (len);
    for (int ch = 0; ch < 2; ++ch)
    {
        Rng r (seed + (uint32_t) ch);
        auto& d = out[(size_t) ch];
        for (const auto& m : modes)
        {
            const float ph = r.next() * 6.28f, w = 2 * kPi * m.f * (1 + (ch ? 0.006f : 0.0f)) / sr;
            for (size_t i = 0; i < len; ++i) d[i] += m.amp * std::exp (-(float) i / (m.dec * sr)) * std::sin (w * (float) i + ph);
        }
        const size_t nb = (size_t) (sr * burst);
        for (size_t i = 0; i < nb && i < len; ++i) d[i] += (r.next() * 2 - 1) * (1 - (float) i / (float) nb) * noise;
        if (lp > 0) filt (d, bq ('l', lp, 0.7f, 0, sr));
    }
    return out;
}

struct Peak { float f, q, g; };
inline Stereo speaker (float sr, uint32_t seed, float hp, float lp, std::vector<Peak> peaks, float lenS = 0.05f)
{
    const size_t len = (size_t) (sr * lenS);
    auto out = stereo (len);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& d = out[(size_t) ch];
        d[0] = 1;
        Rng r (seed + (uint32_t) ch);
        const size_t nn = (size_t) std::min ((float) len, sr * 0.004f);
        for (size_t i = 1; i < nn; ++i) d[i] = (r.next() * 2 - 1) * 0.08f * (1 - (float) i / (sr * 0.004f));
        for (int k = 0; k < 2; ++k) { filt (d, bq ('h', hp, 0.75f, 0, sr)); filt (d, bq ('l', lp, 0.75f, 0, sr)); }
        for (const auto& p : peaks) filt (d, bq ('p', p.f * (ch ? 1.02f : 1.0f), p.q, p.g, sr));
    }
    return out;
}

inline Stereo comb (float sr, uint32_t seed, float lenS, float period, float g, float lp, float hp, bool alt = false)
{
    const size_t len = (size_t) (sr * lenS);
    auto out = stereo (len);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& d = out[(size_t) ch];
        const size_t p = std::max<size_t> (1, (size_t) (sr * period * (ch ? 1.03f : 1.0f)));
        Rng r (seed + (uint32_t) ch);
        int k = 0;
        for (size_t i = 0; i < len; i += p, ++k) d[i] += std::pow (g, (float) k) * ((k % 2) && alt ? -1.0f : 1.0f);
        for (size_t i = 0; i < len; ++i) d[i] += (r.next() * 2 - 1) * 0.04f * std::exp (-(float) i / (sr * 0.05f));
        filt (d, bq ('l', lp, 0.7f, 0, sr)); filt (d, bq ('h', hp, 0.7f, 0, sr));
    }
    return out;
}

inline Stereo sparse (float sr, uint32_t seed, float lenS, float density, float decay, float hp, float lp = 9000)
{
    const size_t len = (size_t) (sr * lenS);
    auto out = stereo (len);
    for (int ch = 0; ch < 2; ++ch)
    {
        Rng r (seed + (uint32_t) ch * 31u);
        auto& d = out[(size_t) ch];
        const int n = (int) (density * lenS);
        for (int k = 0; k < n; ++k)
        {
            const float a = r.next(), b = r.next();
            const size_t i = std::min (len - 1, (size_t) (a * b * (float) len));
            d[i] += (r.next() * 2 - 1) * std::exp (-(float) i / (sr * decay));
        }
        filt (d, bq ('h', hp, 0.7f, 0, sr)); filt (d, bq ('l', lp, 0.7f, 0, sr));
    }
    return out;
}

inline Stereo reversed (Stereo s)
{
    for (auto& d : s)
    {
        std::reverse (d.begin(), d.end());
        const size_t n = d.size() * 3 / 100;
        for (size_t i = 0; i < n; ++i) d[d.size() - 1 - i] *= (float) i / (float) n;
    }
    return s;
}

inline void addInto (Stereo& a, const Stereo& b, float gain)
{
    for (int c = 0; c < 2; ++c)
        for (size_t i = 0; i < std::min (a[(size_t) c].size(), b[(size_t) c].size()); ++i) a[(size_t) c][i] += b[(size_t) c][i] * gain;
}

struct IrType { const char* id; const char* name; const char* cat; std::function<Stereo (float)> gen; };

inline const std::vector<IrType>& irTypes()
{
    static const std::vector<IrType> list = {
        { "bunker", "Bunker", "Räume", [] (float sr) { return room (sr, { 11, 1.0f, 0.9f, 0.85f, 0.5f, 10, 0.03f, 60 }); } },
        { "lagerhalle", "Lagerhalle", "Räume", [] (float sr) { return room (sr, { 12, 2.6f, 2.8f, 0.6f, 0.45f, 6, 0.09f, 80 }); } },
        { "treppenhaus", "Treppenhaus", "Räume", [] (float sr) { auto a = room (sr, { 13, 1.6f, 1.5f, 0.95f, 0.3f, 4, 0.02f, 150 }); addInto (a, comb (sr, 3, 1.6f, 0.021f, 0.86f, 7000, 300), 0.9f); return a; } },
        { "fliesenklo", "Fliesenklo", "Räume", [] (float sr) { return room (sr, { 14, 0.7f, 0.6f, 1.0f, 0.2f, 14, 0.012f, 200 }); } },
        { "pappkarton", "Pappkarton", "Kisten", [] (float sr) { return modal (sr, 21, 0.25f, { { 180, 0.05f, 1 }, { 310, 0.04f, 0.7f }, { 520, 0.03f, 0.5f }, { 790, 0.02f, 0.3f } }, 0.6f, 0.003f, 3500); } },
        { "blechdose", "Blechdose", "Kisten", [] (float sr) { return modal (sr, 22, 0.6f, { { 1250, 0.18f, 1 }, { 2210, 0.15f, 0.8f }, { 3480, 0.12f, 0.6f }, { 5100, 0.1f, 0.4f }, { 7300, 0.08f, 0.3f } }, 0.4f); } },
        { "muelltonne", "Mülltonne", "Kisten", [] (float sr) { return modal (sr, 23, 0.7f, { { 95, 0.2f, 1 }, { 160, 0.15f, 0.8f }, { 245, 0.12f, 0.6f }, { 410, 0.1f, 0.5f }, { 880, 0.06f, 0.3f } }, 0.8f, 0.01f, 5000); } },
        { "rohr", "Rohr", "Kisten", [] (float sr) { return comb (sr, 24, 0.5f, 0.0027f, 0.92f, 4500, 160); } },
        { "telefon", "Telefon", "Lautsprecher", [] (float sr) { return speaker (sr, 31, 380, 3200, { { 1500, 1.2f, 6 } }); } },
        { "megafon", "Megafon", "Lautsprecher", [] (float sr) { return speaker (sr, 32, 650, 4600, { { 1800, 3, 9 }, { 3200, 2, 6 } }, 0.08f); } },
        { "gitarrenbox", "Gitarrenbox", "Lautsprecher", [] (float sr) { return speaker (sr, 33, 85, 5200, { { 120, 1, 3 }, { 2500, 1.4f, 5 }, { 4200, 3, -4 } }); } },
        { "kofferradio", "Kofferradio", "Lautsprecher", [] (float sr) { return speaker (sr, 34, 220, 5800, { { 900, 1.5f, 4 }, { 3000, 2, 3 } }); } },
        { "rueckwaerts", "Rückwärts-Hall", "Effekte", [] (float sr) { return reversed (room (sr, { 41, 1.8f, 1.7f, 0.8f, 0.4f, 0, 0.0f, 120 })); } },
        { "gitterrost", "Gitterrost", "Effekte", [] (float sr) { return sparse (sr, 42, 0.5f, 260, 0.18f, 400); } },
        { "flatterecho", "Flatterecho", "Effekte", [] (float sr) { return comb (sr, 43, 1.4f, 0.032f, 0.82f, 6500, 250, true); } },
        { "schrapnell", "Schrapnell", "Effekte", [] (float sr) { auto a = sparse (sr, 44, 0.45f, 900, 0.12f, 900, 12000); addInto (a, modal (sr, 45, 0.45f, { { 2300, 0.15f, 0.4f }, { 3900, 0.12f, 0.3f }, { 6100, 0.09f, 0.25f } }), 0.3f); return a; } },
    };
    return list;
}

inline int numIRs() { return (int) irTypes().size(); }

inline const std::vector<std::string>& irCategories()
{
    static const std::vector<std::string> c = { "Räume", "Kisten", "Lautsprecher", "Effekte" };
    return c;
}

inline int findIR (const std::string& id)
{
    const auto& l = irTypes();
    for (size_t i = 0; i < l.size(); ++i) if (id == l[i].id) return (int) i;
    return 0;
}

// Erzeugt und merkt sich die Impulsantworten pro Abtastrate (auf Energie 1 normiert)
class IrLibrary
{
public:
    const Stereo& get (int index, float sr)
    {
        std::lock_guard<std::mutex> lock (mutex);
        const auto key = std::make_pair (std::clamp (index, 0, numIRs() - 1), (int) sr);
        auto it = cache.find (key);
        if (it != cache.end()) return it->second;
        Stereo s = irTypes()[(size_t) key.first].gen (sr);
        double e = 0;
        for (const auto& d : s) for (float v : d) e += (double) v * v;
        const float g = e > 0 ? (float) (1.0 / std::sqrt (e / 2.0)) : 1.0f;
        for (auto& d : s) for (auto& v : d) v *= g;
        return cache.emplace (key, std::move (s)).first->second;
    }

    // Mischt bis zu vier Impulsantworten nach den XY-Gewichten (energierichtig)
    Stereo blend (const std::array<int, 4>& ids, float x, float y, float sr)
    {
        const auto w = weights (x, y);
        size_t len = 1; float ws = 0;
        for (int i = 0; i < 4; ++i)
            if (w[(size_t) i] > 0.002f) { len = std::max (len, get (ids[(size_t) i], sr)[0].size()); ws += w[(size_t) i] * w[(size_t) i]; }
        Stereo out = stereo (len);
        const float norm = ws > 0 ? 1.0f / std::sqrt (ws) : 1.0f;
        for (int i = 0; i < 4; ++i)
        {
            if (w[(size_t) i] <= 0.002f) continue;
            const auto& p = get (ids[(size_t) i], sr);
            for (int c = 0; c < 2; ++c)
                for (size_t k = 0; k < p[(size_t) c].size(); ++k) out[(size_t) c][k] += p[(size_t) c][k] * w[(size_t) i] * norm;
        }
        return out;
    }

    static IrLibrary& shared() { static IrLibrary lib; return lib; }

private:
    std::mutex mutex;
    std::map<std::pair<int, int>, Stereo> cache;
};

} // namespace sp
