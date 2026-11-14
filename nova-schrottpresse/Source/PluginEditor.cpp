#include "PluginEditor.h"
#include "BinaryData.h"

using namespace sp;

// ============================================================ Stil
namespace ui
{
    const juce::Colour concrete { 0xffc6c4be }, concrete2 { 0xffb5b3ac }, ink { 0xff141414 }, inkSoft { 0xff4f4c47 },
                       hot { 0xffff5b1f }, amber { 0xffffb224 }, machine { 0xff121314 }, machine2 { 0xff1d1e20 }, paper { 0xfff2efe7 };

    enum class F { Stencil, Semi, Med, Mono };

    static juce::Font font (F f, float h)
    {
        static juce::Typeface::Ptr tf[4] = {
            juce::Typeface::createSystemTypefaceFor (BinaryData::Stencil_ttf, (size_t) BinaryData::Stencil_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::PlexCondSemi_ttf, (size_t) BinaryData::PlexCondSemi_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::PlexCondMed_ttf, (size_t) BinaryData::PlexCondMed_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::PlexMono_ttf, (size_t) BinaryData::PlexMono_ttfSize)
        };
        const auto& t = tf[(int) f];
        if (t == nullptr) return juce::Font (juce::FontOptions (h, f == F::Med ? juce::Font::plain : juce::Font::bold));
        return juce::Font (juce::FontOptions (t).withHeight (h));
    }

    static juce::String up (const juce::String& s)
    {
        juce::String out;
        for (auto t = s.getCharPointer(); ! t.isEmpty();)
        {
            const auto c = t.getAndAdvance();
            if (c == 0xe4) out << juce::String::charToString (0xc4);
            else if (c == 0xf6) out << juce::String::charToString (0xd6);
            else if (c == 0xfc) out << juce::String::charToString (0xdc);
            else if (c == 0xdf) out << "SS";
            else out << juce::String::charToString (juce::CharacterFunctions::toUpperCase (c));
        }
        return out;
    }

    static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    static juce::String U (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

    static void panel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (ink);
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 2.0f);
    }

    static void led (juce::Graphics& g, juce::Point<float> c, float r, bool on)
    {
        if (on)
        {
            g.setColour (hot.withAlpha (0.35f)); g.fillEllipse (c.x - r * 2.2f, c.y - r * 2.2f, r * 4.4f, r * 4.4f);
            g.setColour (hot);
        }
        else g.setColour (juce::Colour (0xff7d7a73));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    }

    static const char* cornerNames[] = { "oben links", "oben rechts", "unten links", "unten rechts" };
}

class SchrottLnf : public juce::LookAndFeel_V4
{
public:
    SchrottLnf()
    {
        setColour (juce::PopupMenu::backgroundColourId, ui::concrete);
        setColour (juce::PopupMenu::textColourId, ui::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::ink);
        setColour (juce::PopupMenu::highlightedTextColourId, ui::concrete);
        setColour (juce::PopupMenu::headerTextColourId, ui::inkSoft);
    }
    juce::Font getPopupMenuFont() override { return ui::font (ui::F::Semi, 16.0f); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) override
    {
        const float size = (float) juce::jmin (w, h), cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
        const float r = size * 0.5f - 3.5f, angle = start + pos * (end - start);
        const juce::PathStrokeType stroke (4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Path track; track.addCentredArc (cx, cy, r, r, 0, start, end, true);
        g.setColour (ui::concrete2); g.strokePath (track, stroke);
        const bool bip = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const float from = bip ? (start + end) * 0.5f : start;
        if (std::abs (angle - from) > 0.01f)
        {
            juce::Path v; v.addCentredArc (cx, cy, r, r, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
            g.setColour (ui::hot); g.strokePath (v, stroke);
        }
        const float cr = r * 0.66f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4b4d), cx - cr * 0.35f, cy - cr * 0.45f, juce::Colour (0xff0b0b0c), cx + cr * 0.7f, cy + cr * 0.75f, true));
        g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 1.0f);
        const float sx = std::sin (angle), sy = -std::cos (angle);
        g.setColour (ui::paper);
        g.drawLine (cx + sx * cr * 0.3f, cy + sy * cr * 0.3f, cx + sx * cr * 0.85f, cy + sy * cr * 0.85f, 2.6f);
    }
};

// ============================================================ Bausteine
using Readout = std::function<void (const juce::String&)>;

class Knob : public juce::Component
{
public:
    Knob (SchrottProcessor& p, int paramId, const juce::String& label, Readout ro, int knobSize = 50)
        : name (ui::up (label)), readout (std::move (ro)), ks (knobSize)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (190);
        slider.getProperties().set ("bipolar", def (paramId).bipolar);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, def (paramId).id, slider);
        auto* prm = p.param (paramId);
        slider.setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
        slider.onValueChange = [this] { repaint(); if (slider.isMouseButtonDown()) show(); };
        slider.onDragStart = [this] { show(); };
    }
    void resized() override { slider.setBounds (getLocalBounds().removeFromTop (ks + 4).withSizeKeepingCentre (ks + 4, ks + 4)); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds(); r.removeFromTop (ks + 6);
        g.setColour (ui::ink); g.setFont (ui::font (ui::F::Semi, 13.0f).withExtraKerningFactor (0.05f));
        g.drawFittedText (name, r.removeFromTop (15).reduced (2, 0), juce::Justification::centred, 1, 0.7f);
        g.setColour (ui::inkSoft); g.setFont (ui::font (ui::F::Mono, 11.5f));
        g.drawFittedText (slider.getTextFromValue (slider.getValue()), r.removeFromTop (14), juce::Justification::centred, 1, 0.8f);
    }
private:
    void show() { if (readout) readout (name + "   " + slider.getTextFromValue (slider.getValue())); }
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::String name;
    Readout readout;
    int ks;
};

// Schalter, der seinen Zustand selbst abfragt (Parameter oder Oberflächen-Zustand)
class Toggle : public juce::Component
{
public:
    enum class Style { Power, Box, Mini };
    Toggle (Style s, juce::String t, std::function<bool()> g, std::function<void()> tg) : style (s), text (std::move (t)), get (std::move (g)), toggle (std::move (tg))
    {
        setRepaintsOnMouseActivity (true);
        setTitle (text);
    }
    void poll() { const bool v = get(); if (v != last) { last = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        const bool on = get(); last = on;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        if (style == Style::Power)
        {
            const float d = juce::jmin (r.getWidth(), r.getHeight());
            auto c = r.withSizeKeepingCentre (d, d);
            g.setColour (ui::concrete2); g.fillEllipse (c);
            g.setColour (ui::ink); g.drawEllipse (c.reduced (1.0f), 2.0f);
            ui::led (g, c.getCentre(), d * 0.15f, on);
            return;
        }
        const bool mini = style == Style::Mini;
        if (on) { g.setColour (mini ? ui::hot : ui::ink); g.fillRoundedRectangle (r, 3.0f); }
        else if (mini) { g.setColour (isMouseOver() ? ui::paper : ui::concrete); g.fillRoundedRectangle (r, 3.0f); }
        else if (isMouseOver()) { g.setColour (juce::Colours::white.withAlpha (0.3f)); g.fillRoundedRectangle (r, 3.0f); }
        g.setColour (on && ! mini ? ui::ink : (mini ? juce::Colour (on ? ui::hot : ui::ink) : ui::ink));
        g.drawRoundedRectangle (r, 3.0f, mini ? 1.5f : 2.0f);
        g.setColour (on ? (mini ? ui::ink : ui::concrete) : ui::ink);
        g.setFont (ui::font (ui::F::Semi, mini ? 12.0f : 14.0f).withExtraKerningFactor (0.06f));
        g.drawFittedText (ui::up (text), getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.7f);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition())) { toggle(); repaint(); } }
