#include "PluginEditor.h"

SnareProcessorEditor::SnareProcessorEditor(SnareProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setSize(940, 700);
    setResizable(true, true);
    setResizeLimits(800, 600, 1400, 1000);

    knobs = { &kBpm, &kBars, &kComplex, &kDensity, &kSynco, &kSwing,
              &kHuman, &kAccent, &kGhost, &kFills, &kVar, &kMotif,
              &kLoose, &kProb, &kFlam, &kVMin, &kVMax };
    for (auto* k : knobs) {
        k->onChange = [this] { generate(); };
        addAndMakeVisible(k);
    }

    auto sty = [](juce::TextButton& b, juce::Colour bg, juce::Colour fg) {
        b.setColour(juce::TextButton::buttonColourId, bg);
        b.setColour(juce::TextButton::textColourOffId, fg);
    };
    sty(btnGen,  Col::accent(), Col::textW());
    sty(btnPlay, Col::bg3(),    Col::green());
    sty(btnExp,  Col::bg3(),    Col::blue());
    sty(btnDice, Col::bg3(),    Col::amber());

    btnGen.onClick = [this] { generate(); };
    btnPlay.onClick = [this, sty] {
        if (proc.isPlaying()) {
            proc.stopPlayback();
            btnPlay.setButtonText("PLAY");
            sty(btnPlay, Col::bg3(), Col::green());
        } else {
            proc.startPlayback();
            btnPlay.setButtonText("STOP");
            sty(btnPlay, Col::accent().darker(0.4f), Col::textW());
        }
    };
    btnExp.onClick = [this] {
        auto ch = std::make_shared<juce::FileChooser>("Save MIDI", juce::File{}, "*.mid");
        ch->launchAsync(juce::FileBrowserComponent::saveMode,
            [this, ch](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f != juce::File{}) proc.exportMidi(f);
            });
    };
    btnDice.onClick = [this] {
        proc.params.seed = juce::Random::getSystemRandom().nextInt(99999);
        generate();
    };

    addAndMakeVisible(btnGen);
    addAndMakeVisible(btnPlay);
    addAndMakeVisible(btnExp);
    addAndMakeVisible(btnDice);

    btnBB.setColour(juce::ToggleButton::textColourId, Col::textG());
    btnBB.setColour(juce::ToggleButton::tickColourId, Col::accent());
    btnBB.setToggleState(proc.params.backbeatLock, juce::dontSendNotification);
    btnBB.onClick = [this] { proc.params.backbeatLock = btnBB.getToggleState(); generate(); };
    addAndMakeVisible(btnBB);

    // Init knobs from params
    auto& pa = proc.params;
    kBpm.set((float)pa.bpm); kBars.set((float)pa.bars);
    kComplex.set(pa.complexity); kDensity.set(pa.density);
    kSynco.set(pa.syncopation); kSwing.set(pa.swing);
    kHuman.set(pa.humanize); kAccent.set(pa.accentStr);
    kGhost.set(pa.ghostNotes); kFills.set(pa.fillFreq);
    kVar.set(pa.variation); kMotif.set(pa.motifStr);
    kLoose.set(pa.looseness); kProb.set(pa.probability);
    kFlam.set(pa.flam); kVMin.set((float)pa.velMin); kVMax.set((float)pa.velMax);

    auto& names = snare::genreNames();
    for (int i = 0; i < (int)names.size(); ++i)
        if (names[i] == pa.genre) { selGenre = i; break; }

    startTimerHz(30);
}

void SnareProcessorEditor::sync() {
    auto& pa = proc.params;
    pa.bpm = (int)kBpm.get(); pa.bars = (int)kBars.get();
    pa.complexity = kComplex.get(); pa.density = kDensity.get();
    pa.syncopation = kSynco.get(); pa.swing = kSwing.get();
    pa.humanize = kHuman.get(); pa.accentStr = kAccent.get();
    pa.ghostNotes = kGhost.get(); pa.fillFreq = kFills.get();
    pa.variation = kVar.get(); pa.motifStr = kMotif.get();
    pa.looseness = kLoose.get(); pa.probability = kProb.get();
    pa.flam = kFlam.get();
    pa.velMin = (int)kVMin.get(); pa.velMax = (int)kVMax.get();
    if (pa.velMin >= pa.velMax) {
        pa.velMax = pa.velMin + 1;
        kVMax.set((float)pa.velMax);
    }
    pa.backbeatLock = btnBB.getToggleState();
}

