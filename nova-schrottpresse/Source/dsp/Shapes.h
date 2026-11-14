// NOVA Schrottpresse: die 25 Verzerrer als Kennlinien
// Reines C++17. Jede Kennlinie wird für 17 Stil-Stufen vorab als Tabelle berechnet,
// dazwischen wird interpoliert. So kostet Automation des Stils im Audio-Thread nichts.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace sp {

constexpr float kPi     = 3.14159265358979323846f;
constexpr float kRange  = 16.0f;   // Kennlinien decken Eingangswerte von -16 bis +16 ab
constexpr int   kTable  = 4096;
constexpr int   kStyles = 17;

inline float sgn (float x) { return (float) ((x > 0.0f) - (x < 0.0f)); }
inline float fold (float z) { return 2.0f / kPi * std::asin (std::sin (kPi * z * 0.5f)); }
inline float T5 (float z) { return 16.0f * z * z * z * z * z - 20.0f * z * z * z + 5.0f * z; }
inline float quant (float v, float bits)
{
    const float L = std::pow (2.0f, std::max (1.0f, bits) - 1.0f);
    return std::round (v * L) / L;
}
inline float clampf (float v, float a, float b) { return std::min (b, std::max (a, v)); }

struct ShapeType
{
    const char* id;
    const char* name;
    const char* cat;
    float (*f) (float x, float s);
};

inline const std::vector<ShapeType>& shapeTypes()
{
    static const std::vector<ShapeType> list = {
        { "band", "Bandmaschine", "Sättigung", [] (float x, float s) { return std::tanh (x * (0.6f + 0.8f * s) + 0.12f) - std::tanh (0.12f); } },
        { "roehre", "Röhre", "Sättigung", [] (float x, float s) { return x >= 0 ? std::tanh (x * (1 + s)) : std::tanh (x * (1 + s) * 0.55f) * 1.25f; } },
        { "warm", "Warm", "Sättigung", [] (float x, float s) { return std::atan (x * (0.5f + 2 * s)); } },
        { "trafo", "Trafo", "Sättigung", [] (float x, float s) { return std::tanh (x + s * 0.35f * x * x); } },
        { "overdrive", "Overdrive", "Antrieb", [] (float x, float s) { const float k = 1 + 5 * s; return x * (1 + k) / (1 + k * std::fabs (x)); } },
        { "kante", "Kante", "Antrieb", [] (float x, float s) { const float z = x * (0.4f + s); return std::fabs (z) < 1 ? 1.5f * (z - z * z * z / 3) : sgn (z); } },
        { "diode", "Diode", "Antrieb", [] (float x, float s) { return x > 0 ? 1 - std::exp (-x * (1 + 2 * s)) : -(1 - std::exp (x * 0.3f * (1 + 2 * s))) * 0.45f; } },
        { "hartclip", "Hartclip", "Verzerrung", [] (float x, float s) { return clampf (x * (0.5f + 3 * s), -1, 1); } },
        { "fraese", "Fräse", "Verzerrung", [] (float x, float s) { return sgn (x) * std::min (1.0f, std::pow (std::fabs (x) * (1 + 3 * s), 0.45f)); } },
        { "rasierklinge", "Rasierklinge", "Verzerrung", [] (float x, float s) { return sgn (x) * std::pow (std::fabs (std::tanh (x * 2)), 0.15f + 0.5f * (1 - s)); } },
        { "doppelfalt", "Doppelfalter", "Verzerrung", [] (float x, float s) { return fold (x * (0.5f + 2 * s)); } },
        { "sinusfalte", "Sinusfalte", "Verzerrung", [] (float x, float s) { return std::sin (x * (0.7f + 3 * s) * kPi / 2); } },
        { "fuzz", "Fuzz", "Fuzz", [] (float x, float s) { return sgn (x) * (1 - std::exp (-std::fabs (x) * (2 + 10 * s))); } },
        { "gatterfuzz", "Gatter-Fuzz", "Fuzz", [] (float x, float s) { const float t = 0.05f + 0.3f * s; return std::fabs (x) < t ? 0.0f : sgn (x) * (1 - std::exp (-(std::fabs (x) - t) * 8)); } },
        { "oktavfuzz", "Oktav-Fuzz", "Fuzz", [] (float x, float s) { const float a = std::tanh (3 * std::fabs (x) * (1 + 2 * s)); return a * sgn (x + 0.0001f) * 0.3f + a * 0.7f; } },
        { "presslufthammer", "Presslufthammer", "Schwer", [] (float x, float s) { return std::tanh (fold (x * (1 + 3 * s)) * 4); } },
        { "abrissbirne", "Abrissbirne", "Schwer", [] (float x, float s) { const float z = std::tanh (x * (0.8f + s)); return 0.45f * z + 0.55f * T5 (z); } },
        { "schrott", "Schrott", "Schwer", [] (float x, float s) { const float z = x * (1 + 4 * s); return clampf (fold (z) * 1.5f + 0.3f * std::sin (7 * z), -1, 1); } },
        { "wackelkontakt", "Wackelkontakt", "Kaputt", [] (float x, float s) { const float t = 0.02f + 0.25f * s; return std::fabs (x) < t ? 0.0f : std::tanh (sgn (x) * (std::fabs (x) - t) * 2); } },
        { "halbwelle", "Halbwelle", "Kaputt", [] (float x, float s) { return x > 0 ? std::tanh (x * (1 + 2 * s)) : 0.0f; } },
        { "lautsprecher", "Kaputter Lautsprecher", "Kaputt", [] (float x, float s) { return x > 0 ? std::tanh (x * 2) : -std::tanh (-x * (0.3f + s)) + 0.2f * std::sin (5 * x); } },
        { "batterie", "Leere Batterie", "Kaputt", [] (float x, float s) { return sgn (x) * std::pow (std::max (0.0f, std::fabs (std::tanh (x)) - 0.05f - 0.2f * s), 0.6f); } },
        { "bit8", "8-Bit", "Retro", [] (float x, float s) { return quant (std::tanh (x), 8 - 5 * s); } },
        { "bitklotz", "Bitklotz", "Retro", [] (float x, float s) { return quant (std::tanh (x * 1.5f), 3 - 2 * s); } },
        { "mulaw", "Mu-Law", "Retro", [] (float x, float s) {
              const float t = std::tanh (x), mu = 255.0f;
              const float c = sgn (t) * std::log (1 + mu * std::fabs (t)) / std::log (1 + mu);
              const float q = quant (c, 6 - 3 * s);
              return sgn (q) * (std::pow (1 + mu, std::fabs (q)) - 1) / mu; } },
    };
    return list;
}

