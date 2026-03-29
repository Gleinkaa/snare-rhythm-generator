#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace snare {

// ── Hit Types ───────────────────────────────────────────────────────────────
enum HitType { Primary = 0, Ghost, Accent, Fill, Flam };

struct SnareHit {
    float tick       = 0.f;
    int   velocity   = 80;
    float duration   = 0.25f;
    int   hitType    = Primary;
    float probability= 1.f;
    int   bar        = 0;
};

// ── Genre Profile ───────────────────────────────────────────────────────────
struct GenreProfile {
    std::string name;
    int bpmLo, bpmHi;
    float backbeatStrength;
    float defaultSwing;
    float ghostAffinity;
    int   fillStyle;           // 0=buildup, 1=breakout, 2=minimal, 3=rolls
    float syncopationBias;
    float densityBias;
    std::array<float, 4> beatWeights;
    std::vector<int> strongBeats;
    std::vector<float> barShape;
};

enum FillStyle { Buildup = 0, Breakout, Minimal, Rolls };

inline const std::vector<std::string>& genreNames() {
    static const std::vector<std::string> names = {
        "boom-bap", "breakbeat", "drum-and-bass", "experimental",
        "funk", "hip-hop", "house", "jazz", "pop",
        "reggae", "reggaeton", "techno", "trap"
    };
    return names;
}

inline const std::map<std::string, GenreProfile>& genres() {
    static const std::map<std::string, GenreProfile> g = {
        {"hip-hop",       {"hip-hop",       80,100, 0.9f, 0.15f,0.4f, Buildup, 0.3f, 0.3f, {1.f,.05f,.25f,.1f},  {1,3}, {}}},
        {"trap",          {"trap",         130,170, 0.5f, 0.f,  0.2f, Rolls,   0.4f, 0.3f, {1.f,.15f,.35f,.2f},  {2},   {}}},
        {"boom-bap",      {"boom-bap",      85, 98, 0.95f,0.25f,0.5f, Buildup, 0.35f,0.35f,{1.f,.05f,.3f,.15f},  {1,3}, {}}},
        {"house",         {"house",        120,130, 0.85f,0.1f, 0.3f, Minimal, 0.15f,0.2f, {1.f,.05f,.15f,.05f}, {1,3}, {}}},
        {"techno",        {"techno",       125,140, 0.8f, 0.f,  0.2f, Rolls,   0.2f, 0.25f,{1.f,.1f,.2f,.1f},    {1,3}, {}}},
        {"drum-and-bass", {"drum-and-bass",170,180, 0.7f, 0.f,  0.6f, Breakout,0.6f, 0.5f, {1.f,.2f,.5f,.3f},    {1},   {}}},
        {"breakbeat",     {"breakbeat",    120,140, 0.5f, 0.1f, 0.7f, Breakout,0.65f,0.55f,{1.f,.25f,.5f,.35f},  {1,3}, {}}},
        {"reggaeton",     {"reggaeton",     88,100, 0.6f, 0.f,  0.1f, Minimal, 0.3f, 0.4f, {.8f,.1f,.6f,.1f},    {1,3}, {0.7f,1.f,0.8f,1.f}}},
        {"pop",           {"pop",          100,130, 0.95f,0.f,  0.15f,Buildup, 0.1f, 0.2f, {1.f,.02f,.1f,.02f},  {1,3}, {}}},
        {"jazz",          {"jazz",         100,160, 0.3f, 0.35f,0.8f, Breakout,0.5f, 0.5f, {.6f,.2f,.5f,.3f},    {1,3}, {}}},
        {"funk",          {"funk",          95,115, 0.85f,0.15f,0.9f, Breakout,0.55f,0.55f,{1.f,.2f,.45f,.3f},   {1,3}, {}}},
        {"reggae",        {"reggae",        70, 90, 0.6f, 0.1f, 0.3f, Minimal, 0.2f, 0.25f,{.7f,.05f,.3f,.05f},  {2},   {}}},
        {"experimental",  {"experimental",  60,180, 0.1f, 0.f,  0.5f, Breakout,0.7f, 0.5f, {.5f,.4f,.5f,.4f},    {},    {}}},
    };
    return g;
}

inline const GenreProfile& getGenre(const std::string& name) {
    auto& g = genres();
    auto it = g.find(name);
    return it != g.end() ? it->second : g.at("hip-hop");
}

