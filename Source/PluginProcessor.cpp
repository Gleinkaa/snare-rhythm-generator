#include "PluginProcessor.h"
#include "PluginEditor.h"

SnareProcessor::SnareProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
    regenerate();
}

void SnareProcessor::prepareToPlay(double sr, int) {
    sampleRate = sr;
}

void SnareProcessor::releaseResources() {
    playing.store(false);
    resetRequested.store(true);
}

void SnareProcessor::regenerate() {
    snare::Generator gen;
    auto result = gen.run(params);

    auto dd = std::make_shared<DisplayData>();
    dd->hits = result.hits;
    dd->scores = result.scores;
    std::atomic_store(&displayData, dd);

    auto ad = std::make_shared<AudioData>();
    ad->hits = std::move(result.hits);
    ad->snareNote = params.snareNote;
    ad->bpm = params.bpm;
    ad->bars = params.bars;
    ad->timeSig = params.timeSigNum;
    ad->resolution = params.resolution;
    ad->gateLen = gateLength;
    std::atomic_store(&audioData, ad);
}

void SnareProcessor::loadSample(const juce::File& file) {
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) return;

    auto sd = std::make_shared<SampleData>();
    sd->name = file.getFileNameWithoutExtension();
    sd->path = file.getFullPathName();

    juce::AudioBuffer<float> buf((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&buf, 0, (int)reader->lengthInSamples, 0, true, true);

    // Resample if needed
    if (std::abs(reader->sampleRate - sampleRate) > 1.0) {
        double ratio = sampleRate / reader->sampleRate;
        int newLen = (int)(buf.getNumSamples() * ratio);
        juce::AudioBuffer<float> resampled(buf.getNumChannels(), newLen);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch) {
            auto* src = buf.getReadPointer(ch);
            auto* dst = resampled.getWritePointer(ch);
            for (int i = 0; i < newLen; ++i) {
                double srcIdx = i / ratio;
                int idx0 = (int)srcIdx;
                float frac = (float)(srcIdx - idx0);
                int idx1 = std::min(idx0 + 1, buf.getNumSamples() - 1);
                dst[i] = src[idx0] * (1.f - frac) + src[idx1] * frac;
            }
        }
        sd->buffer = std::move(resampled);
    } else {
        sd->buffer = std::move(buf);
    }

    sd->sampleRate = sampleRate;
    std::atomic_store(&sampleData, sd);
}

void SnareProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    buffer.clear();
    midi.clear();

    int numSamples = buffer.getNumSamples();

    // Handle reset request from startPlayback/releaseResources (H3)
    if (resetRequested.exchange(false)) {
        playPos.store(0.f);
        pendingOffs.clear();
        for (auto& v : sampleVoices) v.active = false;
    }

    if (!playing.load())
        return;

    if (auto* bypassParam = getBypassParameter())
        if (bypassParam->getValue() >= 0.5f)
            return;

    auto ad = std::atomic_load(&audioData);
    if (!ad || ad->hits.empty())
        return;

    // Use double BPM from host to avoid timing drift (M3)
    double bpm = (double)ad->bpm;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm())
                bpm = *b;
        }
    }

    double samplesPerQuarter = sampleRate * 60.0 / bpm;
    double quartersPerSample = 1.0 / samplesPerQuarter;
    float ticksPerStep = 1.f / (float)ad->resolution;
    float totalQuarters = (float)(ad->bars * ad->timeSig);
    float svol = atomicSampleVol.load();

    // Thread-safe sample snapshot (H1)
    auto sd = std::atomic_load(&sampleData);

    float pos = playPos.load();
    int note = ad->snareNote;

    for (int i = 0; i < numSamples; ++i) {
        float nextPos = pos + (float)quartersPerSample;

        for (auto& h : ad->hits) {
            float hitTime = (float)(h.bar * ad->timeSig) + h.tick * ticksPerStep;
            if (hitTime >= pos && hitTime < nextPos) {
                auto vel = (juce::uint8)std::clamp(h.velocity, 1, 127);
                midi.addEvent(juce::MidiMessage::noteOn(10, note, vel), i);

                int durSamples = std::max(1, (int)(h.duration * ad->gateLen * samplesPerQuarter));
                int offSample = i + durSamples;
                if (offSample < numSamples)
                    midi.addEvent(juce::MidiMessage::noteOff(10, note), offSample);
                else
                    pendingOffs.push_back({offSample - numSamples, note});

                // Trigger sample voice using thread-safe snapshot
                if (sd && sd->buffer.getNumSamples() > 0 && svol > 0.001f) {
                    float gain = (float)vel / 127.f * svol;
                    int oldest = 0;
                    for (int v = 0; v < 8; ++v) {
                        if (!sampleVoices[v].active) { oldest = v; break; }
                        if (sampleVoices[v].pos > sampleVoices[oldest].pos) oldest = v;
                    }
                    sampleVoices[oldest] = { 0, gain, true };
                }
            }
        }

        pos = nextPos;
        if (pos >= totalQuarters) pos -= totalQuarters;
    }

    // Process pending note-offs
    for (auto it = pendingOffs.begin(); it != pendingOffs.end(); ) {
        if (it->sampleOffset < numSamples) {
            midi.addEvent(juce::MidiMessage::noteOff(10, it->note),
                          std::min(it->sampleOffset, numSamples - 1));
            it = pendingOffs.erase(it);
        } else {
            it->sampleOffset -= numSamples;
            ++it;
        }
    }

    // Render sample voices into audio buffer using thread-safe snapshot (H1)
    if (sd && sd->buffer.getNumSamples() > 0) {
        int smpLen = sd->buffer.getNumSamples();
        int smpCh = sd->buffer.getNumChannels();
        int outCh = buffer.getNumChannels();

        for (int i = 0; i < numSamples; ++i) {
            float mix = 0.f;
            for (auto& v : sampleVoices) {
                if (!v.active) continue;
                if (v.pos >= smpLen) { v.active = false; continue; }
                float s = 0.f;
                for (int c = 0; c < smpCh; ++c)
                    s += sd->buffer.getSample(c, v.pos);
                s /= (float)smpCh;
                mix += s * v.gain;
                v.pos++;
            }
            for (int c = 0; c < outCh; ++c)
                buffer.addSample(c, i, mix);
        }
    }

    // Publish playback state for GUI cursor extrapolation (C1)
    totalSamplesProcessed += numSamples;
    playPos.store(pos);
    playPosSampleCount.store(totalSamplesProcessed);
    playPosQuartersPerSample.store(quartersPerSample);
}

void SnareProcessor::exportMidi(const juce::File& file) {
    auto dd = getDisplayData();
    if (!dd) return;

    juce::MidiMessageSequence seq;
    double bpm = (double)params.bpm;
    float ticksPerStep = 1.f / (float)params.resolution;

    for (auto& h : dd->hits) {
        float hitTime = (float)(h.bar * params.timeSigNum) + h.tick * ticksPerStep;
        double timeSec = hitTime * 60.0 / bpm;
        auto vel = (juce::uint8)std::clamp(h.velocity, 1, 127);
        seq.addEvent(juce::MidiMessage::noteOn(10, params.snareNote, vel), timeSec);
        seq.addEvent(juce::MidiMessage::noteOff(10, params.snareNote),
                     timeSec + h.duration * gateLength * 60.0 / bpm);
    }
    seq.sort();
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote(480);
    mf.addTrack(seq);

    if (auto out = file.createOutputStream())
        mf.writeTo(*out);
}