void SnareProcessorEditor::generate() {
    sync();
    proc.regenerate();
    repaint();
}

void SnareProcessorEditor::applyGenreDefaults(const snare::GenreProfile& gp) {
    kBpm.set((float)(gp.bpmLo + gp.bpmHi) / 2.f);
    kSwing.set(gp.defaultSwing);
    kGhost.set(gp.ghostAffinity * 0.5f);
    kSynco.set(gp.syncopationBias);
    kDensity.set(gp.densityBias);
    kComplex.set(0.3f + gp.densityBias * 0.4f);
}

void SnareProcessorEditor::timerCallback() {
    if (proc.isPlaying()) repaint();
}

// ── Paint ───────────────────────────────────────────────────────────────────
void SnareProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(Col::bg());

    auto dd = proc.getDisplayData();

    paintHeader(g);
    paintGenres(g, {14, 50, getWidth() - 28, 26});

    int knobAreaY = 82;
    g.setColour(Col::border());
    g.drawHorizontalLine(knobAreaY - 2, 14, (float)getWidth() - 14);
    g.setColour(Col::textD());
    g.setFont(juce::Font(10.f).boldened());
    g.drawText("STRUCTURE", 14, knobAreaY, 80, 12, juce::Justification::left);
    g.drawText("FEEL", 156, knobAreaY, 50, 12, juce::Justification::left);
    g.drawText("VELOCITY", 14, knobAreaY + 80, 80, 12, juce::Justification::left);
    g.drawText("TEXTURE", 156, knobAreaY + 80, 80, 12, juce::Justification::left);

    int gridTop = 260;
    g.setColour(Col::border());
    g.drawHorizontalLine(gridTop - 6, 14, (float)getWidth() - 14);
    g.setColour(Col::textD());
    g.setFont(juce::Font(10.f).boldened());
    g.drawText("PATTERN", 14, gridTop - 4, 80, 12, juce::Justification::left);

    if (dd) {
        int total = (int)dd->hits.size();
        int pri = 0, gho = 0, fil = 0, acc = 0;
        for (auto& h : dd->hits) {
            if (h.hitType == snare::Accent) acc++;
            else if (h.hitType == snare::Primary) pri++;
            else if (h.hitType == snare::Ghost) gho++;
            else if (h.hitType == snare::Fill) fil++;
        }
        g.setColour(Col::textG());
        g.setFont(9.f);
        g.drawText(juce::String(total) + " hits  |  " + juce::String(acc + pri) + " primary  " +
                   juce::String(gho) + " ghost  " + juce::String(fil) + " fill",
                   90, gridTop - 4, 400, 12, juce::Justification::left);
    }

    int gridH = getHeight() - gridTop - 16;
    int scoreW = 200;
    int gridW = getWidth() - scoreW - 34;
    paintGrid(g, {14, gridTop + 10, gridW, gridH - 10});

    if (dd) {
        paintScores(g, {getWidth() - scoreW - 10, gridTop + 10, scoreW, gridH - 80});
        paintLegend(g, {getWidth() - scoreW - 10, getHeight() - 80, scoreW, 70});
    }
}

void SnareProcessorEditor::paintHeader(juce::Graphics& g) {
    g.setColour(Col::bg2());
    g.fillRect(0, 0, getWidth(), 44);
    g.setColour(Col::accent());
    g.fillRect(0, 42, getWidth(), 2);

    g.setColour(Col::textW());
    g.setFont(juce::Font(15.f).boldened());
    g.drawText("SNARE RHYTHM GENERATOR", 14, 4, 280, 36, juce::Justification::centredLeft);

    g.setColour(Col::textD());
    g.setFont(9.f);
    g.drawText("VST3  v1.1", 290, 14, 80, 16, juce::Justification::left);

    g.setColour(Col::textG());
    g.setFont(9.f);
    juce::String seedStr = "SEED " + (proc.params.seed >= 0 ? juce::String(proc.params.seed) : juce::String("AUTO"));
    g.drawText(seedStr, getWidth() - 130, 4, 120, 36, juce::Justification::centredRight);
}

