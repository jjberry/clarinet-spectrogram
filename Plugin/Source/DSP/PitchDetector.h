#pragma once
#include <juce_core/juce_core.h>

// Strategy interface for fundamental pitch estimation.
// Input: dB magnitude spectrum from SpectralProcessor.
// Output: fundamental Hz, or 0.0f if no pitch detected / signal too quiet.
class PitchDetector
{
public:
    virtual ~PitchDetector() = default;
    virtual float detectPitch (const float* magnitudeDb, int binCount, float binHz) = 0;
    virtual juce::String getName() const = 0;

    // Restrict the fundamental search to [minHz, maxHz]. Default: no-op (detector ignores it).
    virtual void setSearchRange (float /*minHz*/, float /*maxHz*/) {}
};
