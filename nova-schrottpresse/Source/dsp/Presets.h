// NOVA Schrottpresse: 31 Werkspresets, gebaut für Schranz und harten Techno
#pragma once

#include "Params.h"

namespace sp {

struct BandCfg
{
    std::array<int, 4> t { findShape ("band"), findShape ("roehre"), findShape ("hartclip"), findShape ("fuzz") };
    std::array<float, 4> st { 0.5f, 0.5f, 0.5f, 0.5f };
    float x = 0.25f, y = 0.25f, x2 = 0.75f, y2 = 0.75f, drv = 12, tlt = 0, mx = 1, eDrive = 0, eXY = 0;
    bool pOn = true, fOn = false;
    std::array<int, 4> ir { findIR ("bunker"), findIR ("pappkarton"), findIR ("gitarrenbox"), findIR ("telefon") };
    float fx = 0.5f, fy = 0.5f, fmx = 0.25f, wid = 1, pre = 0, lvl = 0;
    int order = 0;

    BandCfg types (const char* a, const char* b, const char* c, const char* d) const { auto r = *this; r.t = { findShape (a), findShape (b), findShape (c), findShape (d) }; return r; }
    BandCfg pos (float px, float py) const { auto r = *this; r.x = px; r.y = py; return r; }
    BandCfg target (float px, float py, float amount) const { auto r = *this; r.x2 = px; r.y2 = py; r.eXY = amount; return r; }
    BandCfg drive (float d) const { auto r = *this; r.drv = d; return r; }
    BandCfg tilt (float tv) const { auto r = *this; r.tlt = tv; return r; }
    BandCfg mix (float m) const { auto r = *this; r.mx = m; return r; }
    BandCfg envDrive (float e) const { auto r = *this; r.eDrive = e; return r; }
    BandCfg falt (const char* a, const char* b, const char* c, const char* d, float px, float py, float m, float w = 1, float p = 0) const
    { auto r = *this; r.fOn = true; r.ir = { findIR (a), findIR (b), findIR (c), findIR (d) }; r.fx = px; r.fy = py; r.fmx = m; r.wid = w; r.pre = p; return r; }
    BandCfg faltFirst() const { auto r = *this; r.order = 1; return r; }
};

struct PresetDef
{
    std::string name, cat;
    int bands = 1;
    float x1 = 150, x2 = 2500;
    std::array<BandCfg, 3> b {};
    float sens = 6, smooth = 0.06f;
    bool fOn = false; int fType = 0; float cut = 9000, res = 0.2f, scream = 0.2f, fenv = 0;
    float input = 0, mix = 1, output = 0; bool ag = true, lim = true;