private:
    Style style; juce::String text;
    std::function<bool()> get; std::function<void()> toggle;
    bool last = false;
};

// Auswahl-Reihe (Bänder, Filtertyp)
class Seg : public juce::Component
{
public:
    Seg (juce::StringArray l, std::function<int()> g, std::function<void (int)> s) : labels (std::move (l)), get (std::move (g)), set (std::move (s)) {}
    void poll() { const int v = get(); if (v != last) { last = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        const int n = labels.size(), sel = get(); last = sel;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float w = r.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
            if (i == sel) { g.setColour (ui::ink); g.fillRect (c); }
            g.setColour (i == sel ? ui::concrete : ui::ink);
            g.setFont (ui::font (ui::F::Semi, 14.0f));
            g.drawFittedText (labels[i], c.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.7f);
            if (i > 0) { g.setColour (ui::ink); g.fillRect (c.getX() - 1.0f, r.getY(), 2.0f, r.getHeight()); }
        }
        g.setColour (ui::ink); g.drawRoundedRectangle (r, 3.0f, 2.0f);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        set (juce::jlimit (0, labels.size() - 1, e.x * labels.size() / juce::jmax (1, getWidth())));
        repaint();
    }
private:
    juce::StringArray labels; std::function<int()> get; std::function<void (int)> set; int last = -1;
};

class PlainButton : public juce::Component
{
public:
    enum Icon { None, Prev, Next, Dice, Swap };
    PlainButton (juce::String t, std::function<void()> c, float fs = 15.0f, Icon ic = None) : text (std::move (t)), icon (ic), click (std::move (c)), size (fs) { setRepaintsOnMouseActivity (true); setTitle (text); }
    juce::String text;
    Icon icon;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::white.withAlpha (isMouseOver() ? 0.45f : 0.25f)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (ui::ink); g.drawRoundedRectangle (r, 3.0f, 2.0f);
        const auto c = r.getCentre();
        if (icon == Prev || icon == Next)
        {
            juce::Path t; const float d = icon == Prev ? -1.0f : 1.0f;
            t.addTriangle (c.x - 6 * d, c.y - 7, c.x - 6 * d, c.y + 7, c.x + 7 * d, c.y);
            g.fillPath (t); return;
        }
        if (icon == Dice)
        {
            auto b = juce::Rectangle<float> (18, 18).withCentre (c);
            g.drawRoundedRectangle (b, 3.5f, 2.0f);
            for (auto p : { juce::Point<float> (-4.5f, -4.5f), { 4.5f, -4.5f }, { 0, 0 }, { -4.5f, 4.5f }, { 4.5f, 4.5f } })
                g.fillEllipse (c.x + p.x - 1.8f, c.y + p.y - 1.8f, 3.6f, 3.6f);
            return;
        }
        if (icon == Swap)
        {
            const float y = r.getY() + 30;
            juce::Path a;
            a.addArrow ({ c.x - 10, y - 5, c.x + 10, y - 5 }, 2.0f, 8.0f, 6.0f);
            a.addArrow ({ c.x + 10, y + 5, c.x - 10, y + 5 }, 2.0f, 8.0f, 6.0f);
            g.fillPath (a);
            g.setFont (ui::font (ui::F::Semi, size));
            g.drawFittedText (text, getLocalBounds().withTrimmedTop (48).reduced (2, 0), juce::Justification::centredTop, 3, 0.6f);
            return;
        }
        g.setFont (ui::font (ui::F::Semi, size));
        g.drawFittedText (text, getLocalBounds().reduced (8, 0), juce::Justification::centred, 2, 0.7f);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && click) click(); }
private:
    std::function<void()> click; float size;
};

struct UiState { int band = 0; bool target = false; };

