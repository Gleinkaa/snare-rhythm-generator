#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ── Blue Dark Theme ────────────────────────────────────────────────────────
namespace Col {
    inline juce::Colour bg()        { return juce::Colour(0xff0a0e14u); }  // deep dark blue-black
    inline juce::Colour bg2()       { return juce::Colour(0xff111822u); }  // panel bg
    inline juce::Colour bg3()       { return juce::Colour(0xff1a2332u); }  // element bg
    inline juce::Colour border()    { return juce::Colour(0xff2a3545u); }
    inline juce::Colour accent()    { return juce::Colour(0xff00b4d8u); }  // cyan accent
    inline juce::Colour accentDim() { return juce::Colour(0x4400b4d8u); }
    inline juce::Colour blue()      { return juce::Colour(0xff48cae4u); }  // light cyan
    inline juce::Colour green()     { return juce::Colour(0xff52b788u); }
    inline juce::Colour amber()     { return juce::Colour(0xffffb347u); }  // orange for accents
    inline juce::Colour purple()    { return juce::Colour(0xff9d8df1u); }
    inline juce::Colour teal()      { return juce::Colour(0xff2ec4b6u); }
    inline juce::Colour textW()     { return juce::Colour(0xffe8edf3u); }
    inline juce::Colour textG()     { return juce::Colour(0xff7a8a9eu); }
    inline juce::Colour textD()     { return juce::Colour(0xff4a5568u); }
    inline juce::Colour sampleOk()  { return juce::Colour(0xff52b788u); }  // green border when loaded
    inline juce::Colour sampleEmpty(){ return juce::Colour(0xff4a5568u); } // gray dashed border
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
        juce::String vt = mx <= 2.01f && mn < 1.f ? juce::String(val, 2)
                        : mx <= 127.f && step >= 0.9f ? juce::String((int)val)
                        : juce::String(val, 2);
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
class SnareProcessorEditor : public juce::AudioProcessorEditor,
                              public juce::Timer,
                              public juce::FileDragAndDropTarget {
public:
    SnareProcessorEditor(SnareProcessor&);
    ~SnareProcessorEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    // Drag and drop for sample loading
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { dragOver = false; repaint(); }

private:
    SnareProcessor& proc;
    int selGenre = 5;
    int hoverGenre = -1;
    bool dragOver = false;

    juce::TooltipWindow tooltipWindow { this };

    // Row 1: Rhythm
    Knob kComplex  {"CMPLX",   "Pattern complexity",                       0.3f,  0,  1, 0.01f, Col::accent()};
    Knob kDensity  {"DENSE",   "Hit density",                              0.4f,  0,  1, 0.01f, Col::accent()};
    Knob kSynco    {"SYNC",    "Syncopation",                              0.2f,  0,  1, 0.01f, Col::blue()};
    Knob kSwing    {"SWING",   "Swing feel",                               0.0f,  0,  1, 0.01f, Col::blue()};

    // Row 1: Feel
    Knob kHuman    {"HUMAN",   "Humanize velocity variation",              0.15f, 0,  1, 0.01f, Col::teal()};
    Knob kAccent   {"ACCNT",   "Accent strength on beats 2 and 4",        0.7f,  0,  1, 0.01f, Col::amber()};
    Knob kGhost    {"GHOST",   "Ghost note amount",                        0.3f,  0,  1, 0.01f, Col::purple()};
    Knob kLoose    {"LOOSE",   "Timing looseness",                         0.1f,  0,  1, 0.01f, Col::teal()};

    // Row 2: Structure
    Knob kVar      {"VARI",    "Bar-to-bar variation",                     0.2f,  0,  1, 0.01f, Col::green()};
    Knob kMotif    {"MOTIF",   "Motif strength",                           0.7f,  0,  1, 0.01f, Col::green()};
    Knob kFills    {"FILL",    "Fill frequency at phrase boundaries",      0.15f, 0,  1, 0.01f, Col::amber()};
    Knob kFlam     {"FLAM",    "Flam double-stroke",                       0.0f,  0,  1, 0.01f, Col::purple()};

    // Row 2: Output
    Knob kVMin     {"V.MIN",   "Minimum velocity",                          79,    1, 126, 1,    Col::textG()};
    Knob kVMax     {"V.MAX",   "Maximum velocity",                         127,    2, 127, 1,    Col::textW()};
    Knob kGate     {"GATE",    "Note duration multiplier (0.25x-2.0x)",    1.0f, 0.25f, 2.0f, 0.01f, Col::accent()};
    Knob kSVol     {"S.VOL",   "Sample player volume",                     0.8f,  0,  1, 0.01f, Col::green()};

    // Right column
    Knob kBars     {"BARS",    "Phrase length in bars",                      8,    1,  16, 1,    Col::accent()};
    Knob kNote     {"NOTE",    "MIDI note number (38=snare)",               38,    0, 127, 1,    Col::blue()};
    Knob kSeed     {"SEED",    "Random seed (0-9999)",                      42,    0, 9999, 1,   Col::amber()};

    std::vector<Knob*> knobs;

    juce::TextButton btnGen  {"GENERATE"};
    juce::TextButton btnPlay {"PLAY"};
    juce::TextButton btnExp  {"EXPORT MIDI"};
    juce::TextButton btnSave {"SAVE"};
    juce::TextButton btnLoad {"LOAD"};
    juce::TextButton btnLoadSample {"LOAD SAMPLE"};
    juce::ToggleButton btnBB {"Backbeat Lock"};

    void sync();
    void generate();
    void applyGenreDefaults(const snare::GenreProfile& gp);
    void paintHeader(juce::Graphics&);
    void paintGenres(juce::Graphics&, juce::Rectangle<int>);
    void paintSampleZone(juce::Graphics&, juce::Rectangle<int>);
    void paintGrid(juce::Graphics&, juce::Rectangle<int>);
    void paintScores(juce::Graphics&, juce::Rectangle<int>);
    void paintLegend(juce::Graphics&, juce::Rectangle<int>);
    void paintFooter(juce::Graphics&, juce::Rectangle<int>);
    void syncFromProcessor();

    juce::Rectangle<int> sampleZoneBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SnareProcessorEditor)
};