    PresetDef split (int n, float a = 150, float c = 2500) const { auto r = *this; r.bands = n; r.x1 = a; r.x2 = c; return r; }
    PresetDef band (int i, BandCfg cfg) const { auto r = *this; r.b[(size_t) i] = cfg; return r; }
    PresetDef env (float s, float sm = 0.06f) const { auto r = *this; r.sens = s; r.smooth = sm; return r; }
    PresetDef filter (int type, float c, float rs, float sc, float e = 0) const { auto r = *this; r.fOn = true; r.fType = type; r.cut = c; r.res = rs; r.scream = sc; r.fenv = e; return r; }
};

inline PresetDef P (const char* name, const char* cat) { PresetDef p; p.name = name; p.cat = cat; return p; }
inline BandCfg Bd() { return {}; }
inline BandCfg clean() { return Bd().types ("band", "warm", "trafo", "roehre").drive (3).mix (0.5f); }

inline const std::vector<PresetDef>& presets()
{
    static const std::vector<PresetDef> list = {
        // Kick
        P ("Schranz-Kick Brutal", "Kick").band (0, Bd().types ("hartclip", "presslufthammer", "roehre", "fuzz").pos (0.35f, 0.3f).drive (24).tilt (3).falt ("bunker", "pappkarton", "gitarrenbox", "fliesenklo", 0.1f, 0.2f, 0.12f)),
        P ("Kick-Fräse", "Kick").split (2, 110).band (0, Bd().types ("band", "warm", "roehre", "trafo").pos (0.2f, 0.3f).drive (8)).band (1, Bd().types ("fraese", "fuzz", "hartclip", "rasierklinge").pos (0.45f, 0.35f).drive (30).tilt (2)),
        P ("Hallen-Kick", "Kick").band (0, Bd().types ("roehre", "hartclip", "band", "overdrive").pos (0.4f, 0.25f).drive (18).falt ("lagerhalle", "bunker", "treppenhaus", "rueckwaerts", 0.15f, 0.2f, 0.32f, 1.5f, 0.02f)),
        P ("Tribal-Klopfer", "Kick").band (0, Bd().types ("abrissbirne", "kante", "roehre", "fuzz").pos (0.3f, 0.4f).drive (15).falt ("pappkarton", "muelltonne", "rohr", "bunker", 0.3f, 0.3f, 0.4f)),
        P ("Kick am Telefon", "Kick").band (0, Bd().types ("fuzz", "hartclip", "gatterfuzz", "bit8").pos (0.2f, 0.2f).drive (22).falt ("telefon", "megafon", "kofferradio", "gitarrenbox", 0.15f, 0.2f, 0.85f).faltFirst()),
        P ("Pump-Kick", "Kick").band (0, Bd().types ("hartclip", "fuzz", "roehre", "presslufthammer").pos (0.15f, 0.15f).target (0.85f, 0.85f, 0.8f).drive (20).envDrive (9)).env (9, 0.04f),
        // Rumble & Bass
        P ("Rumble-Bus", "Rumble & Bass").split (2, 170).band (0, Bd().types ("band", "warm", "roehre", "trafo").pos (0.3f, 0.3f).drive (10).falt ("lagerhalle", "muelltonne", "bunker", "rohr", 0.2f, 0.3f, 0.45f, 0.6f)).band (1, Bd().types ("hartclip", "fraese", "fuzz", "kante").pos (0.3f, 0.4f).drive (20).mix (0.6f)).filter (0, 2600, 0.25f, 0.15f),
        P ("Rumble-Matsch", "Rumble & Bass").band (0, Bd().types ("sinusfalte", "warm", "doppelfalt", "roehre").pos (0.35f, 0.5f).drive (14).falt ("muelltonne", "lagerhalle", "rohr", "bunker", 0.3f, 0.35f, 0.5f)).filter (0, 420, 0.55f, 0.3f),
        P ("Sub-Kante", "Rumble & Bass").split (2, 90).band (0, Bd().types ("kante", "band", "warm", "trafo").pos (0.2f, 0.2f).drive (6).mix (0.8f)).band (1, Bd().types ("overdrive", "fuzz", "roehre", "hartclip").pos (0.4f, 0.4f).drive (18)),
        P ("Schranz-Bass Säge", "Rumble & Bass").split (3, 120, 1800).band (0, Bd().types ("band", "roehre", "warm", "trafo").drive (6)).band (1, Bd().types ("fraese", "sinusfalte", "fuzz", "schrott").pos (0.4f, 0.45f).drive (26).tilt (2)).band (2, Bd().types ("hartclip", "bit8", "fuzz", "rasierklinge").pos (0.3f, 0.3f).drive (14).mix (0.6f)),
        P ("Rollender Bass", "Rumble & Bass").band (0, Bd().types ("warm", "roehre", "fuzz", "abrissbirne").pos (0.1f, 0.1f).target (0.7f, 0.8f, 0.9f).drive (12).envDrive (6).falt ("lagerhalle", "bunker", "muelltonne", "rohr", 0.3f, 0.2f, 0.25f)).env (8, 0.05f),
        // Loops & Drums
        P ("Loop-Säge", "Loops & Drums").split (3, 150, 3000).band (0, Bd().types ("band", "warm", "roehre", "trafo").drive (6)).band (1, Bd().types ("fraese", "fuzz", "hartclip", "doppelfalt").pos (0.35f, 0.4f).drive (22)).band (2, Bd().types ("hartclip", "rasierklinge", "fuzz", "bit8").pos (0.3f, 0.3f).drive (12).mix (0.7f).falt ("telefon", "kofferradio", "megafon", "blechdose", 0.3f, 0.3f, 0.3f)),
        P ("Industrie-Halle", "Loops & Drums").band (0, Bd().types ("roehre", "hartclip", "fuzz", "schrott").pos (0.3f, 0.35f).drive (18).falt ("lagerhalle", "treppenhaus", "bunker", "flatterecho", 0.35f, 0.25f, 0.35f, 1.6f)),
        P ("Zerhackt", "Loops & Drums").band (0, Bd().types ("wackelkontakt", "gatterfuzz", "bit8", "bitklotz").pos (0.1f, 0.1f).target (0.9f, 0.8f, 1.0f).drive (16)).env (10, 0.02f),
        P ("Blechdosen-Loop", "Loops & Drums").band (0, Bd().types ("fuzz", "hartclip", "oktavfuzz", "roehre").pos (0.3f, 0.3f).drive (16).falt ("blechdose", "muelltonne", "rohr", "pappkarton", 0.15f, 0.25f, 0.55f)),
        P ("Schranz-Druck", "Loops & Drums").split (3, 140, 2200).band (0, Bd().types ("roehre", "band", "kante", "warm").drive (9)).band (1, Bd().types ("oktavfuzz", "fuzz", "fraese", "abrissbirne").pos (0.35f, 0.3f).drive (18).mix (0.75f)).band (2, Bd().types ("hartclip", "kante", "rasierklinge", "bit8").drive (10).mix (0.6f)),
        P ("Funkenflug", "Loops & Drums").split (2, 180, 4000).band (0, Bd().types ("band", "warm", "roehre", "trafo").drive (4).mix (0.6f)).band (1, Bd().types ("oktavfuzz", "sinusfalte", "schrott", "doppelfalt").pos (0.4f, 0.5f).drive (20).tilt (4).falt ("gitterrost", "schrapnell", "blechdose", "fliesenklo", 0.3f, 0.3f, 0.35f)),
        // Perc & Hats
        P ("Hat-Kratzer", "Perc & Hats").split (2, 6500).band (0, Bd().types ("band", "warm", "roehre", "trafo").drive (2).mix (0.3f)).band (1, Bd().types ("fraese", "bitklotz", "rasierklinge", "bit8").pos (0.3f, 0.35f).drive (18)),
        P ("Metall-Percussion", "Perc & Hats").band (0, Bd().types ("sinusfalte", "doppelfalt", "fuzz", "roehre").pos (0.3f, 0.3f).drive (14).falt ("blechdose", "rohr", "schrapnell", "treppenhaus", 0.3f, 0.3f, 0.5f)),
        P ("Klatsch-Gewitter", "Perc & Hats").band (0, Bd().types ("doppelfalt", "hartclip", "fuzz", "schrott").pos (0.3f, 0.3f).drive (16).falt ("treppenhaus", "flatterecho", "lagerhalle", "fliesenklo", 0.3f, 0.3f, 0.4f, 1.6f)),
        P ("Rückwärts-Rauschen", "Perc & Hats").band (0, Bd().types ("band", "fuzz", "roehre", "hartclip").pos (0.25f, 0.25f).drive (10).falt ("rueckwaerts", "lagerhalle", "gitterrost", "flatterecho", 0.15f, 0.15f, 0.5f, 1.8f).faltFirst()),
        // Bus & Master
        P ("Bus-Kleber", "Bus & Master").band (0, clean().pos (0.3f, 0.3f).drive (5).mix (0.6f)),
        P ("Lautheits-Wand", "Bus & Master").split (3, 140, 2800).band (0, clean().drive (7).mix (0.7f)).band (1, Bd().types ("kante", "roehre", "overdrive", "band").pos (0.3f, 0.3f).drive (8).mix (0.6f)).band (2, Bd().types ("kante", "hartclip", "band", "warm").drive (6).mix (0.5f)),
        P ("Warmes Band", "Bus & Master").band (0, Bd().types ("band", "trafo", "warm", "roehre").pos (0.1f, 0.2f).drive (9).tilt (-2).mix (0.8f)),
        P ("Röhren-Glanz", "Bus & Master").band (0, Bd().types ("roehre", "trafo", "band", "warm").pos (0.1f, 0.15f).drive (7).tilt (2).mix (0.7f)),
        // Krach & FX
        P ("Abrissbirne", "Krach & FX").split (3, 130, 2400).band (0, Bd().types ("abrissbirne", "presslufthammer", "schrott", "fuzz").pos (0.5f, 0.5f).drive (30)).band (1, Bd().types ("schrott", "fraese", "doppelfalt", "abrissbirne").pos (0.6f, 0.5f).drive (36)).band (2, Bd().types ("bitklotz", "rasierklinge", "fuzz", "schrott").pos (0.4f, 0.5f).drive (28)).filter (0, 3000, 0.6f, 0.6f, 0.5f),
        P ("Bitklotz-Gewitter", "Krach & FX").band (0, Bd().types ("bit8", "bitklotz", "mulaw", "wackelkontakt").pos (0.2f, 0.2f).target (0.9f, 0.9f, 1.0f).drive (14)).env (10, 0.03f),
        P ("Kreisch-Sirene", "Krach & FX").band (0, Bd().types ("fuzz", "roehre", "hartclip", "sinusfalte").pos (0.3f, 0.3f).drive (16)).filter (1, 900, 0.85f, 0.7f, 0.8f).env (10, 0.05f),
        P ("Kaputter Lautsprecher", "Krach & FX").band (0, Bd().types ("lautsprecher", "halbwelle", "batterie", "wackelkontakt").pos (0.3f, 0.3f).drive (18).falt ("megafon", "kofferradio", "telefon", "gitarrenbox", 0.2f, 0.3f, 0.75f)),
        P ("Schrottpresse", "Krach & FX").split (3, 140, 2600)
            .band (0, Bd().types ("roehre", "presslufthammer", "band", "abrissbirne").pos (0.3f, 0.3f).target (0.7f, 0.7f, 0.7f).drive (16).falt ("bunker", "lagerhalle", "muelltonne", "rohr", 0.2f, 0.3f, 0.2f))
            .band (1, Bd().types ("fraese", "schrott", "oktavfuzz", "sinusfalte").pos (0.3f, 0.4f).target (0.8f, 0.6f, 0.8f).drive (24))
            .band (2, Bd().types ("bit8", "rasierklinge", "gatterfuzz", "hartclip").pos (0.4f, 0.3f).drive (16).falt ("schrapnell", "gitterrost", "blechdose", "telefon", 0.3f, 0.3f, 0.3f))
            .filter (0, 6000, 0.4f, 0.35f, 0.4f).env (9),
        // Basis
        P ("Leer", "Basis").band (0, Bd().pos (0, 0).drive (0)),
    };
    return list;
}

inline int numPresets() { return (int) presets().size(); }

inline const std::vector<std::string>& presetCategories()
{
    static const std::vector<std::string> c = { "Kick", "Rumble & Bass", "Loops & Drums", "Perc & Hats", "Bus & Master", "Krach & FX", "Basis" };
    return c;
}

// Preset -> komplette Parameterwerte
inline Values presetValues (int index)
{
    Values v = defaultValues();
    const auto& p = presets()[(size_t) std::clamp (index, 0, numPresets() - 1)];
    v[gBands] = (float) (p.bands - 1); v[gX1] = p.x1; v[gX2] = p.x2;
    v[gEnvSens] = p.sens; v[gEnvSmooth] = p.smooth;
    v[gFOn] = p.fOn ? 1.0f : 0.0f; v[gFType] = (float) p.fType; v[gFCut] = p.cut; v[gFRes] = p.res; v[gFScream] = p.scream; v[gFEnv] = p.fenv;
    v[gInput] = p.input; v[gMix] = p.mix; v[gOutput] = p.output; v[gAutoGain] = p.ag ? 1.0f : 0.0f; v[gLimiter] = p.lim ? 1.0f : 0.0f;
    for (int b = 0; b < kBands; ++b)
    {
        const auto& c = p.b[(size_t) b];
        auto set = [&] (int k, float val) { v[(size_t) bp (b, k)] = val; };
        set (bPOn, c.pOn ? 1.0f : 0.0f);
        for (int i = 0; i < 4; ++i) { set (bT0 + i, (float) c.t[(size_t) i]); set (bS0 + i, c.st[(size_t) i]); set (bI0 + i, (float) c.ir[(size_t) i]); }
        set (bX, c.x); set (bY, c.y); set (bX2, c.x2); set (bY2, c.y2);
        set (bDrive, c.drv); set (bTilt, c.tlt); set (bPMix, c.mx); set (bEnvDrive, c.eDrive); set (bEnvXY, c.eXY);
        set (bFOn, c.fOn ? 1.0f : 0.0f); set (bFX, c.fx); set (bFY, c.fy); set (bFMix, c.fmx); set (bWidth, c.wid); set (bPre, c.pre);
        set (bOrder, (float) c.order); set (bLevel, c.lvl); set (bSolo, 0); set (bMute, 0);
    }
    return v;
}

} // namespace sp