// ============================================================ XY-Pads
class MorphPad : public juce::Component
{
public:
    MorphPad (SchrottProcessor& p, UiState& s, bool presseMode, Readout ro) : proc (p), st (s), presse (presseMode), readout (std::move (ro))
    {
        setOpaque (true);
        setTitle (presse ? "Verzerrer-Mischung" : "Raum-Mischung");
        setWantsKeyboardFocus (true);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.fillAll (ui::concrete);
        g.setColour (ui::machine2); g.fillRoundedRectangle (r, 6.0f);
        auto s = r.reduced (4.0f);
        g.setColour (ui::machine); g.fillRoundedRectangle (s, 4.0f);
        g.setColour (ui::amber.withAlpha (0.07f));
        for (int i = 1; i < 8; ++i)
        {
            g.fillRect (s.getX() + s.getWidth() * (float) i / 8, s.getY(), 1.0f, s.getHeight());
            g.fillRect (s.getX(), s.getY() + s.getHeight() * (float) i / 8, s.getWidth(), 1.0f);
        }
        const int b = st.band;
        const float W = s.getWidth(), H = s.getHeight(), X = s.getX(), Y = s.getY();
        auto node = [&] (float nx, float ny, float rad, bool hollow, float glow)
        {
            const float px = X + nx * W, py = Y + ny * H;
            if (glow > 0)
            {
                g.setGradientFill (juce::ColourGradient (ui::hot.withAlpha (0.55f * juce::jmin (1.0f, glow)), px, py, ui::hot.withAlpha (0.0f), px + rad * 6, py, true));
                g.fillEllipse (px - rad * 6, py - rad * 6, rad * 12, rad * 12);
            }
            if (hollow)
            {
                juce::Path c; c.addEllipse (px - rad, py - rad, rad * 2, rad * 2);
                juce::Path d; const float dash[] = { 3.0f, 3.0f };
                juce::PathStrokeType (2.0f).createDashedStroke (d, c, dash, 2);
                g.setColour (ui::amber); g.fillPath (d);
            }
            else
            {
                g.setColour (ui::hot); g.fillEllipse (px - rad, py - rad, rad * 2, rad * 2);
                g.setColour (juce::Colour (0xfffff3e0)); g.fillEllipse (px - rad * 0.38f, py - rad * 0.38f, rad * 0.76f, rad * 0.76f);
            }
        };
        const float env = proc.engine.uiEnv.load(), outLvl = juce::jmin (1.0f, proc.engine.uiOut.load());

        if (presse)
        {
            const bool on = proc.value (bp (b, bPOn)) >= 0.5f;
            const float x = proc.value (bp (b, bX)), y = proc.value (bp (b, bY)), x2 = proc.value (bp (b, bX2)), y2 = proc.value (bp (b, bY2));
            const float exy = proc.value (bp (b, bEnvXY)), drive = proc.value (bp (b, bDrive)), ed = proc.value (bp (b, bEnvDrive));
            const float ex = x + (x2 - x) * env * exy, ey = y + (y2 - y) * env * exy;
            const auto w = weights (ex, ey);
            const float A = juce::jmin (kRange, std::pow (10.0f, (drive + ed * env) / 20.0f));
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.drawLine (X, Y + H, X + W, Y, 1.0f);
            g.drawLine (X + W / 2, Y, X + W / 2, Y + H, 1.0f); g.drawLine (X, Y + H / 2, X + W, Y + H / 2, 1.0f);
            const auto& T = ShapeTables::get();
            int types[4]; float styles[4];
            for (int c = 0; c < 4; ++c) { types[c] = (int) proc.value (bp (b, bT0 + c)); styles[c] = proc.value (bp (b, bS0 + c)); }
            juce::Path curve;
            const int N = 200;
            for (int i = 0; i <= N; ++i)
            {
                const float u = (float) i / N * 2 - 1, xin = u * A;
                float yv = 0;
                for (int c = 0; c < 4; ++c) if (w[(size_t) c] > 0.001f) yv += w[(size_t) c] * T.eval (types[c], styles[c], xin);
                const float px = X + (u + 1) / 2 * W, py = Y + (1 - (juce::jlimit (-1.1f, 1.1f, yv) + 1) / 2) * H;
                if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
            }
            g.saveState();
            g.reduceClipRegion (s.toNearestInt());
            g.setColour (ui::hot.withAlpha (0.10f + 0.12f * outLvl)); g.strokePath (curve, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ui::hot.withAlpha (0.25f + 0.2f * outLvl)); g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ui::amber); g.strokePath (curve, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (exy > 0 || st.target)
            {
                juce::Path l; l.startNewSubPath (X + x * W, Y + y * H); l.lineTo (X + x2 * W, Y + y2 * H);
                juce::Path d; const float dash[] = { 4.0f, 4.0f };
                juce::PathStrokeType (1.5f).createDashedStroke (d, l, dash, 2);
                g.setColour (ui::amber.withAlpha (0.55f)); g.fillPath (d);
                node (x2, y2, 9, true, 0);
                if (exy > 0) { g.setColour (ui::amber); g.fillEllipse (X + ex * W - 4, Y + ey * H - 4, 8, 8); }
            }
            node (x, y, 10, false, on ? 0.4f + outLvl : 0.1f);
            g.restoreState();
            if (! on) { g.setColour (ui::machine.withAlpha (0.6f)); g.fillRoundedRectangle (s, 4.0f); }
            g.setColour (ui::amber.withAlpha (0.8f)); g.setFont (ui::font (ui::F::Mono, 11.5f));
            g.drawText (! on ? "AUS" : (st.target ? "ZIEL VERSCHIEBEN" : "KENNLINIE"), s.reduced (10, 6), juce::Justification::bottomLeft);
        }
        else
        {
            const bool on = proc.value (bp (b, bFOn)) >= 0.5f;
            const float x = proc.value (bp (b, bFX)), y = proc.value (bp (b, bFY));
            const auto& d = proc.engine.irPreview (b);
            if (on && ! d.empty())
            {
                const int cols = (int) W;
                const size_t step = std::max<size_t> (1, d.size() / (size_t) cols);
                float peak = 1e-9f;
                for (size_t i = 0; i < d.size(); i += 4) peak = juce::jmax (peak, std::abs (d[i]));
                g.setColour (ui::amber.withAlpha (0.55f));
                for (int c = 0; c < cols; ++c)
                {
                    float m = 0;
                    for (size_t i = (size_t) c * step; i < std::min (d.size(), (size_t) (c + 1) * step); ++i) m = juce::jmax (m, std::abs (d[i]));
                    const float v = std::sqrt (m / peak) * H * 0.42f;
                    g.fillRect (X + (float) c, Y + H / 2 - v, 1.0f, v * 2);
                }
            }
            else
            {
                g.setColour (ui::amber.withAlpha (0.6f)); g.setFont (ui::font (ui::F::Mono, 12.5f));
                g.drawFittedText (on ? "Impulsantwort wird gemischt" : "AUS\nKnopf oben links schaltet ein", s.toNearestInt().reduced (20), juce::Justification::centred, 3);
            }
            g.saveState(); g.reduceClipRegion (s.toNearestInt());
            node (x, y, 10, false, on ? 0.5f : 0.1f);
            g.restoreState();
            g.setColour (ui::amber.withAlpha (0.8f)); g.setFont (ui::font (ui::F::Mono, 11.5f));
            g.drawText ("IMPULSANTWORT", s.reduced (10, 6), juce::Justification::bottomLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { grabKeyboardFocus(); begin(); moveTo (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { moveTo (e.position); }
    void mouseUp (const juce::MouseEvent&) override { end(); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        float dx = 0, dy = 0;
        if (k == juce::KeyPress::leftKey) dx = -0.02f; else if (k == juce::KeyPress::rightKey) dx = 0.02f;
        else if (k == juce::KeyPress::upKey) dy = -0.02f; else if (k == juce::KeyPress::downKey) dy = 0.02f; else return false;
        const auto [ix, iy] = ids();
        proc.setValue (ix, juce::jlimit (0.0f, 1.0f, proc.value (ix) + dx));
        proc.setValue (iy, juce::jlimit (0.0f, 1.0f, proc.value (iy) + dy));
        return true;
    }

private:
    std::pair<int, int> ids() const
    {
        const int b = st.band;
        if (! presse) return { bp (b, bFX), bp (b, bFY) };
        return st.target ? std::make_pair (bp (b, bX2), bp (b, bY2)) : std::make_pair (bp (b, bX), bp (b, bY));
    }
    void begin() { const auto [ix, iy] = ids(); proc.param (ix)->beginChangeGesture(); proc.param (iy)->beginChangeGesture(); }
    void end()   { const auto [ix, iy] = ids(); proc.param (ix)->endChangeGesture(); proc.param (iy)->endChangeGesture(); }
    void moveTo (juce::Point<float> p)
    {
        const auto s = getLocalBounds().toFloat().reduced (4.0f);
        const float x = juce::jlimit (0.0f, 1.0f, (p.x - s.getX()) / s.getWidth()), y = juce::jlimit (0.0f, 1.0f, (p.y - s.getY()) / s.getHeight());
        const auto [ix, iy] = ids();
        proc.setValueInGesture (ix, x); proc.setValueInGesture (iy, y);
        if (! readout) return;
        if (presse && st.target) { readout ("ZIEL   " + juce::String (juce::roundToInt (x * 100)) + " / " + juce::String (juce::roundToInt (y * 100))); return; }
        const int b = st.band;
        const auto w = weights (x, y);
        int o[4] = { 0, 1, 2, 3 };
        std::sort (o, o + 4, [&] (int a, int c) { return w[(size_t) a] > w[(size_t) c]; });
        auto nm = [&] (int c) {
            return presse ? ui::U (shapeTypes()[(size_t) juce::jlimit (0, numShapes() - 1, (int) proc.value (bp (b, bT0 + c)))].name)
                          : ui::U (irTypes()[(size_t) juce::jlimit (0, numIRs() - 1, (int) proc.value (bp (b, bI0 + c)))].name); };
        readout (nm (o[0]) + " " + juce::String (juce::roundToInt (w[(size_t) o[0]] * 100)) + " · " + nm (o[1]) + " " + juce::String (juce::roundToInt (w[(size_t) o[1]] * 100)));
    }

    SchrottProcessor& proc; UiState& st; bool presse; Readout readout;
};

class CornerLabel : public juce::Component
{
public:
    CornerLabel (SchrottProcessor& p, UiState& s, bool presseMode, int corner, Readout ro) : proc (p), st (s), presse (presseMode), c (corner), readout (std::move (ro))
    {
        setRepaintsOnMouseActivity (true);
        setTitle (juce::String ("Ecke ") + ui::cornerNames[c]);
    }
    void paint (juce::Graphics& g) override
    {
        const int b = st.band;
        const bool right = c % 2 == 1;
        const float x = proc.value (presse ? bp (b, bX) : bp (b, bFX)), y = proc.value (presse ? bp (b, bY) : bp (b, bFY));
        const float w = weights (x, y)[(size_t) c];
        auto r = getLocalBounds();
        auto wr = right ? r.removeFromRight (44) : r.removeFromLeft (44);
        g.setFont (ui::font (ui::F::Mono, 12.0f));
        g.setColour (w > 0.5f ? ui::hot : ui::inkSoft);
        g.drawText (juce::String (juce::roundToInt (w * 100)) + " %", wr, right ? juce::Justification::centredRight : juce::Justification::centredLeft);
        g.setFont (ui::font (ui::F::Semi, 16.0f)); g.setColour (ui::ink);
        auto nr = right ? r.withTrimmedRight (6) : r.withTrimmedLeft (6);
        g.drawFittedText (name(), nr, right ? juce::Justification::centredRight : juce::Justification::centredLeft, 1, 0.75f);
        const int tw = juce::jmin (nr.getWidth(), juce::GlyphArrangement::getStringWidthInt (ui::font (ui::F::Semi, 16.0f), name()));
        const int lx = right ? nr.getRight() - tw : nr.getX();
        g.setColour (isMouseOver() ? ui::hot : ui::inkSoft);
        for (int px = lx; px < lx + tw; px += 4) g.fillRect (px, nr.getCentreY() + 10, 2, 2);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        const int b = st.band;
        const int id = presse ? bp (b, bT0 + c) : bp (b, bI0 + c);
        const int cur = (int) proc.value (id);
        juce::PopupMenu menu;
        menu.addSectionHeader ((presse ? juce::String ("Verzerrer") : juce::String ("Raum")) + " für Ecke " + ui::cornerNames[c]);
        const auto& cats = presse ? shapeCategories() : irCategories();
        for (const auto& cat : cats)
        {
            juce::PopupMenu sub;
            if (presse) { for (int i = 0; i < numShapes(); ++i) if (cat == shapeTypes()[(size_t) i].cat) sub.addItem (i + 1, ui::U (shapeTypes()[(size_t) i].name), true, i == cur); }
            else        { for (int i = 0; i < numIRs(); ++i)    if (cat == irTypes()[(size_t) i].cat)    sub.addItem (i + 1, ui::U (irTypes()[(size_t) i].name), true, i == cur); }
            menu.addSubMenu (ui::U (cat), sub);
        }
        juce::Component::SafePointer<CornerLabel> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, id] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            safe->proc.setValue (id, (float) (r - 1));
            if (safe->readout) safe->readout (safe->name());
            safe->repaint();
        });
    }
    juce::String name() const
    {
        const int b = st.band;
        if (presse) return ui::U (shapeTypes()[(size_t) juce::jlimit (0, numShapes() - 1, (int) proc.value (bp (b, bT0 + c)))].name);
        return ui::U (irTypes()[(size_t) juce::jlimit (0, numIRs() - 1, (int) proc.value (bp (b, bI0 + c)))].name);
    }
private:
    SchrottProcessor& proc; UiState& st; bool presse; int c; Readout readout;
};

// ============================================================ Spektrum mit Bändern
class Spectrum : public juce::Component
{
public:
    Spectrum (SchrottProcessor& p, UiState& s, std::function<void()> sel, Readout ro) : proc (p), st (s), onSelect (std::move (sel)), readout (std::move (ro))
    {
        setOpaque (true);
        inDb.fill (-120.0f); outDb.fill (-120.0f);
        setTitle ("Spektrum mit Bändern");
    }
    void update()
    {
        analyse (proc.engine.inFifo, inDb);
        analyse (proc.engine.outFifo, outDb);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const float w = (float) getWidth(), h = (float) getHeight();
        g.fillAll (ui::machine);
        auto fx = [&] (float f) { return std::log10 (f / 20.0f) / 3.0f * w; };
        const int nb = (int) proc.value (gBands) + 1;
        const float f1 = proc.value (gX1), f2 = juce::jmax (proc.value (gX2), f1 * 1.5f);
        float e[4]; int ne = 0; e[ne++] = 20; if (nb > 1) e[ne++] = f1; if (nb > 2) e[ne++] = f2; e[ne++] = 20000;
        const int sel = juce::jmin (st.band, nb - 1);
        g.setColour (ui::hot.withAlpha (0.12f)); g.fillRect (fx (e[sel]), 0.0f, fx (e[sel + 1]) - fx (e[sel]), h);
        g.setFont (ui::font (ui::F::Mono, 11.0f));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillRect (fx (f), 0.0f, 1.0f, h);
            g.setColour (juce::Colours::white.withAlpha (0.35f)); g.drawText (f >= 1000 ? juce::String ((int) f / 1000) + "k" : juce::String ((int) f), (int) fx (f) + 3, (int) h - 16, 40, 14, juce::Justification::left);
        }
        auto curve = [&] (const std::array<float, 2048>& db, bool fill)
        {
            juce::Path p;
            const float sr = (float) (proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
            for (int x = 0; x <= (int) w; x += 2)
            {
                const float f = 20.0f * std::pow (10.0f, (float) x / w * 3.0f);
                const int bin = juce::jlimit (0, 2047, (int) std::lround (f / (sr / 2) * 2048));
                const float v = juce::jlimit (0.0f, 1.0f, (db[(size_t) bin] + 100.0f) / 90.0f), y = h - v * (h - 16);
                if (x == 0) p.startNewSubPath ((float) x, y); else p.lineTo ((float) x, y);
            }
            if (fill)
            {
                p.lineTo (w, h); p.lineTo (0, h); p.closeSubPath();
                g.setGradientFill (juce::ColourGradient (ui::hot.withAlpha (0.75f), 0, 0, ui::amber.withAlpha (0.08f), 0, h, false));
                g.fillPath (p);
            }
            else { g.setColour (juce::Colours::white.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (1.5f)); }
        };
        curve (inDb, false); curve (outDb, true);
        for (int i = 1; i < ne - 1; ++i)
        {
            const float x = fx (e[i]);
            g.setColour (drag == i ? ui::hot : ui::amber);
            g.fillRect (x - 1.0f, 0.0f, 3.0f, h);
            g.fillEllipse (x - 6.5f, 7.0f, 13.0f, 13.0f);
            g.setColour (ui::machine);
            juce::Path a; a.addArrow ({ x - 4.5f, 13.5f, x + 5.0f, 13.5f }, 1.2f, 4.0f, 3.0f); a.addArrow ({ x + 4.5f, 13.5f, x - 5.0f, 13.5f }, 1.2f, 4.0f, 3.0f);
            g.fillPath (a);
        }
        static const char* names3[] = { "TIEF", "MITTE", "HOCH" };
        static const char* names2[] = { "TIEF", "HOCH" };
        g.setFont (ui::font (ui::F::Semi, 13.0f));
        for (int i = 0; i < nb; ++i)
        {
            g.setColour (i == sel ? ui::hot : juce::Colours::white.withAlpha (0.5f));
            g.drawText (nb == 1 ? "VOLL" : (nb == 2 ? names2[i] : names3[i]), (int) fx (e[i]) + 10, 4, 80, 18, juce::Justification::left);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int nb = (int) proc.value (gBands) + 1;
        const float u = (float) e.x / (float) getWidth();
        auto xu = [] (float f) { return std::log10 (f / 20.0f) / 3.0f; };
        drag = 0;
        if (nb > 1 && std::abs (xu (proc.value (gX1)) - u) < 0.012f) drag = 1;
        else if (nb > 2 && std::abs (xu (juce::jmax (proc.value (gX2), proc.value (gX1) * 1.5f)) - u) < 0.012f) drag = 2;
        if (drag) { proc.param (drag == 1 ? gX1 : gX2)->beginChangeGesture(); return; }
        const float f = 20.0f * std::pow (10.0f, u * 3.0f);
        int band = 0;
        if (nb > 1 && f > proc.value (gX1)) band = 1;
        if (nb > 2 && f > juce::jmax (proc.value (gX2), proc.value (gX1) * 1.5f)) band = 2;
        st.band = band; if (onSelect) onSelect();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! drag) return;
        float f = 20.0f * std::pow (10.0f, juce::jlimit (0.0f, 1.0f, (float) e.x / (float) getWidth()) * 3.0f);
        const int nb = (int) proc.value (gBands) + 1;
        if (drag == 1) f = juce::jlimit (30.0f, nb > 2 ? proc.value (gX2) / 1.5f : 16000.0f, f);
        else           f = juce::jlimit (proc.value (gX1) * 1.5f, 16000.0f, f);
        proc.setValueInGesture (drag == 1 ? gX1 : gX2, f);
        if (readout) readout ("TRENNUNG   " + ui::U (formatValue (gX1, f)));
        if (onSelect) onSelect();
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag) proc.param (drag == 1 ? gX1 : gX2)->endChangeGesture(); drag = 0; }
private:
    void analyse (const ScopeFifo& fifo, std::array<float, 2048>& db)
    {
        fifo.read (data.data(), 4096);
        window.multiplyWithWindowingTable (data.data(), 4096);
        std::fill (data.begin() + 4096, data.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (data.data());
        for (size_t i = 0; i < 2048; ++i)
        {
            const float v = 20.0f * std::log10 (data[i] / 1024.0f + 1e-9f);
            db[i] = db[i] * 0.8f + v * 0.2f;
        }
    }
    SchrottProcessor& proc; UiState& st; std::function<void()> onSelect; Readout readout;
    juce::dsp::FFT fft { 12 };
    juce::dsp::WindowingFunction<float> window { 4096, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 8192> data {};
    std::array<float, 2048> inDb {}, outDb {};
    int drag = 0;
};

// ============================================================ Hauptfläche
class MainPanel : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    explicit MainPanel (SchrottProcessor& p) : proc (p),
        presetBtn ("", [this] { presetMenu(); }, 16.0f),
        prevBtn ("Voriges Preset", [this] { proc.loadPreset (proc.getCurrentProgram() - 1); }, 15.0f, PlainButton::Prev),
        nextBtn (juce::String::fromUTF8 ("Nächstes Preset"), [this] { proc.loadPreset (proc.getCurrentProgram() + 1); }, 15.0f, PlainButton::Next),
        diceBtn (juce::String::fromUTF8 ("Zufälliges Preset"), [this] { proc.randomPreset(); }, 15.0f, PlainButton::Dice),
        pDice ("Vier zufällige Verzerrer", [this] { proc.randomTypes (st.band); setReadout ("NEUE VERZERRER"); }, 15.0f, PlainButton::Dice),
        fDice (juce::String::fromUTF8 ("Vier zufällige Räume"), [this] { proc.randomIRs (st.band); setReadout (juce::String::fromUTF8 ("NEUE RÄUME")); }, 15.0f, PlainButton::Dice),
        swapBtn ("", [this] { const int id = bp (st.band, bOrder); proc.setValue (id, proc.value (id) >= 0.5f ? 0.0f : 1.0f); refreshSwap(); }, 12.0f, PlainButton::Swap),
        spectrum (p, st, [this] { bandChanged(); }, [this] (const juce::String& t) { setReadout (t); })
    {
        auto ro = [this] (const juce::String& t) { setReadout (t); };

        for (auto* c : { &presetBtn, &prevBtn, &nextBtn, &diceBtn, &pDice, &fDice, &swapBtn }) addAndMakeVisible (c);
        addAndMakeVisible (spectrum);

        auto addToggle = [this] (Toggle::Style s, juce::String text, std::function<bool()> g, std::function<void()> t)
        {
            auto* x = toggles.add (new Toggle (s, std::move (text), std::move (g), std::move (t)));
            addAndMakeVisible (x); return x;
        };
        auto paramToggle = [this, addToggle] (Toggle::Style s, juce::String text, int id)
        {
            return addToggle (s, std::move (text), [this, id] { return proc.value (id) >= 0.5f; }, [this, id] { proc.setValue (id, proc.value (id) >= 0.5f ? 0.0f : 1.0f); });
        };

        original = addToggle (Toggle::Style::Box, "Original", [this] { return proc.compare.load(); }, [this] { proc.compare = ! proc.compare.load(); setReadout (proc.compare ? "ORIGINAL   ohne Effekt" : "EFFEKT   an"); });
        target = addToggle (Toggle::Style::Box, "Ziel", [this] { return st.target; }, [this]
        {
            st.target = ! st.target;
            if (st.target && proc.value (bp (st.band, bEnvXY)) < 0.01f) proc.setValue (bp (st.band, bEnvXY), 0.7f);
            setReadout (st.target ? "ZIEL   Punkt ziehen, die Hüllkurve wandert dorthin" : "ZIEL   gesetzt");
        });
        filterOn = paramToggle (Toggle::Style::Power, "Kreisch-Filter", gFOn);
        autoGain = paramToggle (Toggle::Style::Box, "Auto-Gain", gAutoGain);
        limiter = paramToggle (Toggle::Style::Box, "Limiter", gLimiter);

        bandSeg = std::make_unique<Seg> (juce::StringArray { "1 Band", juce::String::fromUTF8 ("2 Bänder"), juce::String::fromUTF8 ("3 Bänder") },
                                         [this] { return (int) proc.value (gBands); }, [this] (int i) { proc.setValue (gBands, (float) i); bandChanged(); });
        typeSeg = std::make_unique<Seg> (juce::StringArray { "Tief", "Band", "Hoch" }, [this] { return (int) proc.value (gFType); }, [this] (int i) { proc.setValue (gFType, (float) i); });
        addAndMakeVisible (*bandSeg); addAndMakeVisible (*typeSeg);

        // Pro Band eigene Regler, nur das gewählte Band ist sichtbar
        for (int b = 0; b < kBands; ++b)
        {
            auto& s = sets[(size_t) b];
            auto k = [&] (int id, const char* label, int size = 50) { auto* x = s.knobs.add (new Knob (p, bp (b, id), ui::U (label), ro, size)); addChildComponent (x); return x; };
            s.drive = k (bDrive, "Drive", 60); s.tilt = k (bTilt, "Tilt"); s.pmix = k (bPMix, "Mix");
            s.envDrive = k (bEnvDrive, "Hüllk. → Drive"); s.envXY = k (bEnvXY, "Hüllk. → Ziel"); s.level = k (bLevel, "Band-Pegel");
            for (int c = 0; c < 4; ++c) s.style[c] = k (bS0 + c, (juce::String ("Stil ") + juce::String::fromUTF8 (c == 0 ? "↖" : c == 1 ? "↗" : c == 2 ? "↙" : "↘")).toRawUTF8(), 38);
            s.fmix = k (bFMix, "Mix"); s.width = k (bWidth, "Breite"); s.pre = k (bPre, "Vorverzög.");
            s.pOn = paramToggle (Toggle::Style::Power, "Presse", bp (b, bPOn)); s.pOn->setVisible (false);
            s.fOn = paramToggle (Toggle::Style::Power, "Faltung", bp (b, bFOn)); s.fOn->setVisible (false);
            s.solo = paramToggle (Toggle::Style::Mini, "S", bp (b, bSolo)); s.mute = paramToggle (Toggle::Style::Mini, "M", bp (b, bMute));
        }
        // globale Regler
        auto g = [&] (int id, const char* label) { auto* x = globalKnobs.add (new Knob (p, id, ui::U (label), ro)); addAndMakeVisible (x); return x; };
        envSens = g (gEnvSens, "Empfindl."); envSmooth = g (gEnvSmooth, "Glättung");
        cutoff = g (gFCut, "Cutoff"); res = g (gFRes, "Resonanz"); scream = g (gFScream, "Kreischen"); fenv = g (gFEnv, "Hüllk. → Cutoff");
        input = g (gInput, "Eingang"); mix = g (gMix, "Mix"); output = g (gOutput, "Ausgang");

        pPad = std::make_unique<MorphPad> (p, st, true, ro); fPad = std::make_unique<MorphPad> (p, st, false, ro);
        addAndMakeVisible (*pPad); addAndMakeVisible (*fPad);
        for (int c = 0; c < 4; ++c)
        {
            pCorners[(size_t) c] = std::make_unique<CornerLabel> (p, st, true, c, ro); addAndMakeVisible (*pCorners[(size_t) c]);
            fCorners[(size_t) c] = std::make_unique<CornerLabel> (p, st, false, c, ro); addAndMakeVisible (*fCorners[(size_t) c]);
        }

        // Rauschtextur für den Beton
        texture = juce::Image (juce::Image::ARGB, 220, 220, true);
        juce::Random rnd (7);
        for (int y = 0; y < 220; ++y)
            for (int x = 0; x < 220; ++x)
            {
                const float a = rnd.nextFloat();
                texture.setPixelAt (x, y, (a > 0.5f ? juce::Colours::white : juce::Colours::black).withAlpha (std::abs (a - 0.5f) * 0.16f));
            }

        proc.addChangeListener (this);
        bandChanged(); refreshPreset();
        startTimerHz (30);
    }
    ~MainPanel() override { proc.removeChangeListener (this); }

    void setReadout (const juce::String& t) { readout = t; readoutUntil = juce::Time::getMillisecondCounterHiRes() + 1600; repaint (readoutRect); }

    void resized() override
    {
        // Kopf
        prevBtn.setBounds (520, 30, 38, 38); presetBtn.setBounds (564, 30, 270, 38); nextBtn.setBounds (840, 30, 38, 38);
        diceBtn.setBounds (884, 30, 42, 38); original->setBounds (932, 30, 112, 38);
        readoutRect = { 1052, 30, 174, 38 };
        // Multiband
        mbRect = { 14, 96, 1212, 214 };
        bandSeg->setBounds (1036, 106, 176, 30);
        inMeter = { 846, 108, 86, 26 }; outMeter = { 940, 108, 86, 26 };
        spectrum.setBounds (30, 144, 1180, 112);
        // Module
        pRect = { 14, 322, 584, 362 }; fRect = { 642, 322, 584, 362 };
        swapBtn.setBounds (602, 440, 36, 126);
        target->setBounds (pRect.getRight() - 14 - 42 - 8 - 70, 334, 70, 32);
        pDice.setBounds (pRect.getRight() - 56, 334, 42, 32);
        fDice.setBounds (fRect.getRight() - 56, 334, 42, 32);
        for (int side = 0; side < 2; ++side)
        {
            const int x0 = (side == 0 ? pRect : fRect).getX() + 16;
            auto& corners = side == 0 ? pCorners : fCorners;
            corners[0]->setBounds (x0, 376, 126, 22); corners[1]->setBounds (x0 + 126, 376, 126, 22);
            (side == 0 ? pPad : fPad)->setBounds (x0, 400, 252, 252);
            corners[2]->setBounds (x0, 656, 126, 22); corners[3]->setBounds (x0 + 126, 656, 126, 22);
        }
        for (int b = 0; b < kBands; ++b)
        {
            auto& s = sets[(size_t) b];
            s.pOn->setBounds (pRect.getX() + 16, 334, 32, 32);
            s.fOn->setBounds (fRect.getX() + 16, 334, 32, 32);
            const int kx = pRect.getX() + 16 + 252 + 18, kw = (pRect.getRight() - 14 - kx) / 3;
            s.drive->setBounds (kx, 380, kw, 92); s.tilt->setBounds (kx + kw, 380, kw, 92); s.pmix->setBounds (kx + 2 * kw, 380, kw, 92);
            s.envDrive->setBounds (kx, 476, kw, 86); s.envXY->setBounds (kx + kw, 476, kw, 86); s.level->setBounds (kx + 2 * kw, 476, kw, 86);
            const int sw = (pRect.getRight() - 14 - kx) / 4;
            for (int c = 0; c < 4; ++c) s.style[c]->setBounds (kx + c * sw, 574, sw, 76);
            const int fx = fRect.getX() + 16 + 252 + 18, fwid = (fRect.getRight() - 14 - fx) / 3;
            s.fmix->setBounds (fx, 380, fwid, 86); s.width->setBounds (fx + fwid, 380, fwid, 86); s.pre->setBounds (fx + 2 * fwid, 380, fwid, 86);
        }
        faltNote = { fRect.getX() + 16 + 252 + 22, 486, 270, 170 };
        layoutTabs();
        // Unten
        envRect = { 14, 696, 396, 132 }; filtRect = { 418, 696, 404, 132 }; outRect = { 830, 696, 396, 132 };
        envSens->setBounds (28, 734, 90, 88); envSmooth->setBounds (120, 734, 90, 88);
        envMeter = { 226, 748, 168, 14 }; envNote = { 226, 768, 172, 54 };
        filterOn->setBounds (filtRect.getX() + 16, 706, 30, 30);
        typeSeg->setBounds (filtRect.getRight() - 14 - 168, 706, 168, 28);
        const int fk = (filtRect.getWidth() - 28) / 4;
        cutoff->setBounds (filtRect.getX() + 14, 738, fk, 88); res->setBounds (filtRect.getX() + 14 + fk, 738, fk, 88);
        scream->setBounds (filtRect.getX() + 14 + 2 * fk, 738, fk, 88); fenv->setBounds (filtRect.getX() + 14 + 3 * fk, 738, fk, 88);
        input->setBounds (844, 738, 86, 88); mix->setBounds (930, 738, 86, 88); output->setBounds (1016, 738, 86, 88);
        autoGain->setBounds (1110, 744, 102, 32); limiter->setBounds (1110, 784, 102, 32);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace ui;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfcdc7), 0, 0, juce::Colour (0xffbdbbb5), (float) getWidth(), (float) getHeight(), false));
        g.fillAll();
        g.setTiledImageFill (texture, 0, 0, 1.0f); g.fillAll();

