#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace Col {
    inline juce::Colour bg()        { return juce::Colour(0xff0d0d0du); }
    inline juce::Colour bg2()       { return juce::Colour(0xff1a1a1au); }
    inline juce::Colour bg3()       { return juce::Colour(0xff252525u); }
    inline juce::Colour border()    { return juce::Colour(0xff3a3a3au); }
    inline juce::Colour accent()    { return juce::Colour(0xffff3366u); }
    inline juce::Colour accentDim() { return juce::Colour(0x44ff3366u); }
    inline juce::Colour blue()      { return juce::Colour(0xff4fc3f7u); }
    inline juce::Colour green()     { return juce::Colour(0xff66bb6au); }
    inline juce::Colour amber()     { return juce::Colour(0xffffa726u); }
    inline juce::Colour purple()    { return juce::Colour(0xffab47bcu); }
    inline juce::Colour teal()      { return juce::Colour(0xff26c6dau); }
    inline juce::Colour textW()     { return juce::Colour(0xfff0f0f0u); }
    inline juce::Colour textG()     { return juce::Colour(0xff909090u); }
    inline juce::Colour textD()     { return juce::Colour(0xff707070u); }
}

// ── Knob ────────────────────────────────────────────────────────────────────
class Knob : public juce::Component, public juce::SettableTooltipClient {
public:
    Knob(const juce::String& n, const juce::String& tip, float init, float lo, float hi, float st, juce::Colour c)
        : name(n), defaultVal(init), val(init), mn(lo), mx(hi), step(st), col(c) { setTooltip(tip); }

    float get() const { return val; }
    void  set(float v) { val = std::clamp(v, mn, mx); repaint(); }

    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat();
        float sz = std::max(4.f, std::min(b.getWidth(), b.getHeight() - 16));
        float cx = b.getCentreX(), cy = b.getY() + sz * 0.5f + 2;
        float r = sz * 0.34f;

        constexpr float sa = 2.35619f;
        constexpr float ea = 7.06858f;
        float n = (val - mn) / std::max(0.001f, mx - mn);
        float va = sa + n * (ea - sa);

