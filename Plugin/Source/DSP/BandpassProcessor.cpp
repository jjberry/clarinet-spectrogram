#include "BandpassProcessor.h"
#include <cmath>

void BandpassProcessor::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    for (auto& band : bands)
    {
        for (auto& f : band.filters)
            f.prepare (spec);
        band.gainLinear.reset (sr, 0.01); // 10 ms smoothing
        band.gainLinear.setCurrentAndTargetValue (1.0f);
        band.currentCentreHz = 0.0f;
    }
}

void BandpassProcessor::reset()
{
    for (auto& band : bands)
        for (auto& f : band.filters)
            f.reset();
}

float BandpassProcessor::computeHarmonicGainLinear (int harmonic, const HarmonicParams& params)
{
    // harmonic is 1-based (1 = fundamental)
    float gainDb = (harmonic <= kMaxHarmonics) ? params.harmonicGainDb[harmonic - 1] : 0.0f;

    // Odd/even offset
    if (harmonic % 2 != 0)
        gainDb += params.oddEvenBoostDb;

    // Spectral tilt: dB per octave relative to fundamental (harmonic 1)
    if (harmonic > 1 && params.centroidTiltDbPerOctave != 0.0f)
    {
        float octavesAbove = std::log2f ((float) harmonic);
        gainDb += params.centroidTiltDbPerOctave * octavesAbove;
    }

    return juce::Decibels::decibelsToGain (gainDb);
}

void BandpassProcessor::updateCoefficients (HarmonicBand& band, float centreHz,
                                            int harmonic, const HarmonicParams& params)
{
    if (centreHz == band.currentCentreHz)
        return;

    band.currentCentreHz = centreHz;
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (
        sampleRate, centreHz, kQFactor);

    for (auto& f : band.filters)
        *f.coefficients = *coeffs;

    band.gainLinear.setTargetValue (computeHarmonicGainLinear (harmonic, params));
}

void BandpassProcessor::process (juce::AudioBuffer<float>& buffer,
                                 float fundamentalHz,
                                 const HarmonicParams& params)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (fundamentalHz <= 0.0f)
        return; // no pitch — pass through dry

    // Accumulate processed harmonics into a separate buffer, then mix
    juce::AudioBuffer<float> wetBuffer (numChannels, numSamples);
    wetBuffer.clear();

    const double nyquist = sampleRate * 0.5;

    for (int h = 0; h < kMaxHarmonics; ++h)
    {
        float centreHz = fundamentalHz * (float)(h + 1);
        if (centreHz >= (float) nyquist)
            break;

        auto& band = bands[h];
        updateCoefficients (band, centreHz, h + 1, params);

        // Copy dry signal into a scratch buffer, filter it, accumulate into wet
        juce::AudioBuffer<float> scratch (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            scratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        for (int ch = 0; ch < std::min (numChannels, 2); ++ch)
        {
            auto* data = scratch.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = band.filters[(size_t) ch].processSample (data[i]);
        }

        float gain = band.gainLinear.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            wetBuffer.addFrom (ch, 0, scratch, ch, 0, numSamples, gain);
    }

    // Mix dry + wet
    const float wet = params.mix;
    const float dry = 1.0f - wet;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        const auto* w = wetBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            out[i] = dry * out[i] + wet * w[i];
    }
}
