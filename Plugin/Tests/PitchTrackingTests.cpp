// Unit tests for clarisynth::holdMsToFrames — the ms -> FFT-frame conversion behind the
// user-facing "Pitch Hold" parameter.

#include "PitchTracking.h"

#include <cstdio>

namespace
{
int failures = 0;

void check (bool cond, const char* name)
{
    if (cond)
    {
        std::printf ("  ok   %s\n", name);
    }
    else
    {
        std::printf ("  FAIL %s\n", name);
        ++failures;
    }
}
} // namespace

int main()
{
    using clarisynth::holdMsToFrames;

    std::printf ("holdMsToFrames:\n");

    // At 48 kHz with a 4096-sample FFT, one frame is 4096/48000 s = 85.33 ms.
    // The old hardcoded default was 12 frames; ~1000 ms should round-trip to it.
    check (holdMsToFrames (1000.0f, 48000.0, 4096) == 12,
           "1000 ms @ 48k/4096 -> 12 frames (matches the old default)");

    // Zero hold -> release immediately.
    check (holdMsToFrames (0.0f, 48000.0, 4096) == 0, "0 ms -> 0 frames");

    // Rounds to nearest: half a frame (~42.7 ms) rounds up to 1.
    check (holdMsToFrames (43.0f, 48000.0, 4096) == 1, "~half a frame -> rounds to 1");
    check (holdMsToFrames (40.0f, 48000.0, 4096) == 0, "under half a frame -> rounds to 0");

    // Max of the parameter range (2000 ms) -> ~23 frames.
    check (holdMsToFrames (2000.0f, 48000.0, 4096) == 23, "2000 ms @ 48k/4096 -> 23 frames");

    // Different sample rate scales the frame length: 44.1k/4096 -> ~92.9 ms/frame.
    check (holdMsToFrames (1000.0f, 44100.0, 4096) == 11, "1000 ms @ 44.1k/4096 -> 11 frames");

    // Degenerate inputs are safe.
    check (holdMsToFrames (1000.0f, 0.0,    4096) == 0, "zero sample rate -> 0");
    check (holdMsToFrames (1000.0f, 48000.0, 0)   == 0, "zero fft size -> 0");
    check (holdMsToFrames (-50.0f, 48000.0, 4096) == 0, "negative ms -> 0");

    std::printf ("\n%s (%d failure%s)\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