        // Marke
        g.setColour (ink); g.fillRoundedRectangle (22.0f, 14.0f, 76.0f, 24.0f, 2.0f);
        g.setColour (concrete); g.setFont (font (F::Stencil, 21.0f).withExtraKerningFactor (0.3f));
        g.drawText ("NOVA", 22, 15, 80, 24, juce::Justification::centred);
        const auto logo = font (F::Stencil, 54.0f);
        g.setFont (logo); g.setColour (ink);
        g.drawText ("SCHROTT", 20, 38, 300, 52, juce::Justification::centredLeft);
        g.setColour (hot);
        g.drawText ("PRESSE", 20 + juce::GlyphArrangement::getStringWidthInt (logo, "SCHROTT"), 38, 300, 52, juce::Justification::centredLeft);

        // Anzeige
        g.setColour (machine); g.fillRoundedRectangle (readoutRect.toFloat(), 3.0f);
        g.setColour (amber); g.setFont (font (F::Mono, 13.0f));
        const bool showRead = juce::Time::getMillisecondCounterHiRes() < readoutUntil;
        g.drawFittedText (showRead ? readout : idleText(), readoutRect.reduced (10, 0), juce::Justification::centredLeft, 1, 0.75f);

        // Multiband
        panel (g, mbRect.toFloat());
        g.setColour (ink); g.setFont (font (F::Stencil, 30.0f).withExtraKerningFactor (0.04f));
        g.drawText ("MULTIBAND", 30, 104, 200, 32, juce::Justification::centredLeft);
        g.setColour (inkSoft); g.setFont (font (F::Med, 14.5f));
        g.drawText ("Trennfrequenzen im Spektrum ziehen, Band antippen zum Bearbeiten", 196, 104, 560, 32, juce::Justification::centredLeft);
        meter (g, inMeter, inLevel, "IN"); meter (g, outMeter, outLevel, "OUT");