void SnareProcessorEditor::paintGenres(juce::Graphics& g, juce::Rectangle<int> area) {
    auto& names = snare::genreNames();
    int n = (int)names.size();
    float bw = (float)(area.getWidth() - (n - 1) * 3) / (float)n;

    for (int i = 0; i < n; ++i) {
        float x = area.getX() + i * (bw + 3);
        auto r = juce::Rectangle<float>(x, (float)area.getY(), bw, (float)area.getHeight());
        bool sel = (i == selGenre);
        bool hov = (i == hoverGenre && !sel);

        if (sel) {
            g.setColour(Col::accent());
            g.fillRoundedRectangle(r, 3.f);
            g.setColour(Col::textW());
        } else if (hov) {
            g.setColour(Col::bg3().brighter(0.15f));
            g.fillRoundedRectangle(r, 3.f);
            g.setColour(Col::textW().withAlpha(0.7f));
        } else {
            g.setColour(Col::bg3());
            g.fillRoundedRectangle(r, 3.f);
            g.setColour(Col::textG());
        }
        g.setFont(juce::Font(8.5f).boldened());
        g.drawText(juce::String(names[i]).toUpperCase(), r, juce::Justification::centred);
    }
}

void SnareProcessorEditor::paintGrid(juce::Graphics& g, juce::Rectangle<int> area) {
    auto af = area.toFloat();
    g.setColour(Col::bg2());
    g.fillRoundedRectangle(af, 4.f);
    g.setColour(Col::border());
    g.drawRoundedRectangle(af, 4.f, 1.f);

    auto dd = proc.getDisplayData();
    if (!dd || dd->hits.empty()) {
        g.setColour(Col::textD());
        g.setFont(13.f);
        g.drawText("Click GENERATE or pick a genre", af, juce::Justification::centred);
        return;
    }

    int bars = proc.params.bars;
    int spb = proc.params.resolution;
    int bpb = proc.params.timeSigNum;
    int spbr = spb * bpb;

    float pad = 4.f;
    float iw = af.getWidth() - pad * 2;
    float ih = af.getHeight() - pad * 2;
    float rowH = ih / (float)bars;
    float colW = iw / (float)spbr;

    float ox = af.getX() + pad;
    float oy = af.getY() + pad;

    // Clip to grid area
    g.saveState();
    g.reduceClipRegion(area);

    // Beat lines
    for (int beat = 0; beat <= bpb; ++beat) {
        float x = ox + beat * spb * colW;
        g.setColour(beat == 0 || beat == bpb ? Col::border() : juce::Colour(0x30ffffffu));
        g.drawVerticalLine((int)x, oy, oy + ih);
    }

    // Sub lines
    for (int s = 0; s < spbr; ++s) {
        if (s % spb != 0) {
            float x = ox + s * colW;
            g.setColour(juce::Colour(0x10ffffffu));
            g.drawVerticalLine((int)x, oy, oy + ih);
        }
    }

    // Bar lines
    for (int b = 0; b <= bars; ++b) {
        float y = oy + b * rowH;
        g.setColour(juce::Colour(0x20ffffffu));
        g.drawHorizontalLine((int)y, ox, ox + iw);
    }

    // Bar numbers
    g.setColour(Col::textD());
    g.setFont(8.f);
    for (int b = 0; b < bars; ++b)
        g.drawText(juce::String(b + 1), (int)ox + 2, (int)(oy + b * rowH + 1), 10, 10, juce::Justification::left);

    // Hits
    float m = 1.f;
    for (auto& h : dd->hits) {
        if (h.bar < 0 || h.bar >= bars) continue;
        float x = ox + h.tick * colW + m;
        float y = oy + h.bar * rowH + m;
        float cw = std::max(1.f, colW - m * 2);
        float ch = std::max(1.f, rowH - m * 2);

        float alpha = std::clamp((float)h.velocity / 127.f, 0.25f, 1.f);
        juce::Colour c;
        switch (h.hitType) {
            case snare::Accent:  c = Col::accent(); break;
            case snare::Primary: c = Col::blue(); break;
            case snare::Ghost:   c = Col::purple(); break;
            case snare::Fill:    c = Col::amber(); break;
            case snare::Flam:    c = Col::teal(); break;
            default:             c = Col::textG(); break;
        }

        // Soft glow (clipped to grid)
        g.setColour(c.withAlpha(alpha * 0.08f));
        g.fillRoundedRectangle(x - 2, y - 2, cw + 4, ch + 4, 3.f);

        // Cell
        g.setColour(c.withAlpha(alpha * 0.85f));
        g.fillRoundedRectangle(x, y, cw, ch, 2.f);

        // Velocity indicator
        g.setColour(juce::Colours::white.withAlpha(alpha * 0.3f));
        g.fillRect(x + 1, y, cw - 2, std::min(2.f, ch * 0.12f));
    }

    // Playback cursor
    if (proc.isPlaying()) {
        float totalQ = (float)(bars * bpb);
        float px = ox + (proc.getPlaybackPos() / totalQ) * iw;
        g.setColour(Col::green().withAlpha(0.9f));
        g.fillRect(px - 1, oy, 2.f, ih);
        g.setColour(Col::green().withAlpha(0.15f));
        g.fillRect(px - 4, oy, 8.f, ih);
    }

    g.restoreState();
}

