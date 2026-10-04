#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "PadSynth.h"

class ChordForgeProcessor : public juce::AudioProcessor,
                            private juce::AsyncUpdater,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    ChordForgeProcessor();
    ~ChordForgeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ChordForge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- used by the editor ---
    juce::AudioProcessorValueTreeState apvts;
    cf::Progression getProgression() const;
    void newIdea();
    void setPlaying (bool);
    bool isPlaying() const { return playing.load(); }
    double getPlayBeat() const { return uiBeat.load(); }
    int getVersion() const { return version.load(); }
    int getSeed() const { return seed.load(); }
    juce::File exportMidi();

private:
    struct Ev { double beat; bool on; int pitch; int vel; };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { regenerate(); }
    void regenerate();

    mutable juce::SpinLock lock;
    cf::Progression progression;
    std::vector<Ev> events;
    std::atomic<int> seed { 1 }, version { 0 };
    std::atomic<bool> playing { false }, restart { false };
    std::atomic<double> uiBeat { 0.0 };
    bool wasPlaying = false;
    double playBeat = 0.0;
    static constexpr double kLoop = 32.0;

    juce::Synthesiser synth;
};