        // Module
        panel (g, pRect.toFloat()); panel (g, fRect.toFloat());
        const int nb = (int) proc.value (gBands) + 1;
        static const char* bn3[] = { "Band Tief", "Band Mitte", "Band Hoch" };
        static const char* bn2[] = { "Band Tief", "Band Hoch" };
        const juce::String bandName = nb == 1 ? juce::String ("Ganzes Signal") : juce::String (nb == 2 ? bn2[juce::jmin (st.band, 1)] : bn3[st.band]);
        title (g, "PRESSE", pRect.getX() + 56, 334, bandName);
        title (g, "FALTUNG", fRect.getX() + 56, 334, juce::String::fromUTF8 ("Räume, Kisten, Lautsprecher"));
        g.setColour (inkSoft); g.setFont (font (F::Med, 14.5f));
        g.drawFittedText (juce::String::fromUTF8 ("Mischt bis zu vier Impulsantworten. Ecken-Namen antippen zum Tauschen, der Würfel wählt vier zufällige. Reihenfolge mit ⇄ tauschen."),
                          faltNote, juce::Justification::topLeft, 7, 1.0f);

        // Unten
        panel (g, envRect.toFloat()); panel (g, filtRect.toFloat()); panel (g, outRect.toFloat());
        title (g, juce::String::fromUTF8 ("HÜLLKURVE"), envRect.getX() + 16, 706, {});
        title (g, "KREISCH-FILTER", filtRect.getX() + 54, 706, {});
        title (g, "AUSGANG", outRect.getX() + 16, 706, {});
        g.setColour (machine); g.fillRoundedRectangle (envMeter.toFloat(), 2.0f);
        g.setColour (hot); g.fillRect (envMeter.withWidth (juce::roundToInt ((float) envMeter.getWidth() * envLevel)));
        g.setColour (inkSoft); g.setFont (font (F::Med, 13.5f));
        g.drawFittedText ("Bewegt Drive und Ziel in der Presse und den Cutoff. Bei Kicks pumpt das im Takt.", envNote, juce::Justification::topLeft, 3, 1.0f);
        paintTabs (g);
    }

