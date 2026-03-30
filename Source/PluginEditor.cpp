#include "PluginEditor.h"

SnareProcessorEditor::SnareProcessorEditor(SnareProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setSize(960, 740);
    setResizable(true, true);
    setResizeLimits(860, 660, 1400, 1000);

    knobs = { &kComplex, &kDensity, &kSynco, &kSwing,
              &kHuman, &kAccent, &kGhost, &kLoose,
              &kVar, &kMotif, &kFills, &kFlam,
              &kVMin, &kVMax, &kGate, &kSVol,
              &kBars, &kNote, &kSeed };

    for (auto* k : knobs) {
        k->onChange = [this] { generate(); };
        addAndMakeVisible(k);
    }

    // Button styling
    auto sty = [](juce::TextButton& b, juce::Colour bg, juce::Colour fg) {
        b.setColour(juce::TextButton::buttonColourId, bg);
        b.setColour(juce::TextButton::textColourOffId, fg);
    };
    sty(btnGen,        Col::accent(),            Col::textW());
    sty(btnPlay,       Col::bg3(),               Col::green());
    sty(btnExp,        Col::bg3(),               Col::blue());
    sty(btnSave,       Col::bg3(),               Col::amber());
    sty(btnLoad,       Col::bg3(),               Col::amber());
    sty(btnLoadSample, Col::bg3(),               Col::green());

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
    btnSave.onClick = [this] {
        auto ch = std::make_shared<juce::FileChooser>("Save Preset", juce::File{}, "*.srpreset");
        ch->launchAsync(juce::FileBrowserComponent::saveMode,
            [this, ch](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f != juce::File{}) proc.savePreset(f);
            });
    };
    btnLoad.onClick = [this] {
        auto ch = std::make_shared<juce::FileChooser>("Load Preset", juce::File{}, "*.srpreset");
        ch->launchAsync(juce::FileBrowserComponent::openMode,
            [this, ch](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f != juce::File{}) {
                    proc.loadPreset(f);
                    syncFromProcessor();
                }
            });
    };
    btnLoadSample.onClick = [this] {
        auto ch = std::make_shared<juce::FileChooser>("Load Sample", juce::File{}, "*.wav;*.aiff;*.mp3;*.flac;*.ogg");
        ch->launchAsync(juce::FileBrowserComponent::openMode,
            [this, ch](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f != juce::File{}) { proc.loadSample(f); repaint(); }
            });
    };

    addAndMakeVisible(btnGen);
    addAndMakeVisible(btnPlay);
    addAndMakeVisible(btnExp);
    addAndMakeVisible(btnSave);
    addAndMakeVisible(btnLoad);
    addAndMakeVisible(btnLoadSample);

    btnBB.setColour(juce::ToggleButton::textColourId, Col::textG());
    btnBB.setColour(juce::ToggleButton::tickColourId, Col::accent());
    btnBB.setToggleState(proc.params.backbeatLock, juce::dontSendNotification);
    btnBB.onClick = [this] { proc.params.backbeatLock = btnBB.getToggleState(); generate(); };
    addAndMakeVisible(btnBB);

    // Init knobs from params
    syncFromProcessor();

    auto& names = snare::genreNames();
    for (int i = 0; i < (int)names.size(); ++i)
        if (names[i] == proc.params.genre) { selGenre = i; break; }

    startTimerHz(60);
}

void SnareProcessorEditor::syncFromProcessor() {
    auto& pa = proc.params;
    kComplex.set(pa.complexity); kDensity.set(pa.density);
    kSynco.set(pa.syncopation); kSwing.set(pa.swing);
    kHuman.set(pa.humanize); kAccent.set(pa.accentStr);
    kGhost.set(pa.ghostNotes); kLoose.set(pa.looseness);
    kVar.set(pa.variation); kMotif.set(pa.motifStr);
    kFills.set(pa.fillFreq); kFlam.set(pa.flam);
    kVMin.set((float)pa.velMin); kVMax.set((float)pa.velMax);
    kGate.set(proc.gateLength); kSVol.set(proc.sampleVol);
    kBars.set((float)pa.bars); kNote.set((float)pa.snareNote);
    kSeed.set((float)pa.seed);
    btnBB.setToggleState(pa.backbeatLock, juce::dontSendNotification);
}

