#pragma once
#include <JuceHeader.h>
#include "SnareEngine.h"
#include <atomic>
#include <memory>

class SnareProcessor : public juce::AudioProcessor {
public:
    SnareProcessor();
    ~SnareProcessor() override = default;

    void prepareToPlay(double sr, int) override { sampleRate = sr; }
    void releaseResources() override { playing.store(false); playPos.store(0.f); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Snare Rhythm Generator"; }
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
    void startPlayback() { playPos.store(0.f); playing.store(true); }
    void stopPlayback()  { playing.store(false); }
    bool isPlaying() const { return playing.load(); }
    float getPlaybackPos() const { return playPos.load(); }

    // Params — only touched from message thread (editor)
    snare::Params params;

    // Display data — lock-free double buffer (written by regenerate on msg thread, read by editor paint)
    struct DisplayData {
        std::vector<snare::SnareHit> hits;
        snare::Scores scores;
    };
    std::shared_ptr<DisplayData> getDisplayData() const { return std::atomic_load(&displayData); }

private:
    double sampleRate = 44100.0;
    std::atomic<bool> playing { false };
    std::atomic<float> playPos { 0.f };

    // Display data — atomic shared_ptr for lock-free thread safety
    std::shared_ptr<DisplayData> displayData = std::make_shared<DisplayData>();

    // Audio thread data — lock-free swap via atomic shared_ptr
    struct AudioData {
        std::vector<snare::SnareHit> hits;
        int snareNote = 38;
        int bpm = 90;
        int bars = 4;
        int timeSig = 4;
        int resolution = 4;
    };
    std::shared_ptr<AudioData> audioData = std::make_shared<AudioData>();

    // Pending note-offs tracked across buffers
    struct PendingNoteOff {
        int sampleOffset;  // absolute sample from playback start (modulo pattern length)
        int note;
    };
    std::vector<PendingNoteOff> pendingOffs;
    int64_t absoluteSample = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SnareProcessor)
};