private:
    struct BandSet
    {
        juce::OwnedArray<Knob> knobs;
        Knob *drive = nullptr, *tilt = nullptr, *pmix = nullptr, *envDrive = nullptr, *envXY = nullptr, *level = nullptr, *fmix = nullptr, *width = nullptr, *pre = nullptr;
        Knob* style[4] {};
        Toggle *pOn = nullptr, *fOn = nullptr, *solo = nullptr, *mute = nullptr;
    };

    void title (juce::Graphics& g, const juce::String& t, int x, int y, const juce::String& sub)
    {
        const auto f = ui::font (ui::F::Stencil, 30.0f).withExtraKerningFactor (0.04f);
        g.setColour (ui::ink); g.setFont (f);
        g.drawText (t, x, y, 300, 32, juce::Justification::centredLeft);
        if (sub.isNotEmpty())
        {
            g.setColour (ui::inkSoft); g.setFont (ui::font (ui::F::Med, 14.5f));
            g.drawText (sub, x + juce::GlyphArrangement::getStringWidthInt (f, t) + 12, y, 220, 32, juce::Justification::centredLeft);
        }
    }

    void meter (juce::Graphics& g, juce::Rectangle<int> r, float v, const char* label)
    {
        g.setColour (ui::machine); g.fillRoundedRectangle (r.toFloat(), 3.0f);
        const float db = 20.0f * std::log10 (juce::jmax (1.0e-5f, v)), n = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        g.setGradientFill (juce::ColourGradient (ui::amber, (float) r.getX(), 0, juce::Colour (0xffff3b1f), (float) r.getRight(), 0, false));
        g.fillRect (juce::Rectangle<float> ((float) r.getX() + 4, (float) r.getBottom() - 9, ((float) r.getWidth() - 8) * n, 5.0f));
        g.setColour (ui::amber.withAlpha (0.8f)); g.setFont (ui::font (ui::F::Mono, 10.5f));
        g.drawText (juce::String (label) + " " + (db < -60 ? juce::String::fromUTF8 ("−∞") : juce::String (db, 1).replaceCharacter ('.', ',')), r.getX() + 5, r.getY() + 2, r.getWidth() - 6, 12, juce::Justification::left);
    }

    juce::String idleText() const
    {
        return proc.getProgramName (proc.getCurrentProgram()) + (modified ? juce::String::fromUTF8 (" (geändert)") : juce::String());
    }

    void layoutTabs()
    {
        const int nb = (int) proc.value (gBands) + 1;
        const int x0 = 30, w = 1180, gap = 8, tw = (w - gap * (nb - 1)) / nb;
        tabRects.clear();
        for (int b = 0; b < kBands; ++b)
        {
            auto& s = sets[(size_t) b];
            const bool vis = b < nb;
            if (! vis) { s.solo->setVisible (false); s.mute->setVisible (false); continue; }
            juce::Rectangle<int> r (x0 + b * (tw + gap), 264, tw, 36);
            tabRects.push_back (r);
            s.solo->setVisible (nb > 1); s.mute->setVisible (nb > 1);
            s.solo->setBounds (r.getRight() - 62, r.getY() + 7, 24, 22); s.mute->setBounds (r.getRight() - 32, r.getY() + 7, 24, 22);
        }
    }

    void paintTabs (juce::Graphics& g)
    {
        const int nb = (int) proc.value (gBands) + 1;
        static const char* n3[] = { "TIEF", "MITTE", "HOCH" };
        static const char* n2[] = { "TIEF", "HOCH" };
        for (int b = 0; b < (int) tabRects.size(); ++b)
        {
            auto r = tabRects[(size_t) b].toFloat();
            const bool sel = b == st.band;
            if (sel) { g.setColour (ui::ink); g.fillRoundedRectangle (r, 3.0f); }
            g.setColour (ui::ink); g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 2.0f);
            g.setColour (sel ? ui::concrete : ui::ink); g.setFont (ui::font (ui::F::Stencil, 22.0f).withExtraKerningFactor (0.05f));
            const juce::String nm = nb == 1 ? "VOLL" : juce::String (nb == 2 ? n2[b] : n3[b]);
            g.drawText (nm, (int) r.getX() + 10, (int) r.getY(), 100, (int) r.getHeight(), juce::Justification::centredLeft);
            const float lo = b == 0 ? 20.0f : (b == 1 ? proc.value (gX1) : juce::jmax (proc.value (gX2), proc.value (gX1) * 1.5f));
            const float hi = b == nb - 1 ? 20000.0f : (b == 0 ? proc.value (gX1) : juce::jmax (proc.value (gX2), proc.value (gX1) * 1.5f));
            g.setFont (ui::font (ui::F::Mono, 12.0f)); g.setColour ((sel ? ui::concrete : ui::ink).withAlpha (0.75f));
            g.drawText (ui::U (formatValue (gX1, lo)) + juce::String::fromUTF8 (" – ") + ui::U (formatValue (gX1, hi)), (int) r.getX() + 96, (int) r.getY(), (int) r.getWidth() - 170, (int) r.getHeight(), juce::Justification::centredLeft);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int b = 0; b < (int) tabRects.size(); ++b)
            if (tabRects[(size_t) b].contains (e.getPosition())) { st.band = b; bandChanged(); return; }
    }

    void bandChanged()
    {
        const int nb = (int) proc.value (gBands) + 1;
        st.band = juce::jlimit (0, nb - 1, st.band);
        for (int b = 0; b < kBands; ++b)
        {
            auto& s = sets[(size_t) b];
            const bool v = b == st.band;
            for (auto* k : s.knobs) k->setVisible (v);
            s.pOn->setVisible (v); s.fOn->setVisible (v);
        }
        layoutTabs(); refreshSwap();
        repaint();
    }

    void refreshSwap()
    {
        swapBtn.text = proc.value (bp (st.band, bOrder)) >= 0.5f ? "FAL-\nTUNG\nZU-\nERST" : "PRES-\nSE\nZU-\nERST";
        swapBtn.setTitle (proc.value (bp (st.band, bOrder)) >= 0.5f ? "Faltung zuerst, antippen zum Tauschen" : "Presse zuerst, antippen zum Tauschen");
        swapBtn.repaint();
    }

    void presetMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Presets");
        for (const auto& cat : presetCategories())
        {
            juce::PopupMenu sub;
            for (int i = 0; i < numPresets(); ++i)
                if (cat == presets()[(size_t) i].cat) sub.addItem (i + 1, ui::U (presets()[(size_t) i].name), true, i == proc.getCurrentProgram());
            menu.addSubMenu (ui::U (cat), sub);
        }
        menu.addSeparator();
        menu.addItem (1000, juce::String::fromUTF8 ("Würfeln"));
        juce::Component::SafePointer<MainPanel> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (r == 1000) safe->proc.randomPreset(); else safe->proc.loadPreset (r - 1);
        });
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { modified = false; refreshPreset(); bandChanged(); }

    void refreshPreset()
    {
        presetBtn.text = proc.getProgramName (proc.getCurrentProgram()) + (modified ? " *" : "") + juce::String::fromUTF8 ("  ▾");
        presetBtn.repaint(); repaint (readoutRect);
    }

    void timerCallback() override
    {
        spectrum.update();
        pPad->repaint(); fPad->repaint();
        for (auto& c : pCorners) c->repaint();
        for (auto& c : fCorners) c->repaint();
        for (auto* t : toggles) t->poll();
        bandSeg->poll(); typeSeg->poll();
        inLevel = juce::jmax (proc.engine.uiIn.load(), inLevel * 0.9f);
        outLevel = juce::jmax (proc.engine.uiOut.load(), outLevel * 0.9f);
        envLevel = proc.engine.uiEnv.load();
        repaint (inMeter); repaint (outMeter); repaint (envMeter);
        const int nb = (int) proc.value (gBands) + 1;
        if (nb != lastBands) { lastBands = nb; bandChanged(); }
        if (++tick % 15 == 0)
        {
            const bool m = proc.isModified();
            if (m != modified) { modified = m; refreshPreset(); }
            const bool order = proc.value (bp (st.band, bOrder)) >= 0.5f;
            if (order != lastOrder) { lastOrder = order; refreshSwap(); }
            repaint (mbRect.withTrimmedTop (160));
        }
        const bool showRead = juce::Time::getMillisecondCounterHiRes() < readoutUntil;
        if (showRead != readoutShown) { readoutShown = showRead; repaint (readoutRect); }
    }

    SchrottProcessor& proc;
    UiState st;
    PlainButton presetBtn, prevBtn, nextBtn, diceBtn, pDice, fDice, swapBtn;
    Spectrum spectrum;
    juce::OwnedArray<Toggle> toggles;
    juce::OwnedArray<Knob> globalKnobs;
    Toggle *original = nullptr, *target = nullptr, *filterOn = nullptr, *autoGain = nullptr, *limiter = nullptr;
    std::unique_ptr<Seg> bandSeg, typeSeg;
    std::array<BandSet, kBands> sets;
    Knob *envSens = nullptr, *envSmooth = nullptr, *cutoff = nullptr, *res = nullptr, *scream = nullptr, *fenv = nullptr, *input = nullptr, *mix = nullptr, *output = nullptr;
    std::unique_ptr<MorphPad> pPad, fPad;
    std::array<std::unique_ptr<CornerLabel>, 4> pCorners, fCorners;
    juce::Image texture;
    juce::Rectangle<int> readoutRect, mbRect, inMeter, outMeter, pRect, fRect, faltNote, envRect, filtRect, outRect, envMeter, envNote;
    std::vector<juce::Rectangle<int>> tabRects;
    juce::String readout;
    double readoutUntil = 0;
    bool readoutShown = false, modified = false, lastOrder = false;
    float inLevel = 0, outLevel = 0, envLevel = 0;
    int lastBands = -1, tick = 0;
};

// ============================================================ Editor
SchrottEditor::SchrottEditor (SchrottProcessor& p) : AudioProcessorEditor (&p)
{
    lnf = std::make_unique<SchrottLnf>();
    setLookAndFeel (lnf.get());
    panel = std::make_unique<MainPanel> (p);
    addAndMakeVisible (*panel);
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize ((int) (baseW * 0.85f), (int) (baseH * 0.85f));
}

SchrottEditor::~SchrottEditor()
{
    setLookAndFeel (nullptr);
    panel.reset();
}

void SchrottEditor::resized()
{
    panel->setBounds (0, 0, baseW, baseH);
    panel->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
}