// ── Preset system ──────────────────────────────────────────────────────────
void SnareProcessor::savePreset(const juce::File& file) {
    auto xml = std::make_unique<juce::XmlElement>("SnarePreset");
    xml->setAttribute("version", "1.2");
    xml->setAttribute("genre", juce::String(params.genre));
    xml->setAttribute("bars", params.bars);
    xml->setAttribute("bpm", params.bpm);
    xml->setAttribute("timeSigNum", params.timeSigNum);
    xml->setAttribute("resolution", params.resolution);
    xml->setAttribute("complexity",  (double)params.complexity);
    xml->setAttribute("density",     (double)params.density);
    xml->setAttribute("syncopation", (double)params.syncopation);
    xml->setAttribute("swing",       (double)params.swing);
    xml->setAttribute("humanize",    (double)params.humanize);
    xml->setAttribute("accentStr",   (double)params.accentStr);
    xml->setAttribute("ghostNotes",  (double)params.ghostNotes);
    xml->setAttribute("fillFreq",    (double)params.fillFreq);
    xml->setAttribute("variation",   (double)params.variation);
    xml->setAttribute("motifStr",    (double)params.motifStr);
    xml->setAttribute("looseness",   (double)params.looseness);
    xml->setAttribute("probability", (double)params.probability);
    xml->setAttribute("flam",        (double)params.flam);
    xml->setAttribute("velMin", params.velMin);
    xml->setAttribute("velMax", params.velMax);
    xml->setAttribute("backbeatLock", params.backbeatLock);
    xml->setAttribute("snareNote", params.snareNote);
    xml->setAttribute("seed", params.seed);
    xml->setAttribute("gateLength", (double)gateLength);
    xml->setAttribute("sampleVol", (double)sampleVol);
    xml->setAttribute("samplePath", std::atomic_load(&sampleData)->path);
    xml->writeTo(file);
}

void SnareProcessor::loadPreset(const juce::File& file) {
    auto xml = juce::XmlDocument::parse(file);
    if (!xml || !xml->hasTagName("SnarePreset")) return;

    params.genre       = xml->getStringAttribute("genre", "hip-hop").toStdString();
    params.bars        = xml->getIntAttribute("bars", 4);
    params.bpm         = xml->getIntAttribute("bpm", 90);
    params.timeSigNum  = xml->getIntAttribute("timeSigNum", 4);
    params.resolution  = xml->getIntAttribute("resolution", 4);
    params.complexity  = (float)xml->getDoubleAttribute("complexity", 0.4);
    params.density     = (float)xml->getDoubleAttribute("density", 0.4);
    params.syncopation = (float)xml->getDoubleAttribute("syncopation", 0.3);
    params.swing       = (float)xml->getDoubleAttribute("swing", 0.0);
    params.humanize    = (float)xml->getDoubleAttribute("humanize", 0.3);
    params.accentStr   = (float)xml->getDoubleAttribute("accentStr", 0.6);
    params.ghostNotes  = (float)xml->getDoubleAttribute("ghostNotes", 0.3);
    params.fillFreq    = (float)xml->getDoubleAttribute("fillFreq", 0.2);
    params.variation   = (float)xml->getDoubleAttribute("variation", 0.2);
    params.motifStr    = (float)xml->getDoubleAttribute("motifStr", 0.7);
    params.looseness   = (float)xml->getDoubleAttribute("looseness", 0.2);
    params.probability = (float)xml->getDoubleAttribute("probability", 1.0);
    params.flam        = (float)xml->getDoubleAttribute("flam", 0.0);
    params.velMin      = xml->getIntAttribute("velMin", 79);
    params.velMax      = xml->getIntAttribute("velMax", 127);
    params.backbeatLock= xml->getBoolAttribute("backbeatLock", true);
    params.snareNote   = xml->getIntAttribute("snareNote", 38);
    params.seed        = xml->getIntAttribute("seed", -1);
    gateLength         = (float)xml->getDoubleAttribute("gateLength", 1.0);
    sampleVol          = (float)xml->getDoubleAttribute("sampleVol", 0.8);

    auto sp = xml->getStringAttribute("samplePath", "");
    if (sp.isNotEmpty()) {
        juce::File f(sp);
        if (f.existsAsFile()) loadSample(f);
    }

    regenerate();
}

