#pragma once
#include "HarmonicProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <array>

// Harmonic processor using a bank of IIR bandpass filters, one per harmonic.
// Filter centre frequencies track the live fundamental; gains are updated per block.
// Near-zero latency. Coefficient interpolation is smoothed to avoid zipper noise.
class BandpassProcessor : public HarmonicProcessor
{
public:
    static constexpr int kMaxHarmonics = 16;
    static constexpr float kQFactor    = 15.0f; // wide enough to tolerate pitch wobble

    void prepare (double sampleRate, int samplesPerBlock) override;
    void process (juce::AudioBuffer<float>& buffer,
                  float fundamentalHz,
                  const HarmonicParams& params) override;
    void reset() override;
    juce::String getName() const override { return "Bandpass"; }

private:
    double sampleRate = 44100.0;

    struct HarmonicBand
    {
        // One filter chain per channel (stereo support)
        std::array<juce::dsp::IIR::Filter<float>, 2> filters;
        juce::SmoothedValue<float> gainLinear;
        float currentCentreHz = 0.0f;
    };

    std::array<HarmonicBand, kMaxHarmonics> bands;

    void updateCoefficients (HarmonicBand& band, float centreHz, int harmonic,
                             const HarmonicParams& params);
    float computeHarmonicGainLinear (int harmonic, const HarmonicParams& params);
};
