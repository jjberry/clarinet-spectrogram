#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

struct HarmonicParams
{
    // Per-harmonic gain offsets in dB. Index 0 = fundamental, 1 = H2, etc.
    // Applied on top of the raw analyzed spectrum.
    float harmonicGainDb[16] = {};

    // Additive dB applied to all odd harmonics (1, 3, 5…) relative to even.
    // Positive = boost odd = more cylindrical-bore character.
    float oddEvenBoostDb = 0.0f;

    // Spectral tilt: dB per octave above fundamental. Negative = darker.
    float centroidTiltDbPerOctave = 0.0f;

    // 0.0 = dry only, 1.0 = wet only
    float mix = 1.0f;
};

// Strategy interface for harmonic audio processing.
// Implementations receive the raw audio buffer, current fundamental pitch,
// and parameter snapshot, and write their output back into the buffer.
class HarmonicProcessor
{
public:
    virtual ~HarmonicProcessor() = default;
    virtual void prepare (double sampleRate, int samplesPerBlock) = 0;
    virtual void process (juce::AudioBuffer<float>& buffer,
                          float fundamentalHz,
                          const HarmonicParams& params) = 0;
    virtual void reset() = 0;
    virtual juce::String getName() const = 0;
};