void SnareProcessorEditor::paintScores(juce::Graphics& g, juce::Rectangle<int> area) {
    auto dd = proc.getDisplayData();
    if (!dd) return;

    g.setColour(Col::textD());
    g.setFont(juce::Font(10.f).boldened());
    g.drawText("QUALITY", area.getX(), area.getY() - 2, 60, 12, juce::Justification::left);

    auto& s = dd->scores;
    struct R { const char* n; float v; juce::Colour c; };
    R rows[] = {
        {"Density",   s.density,  Col::blue()},
        {"Backbeat",  s.backbeat, Col::accent()},
        {"Dynamics",  s.dynamics, Col::amber()},
        {"Ghosts",    s.ghostBal, Col::purple()},
        {"Variation", s.variation,Col::green()},
        {"Coverage",  s.coverage, Col::teal()},
    };

    int y = area.getY() + 14;
    int lw = 55;
    int bx = area.getX() + lw + 4;
    int bw = area.getWidth() - lw - 38;

    for (auto& r : rows) {
        g.setColour(Col::textG());
        g.setFont(9.f);
        g.drawText(r.n, area.getX(), y, lw, 14, juce::Justification::right);

        g.setColour(Col::bg3());
        g.fillRoundedRectangle((float)bx, (float)y + 3, (float)bw, 8.f, 2.f);
        float barW = (float)bw * std::clamp(r.v, 0.f, 1.f);
        g.setColour(r.c.withAlpha(0.65f));
        g.fillRoundedRectangle((float)bx, (float)y + 3, barW, 8.f, 2.f);

        g.setColour(Col::textG());
        g.setFont(8.f);
        g.drawText(juce::String((int)(std::clamp(r.v, 0.f, 1.f) * 100)), bx + bw + 2, y, 30, 14, juce::Justification::left);
        y += 18;
    }

    y += 6;
    g.setColour(Col::accent());
    g.setFont(juce::Font(10.f).boldened());
    g.drawText("OVERALL", area.getX(), y, lw, 16, juce::Justification::right);
    g.setColour(Col::bg3());
    g.fillRoundedRectangle((float)bx, (float)y + 3, (float)bw, 10.f, 3.f);
    float ov = std::clamp(s.overall, 0.f, 1.f);
    g.setColour(Col::accent().withAlpha(0.75f));
    g.fillRoundedRectangle((float)bx, (float)y + 3, (float)bw * ov, 10.f, 3.f);
    g.setColour(Col::textW());
    g.setFont(juce::Font(9.f).boldened());
    g.drawText(juce::String((int)(ov * 100)) + "%", bx + bw + 2, y, 34, 16, juce::Justification::left);
}

