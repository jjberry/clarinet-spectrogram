#pragma once

namespace clarisynth
{

/** Selects which input channel to analyse: the one with the greatest RMS level.

    This replaces an earlier approach that fell back from ch0 to ch1 only when ch0's
    RMS dropped below a fixed threshold (1e-6). That failed whenever ch0 carried a
    constant noise floor above the threshold — e.g. an unused hardware input feeding a
    few micro-volts — which left analysis stuck on a silent channel, so pitch tracked
    noise and the main output went dead. Comparing channels against each other is robust
    regardless of noise floor or which channel the source happens to land on.

    Ties are broken toward the lower index (strictly-greater comparison), so when nothing
    is playing the analysis channel stays at 0.

    @param channelRms   per-channel RMS values (length >= numChannels)
    @param numChannels  number of valid entries in channelRms
    @returns the index of the loudest channel, or 0 if there are no channels.
*/
inline int selectAnalysisChannel (const float* channelRms, int numChannels) noexcept
{
    if (channelRms == nullptr || numChannels <= 0)
        return 0;

    int   best    = 0;
    float bestRms = channelRms[0];

    for (int ch = 1; ch < numChannels; ++ch)
    {
        if (channelRms[ch] > bestRms)
        {
            bestRms = channelRms[ch];
            best    = ch;
        }
    }

    return best;
}

/** Copies one channel's samples into every other channel — i.e. broadcasts a mono source
    across all output channels.

    A monophonic instrument lands on a single channel, but the source can arrive on ch0
    (mono input) or ch1 (right of a "stereo" pair where only the right is connected). Either
    way we want both speakers to carry it, so after choosing the analysis channel we broadcast
    it everywhere. Doing this for all channels (rather than only patching ch0) keeps mono-in →
    stereo-out correct regardless of which channel the source landed on.

    @param channels       array of writable channel pointers (length >= numChannels)
    @param numChannels    number of channels to fill
    @param sourceChannel  index of the channel to copy from
    @param numSamples     samples per channel
*/
inline void broadcastToAllChannels (float* const* channels, int numChannels,
                                    int sourceChannel, int numSamples) noexcept
{
    if (channels == nullptr || numSamples <= 0
        || sourceChannel < 0 || sourceChannel >= numChannels)
        return;

    const float* src = channels[sourceChannel];

    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (ch == sourceChannel || channels[ch] == nullptr)
            continue;

        for (int i = 0; i < numSamples; ++i)
            channels[ch][i] = src[i];
    }
}

} // namespace clarisynth
