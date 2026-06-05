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

#ifdef __cplusplus
}
#endif