void SnareProcessorEditor::sync() {
    auto& pa = proc.params;
    pa.complexity = kComplex.get(); pa.density = kDensity.get();
    pa.syncopation = kSynco.get(); pa.swing = kSwing.get();
    pa.humanize = kHuman.get(); pa.accentStr = kAccent.get();
    pa.ghostNotes = kGhost.get(); pa.looseness = kLoose.get();
    pa.variation = kVar.get(); pa.motifStr = kMotif.get();
    pa.fillFreq = kFills.get(); pa.flam = kFlam.get();
    pa.velMin = (int)kVMin.get(); pa.velMax = (int)kVMax.get();
    if (pa.velMin >= pa.velMax) { pa.velMax = pa.velMin + 1; kVMax.set((float)pa.velMax); }
    proc.gateLength = kGate.get();
    proc.sampleVol = kSVol.get();
    proc.atomicSampleVol.store(proc.sampleVol);
    pa.bars = (int)kBars.get();
    pa.snareNote = (int)kNote.get();
    pa.seed = (int)kSeed.get();
    pa.backbeatLock = btnBB.getToggleState();
}

void SnareProcessorEditor::generate() {
    sync();
    proc.regenerate();
    repaint();
}

void SnareProcessorEditor::applyGenreDefaults(const snare::GenreProfile& gp) {
    kSwing.set(gp.defaultSwing);
    kGhost.set(gp.ghostAffinity * 0.5f);
    kSynco.set(gp.syncopationBias);
    kDensity.set(gp.densityBias);
    kComplex.set(0.3f + gp.densityBias * 0.4f);
}

void SnareProcessorEditor::timerCallback() {
    if (proc.isPlaying()) {
        if (!gridBounds.isEmpty())
            repaint(gridBounds);  // partial repaint — only the grid area (C4)
        else
            repaint();
    }
}

// ── Drag & Drop ────────────────────────────────────────────────────────────
bool SnareProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files) {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".mp3" || ext == ".flac" || ext == ".ogg")
            return true;
    }
    return false;
}

void SnareProcessorEditor::filesDropped(const juce::StringArray& files, int, int) {
    dragOver = false;
    for (auto& f : files) {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".mp3" || ext == ".flac" || ext == ".ogg") {
            proc.loadSample(juce::File(f));
            repaint();
            break;
        }
    }
}

// ── Paint ───────────────────────────────────────────────────────────────────
void SnareProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(Col::bg());

    auto dd = proc.getDisplayData();

    paintHeader(g);
    paintGenres(g, {14, 52, getWidth() - 28, 24});

    // Section labels row 1
    int knobY1 = 84;
    g.setColour(Col::border());
    g.drawHorizontalLine(knobY1 - 4, 14, (float)getWidth() - 14);
    g.setColour(Col::textD());
    g.setFont(juce::Font(10.f).boldened());
    g.drawText("RHYTHM", 14, knobY1, 80, 12, juce::Justification::left);
    g.drawText("FEEL", 280, knobY1, 50, 12, juce::Justification::left);

    // Section labels row 2
    int knobY2 = knobY1 + 78;
    g.setColour(Col::textD());
    g.setFont(juce::Font(10.f).boldened());
    g.drawText("STRUCTURE", 14, knobY2, 80, 12, juce::Justification::left);
    g.drawText("OUTPUT", 280, knobY2, 80, 12, juce::Justification::left);

    // Sample drop zone
    int szY = knobY2 + 80;
    sampleZoneBounds = { 14, szY, getWidth() - 28, 32 };
    paintSampleZone(g, sampleZoneBounds);

    // Grid
    int gridTop = szY + 42;
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

    int gridH = getHeight() - gridTop - 40;
    int scoreW = 200;
    int gridW = getWidth() - scoreW - 34;
    gridBounds = {14, gridTop + 10, gridW, gridH - 10};
    paintGrid(g, gridBounds);

    if (dd) {
        paintScores(g, {getWidth() - scoreW - 10, gridTop + 10, scoreW, gridH - 80});
        paintLegend(g, {getWidth() - scoreW - 10, getHeight() - 70, scoreW, 40});
    }

    paintFooter(g, {0, getHeight() - 24, getWidth(), 24});
}

