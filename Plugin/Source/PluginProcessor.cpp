#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/HPSPitchDetector.h"
#include "DSP/BandpassProcessor.h"
#include "DSP/ChannelRouting.h"
#include "DSP/PitchTracking.h"
#include <array>

juce::AudioProcessorValueTreeState::ParameterLayout
ClariSynthProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Individual harmonic gains H1–H8
    for (int i = 1; i <= 8; ++i)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            "harmonic" + juce::String(i),
            "H" + juce::String(i) + " Gain",
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f),
            0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "oddEvenBoost", "Odd/Even Boost",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "centroidTilt", "Centroid Tilt (dB/oct)",
        juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    // --- Pitch tracking ---
    // EMA smoothing coefficient: low = smooth/slow glide, high = snappy/responsive.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pitchSmoothing", "Pitch Smoothing",
        juce::NormalisableRange<float> (0.01f, 1.0f, 0.001f), 0.2f));

    // Downward pitch slew limit (semitones/sec). Caps how fast the tracked pitch may FALL,
    // which rides over the brief downward dip during plucked-note attack transients while
    // leaving upward tracking instant. Skewed low (more resolution where it matters); the
    // top of the range is effectively unlimited.
    auto slewRange = juce::NormalisableRange<float> (6.0f, 1200.0f, 1.0f);
    slewRange.setSkewForCentre (90.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pitchSlewDown", "Pitch Slew Down", slewRange, 90.0f));

    // Fundamental search range. Skewed so low frequencies get more of the control's travel.
    auto minRange = juce::NormalisableRange<float> (40.0f, 500.0f, 1.0f);
    minRange.setSkewForCentre (120.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pitchMinHz", "Pitch Min", minRange, 50.0f));  // 50 Hz covers bass clarinet low Bb1 (~58 Hz)

    auto maxRange = juce::NormalisableRange<float> (500.0f, 4000.0f, 1.0f);
    maxRange.setSkewForCentre (1500.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pitchMaxHz", "Pitch Max", maxRange, 2000.0f));

    return layout;
}

ClariSynthProcessor::ClariSynthProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ClariSynth", createParameterLayout())
{
    spectralCtx = spectral_processor_create (kFftSize);
    magnitudeDb.resize ((size_t)(kFftSize / 2 + 1));
    fftAccumulator.resize ((size_t) kFftSize, 0.0f);

    pitchDetector     = std::make_unique<HPSPitchDetector> (5);
    harmonicProcessor = std::make_unique<BandpassProcessor>();
}

ClariSynthProcessor::~ClariSynthProcessor()
{
    spectral_processor_destroy (spectralCtx);
}

bool ClariSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Accept mono or stereo input, mono or stereo output
    auto in  = layouts.getMainInputChannelSet();
    auto out = layouts.getMainOutputChannelSet();
    if (in  != juce::AudioChannelSet::mono() && in  != juce::AudioChannelSet::stereo()) return false;
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void ClariSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;
    accumulatorFill   = 0;
    smoothedHz        = 0.0f;
    lastValidHz       = 0.0f;
    silenceFrames     = 0;

    // Hold time is fixed (1 s feels right); only the smoothing, slew and range are exposed.
    silenceHoldFrames = clarisynth::holdMsToFrames (kSilenceHoldMs, sampleRate, kFftSize);

    harmonicProcessor->prepare (sampleRate, samplesPerBlock);
}

void ClariSynthProcessor::releaseResources()
{
    harmonicProcessor->reset();
}

HarmonicParams ClariSynthProcessor::buildParams() const
{
    HarmonicParams p;
    for (int i = 1; i <= 8; ++i)
        p.harmonicGainDb[i - 1] = apvts.getRawParameterValue ("harmonic" + juce::String(i))->load();
    p.oddEvenBoostDb         = apvts.getRawParameterValue ("oddEvenBoost")->load();
    p.centroidTiltDbPerOctave = apvts.getRawParameterValue ("centroidTilt")->load();
    p.mix                    = apvts.getRawParameterValue ("mix")->load();
    return p;
}

void ClariSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples   = buffer.getNumSamples();
    const int numInChans   = getTotalNumInputChannels();
    const int numOutChans  = getTotalNumOutputChannels();

    // Zero any output channels that have no corresponding input
    for (int ch = numInChans; ch < numOutChans; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Pick the loudest input channel for analysis. A simple "is ch0 silent?" test fails
    // when ch0 carries only a noise floor (e.g. an unused hardware input feeding a few
    // micro-volts), which sits well above any fixed threshold — so we compare channels
    // against each other instead (see selectAnalysisChannel). This is robust whether the
    // source is on ch0, ch1, or both (mono guitar on a stereo pair lands on one channel).
    std::array<float, 64> channelRms {};
    const int numRms = std::min (numInChans, (int) channelRms.size());
    for (int ch = 0; ch < numRms; ++ch)
        channelRms[(size_t) ch] = buffer.getRMSLevel (ch, 0, numSamples);

    const int analysisChannel = clarisynth::selectAnalysisChannel (channelRms.data(), numRms);

    // Snapshot pitch-tracking parameters once per block (user / DAW automation controlled).
    const float pitchAlpha    = apvts.getRawParameterValue ("pitchSmoothing")->load();
    const float pitchSlewDown = apvts.getRawParameterValue ("pitchSlewDown")->load();
    const float pitchMinHz    = apvts.getRawParameterValue ("pitchMinHz")->load();
    const float pitchMaxHz    = apvts.getRawParameterValue ("pitchMaxHz")->load();
    const float downSlewRatio = clarisynth::downwardSlewRatioPerFrame (pitchSlewDown, currentSampleRate, kFftSize);
    pitchDetector->setSearchRange (pitchMinHz, pitchMaxHz);

    // Accumulate into FFT analysis buffer
    const float* readPtr = buffer.getReadPointer (analysisChannel);
    int srcOffset = 0;
    while (srcOffset < numSamples)
    {
        int toCopy = std::min (numSamples - srcOffset, kFftSize - accumulatorFill);
        std::copy (readPtr + srcOffset, readPtr + srcOffset + toCopy,
                   fftAccumulator.begin() + accumulatorFill);
        accumulatorFill += toCopy;
        srcOffset       += toCopy;

        if (accumulatorFill == kFftSize)
        {
            spectral_processor_process (spectralCtx, fftAccumulator.data(), magnitudeDb.data());
            float binHz = (float)(currentSampleRate / kFftSize);
            float rawHz  = pitchDetector->detectPitch (magnitudeDb.data(),
                                                       (int) magnitudeDb.size(), binHz);

            if (rawHz > 0.0f)
            {
                if (smoothedHz > 0.0f)
                {
                    // EMA smoothing — glide toward new estimate
                    float candidate = pitchAlpha * rawHz + (1.0f - pitchAlpha) * smoothedHz;

                    // Downward slew limit: cap how far pitch may fall this frame. Upward
                    // moves are instant; downward dips (attack transients, octave glitches)
                    // are clamped so a brief wrong-low estimate is ridden over before it
                    // can pull the tracked pitch down.
                    const float minHz = smoothedHz * downSlewRatio;
                    if (candidate < minHz)
                        candidate = minHz;

                    smoothedHz = candidate;
                }
                else
                {
                    smoothedHz = rawHz;  // fresh note from silence — start at detected pitch
                }
                lastValidHz   = smoothedHz;
                silenceFrames = 0;
            }
            else
            {
                // Hold last valid pitch for silenceHoldFrames frames before releasing to zero
                ++silenceFrames;
                if (silenceFrames > silenceHoldFrames)
                    smoothedHz = 0.0f;
                // else: smoothedHz keeps its last value
            }

            fundamentalHz.store (smoothedHz);
            accumulatorFill = 0;
        }
    }

    // Broadcast the chosen (mono) source across every output channel so no output is left
    // silent — works whether the source landed on ch0 or ch1.
    clarisynth::broadcastToAllChannels (buffer.getArrayOfWritePointers(),
                                        numOutChans, analysisChannel, numSamples);

    // Apply harmonic processing
    HarmonicParams params = buildParams();
    harmonicProcessor->process (buffer, fundamentalHz.load(), params);
}

juce::AudioProcessorEditor* ClariSynthProcessor::createEditor()
{
    return new ClariSynthEditor (*this);
}

void ClariSynthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, dest);
}

void ClariSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClariSynthProcessor();
}