// ── DAW state ──────────────────────────────────────────────────────────────
void SnareProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto xml = std::make_unique<juce::XmlElement>("SnareState");
    xml->setAttribute("version", "1.2");
    xml->setAttribute("genre", juce::String(params.genre));
    xml->setAttribute("bars", params.bars);
    xml->setAttribute("bpm", params.bpm);
    xml->setAttribute("timeSigNum", params.timeSigNum);
    xml->setAttribute("resolution", params.resolution);
    xml->setAttribute("complexity",  (double)params.complexity);
    xml->setAttribute("density",     (double)params.density);
    xml->setAttribute("syncopation", (double)params.syncopation);
    xml->setAttribute("swing",       (double)params.swing);
    xml->setAttribute("humanize",    (double)params.humanize);
    xml->setAttribute("accentStr",   (double)params.accentStr);
    xml->setAttribute("ghostNotes",  (double)params.ghostNotes);
    xml->setAttribute("fillFreq",    (double)params.fillFreq);
    xml->setAttribute("variation",   (double)params.variation);
    xml->setAttribute("motifStr",    (double)params.motifStr);
    xml->setAttribute("looseness",   (double)params.looseness);
    xml->setAttribute("probability", (double)params.probability);
    xml->setAttribute("flam",        (double)params.flam);
    xml->setAttribute("velMin", params.velMin);
    xml->setAttribute("velMax", params.velMax);
    xml->setAttribute("backbeatLock", params.backbeatLock);
    xml->setAttribute("snareNote", params.snareNote);
    xml->setAttribute("seed", params.seed);
    xml->setAttribute("gateLength", (double)gateLength);
    xml->setAttribute("sampleVol", (double)sampleVol);
    xml->setAttribute("samplePath", std::atomic_load(&sampleData)->path);
    copyXmlToBinary(*xml, dest);
}

void SnareProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName("SnareState")) return;

    auto safeRestore = [this, xml = std::shared_ptr<juce::XmlElement>(xml.release())]() {
        params.genre       = xml->getStringAttribute("genre", "hip-hop").toStdString();
        params.bars        = xml->getIntAttribute("bars", 4);
        params.bpm         = xml->getIntAttribute("bpm", 90);
        params.timeSigNum  = xml->getIntAttribute("timeSigNum", 4);
        params.resolution  = xml->getIntAttribute("resolution", 4);
        params.complexity  = (float)xml->getDoubleAttribute("complexity", 0.4);
        params.density     = (float)xml->getDoubleAttribute("density", 0.4);
        params.syncopation = (float)xml->getDoubleAttribute("syncopation", 0.3);
        params.swing       = (float)xml->getDoubleAttribute("swing", 0.0);
        params.humanize    = (float)xml->getDoubleAttribute("humanize", 0.3);
        params.accentStr   = (float)xml->getDoubleAttribute("accentStr", 0.6);
        params.ghostNotes  = (float)xml->getDoubleAttribute("ghostNotes", 0.3);
        params.fillFreq    = (float)xml->getDoubleAttribute("fillFreq", 0.2);
        params.variation   = (float)xml->getDoubleAttribute("variation", 0.2);
        params.motifStr    = (float)xml->getDoubleAttribute("motifStr", 0.7);
        params.looseness   = (float)xml->getDoubleAttribute("looseness", 0.2);
        params.probability = (float)xml->getDoubleAttribute("probability", 1.0);
        params.flam        = (float)xml->getDoubleAttribute("flam", 0.0);
        params.velMin      = xml->getIntAttribute("velMin", 79);
        params.velMax      = xml->getIntAttribute("velMax", 127);
        params.backbeatLock= xml->getBoolAttribute("backbeatLock", true);
        params.snareNote   = xml->getIntAttribute("snareNote", 38);
        params.seed        = xml->getIntAttribute("seed", -1);
        gateLength         = (float)xml->getDoubleAttribute("gateLength", 1.0);
        sampleVol          = (float)xml->getDoubleAttribute("sampleVol", 0.8);
        atomicSampleVol.store(sampleVol);

        auto sp = xml->getStringAttribute("samplePath", "");
        if (sp.isNotEmpty()) {
            juce::File f(sp);
            if (f.existsAsFile()) loadSample(f);
        }

        regenerate();
    };

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        safeRestore();
    else
        juce::MessageManager::callAsync(safeRestore);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SnareProcessor(); }