// ── Generator Parameters ────────────────────────────────────────────────────
struct Params {
    std::string genre  = "hip-hop";
    int   bars         = 4;
    int   timeSigNum   = 4;
    int   bpm          = 90;
    int   resolution   = 4;
    float complexity   = 0.4f;
    float density      = 0.4f;
    float syncopation  = 0.3f;
    float swing        = 0.0f;
    float humanize     = 0.3f;
    float accentStr    = 0.6f;
    float ghostNotes   = 0.3f;
    float fillFreq     = 0.2f;
    float variation    = 0.2f;
    float motifStr     = 0.7f;
    float looseness    = 0.2f;
    float probability  = 1.0f;
    float flam         = 0.0f;
    int   velMin       = 30;
    int   velMax       = 127;
    bool  backbeatLock = true;
    int   snareNote    = 38;
    int   seed         = -1;
};

// ── Scores ──────────────────────────────────────────────────────────────────
struct Scores {
    float density = 0, backbeat = 0, dynamics = 0;
    float ghostBal = 0, variation = 0, coverage = 0, overall = 0;
};

// ── Generator ───────────────────────────────────────────────────────────────
class Generator {
public:
    struct Result {
        std::vector<SnareHit> hits;
        Scores scores;
    };

    Result run(Params p) {
        par = p;
        clamp();
        gp = getGenre(par.genre);
        rng.seed(par.seed >= 0 ? (unsigned)par.seed : std::random_device{}());
        spb  = par.resolution;
        bpb  = par.timeSigNum;
        spbr = spb * bpb;
        total = spbr * par.bars;

        auto motif = makeMotif();
        std::vector<SnareHit> all;
        for (int b = 0; b < par.bars; ++b) {
            auto bh = makeBar(b, motif);
            all.insert(all.end(), bh.begin(), bh.end());
        }
        all = addFills(std::move(all));
        all = addFlams(std::move(all));
        all = humanize(std::move(all));
        all = doSwing(std::move(all));
        all = probGate(std::move(all));

        Result r;
        r.hits = std::move(all);
        r.scores = evaluate(r.hits);
        return r;
    }

private:
    Params par;
    GenreProfile gp;
    std::mt19937 rng;
    int spb = 4, bpb = 4, spbr = 16, total = 64;