inline int numShapes() { return (int) shapeTypes().size(); }

inline const std::vector<std::string>& shapeCategories()
{
    static const std::vector<std::string> c = { "Sättigung", "Antrieb", "Verzerrung", "Fuzz", "Schwer", "Kaputt", "Retro" };
    return c;
}

inline int findShape (const std::string& id)
{
    const auto& l = shapeTypes();
    for (size_t i = 0; i < l.size(); ++i) if (id == l[i].id) return (int) i;
    return 0;
}

// Alle Tabellen, einmal pro Programmstart berechnet (ca. 7 MB)
class ShapeTables
{
public:
    static const ShapeTables& get()
    {
        static ShapeTables t;
        return t;
    }

    const float* table (int type, int styleStep) const
    {
        return data.data() + ((size_t) type * kStyles + (size_t) styleStep) * kTable;
    }

    // Wert der Kennlinie bei Eingang x (x im Bereich -16..16), Stil 0..1
    float eval (int type, float style, float x) const
    {
        const float sp = clampf (style, 0, 1) * (kStyles - 1);
        const int j = std::min (kStyles - 2, (int) sp);
        const float g = sp - (float) j;
        return lookup (table (type, j), x) * (1 - g) + lookup (table (type, j + 1), x) * g;
    }

    static inline float lookup (const float* t, float x)
    {
        float u = (x / kRange + 1.0f) * 0.5f * (kTable - 1);
        u = clampf (u, 0.0f, (float) (kTable - 1));
        const int i = std::min (kTable - 2, (int) u);
        const float f = u - (float) i;
        return t[i] + (t[i + 1] - t[i]) * f;
    }

private:
    std::vector<float> data;

    ShapeTables()
    {
        const auto& types = shapeTypes();
        data.resize (types.size() * kStyles * kTable);
        for (size_t t = 0; t < types.size(); ++t)
            for (int s = 0; s < kStyles; ++s)
            {
                float* d = data.data() + (t * kStyles + (size_t) s) * kTable;
                const float style = (float) s / (kStyles - 1);
                float m = 0;
                for (int i = 0; i < kTable; ++i)
                {
                    const float x = ((float) i / (kTable - 1) * 2 - 1) * kRange;
                    float y = types[t].f (x, style);
                    if (! std::isfinite (y)) y = 0;
                    d[i] = y; m = std::max (m, std::fabs (y));
                }
                if (m > 1e-6f) for (int i = 0; i < kTable; ++i) d[i] /= m;
            }
    }
};

inline std::array<float, 4> weights (float x, float y)
{
    return { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };
}

} // namespace sp
