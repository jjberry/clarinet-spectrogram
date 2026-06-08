#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SpectralProcessorContext SpectralProcessorContext;

/// Allocate and initialize a processor for the given FFT size (must be a power of two).
SpectralProcessorContext *spectral_processor_create(int fft_size);

/// Free all resources.
void spectral_processor_destroy(SpectralProcessorContext *ctx);

/// Number of magnitude bins produced by process: fft_size/2 + 1.
int spectral_processor_bin_count(SpectralProcessorContext *ctx);

/// Apply Hann window, compute real FFT, convert to dB magnitude.
/// `samples`     — input buffer, exactly fft_size frames
/// `magnitude_db` — output buffer, fft_size/2 + 1 bins
void spectral_processor_process(SpectralProcessorContext *ctx,
                                const float *samples,
                                float *magnitude_db);

/// Spectral centroid in Hz. `bin_hz` = sample_rate / fft_size.
float spectral_processor_centroid(const float *magnitude_db,
                                  int bin_count,
                                  float bin_hz);

/// Ratio of odd-harmonic energy to even-harmonic energy.
/// Returns values > 1 when odd harmonics dominate (normal for clarinet).
float spectral_processor_odd_even_ratio(const float *magnitude_db,
                                        int bin_count,
                                        float fundamental_hz,
                                        float sample_rate);

/// Harmonic Product Spectrum pitch estimator.
/// Multiplies downsampled copies of the magnitude spectrum to find the fundamental.
/// `num_harmonics` — number of HPS stages (4–5 recommended).
/// `min_hz` / `max_hz` — restrict the fundamental search to this range. Pass
/// 80 / 2000 for the previous default (low clarinet to altissimo). If the range is
/// invalid (min >= max, or non-positive) the function returns 0.
/// Returns fundamental frequency in Hz, or 0 if no pitch detected / signal too quiet.
float spectral_processor_hps(const float *magnitude_db,
                              int bin_count,
                              float bin_hz,
                              int num_harmonics,
                              float min_hz,
                              float max_hz);

/// Quadratic (parabolic) peak interpolation. Given three equally-spaced samples
/// straddling a local maximum (`ym` left, `y0` centre/peak, `yp` right), returns the
/// sub-sample offset of the true peak in [-0.5, +0.5]. Used to refine the HPS bin to
/// sub-bin frequency accuracy, which matters most at low pitches where bins are widely
/// spaced. Returns 0 if the three points don't form a peak.
float spectral_processor_parabolic_offset(float ym, float y0, float yp);

#ifdef __cplusplus
}
#endif