        juce::Path bg;
        bg.addCentredArc(cx, cy, r + 2, r + 2, 0, sa, ea, true);
        g.setColour(Col::bg3());
        g.strokePath(bg, juce::PathStrokeType(4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (n > 0.005f) {
            juce::Path arc;
            arc.addCentredArc(cx, cy, r + 2, r + 2, 0, sa, va, true);
            g.setColour(col.withAlpha(0.25f));
            g.strokePath(arc, juce::PathStrokeType(6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(col);
            g.strokePath(arc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(Col::bg2());
        g.fillEllipse(cx - r * 0.55f, cy - r * 0.55f, r * 1.1f, r * 1.1f);

        float px = cx + std::cos(va) * r * 0.35f;
        float py = cy + std::sin(va) * r * 0.35f;
        float px2 = cx + std::cos(va) * r * 0.7f;
        float py2 = cy + std::sin(va) * r * 0.7f;
        g.setColour(col);
        g.drawLine(px, py, px2, py2, 2.f);

        g.setColour(Col::textW());
        g.setFont(10.f);
        juce::String vt = mx <= 1.01f ? juce::String(val, 2) : juce::String((int)val);
        g.drawText(vt, (int)(cx - 20), (int)(cy - 5), 40, 12, juce::Justification::centred);

        g.setColour(Col::textG());
        g.setFont(9.f);
        g.drawText(name, 0, (int)(b.getBottom() - 14), getWidth(), 12, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override { lastY = e.y; }
    void mouseDrag(const juce::MouseEvent& e) override {
        float d = (float)(lastY - e.y) * step * (e.mods.isShiftDown() ? 0.1f : 1.f);
        val = std::clamp(val + d, mn, mx);
        lastY = e.y;
        if (onChange) onChange();
        repaint();
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override {
        val = std::clamp(val + w.deltaY * step * 5.f, mn, mx);
        if (onChange) onChange();
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override {
        val = defaultVal;
        if (onChange) onChange();
        repaint();
    }

    std::function<void()> onChange;
private:
    juce::String name;
    float defaultVal, val, mn, mx, step;
    juce::Colour col;
    int lastY = 0;
};

// ── Editor ──────────────────────────────────────────────────────────────────
class SnareProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer {
public:
    SnareProcessorEditor(SnareProcessor&);
    ~SnareProcessorEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

private:
    SnareProcessor& proc;
    int selGenre = 5;
    int hoverGenre = -1;

    juce::TooltipWindow tooltipWindow { this };

    Knob kBpm      {"BPM",     "Tempo in beats per minute",                90,   40, 300, 1,    Col::accent()};
    Knob kBars     {"BARS",    "Number of bars to generate",                4,    1,  16, 1,    Col::accent()};
    Knob kComplex  {"CMPLX",   "Pattern complexity - more = busier fills", 0.4f,  0,  1, 0.01f, Col::blue()};
    Knob kDensity  {"DENS",    "Hit density - how many notes per bar",     0.4f,  0,  1, 0.01f, Col::blue()};
    Knob kSynco    {"SYNCO",   "Syncopation - off-beat hit placement",     0.3f,  0,  1, 0.01f, Col::teal()};
    Knob kSwing    {"SWING",   "Swing feel - delays off-beat notes",       0.0f,  0,  1, 0.01f, Col::teal()};
    Knob kHuman    {"HUMAN",   "Humanize - velocity & timing variation",   0.3f,  0,  1, 0.01f, Col::amber()};
    Knob kAccent   {"ACCENT",  "Accent strength on strong beats",          0.6f,  0,  1, 0.01f, Col::accent()};
    Knob kGhost    {"GHOST",   "Ghost note amount - soft in-between hits", 0.3f,  0,  1, 0.01f, Col::purple()};
    Knob kFills    {"FILL",    "Fill frequency - drum fills at bar ends",  0.2f,  0,  1, 0.01f, Col::amber()};
    Knob kVar      {"VAR",     "Variation between bars",                   0.2f,  0,  1, 0.01f, Col::green()};
    Knob kMotif    {"MOTIF",   "Motif strength - pattern consistency",     0.7f,  0,  1, 0.01f, Col::green()};
    Knob kLoose    {"LOOSE",   "Looseness - timing imprecision",           0.2f,  0,  1, 0.01f, Col::amber()};
    Knob kProb     {"PROB",    "Probability - chance each hit plays",      1.0f,  0,  1, 0.01f, Col::blue()};
    Knob kFlam     {"FLAM",    "Flam - double-stroke before hits",        0.0f,  0,  1, 0.01f, Col::purple()};
    Knob kVMin     {"V.LO",    "Minimum velocity (softest hit)",            30,    1, 126, 1,    Col::textG()};
    Knob kVMax     {"V.HI",    "Maximum velocity (hardest hit)",           127,    2, 127, 1,    Col::textW()};

    std::vector<Knob*> knobs;

    juce::TextButton btnGen  {"GENERATE"};
    juce::TextButton btnPlay {"PLAY"};
    juce::TextButton btnExp  {"EXPORT MIDI"};
    juce::TextButton btnDice {"NEW SEED"};
    juce::ToggleButton btnBB {"Backbeat Lock"};

    void sync();
    void generate();
    void applyGenreDefaults(const snare::GenreProfile& gp);
    void paintHeader(juce::Graphics&);
    void paintGenres(juce::Graphics&, juce::Rectangle<int>);
    void paintGrid(juce::Graphics&, juce::Rectangle<int>);
    void paintScores(juce::Graphics&, juce::Rectangle<int>);
    void paintLegend(juce::Graphics&, juce::Rectangle<int>);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SnareProcessorEditor)
};
