#pragma once
#include <JuceHeader.h>
#include "SnareEngine.h"
#include <atomic>
#include <memory>

class SnareProcessor : public juce::AudioProcessor {
public:
    SnareProcessor();
    ~SnareProcessor() override = default;

    void prepareToPlay(double sr, int blockSize) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SnareRhythmGen"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Thread-safe interface
    void regenerate();
    void exportMidi(const juce::File& file);
    void startPlayback() { resetRequested.store(true); playing.store(true); }
    void stopPlayback()  { playing.store(false); }
    bool isPlaying() const { return playing.load(); }
    float getPlaybackPos() const { return playPos.load(); }

    // Cursor extrapolation: GUI reads these to compensate for buffer latency
    std::atomic<double> playPosSampleCount { 0.0 };   // monotonic sample counter when playPos was written
    std::atomic<double> playPosQuartersPerSample { 0.0 }; // tempo at time of write
    double getSampleRate() const { return sampleRate; }

    // Sample player
    void loadSample(const juce::File& file);
    juce::String getSampleName() const { return std::atomic_load(&sampleData)->name; }
    bool hasSample() const { return std::atomic_load(&sampleData)->buffer.getNumSamples() > 0; }

    // Preset save/load
    void savePreset(const juce::File& file);
    void loadPreset(const juce::File& file);

    // Params — only touched from message thread (editor)
    snare::Params params;
    float gateLength  = 1.0f;   // 0.25 - 2.0
    float sampleVol   = 0.8f;   // 0 - 1

    // Display data
    struct DisplayData {
        std::vector<snare::SnareHit> hits;
        snare::Scores scores;
    };
    std::shared_ptr<DisplayData> getDisplayData() const { return std::atomic_load(&displayData); }

private:
    double sampleRate = 44100.0;
    std::atomic<bool> playing { false };
    std::atomic<float> playPos { 0.f };
    std::atomic<bool> resetRequested { false };
    double totalSamplesProcessed = 0.0;  // audio thread only

    std::shared_ptr<DisplayData> displayData = std::make_shared<DisplayData>();

    struct AudioData {
        std::vector<snare::SnareHit> hits;
        int snareNote = 38;
        int bpm = 90;
        int bars = 4;
        int timeSig = 4;
        int resolution = 4;
        float gateLen = 1.0f;
    };
    std::shared_ptr<AudioData> audioData = std::make_shared<AudioData>();

    struct PendingNoteOff {
        int sampleOffset;
        int note;
    };
    std::vector<PendingNoteOff> pendingOffs;

    // Sample player (8-voice polyphonic) — thread-safe via shared_ptr
    struct SampleData {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
        juce::String name;
        juce::String path;
    };
    std::shared_ptr<SampleData> sampleData = std::make_shared<SampleData>();

    struct SampleVoice {
        int pos = 0;
        float gain = 1.0f;
        bool active = false;
    };
    std::array<SampleVoice, 8> sampleVoices;
public:
    std::atomic<float> atomicSampleVol { 0.8f };

private:
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SnareProcessor)
};