void SnareProcessorEditor::paintHeader(juce::Graphics& g) {
    g.setColour(Col::bg2());
    g.fillRect(0, 0, getWidth(), 46);
    g.setColour(Col::accent());
    g.fillRect(0, 44, getWidth(), 2);

    g.setColour(Col::textW());
    g.setFont(juce::Font(15.f).boldened());
    g.drawText("SNARE RHYTHM GEN", 14, 4, 220, 36, juce::Justification::centredLeft);

    g.setColour(Col::textG());
    g.setFont(juce::Font(11.f));
    g.drawText("by Gleinkaa", 228, 4, 100, 36, juce::Justification::centredLeft);

    g.setColour(Col::textD());
    g.setFont(9.f);
    g.drawText("v1.2", 330, 16, 40, 16, juce::Justification::left);

    // Sample name
    if (proc.hasSample()) {
        g.setColour(Col::sampleOk());
        g.setFont(9.f);
        g.drawText(proc.getSampleName(), getWidth() - 260, 4, 120, 36, juce::Justification::centredRight);
    }

    g.setColour(Col::textG());
    g.setFont(9.f);
    juce::String seedStr = "SEED " + juce::String((int)kSeed.get());
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

void SnareProcessorEditor::paintSampleZone(juce::Graphics& g, juce::Rectangle<int> area) {
    auto af = area.toFloat();

    if (proc.hasSample()) {
        g.setColour(Col::sampleOk().withAlpha(0.15f));
        g.fillRoundedRectangle(af, 4.f);
        g.setColour(Col::sampleOk().withAlpha(dragOver ? 1.f : 0.6f));
        g.drawRoundedRectangle(af, 4.f, 1.5f);
        g.setColour(Col::textW());
        g.setFont(11.f);
        g.drawText(proc.getSampleName(), af, juce::Justification::centred);
    } else {
        g.setColour(Col::bg3().withAlpha(0.5f));
        g.fillRoundedRectangle(af, 4.f);

        // Dashed border
        juce::Colour bc = dragOver ? Col::accent() : Col::sampleEmpty();
        g.setColour(bc);
        float dashLen[] = { 6.f, 4.f };
        juce::Path border;
        border.addRoundedRectangle(af, 4.f);
        g.strokePath(border, juce::PathStrokeType(1.5f), {});

        g.setColour(Col::textG());
        g.setFont(11.f);
        g.drawText("Drop sample here...", af, juce::Justification::centred);
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
        g.setColour(juce::Colour(0x15ffffffu));
        g.drawHorizontalLine((int)y, ox, ox + iw);
    }

    // Bar numbers
    g.setColour(Col::textD());
    g.setFont(8.f);
    for (int b = 0; b < bars; ++b)
        g.drawText(juce::String(b + 1), (int)ox + 2, (int)(oy + b * rowH + 1), 10, 10, juce::Justification::left);

    // Hits — cyan/orange/gray per manual
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
            case snare::Accent:  c = Col::amber(); break;    // orange for accent
            case snare::Primary: c = Col::accent(); break;   // cyan for primary
            case snare::Ghost:   c = Col::textG(); break;    // gray for ghost
            case snare::Fill:    c = Col::amber(); break;    // orange for fill
            case snare::Flam:    c = Col::teal(); break;
            default:             c = Col::textG(); break;
        }

        g.setColour(c.withAlpha(alpha * 0.08f));
        g.fillRoundedRectangle(x - 2, y - 2, cw + 4, ch + 4, 3.f);

        g.setColour(c.withAlpha(alpha * 0.85f));
        g.fillRoundedRectangle(x, y, cw, ch, 2.f);

        g.setColour(juce::Colours::white.withAlpha(alpha * 0.3f));
        g.fillRect(x + 1, y, cw - 2, std::min(2.f, ch * 0.12f));
    }

    // Playback cursor with latency-compensated extrapolation (C1+C3)
    if (proc.isPlaying()) {
        float totalQ = (float)(bars * bpb);
        float rawPos = proc.getPlaybackPos();
        double snapSampleCount = proc.playPosSampleCount.load();
        double qps = proc.playPosQuartersPerSample.load();

        // Extrapolate forward by elapsed samples since the audio thread snapshot
        double now = juce::Time::getMillisecondCounterHiRes();
        double elapsedSamples = (now - snapSampleCount / proc.getSampleRate() * 1000.0);
        // Simpler: estimate elapsed from timer interval (~16ms at 60Hz)
        // Use the sample counter difference approach
        float extrapolated = rawPos + (float)(qps * proc.getSampleRate() * 0.016);

        // Wrap around
        if (extrapolated >= totalQ) extrapolated -= totalQ;
        if (extrapolated < 0.f) extrapolated = 0.f;

        // Smooth to avoid micro-jitter
        float diff = extrapolated - smoothedCursorPos;
        if (diff < -totalQ * 0.5f) diff += totalQ;  // handle wrap-around
        if (diff > totalQ * 0.5f) diff -= totalQ;
        smoothedCursorPos += diff * 0.45f;
        if (smoothedCursorPos >= totalQ) smoothedCursorPos -= totalQ;
        if (smoothedCursorPos < 0.f) smoothedCursorPos += totalQ;

        float px = ox + (smoothedCursorPos / totalQ) * iw;
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
        {"Density",   s.density,  Col::accent()},
        {"Backbeat",  s.backbeat, Col::amber()},
        {"Dynamics",  s.dynamics, Col::teal()},
        {"Ghosts",    s.ghostBal, Col::purple()},
        {"Variation", s.variation,Col::green()},
        {"Coverage",  s.coverage, Col::blue()},
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
    L items[] = {{"PRI", Col::accent()}, {"ACC", Col::amber()}, {"GHO", Col::textG()},
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

void SnareProcessorEditor::paintFooter(juce::Graphics& g, juce::Rectangle<int> area) {
    g.setColour(Col::bg2());
    g.fillRect(area);
    g.setColour(Col::border());
    g.drawHorizontalLine(area.getY(), 0, (float)getWidth());

    g.setColour(Col::textD().withAlpha(0.4f));
    g.setFont(juce::Font(10.f));
    g.drawText("SnareRhythmGen v1.2 by Gleinkaa", area, juce::Justification::centred);
}

// ── Layout ──────────────────────────────────────────────────────────────────
void SnareProcessorEditor::resized() {
    int w = getWidth();
    int ks = 60, kg = 2;
    int knobY1 = 96;
    int knobY2 = knobY1 + 78;

    auto place = [&](Knob& k, int col, int row) {
        int y = row == 0 ? knobY1 : knobY2;
        k.setBounds(14 + col * (ks + kg), y, ks, ks + 14);
    };

    // Row 1: Rhythm (0-3) + Feel (4-7)
    place(kComplex, 0, 0); place(kDensity, 1, 0); place(kSynco, 2, 0); place(kSwing, 3, 0);
    place(kHuman, 4, 0);   place(kAccent, 5, 0);  place(kGhost, 6, 0); place(kLoose, 7, 0);

    // Row 2: Structure (0-3) + Output (4-7)
    place(kVar, 0, 1);   place(kMotif, 1, 1);  place(kFills, 2, 1);  place(kFlam, 3, 1);
    place(kVMin, 4, 1);  place(kVMax, 5, 1);   place(kGate, 6, 1);   place(kSVol, 7, 1);

    // Right column: knobs
    int rx = w - 186;
    kBars.setBounds(rx, knobY1, ks, ks + 14);
    kNote.setBounds(rx + ks + kg, knobY1, ks, ks + 14);
    kSeed.setBounds(rx, knobY2, ks, ks + 14);

    // Right column: buttons
    btnBB.setBounds(rx, knobY2 + 16, 140, 20);

    int btnY = knobY2 + 42;
    int btnW = 84;
    btnGen.setBounds(rx, btnY, btnW, 28);
    btnPlay.setBounds(rx + btnW + 4, btnY, btnW, 28);

    btnSave.setBounds(rx, btnY + 32, 58, 22);
    btnLoad.setBounds(rx + 62, btnY + 32, 58, 22);
    btnExp.setBounds(rx + 124, btnY + 32, 48, 22);

    btnLoadSample.setBounds(rx + ks * 2 + kg, knobY2, ks * 2, 22);
}

// ── Mouse ───────────────────────────────────────────────────────────────────
void SnareProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    // Genre bar clicks
    int gy = 52, gh = 24;
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
    int gy = 52, gh = 24;
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
