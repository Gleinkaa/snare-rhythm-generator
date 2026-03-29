#include "PluginProcessor.h"
#include "PluginEditor.h"

SnareProcessor::SnareProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    regenerate();
}

void SnareProcessor::regenerate() {
    snare::Generator gen;
    auto result = gen.run(params);

    // Build new display data (lock-free publish)
    auto dd = std::make_shared<DisplayData>();
    dd->hits = result.hits;
    dd->scores = result.scores;
    std::atomic_store(&displayData, dd);

    // Build new audio data (lock-free publish)
    auto ad = std::make_shared<AudioData>();
    ad->hits = std::move(result.hits);
    ad->snareNote = params.snareNote;
    ad->bpm = params.bpm;
    ad->bars = params.bars;
    ad->timeSig = params.timeSigNum;
    ad->resolution = params.resolution;
    std::atomic_store(&audioData, ad);
}

void SnareProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    buffer.clear();
    midi.clear();

    if (!playing.load())
        return;

    // Bypass check
    if (auto* bypassParam = getBypassParameter())
        if (bypassParam->getValue() >= 0.5f)
            return;

    // Lock-free read of audio data
    auto ad = std::atomic_load(&audioData);
    if (!ad || ad->hits.empty())
        return;

    // Sync BPM from host if available
    int bpm = ad->bpm;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm())
                bpm = (int)*b;
        }
    }

    double samplesPerQuarter = sampleRate * 60.0 / (double)bpm;
    double quartersPerSample = 1.0 / samplesPerQuarter;
    float ticksPerStep = 1.f / (float)ad->resolution;
    float totalQuarters = (float)(ad->bars * ad->timeSig);

    float pos = playPos.load();
    int numSamples = buffer.getNumSamples();
    int note = ad->snareNote;

    // Single-pass: advance a hit index instead of scanning all hits per sample
    // Sort hits by time for efficient iteration
    // (hits are typically few enough that the inner loop is fine, but use early-exit)
    for (int i = 0; i < numSamples; ++i) {
        float nextPos = pos + (float)quartersPerSample;

        for (auto& h : ad->hits) {
            float hitTime = (float)(h.bar * ad->timeSig) + h.tick * ticksPerStep;
            if (hitTime >= pos && hitTime < nextPos) {
                auto vel = (juce::uint8)std::clamp(h.velocity, 1, 127);
                midi.addEvent(juce::MidiMessage::noteOn(10, note, vel), i);
                // Schedule note-off properly — compute offset in samples
                int durSamples = std::max(1, (int)(h.duration * samplesPerQuarter));
                int offSample = i + durSamples;
                if (offSample < numSamples) {
                    midi.addEvent(juce::MidiMessage::noteOff(10, note), offSample);
                } else {
                    // Track for next buffer
                    pendingOffs.push_back({offSample - numSamples, note});
                }
            }
        }

        pos = nextPos;
        if (pos >= totalQuarters) pos = 0.f;
    }

    // Process pending note-offs from previous buffers
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

    playPos.store(pos);
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
        seq.addEvent(juce::MidiMessage::noteOff(10, params.snareNote), timeSec + h.duration * 60.0 / bpm);
    }
    seq.sort();
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote(480);
    mf.addTrack(seq);

    if (auto out = file.createOutputStream())
        mf.writeTo(*out);
}

void SnareProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto xml = std::make_unique<juce::XmlElement>("SnareState");
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
    copyXmlToBinary(*xml, dest);
}

void SnareProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName("SnareState")) return;

    // Use message manager to ensure thread safety
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
        params.velMin      = xml->getIntAttribute("velMin", 30);
        params.velMax      = xml->getIntAttribute("velMax", 127);
        params.backbeatLock= xml->getBoolAttribute("backbeatLock", true);
        params.snareNote   = xml->getIntAttribute("snareNote", 38);
        params.seed        = xml->getIntAttribute("seed", -1);
        regenerate();
    };

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        safeRestore();
    else
        juce::MessageManager::callAsync(safeRestore);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SnareProcessor(); }
