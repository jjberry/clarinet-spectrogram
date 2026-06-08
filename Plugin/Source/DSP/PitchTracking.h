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

/** Per-frame downward gain ratio for a pitch slew limit specified in semitones/second.

    The tracked pitch is allowed to fall to at most `smoothedHz * ratio` each analysis
    frame. Returns a value in (0, 1]:
      - ratio == 1  => no downward movement permitted (rate of 0 st/s)
      - ratio -> 0  => effectively unlimited (very high st/s)
    Upward movement is never limited by this. Returns 0 (i.e. unlimited) for an invalid
    sample rate / fft size so tracking is never accidentally frozen.
*/
inline float downwardSlewRatioPerFrame (float semitonesPerSec, double sampleRate, int fftSize) noexcept
{
    if (sampleRate <= 0.0 || fftSize <= 0)
        return 0.0f;

    const double frameSec   = (double) fftSize / sampleRate;
    const double stPerFrame = (double) semitonesPerSec * frameSec;
    return (float) std::pow (2.0, -stPerFrame / 12.0);
}

} // namespace clarisynth
