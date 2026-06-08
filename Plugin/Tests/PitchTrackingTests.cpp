// Unit tests for clarisynth::holdMsToFrames (the ms -> FFT-frame conversion behind the
// user-facing "Pitch Hold" parameter) and the HPS sub-bin parabolic interpolation.

#include "PitchTracking.h"
#include "SpectralProcessor.h"

#include <cstdio>
#include <cmath>
#include <vector>

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

bool approx (float a, float b, float tol) { return std::fabs (a - b) <= tol; }

// Build a dB magnitude spectrum with parabolic "leakage" peaks at each harmonic of f0,
// so HPS has off-bin energy to interpolate. Around the fundamental bin the spectrum is
// dominated by the fundamental's own (cleanly parabolic) leakage.
void buildHarmonicSpectrum (std::vector<float>& magDb, float binHz, float f0Hz,
                            int numHarmonics, float peakDb)
{
    std::fill (magDb.begin(), magDb.end(), -120.0f);
    const int n = (int) magDb.size();
    for (int h = 1; h <= numHarmonics; ++h)
    {
        const float fbin = (f0Hz * (float) h) / binHz;   // fractional bin of this harmonic
        const int   centre = (int) std::lround (fbin);
        for (int d = -8; d <= 8; ++d)
        {
            const int b = centre + d;
            if (b < 0 || b >= n) continue;
            const float dist = (float) b - fbin;
            const float val  = peakDb - 2.0f * dist * dist;
            if (val > magDb[(size_t) b]) magDb[(size_t) b] = val;
        }
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

    using clarisynth::downwardSlewRatioPerFrame;

    std::printf ("\ndownwardSlewRatioPerFrame:\n");

    // One frame at 48k/4096 is 85.33 ms. 90 st/s -> 7.68 st/frame -> ratio 2^(-7.68/12).
    check (approx (downwardSlewRatioPerFrame (90.0f, 48000.0, 4096),
                   std::pow (2.0f, -(90.0f * 4096.0f / 48000.0f) / 12.0f), 1e-5f),
           "90 st/s ratio matches 2^(-st_per_frame/12)");

    // A ratio applied to a pitch caps the per-frame drop: 90 st/s from 200 Hz floors near 128 Hz.
    check (approx (200.0f * downwardSlewRatioPerFrame (90.0f, 48000.0, 4096), 128.3f, 1.0f),
           "200 Hz drops to ~128 Hz in one frame at 90 st/s");

    // Higher rate => weaker limiting (ratio closer to 0 / smaller floor).
    check (downwardSlewRatioPerFrame (1200.0f, 48000.0, 4096)
             < downwardSlewRatioPerFrame (90.0f, 48000.0, 4096),
           "higher st/s -> smaller ratio (less limiting)");

    // Very high rate is effectively unlimited.
    check (downwardSlewRatioPerFrame (1200.0f, 48000.0, 4096) < 0.01f,
           "1200 st/s -> ~unlimited");

    // Invalid config -> 0 (unlimited), never freezes tracking.
    check (downwardSlewRatioPerFrame (90.0f, 0.0, 4096) == 0.0f, "zero sample rate -> 0 (unlimited)");
    check (downwardSlewRatioPerFrame (90.0f, 48000.0, 0) == 0.0f, "zero fft size -> 0 (unlimited)");

    std::printf ("\nspectral_processor_parabolic_offset:\n");

    // Symmetric peak -> vertex exactly at centre.
    check (approx (spectral_processor_parabolic_offset (-1.0f, 0.0f, -1.0f), 0.0f, 1e-6f),
           "symmetric peak -> offset 0");

    // Parabola y = -(x - 0.3)^2 sampled at x = -1, 0, 1 -> vertex at +0.3.
    check (approx (spectral_processor_parabolic_offset (-1.69f, -0.09f, -0.49f), 0.3f, 1e-4f),
           "vertex at +0.3 recovered");

    // Parabola y = -(x + 0.4)^2 -> vertex at -0.4.
    check (approx (spectral_processor_parabolic_offset (-0.36f, -0.16f, -1.96f), -0.4f, 1e-4f),
           "vertex at -0.4 recovered");

    // Concave-up (not a peak) -> 0.
    check (spectral_processor_parabolic_offset (-1.0f, -2.0f, -1.0f) == 0.0f,
           "concave-up triple -> 0 (not a peak)");

    // Runaway estimate is clamped to [-0.5, 0.5].
    check (spectral_processor_parabolic_offset (0.0f, -0.01f, -2.0f) == -0.5f,
           "extreme skew -> clamped to -0.5");

    std::printf ("\nspectral_processor_hps (with interpolation):\n");

    const float binHz = 48000.0f / 4096.0f;   // ~11.72 Hz/bin, the plugin's real resolution
    std::vector<float> magDb (2049);

    // Mid-range A2 whose true frequency lies well off a bin centre.
    buildHarmonicSpectrum (magDb, binHz, 110.0f, 5, -6.0f);
    float a2 = spectral_processor_hps (magDb.data(), (int) magDb.size(), binHz, 5, 50.0f, 2000.0f);
    check (approx (a2, 110.0f, 0.5f), "recovers A2 = 110 Hz (between bins) within 0.5 Hz");

    // Bass clarinet low Bb1 (~58.27 Hz) with the new 50 Hz floor — the motivating case.
    // best_bin (5) equals min_bin here, so this also exercises interpolation at the floor.
    buildHarmonicSpectrum (magDb, binHz, 58.27f, 5, -6.0f);
    float bb1 = spectral_processor_hps (magDb.data(), (int) magDb.size(), binHz, 5, 50.0f, 2000.0f);
    check (approx (bb1, 58.27f, 0.5f), "recovers bass clarinet Bb1 = 58.27 Hz within 0.5 Hz");

    // Without interpolation Bb1 would quantise to bin 5 = 58.59 Hz; confirm we beat that.
    check (std::fabs (bb1 - 58.27f) < std::fabs (5.0f * binHz - 58.27f),
           "interpolated Bb1 beats the raw bin-centre estimate");

    std::printf ("\n%s (%d failure%s)\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