    float rr() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); }
    int ri(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }
    bool isSB(int beat) const { for (int s : gp.strongBeats) if (s == beat) return true; return false; }

    void clamp() {
        auto cf = [](float& v) { v = std::clamp(v, 0.f, 1.f); };
        cf(par.complexity); cf(par.density); cf(par.syncopation); cf(par.swing);
        cf(par.humanize); cf(par.accentStr); cf(par.ghostNotes); cf(par.fillFreq);
        cf(par.variation); cf(par.motifStr); cf(par.looseness); cf(par.probability); cf(par.flam);
        cf(par.accentStr);
        par.velMin = std::clamp(par.velMin, 1, 127);
        par.velMax = std::clamp(par.velMax, par.velMin, 127);
        par.bars = std::clamp(par.bars, 1, 16);
        par.resolution = std::clamp(par.resolution, 2, 8);
    }

    float sw(int beat, int sub) const {
        float w = gp.beatWeights[std::min(sub, 3)];
        if (!gp.barShape.empty() && beat < (int)gp.barShape.size()) w *= gp.barShape[beat];
        if (isSB(beat)) w *= 1.f + gp.backbeatStrength * 0.5f;
        return w;
    }

    int vel(int ht, float vf) const {
        int range = par.velMax - par.velMin;
        int base;
        if      (ht == Accent)  base = par.velMax - (int)(range * 0.1f * (1.f - par.accentStr));
        else if (ht == Primary) base = par.velMin + (int)(range * (0.6f + 0.3f * par.accentStr) * vf);
        else if (ht == Ghost)   { int c = par.velMin + (int)(range * 0.35f); base = par.velMin + (int)((c - par.velMin) * vf); }
        else if (ht == Fill)    base = par.velMin + (int)(range * 0.65f * vf);
        else                    base = par.velMin + (int)(range * 0.4f * vf);
        return std::clamp(base, par.velMin, par.velMax);
    }

    struct MN { int step; int type; float vf; };

    std::vector<MN> makeMotif() {
        std::vector<MN> m;
        std::set<int> used;
        for (int beat = 0; beat < bpb; ++beat) {
            int step = beat * spb;
            if (isSB(beat)) {
                if (par.backbeatLock && gp.backbeatStrength > 0.3f) { m.push_back({step, Accent, 1.f}); used.insert(step); }
                else if (rr() < gp.backbeatStrength) { m.push_back({step, Primary, 0.9f}); used.insert(step); }
            }
        }
        int avail = spbr - (int)used.size();
        int tp = std::max(0, (int)(avail * par.density * 0.4f * (0.5f + par.complexity * 0.5f)));
        tp = (int)(tp * (0.7f + gp.densityBias * 0.6f));
        if (tp > 0) {
            std::vector<std::pair<int,float>> c;
            for (int s = 0; s < spbr; ++s) {
                if (used.count(s)) continue;
                int sub = s % spb;
                float w = sw(s / spb, sub);
                float sc;
                if (sub == 0) sc = w * 0.6f * par.complexity;
                else if (sub == 2) sc = w * (0.4f + par.syncopation * 0.6f);
                else sc = w * par.syncopation * par.complexity * 0.8f;
                sc *= 0.5f + rr() * 0.5f;
                if (sc > 0.01f) c.push_back({s, sc});
            }
            std::sort(c.begin(), c.end(), [](auto& a, auto& b) { return a.second > b.second; });
            for (int i = 0; i < std::min(tp, (int)c.size()); ++i) {
                m.push_back({c[i].first, Primary, c[i].first % spb == 0 ? 0.75f : 0.65f});
                used.insert(c[i].first);
            }
        }
        std::vector<int> gpos;
        for (int s = 0; s < spbr; ++s)
            if (!used.count(s) && s % spb != 0) gpos.push_back(s);
        int tg = std::max(0, (int)(gpos.size() * par.ghostNotes * gp.ghostAffinity * 0.5f));
        if (tg > 0 && !gpos.empty()) {
            std::vector<std::pair<int,float>> gc;
            for (int s : gpos) {
                float w = sw(s / spb, s % spb);
                if (isSB(s / spb)) w *= 1.4f;
                w *= 0.5f + rr() * 0.5f;
                gc.push_back({s, w});
            }
            std::sort(gc.begin(), gc.end(), [](auto& a, auto& b) { return a.second > b.second; });
            for (int i = 0; i < std::min(tg, (int)gc.size()); ++i)
                m.push_back({gc[i].first, Ghost, 0.2f + rr() * 0.2f});
        }
        return m;
    }

    std::vector<SnareHit> makeBar(int bi, const std::vector<MN>& motif) {
        std::vector<SnareHit> hits;
        std::set<int> used;
        float bv = par.variation;
        if (bi > 0) {
            bv *= 0.7f + 0.3f * std::min(1.f, (float)bi / std::max(1, par.bars - 1));
            if (bi % 4 == 3) bv = std::min(1.f, bv * 1.5f);
        }
        for (auto [step, ht, vf] : motif) {
            int s = step; int type = ht; float v = vf;
            bool bb = (type == Accent && isSB(s / spb) && s % spb == 0 && par.backbeatLock);
            if (bi > 0 && !bb && rr() > par.motifStr) {
                float a = rr();
                if (a < 0.35f * bv) continue;
                else if (a < 0.6f * bv) s = std::clamp(s + (rr() < 0.5f ? -1 : 1), 0, spbr - 1);
                else if (a < 0.8f * bv && type == Ghost) { if (rr() < 0.3f) { type = Primary; v = 0.65f; } else continue; }
            }
            float prob = 1.f;
            if (type == Ghost) prob = 0.7f + par.probability * 0.3f;
            else if (type != Primary && type != Accent) prob = 0.8f + par.probability * 0.2f;
            hits.push_back({(float)s, vel(type, v), type == Primary || type == Accent ? 0.25f : 0.15f, type, prob, bi});
            used.insert(s);
        }
        if (bi > 0 && bv > 0.2f) {
            int ex = ri(0, std::max(1, (int)(bv * 3)));
            for (int i = 0; i < ex; ++i) {
                int s = ri(0, spbr - 1);
                if (used.count(s)) continue;
                float w = sw(s / spb, s % spb);
                if (rr() < w * par.density * 0.6f) {
                    int ht = s % spb != 0 ? Ghost : Primary;
                    hits.push_back({(float)s, vel(ht, ht == Ghost ? 0.35f : 0.6f), 0.18f, ht, 0.6f + par.probability * 0.4f, bi});
                    used.insert(s);
                }
            }
        }
        return hits;
    }

    std::vector<SnareHit> addFills(std::vector<SnareHit> hits) {
        if (par.fillFreq <= 0 || par.bars < 2) return hits;
        std::set<int> fb;
        if (par.fillFreq >= 0.8f) for (int b = 1; b < par.bars; ++b) fb.insert(b);
        else if (par.fillFreq >= 0.5f) for (int b = 0; b < par.bars; ++b) { if ((b+1)%2==0) fb.insert(b); }
        else if (par.fillFreq >= 0.25f) for (int b = 0; b < par.bars; ++b) { if ((b+1)%4==0) fb.insert(b); }
        else fb.insert(par.bars - 1);

        for (int bar : fb) {
            if (rr() > par.fillFreq + 0.3f) continue;
            int fz = std::min(8, spb * 2);
            std::vector<SnareHit> fh;
            if (gp.fillStyle == Rolls) {
                for (int i = 0; i < fz; ++i) {
                    float vf = 0.4f + 0.6f * (float)i / std::max(1, fz - 1);
                    fh.push_back({(float)(spbr - fz + i), vel(Fill, vf), 0.15f, Fill, 0.85f + 0.15f * par.probability, bar});
                }
            } else if (gp.fillStyle == Buildup) {
                int cnt = std::min(fz, std::max(2, (int)(fz * (0.3f + par.complexity * 0.5f))));
                std::vector<int> pos; for (int s = spbr - fz; s < spbr; ++s) pos.push_back(s);
                std::shuffle(pos.begin(), pos.end(), rng); pos.resize(cnt);
                std::sort(pos.begin(), pos.end());
                for (int i = 0; i < (int)pos.size(); ++i) {
                    float vf = 0.5f + 0.5f * (float)i / std::max(1, (int)pos.size() - 1);
                    fh.push_back({(float)pos[i], vel(Fill, vf), 0.2f, Fill, 0.9f, bar});
                }
            } else if (gp.fillStyle == Breakout) {
                for (int s = spbr - fz; s < spbr; ++s) {
                    float w = sw(s / spb, s % spb);
                    if (rr() < w * 0.7f + par.syncopation * 0.3f)
                        fh.push_back({(float)s, vel(Fill, 0.5f + 0.4f * rr()), 0.18f, Fill, 0.85f, bar});
                }
            } else {
                if (rr() < 0.7f) fh.push_back({(float)(spbr - spb), vel(Fill, 0.7f), 0.2f, Fill, 0.9f, bar});
                if (rr() < 0.7f) fh.push_back({(float)(spbr - 1), vel(Fill, 0.7f), 0.2f, Fill, 0.9f, bar});
            }
            if (!fh.empty()) {
                float fs = fh[0].tick; for (auto& f : fh) fs = std::min(fs, f.tick);
                hits.erase(std::remove_if(hits.begin(), hits.end(), [&](auto& h) { return h.bar == bar && h.tick >= fs; }), hits.end());
                hits.insert(hits.end(), fh.begin(), fh.end());
            }
        }
        return hits;
    }

    std::vector<SnareHit> addFlams(std::vector<SnareHit> hits) {
        if (par.flam <= 0) return hits;
        std::vector<SnareHit> extra;
        for (auto& h : hits)
            if ((h.hitType == Primary || h.hitType == Accent) && rr() < par.flam * 0.5f)
                extra.push_back({h.tick - 0.15f, std::max(par.velMin, h.velocity - 30), 0.1f, Flam, h.probability, h.bar});
        hits.insert(hits.end(), extra.begin(), extra.end());
        return hits;
    }

    std::vector<SnareHit> humanize(std::vector<SnareHit> hits) {
        if (par.humanize <= 0) return hits;
        for (auto& h : hits) {
            h.velocity = std::clamp(h.velocity + (int)(par.humanize * 12.f * (rr() * 2.f - 1.f)), par.velMin, par.velMax);
            float lo = par.looseness * par.humanize;
            if (h.hitType == Ghost || h.hitType == Fill) lo *= 1.5f;
            else if (h.hitType == Primary || h.hitType == Accent) lo *= 0.5f;
            h.tick += lo * 0.08f * (rr() * 2.f - 1.f);
        }
        return hits;
    }

    std::vector<SnareHit> doSwing(std::vector<SnareHit> hits) {
        float sw = par.swing > 0 ? par.swing : gp.defaultSwing;
        if (sw <= 0) return hits;
        for (auto& h : hits) {
            int local = (int)std::round(h.tick) % spb;
            if (spb >= 4 && (local == 1 || local == 3)) h.tick += sw * 0.4f;
            else if (spb >= 2 && spb < 4 && local == 1) h.tick += sw * 0.5f;
        }
        return hits;
    }

    std::vector<SnareHit> probGate(std::vector<SnareHit> hits) {
        if (par.probability >= 1.f) return hits;
        std::vector<SnareHit> out;
        for (auto& h : hits) {
            if ((h.hitType == Primary || h.hitType == Accent) && isSB((int)h.tick / spb) && (int)h.tick % spb == 0)
            { out.push_back(h); continue; }
            if (rr() < h.probability * par.probability) out.push_back(h);
        }
        return out;
    }

    Scores evaluate(const std::vector<SnareHit>& hits) const {
        Scores s;
        int pc = 0, gc = 0;
        for (auto& h : hits) { if (h.hitType == Primary || h.hitType == Accent) pc++; if (h.hitType == Ghost) gc++; }

        float pr = (float)pc / std::max(1, total);
        s.density = std::max(0.f, 1.f - std::abs(pr - (0.05f + par.density * 0.2f)) * 4.f);

        if (gp.backbeatStrength > 0.3f && !gp.strongBeats.empty()) {
            int present = 0, expected = 0;
            for (int b = 0; b < par.bars; ++b)
                for (int beat : gp.strongBeats) {
                    expected++;
                    int step = beat * spb;
                    for (auto& h : hits)
                        if (h.bar == b && std::abs(h.tick - step) < 0.5f && (h.hitType == Primary || h.hitType == Accent))
                        { present++; break; }
                }
            s.backbeat = (float)present / std::max(1, expected);
        } else s.backbeat = 1.f;

        if (hits.size() >= 2) {
            int mn = 127, mx = 0;
            for (auto& h : hits) { mn = std::min(mn, h.velocity); mx = std::max(mx, h.velocity); }
            s.dynamics = std::min(1.f, (float)(mx - mn) / std::max(1.f, (float)(par.velMax - par.velMin) * (gc > 0 ? 0.4f : 0.15f)));
        } else s.dynamics = hits.empty() ? 0.f : 0.5f;

        if (par.ghostNotes > 0.05f) {
            float gr = (float)gc / std::max(1.f, (float)(gc + pc));
            float tr = std::min(0.7f, par.ghostNotes * gp.ghostAffinity * 0.5f);
            s.ghostBal = std::max(0.f, 1.f - std::abs(gr - tr) * 2.f);
        } else s.ghostBal = gc == 0 ? 1.f : std::max(0.f, 1.f - gc * 0.1f);

        if (par.bars > 1) {
            std::vector<std::vector<int>> bp(par.bars);
            for (auto& h : hits) bp[h.bar].push_back((int)std::round(h.tick));
            for (auto& v : bp) std::sort(v.begin(), v.end());
            std::set<std::vector<int>> uniq(bp.begin(), bp.end());
            float ur = (float)uniq.size() / (float)bp.size();
            if (par.variation < 0.2f) s.variation = ur > 0.3f ? std::max(0.f, 1.f - (ur - 0.3f) * 1.5f) : 1.f;
            else s.variation = std::max(0.f, 1.f - std::abs(ur - (0.4f + par.variation * 0.4f)) * 2.f);
        } else s.variation = 1.f;

        std::set<int> bwh; for (auto& h : hits) bwh.insert(h.bar);
        s.coverage = (float)bwh.size() / std::max(1, par.bars);

        s.overall = s.density * 0.15f + s.backbeat * 0.25f + s.dynamics * 0.10f
                  + s.ghostBal * 0.15f + s.variation * 0.15f + s.coverage * 0.20f;
        return s;
    }
};

} // namespace snare
