#pragma once

#include <cmath>

namespace clarisynth
{

/** Converts a "hold" time in milliseconds to a number of FFT analysis frames.

    Pitch tracking advances one frame each time the processor accumulates `fftSize`
    input samples, so a frame lasts `fftSize / sampleRate` seconds. The "hold" feature
    keeps the last detected pitch alive for this many frames of silence before releasing,
    so the control is exposed to the user in musical milliseconds rather than frames.

    @returns the hold length in frames (>= 0), or 0 for invalid sample rate / fft size.
*/
inline int holdMsToFrames (float holdMs, double sampleRate, int fftSize) noexcept
{
    if (sampleRate <= 0.0 || fftSize <= 0 || holdMs <= 0.0f)
        return 0;

    const double frameMs = 1000.0 * (double) fftSize / sampleRate;
    return (int) std::lround ((double) holdMs / frameMs);
}

} // namespace clarisynth
