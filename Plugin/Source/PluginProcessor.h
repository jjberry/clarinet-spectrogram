#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/PitchDetector.h"
#include "DSP/HarmonicProcessor.h"
#include "SpectralProcessor.h"
#include <memory>
#include <vector>

class ClariSynthProcessor : public juce::AudioProcessor
{
public:
    ClariSynthProcessor();
    ~ClariSynthProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;


    const juce::String getName() const override { return "ClariSynth"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Read-only access for the editor
    float getCurrentFundamental() const { return fundamentalHz.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP pipeline
    static constexpr int kFftSize = 4096;
    SpectralProcessorContext* spectralCtx = nullptr;
    std::vector<float> fftAccumulator;
    std::vector<float> magnitudeDb;
    int accumulatorFill = 0;

    std::unique_ptr<PitchDetector>    pitchDetector;
    std::unique_ptr<HarmonicProcessor> harmonicProcessor;

    std::atomic<float> fundamentalHz { 0.0f };

    double currentSampleRate   = 44100.0;
    int    currentBlockSize    = 512;

    HarmonicParams buildParams() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClariSynthProcessor)
};
