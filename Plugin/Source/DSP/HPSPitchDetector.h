#pragma once
#include "PitchDetector.h"

// Harmonic Product Spectrum pitch detector — wraps spectral_processor_hps() from the C DSP module.
class HPSPitchDetector : public PitchDetector
{
public:
    explicit HPSPitchDetector (int numHarmonics = 5) : numHarmonics (numHarmonics) {}

    float detectPitch (const float* magnitudeDb, int binCount, float binHz) override;
    juce::String getName() const override { return "HPS"; }

private:
    int numHarmonics;
};
