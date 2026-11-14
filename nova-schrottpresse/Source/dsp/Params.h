// NOVA Schrottpresse: alle Parameter (global + drei Bänder)
#pragma once

#include "IRs.h"
#include <cstdio>
#include <string>
#include <vector>

namespace sp {

enum GlobalParam : int {
    gBands, gX1, gX2, gEnvSens, gEnvSmooth,
    gFOn, gFType, gFCut, gFRes, gFScream, gFEnv,
    gInput, gMix, gOutput, gAutoGain, gLimiter,
    gCount
};

enum BandParam : int {
    bPOn, bT0, bT1, bT2, bT3, bS0, bS1, bS2, bS3,
    bX, bY, bX2, bY2, bDrive, bTilt, bPMix, bEnvDrive, bEnvXY,
    bFOn, bI0, bI1, bI2, bI3, bFX, bFY, bFMix, bWidth, bPre,
    bOrder, bLevel, bSolo, bMute,
    bCount
};

constexpr int kBands = 3;
constexpr int kNumParams = gCount + kBands * bCount;
constexpr int bp (int band, int p) { return gCount + band * bCount + p; }

enum class Kind { Float, Choice, Bool };
enum class Unit { None, Db, Percent, Bipolar, Hz, Ms, Pos, Factor };

struct ParamDef
{
    std::string id, name;
    Kind kind;
    float min, max, def;
    bool exp;          // logarithmischer Regelweg
    Unit unit;
    std::vector<std::string> choices;
    bool bipolar;
};

inline std::string fmtNum (float x, int dec)
{
    char b[32]; std::snprintf (b, sizeof (b), "%.*f", dec, (double) x);
    for (char* c = b; *c; ++c) if (*c == '.') *c = ',';
    return b;
}

inline const std::vector<ParamDef>& params()
{
    static const std::vector<ParamDef> list = [] {
        std::vector<ParamDef> v;
        auto F = [&] (std::string id, std::string name, float mn, float mx, float df, Unit u, bool ex = false, bool bip = false)
        { v.push_back ({ std::move (id), std::move (name), Kind::Float, mn, mx, df, ex, u, {}, bip }); };
        auto C = [&] (std::string id, std::string name, std::vector<std::string> ch, int df)
        { v.push_back ({ std::move (id), std::move (name), Kind::Choice, 0.0f, (float) ch.size() - 1, (float) df, false, Unit::None, std::move (ch), false }); };
        auto B = [&] (std::string id, std::string name, bool df)
        { v.push_back ({ std::move (id), std::move (name), Kind::Bool, 0.0f, 1.0f, df ? 1.0f : 0.0f, false, Unit::None, {}, false }); };

        std::vector<std::string> shapes, irs;
        for (const auto& s : shapeTypes()) shapes.push_back (s.name);
        for (const auto& i : irTypes()) irs.push_back (i.name);

        // global (Reihenfolge = GlobalParam)
        C ("bands", "Bänder", { "1 Band", "2 Bänder", "3 Bänder" }, 0);
        F ("xover1", "Trennung 1", 30, 16000, 150, Unit::Hz, true);
        F ("xover2", "Trennung 2", 30, 16000, 2500, Unit::Hz, true);
        F ("envSens", "Hüllkurve Empfindlichkeit", -12, 24, 6, Unit::Db);
        F ("envSmooth", "Hüllkurve Glättung", 0.005f, 0.4f, 0.06f, Unit::Ms, true);
        B ("filterOn", "Kreisch-Filter", false);
        C ("filterType", "Filtertyp", { "Tief", "Band", "Hoch" }, 0);
        F ("cutoff", "Cutoff", 40, 18000, 9000, Unit::Hz, true);
        F ("resonance", "Resonanz", 0, 1, 0.2f, Unit::Percent);
        F ("scream", "Kreischen", 0, 1, 0.2f, Unit::Percent);
        F ("filterEnv", "Hüllkurve → Cutoff", -1, 1, 0, Unit::Bipolar, false, true);
        F ("input", "Eingang", -24, 24, 0, Unit::Db, false, true);
        F ("mix", "Mix", 0, 1, 1, Unit::Percent);
        F ("output", "Ausgang", -24, 12, 0, Unit::Db, false, true);
        B ("autoGain", "Auto-Gain", true);
        B ("limiter", "Limiter", true);

        static const char* corner[] = { "↖", "↗", "↙", "↘" };
        for (int b = 0; b < kBands; ++b)
        {
            const std::string p = "b" + std::to_string (b + 1) + "_", n = "B" + std::to_string (b + 1) + " ";
            B (p + "presseOn", n + "Presse", true);
            const int defT[4] = { findShape ("band"), findShape ("roehre"), findShape ("hartclip"), findShape ("fuzz") };
            for (int c = 0; c < 4; ++c) C (p + "type" + std::to_string (c), n + "Verzerrer " + corner[c], shapes, defT[c]);
            for (int c = 0; c < 4; ++c) F (p + "style" + std::to_string (c), n + "Stil " + corner[c], 0, 1, 0.5f, Unit::Percent);
            F (p + "x", n + "Presse X", 0, 1, 0.25f, Unit::Pos);
            F (p + "y", n + "Presse Y", 0, 1, 0.25f, Unit::Pos);
            F (p + "x2", n + "Ziel X", 0, 1, 0.75f, Unit::Pos);
            F (p + "y2", n + "Ziel Y", 0, 1, 0.75f, Unit::Pos);
            F (p + "drive", n + "Drive", 0, 42, 12, Unit::Db);
            F (p + "tilt", n + "Tilt", -12, 12, 0, Unit::Db, false, true);
            F (p + "presseMix", n + "Presse Mix", 0, 1, 1, Unit::Percent);
            F (p + "envDrive", n + "Hüllkurve → Drive", -24, 24, 0, Unit::Db, false, true);
            F (p + "envXY", n + "Hüllkurve → Ziel", 0, 1, 0, Unit::Percent);
            B (p + "faltungOn", n + "Faltung", false);
            const int defI[4] = { findIR ("bunker"), findIR ("pappkarton"), findIR ("gitarrenbox"), findIR ("telefon") };
            for (int c = 0; c < 4; ++c) C (p + "ir" + std::to_string (c), n + "Raum " + corner[c], irs, defI[c]);
            F (p + "fx", n + "Faltung X", 0, 1, 0.5f, Unit::Pos);
            F (p + "fy", n + "Faltung Y", 0, 1, 0.5f, Unit::Pos);
            F (p + "faltungMix", n + "Faltung Mix", 0, 1, 0.25f, Unit::Percent);
            F (p + "width", n + "Breite", 0, 2, 1, Unit::Percent);
            F (p + "predelay", n + "Vorverzögerung", 0, 0.2f, 0, Unit::Ms);
            C (p + "order", n + "Reihenfolge", { "Presse zuerst", "Faltung zuerst" }, 0);
            F (p + "level", n + "Pegel", -24, 12, 0, Unit::Db, false, true);
            B (p + "solo", n + "Solo", false);
            B (p + "mute", n + "Stumm", false);
        }
        return v;
    }();
    return list;
}

inline const ParamDef& def (int id) { return params()[(size_t) std::clamp (id, 0, kNumParams - 1)]; }

inline float toNorm (int id, float v)
{
    const auto& d = def (id);
    if (d.max <= d.min) return 0;
    if (d.exp) return clampf (std::log (std::max (v, d.min) / d.min) / std::log (d.max / d.min), 0, 1);
    return clampf ((v - d.min) / (d.max - d.min), 0, 1);
}

inline float fromNorm (int id, float n)
{
    const auto& d = def (id);
    n = clampf (n, 0, 1);
    float v = d.exp ? d.min * std::pow (d.max / d.min, n) : d.min + (d.max - d.min) * n;
    if (d.kind != Kind::Float) v = std::round (v);
    return v;
}

inline std::string formatValue (int id, float v)
{
    const auto& d = def (id);
    if (d.kind == Kind::Bool) return v >= 0.5f ? "an" : "aus";
    if (d.kind == Kind::Choice) return d.choices[(size_t) std::clamp ((int) std::lround (v), 0, (int) d.choices.size() - 1)];
    switch (d.unit)
    {
        case Unit::Db: {
            const float a = std::fabs (v);
            return std::string (v > 0.05f ? "+" : (v < -0.05f ? "−" : "")) + fmtNum (a, a < 10 ? 1 : 0) + " dB";
        }
        case Unit::Percent: return std::to_string ((int) std::lround (v * 100)) + " %";
        case Unit::Bipolar: {
            const int p = (int) std::lround (std::fabs (v) * 100);
            return std::string (p == 0 ? "" : (v > 0 ? "+" : "−")) + std::to_string (p) + " %";
        }
        case Unit::Hz: return v < 1000 ? std::to_string ((int) std::lround (v)) + " Hz" : fmtNum (v / 1000, v < 10000 ? 1 : 0) + " kHz";
        case Unit::Ms: return std::to_string ((int) std::lround (v * 1000)) + " ms";
        case Unit::Pos: return std::to_string ((int) std::lround (v * 100));
        case Unit::Factor: return fmtNum (v, 2);
        case Unit::None: break;
    }
    return fmtNum (v, 2);
}

inline float parseValue (int id, const std::string& text)
{
    const auto& d = def (id);
    if (d.kind == Kind::Choice)
        for (size_t i = 0; i < d.choices.size(); ++i) if (text == d.choices[i]) return (float) i;
    std::string t = text;
    for (auto& c : t) if (c == ',') c = '.';
    float v = (float) std::atof (t.c_str());
    if (d.kind == Kind::Bool) return (text == "an" || v >= 0.5f) ? 1.0f : 0.0f;
    switch (d.unit)
    {
        case Unit::Percent: case Unit::Bipolar: v /= 100; break;
        case Unit::Pos: v /= 100; break;
        case Unit::Hz: if (t.find ('k') != std::string::npos) v *= 1000; break;
        case Unit::Ms: v /= 1000; break;
        case Unit::Db: case Unit::Factor: case Unit::None: break;
    }
    return clampf (v, d.min, d.max);
}

using Values = std::array<float, kNumParams>;

inline Values defaultValues()
{
    Values v {};
    for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = def (i).def;
    return v;
}

} // namespace sp