void SnareProcessorEditor::paintLegend(juce::Graphics& g, juce::Rectangle<int> area) {
    struct L { const char* n; juce::Colour c; };
    L items[] = {{"ACC", Col::accent()}, {"PRI", Col::blue()}, {"GHO", Col::purple()},
                 {"FIL", Col::amber()}, {"FLM", Col::teal()}};

    g.setColour(Col::border());
    g.drawHorizontalLine(area.getY(), (float)area.getX(), (float)area.getRight());

    int x = area.getX();
    int y = area.getY() + 8;
    int spacing = std::min(40, area.getWidth() / 5);
    for (auto& l : items) {
        g.setColour(l.c);
        g.fillRoundedRectangle((float)x, (float)y + 1, 8.f, 8.f, 2.f);
        g.setColour(Col::textG());
        g.setFont(8.f);
        g.drawText(l.n, x + 10, y, 28, 12, juce::Justification::left);
        x += spacing;
    }
}

// ── Layout ──────────────────────────────────────────────────────────────────
void SnareProcessorEditor::resized() {
    int w = getWidth();
    int ks = 62, kg = 2;

    auto place = [&](Knob& k, int col, int row) {
        k.setBounds(14 + col * (ks + kg), 96 + row * 78, ks, ks + 14);
    };

    place(kBpm, 0, 0);    place(kBars, 1, 0);
    place(kComplex, 2, 0); place(kDensity, 3, 0);
    place(kSynco, 4, 0);  place(kSwing, 5, 0);
    place(kHuman, 6, 0);  place(kAccent, 7, 0);  place(kGhost, 8, 0);

    place(kVMin, 0, 1);   place(kVMax, 1, 1);
    place(kFills, 2, 1);  place(kVar, 3, 1);
    place(kMotif, 4, 1);  place(kLoose, 5, 1);
    place(kProb, 6, 1);   place(kFlam, 7, 1);

    int rx = w - 186;
    btnBB.setBounds(rx, 100, 140, 20);
    btnDice.setBounds(rx, 126, 172, 24);
    btnGen.setBounds(rx, 162, 84, 30);
    btnPlay.setBounds(rx + 88, 162, 84, 30);
    btnExp.setBounds(rx, 196, 172, 24);
}

// ── Mouse ───────────────────────────────────────────────────────────────────
void SnareProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    int gy = 50, gh = 26;
    if (e.y >= gy && e.y < gy + gh) {
        auto& names = snare::genreNames();
        int n = (int)names.size();
        int gw = getWidth() - 28;
        float bw = (float)(gw - (n - 1) * 3) / (float)n;
        int idx = (int)((e.x - 14) / (bw + 3));
        if (idx >= 0 && idx < n) {
            selGenre = idx;
            proc.params.genre = names[idx];
            applyGenreDefaults(snare::getGenre(names[idx]));
            generate();
        }
    }
}

void SnareProcessorEditor::mouseMove(const juce::MouseEvent& e) {
    int gy = 50, gh = 26;
    int oldHover = hoverGenre;
    if (e.y >= gy && e.y < gy + gh) {
        auto& names = snare::genreNames();
        int n = (int)names.size();
        int gw = getWidth() - 28;
        float bw = (float)(gw - (n - 1) * 3) / (float)n;
        hoverGenre = (int)((e.x - 14) / (bw + 3));
        if (hoverGenre < 0 || hoverGenre >= n) hoverGenre = -1;
    } else {
        hoverGenre = -1;
    }
    if (hoverGenre != oldHover) repaint();
}

juce::AudioProcessorEditor* SnareProcessor::createEditor() { return new SnareProcessorEditor(*this); }
