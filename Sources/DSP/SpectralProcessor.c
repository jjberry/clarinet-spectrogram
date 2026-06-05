#include "SpectralProcessor.h"
#include <Accelerate/Accelerate.h>
#include <stdlib.h>
#include <math.h>

struct SpectralProcessorContext {
    int           fft_size;
    vDSP_Length   log2n;
    FFTSetup      fft_setup;
    float        *window;     // Hann coefficients, fft_size floats
    float        *windowed;   // scratch: windowed input, fft_size floats
    float        *magnitude;  // scratch: linear magnitudes, fft_size/2+1 floats
    DSPSplitComplex split;    // split-complex workspace, each half fft_size/2 floats
};

SpectralProcessorContext *spectral_processor_create(int fft_size) {
    if (fft_size <= 0 || (fft_size & (fft_size - 1)) != 0) return NULL;

    SpectralProcessorContext *ctx = calloc(1, sizeof(SpectralProcessorContext));
    if (!ctx) return NULL;

    ctx->fft_size = fft_size;
    ctx->log2n    = (vDSP_Length)lround(log2((double)fft_size));

    ctx->fft_setup = vDSP_create_fftsetup(ctx->log2n, FFT_RADIX2);
    if (!ctx->fft_setup) { free(ctx); return NULL; }

    int half = fft_size / 2;
    ctx->window      = malloc((size_t)fft_size * sizeof(float));
    ctx->windowed    = malloc((size_t)fft_size * sizeof(float));
    ctx->magnitude   = malloc((size_t)(half + 1) * sizeof(float));
    ctx->split.realp = malloc((size_t)half * sizeof(float));
    ctx->split.imagp = malloc((size_t)half * sizeof(float));

    if (!ctx->window || !ctx->windowed || !ctx->magnitude ||
        !ctx->split.realp || !ctx->split.imagp) {
        spectral_processor_destroy(ctx);
        return NULL;
    }

    // vDSP_HANN_NORM: periodic Hann window, correct for overlap-add spectral analysis
    vDSP_hann_window(ctx->window, (vDSP_Length)fft_size, vDSP_HANN_NORM);
    return ctx;
}

void spectral_processor_destroy(SpectralProcessorContext *ctx) {
    if (!ctx) return;
    if (ctx->fft_setup) vDSP_destroy_fftsetup(ctx->fft_setup);
    free(ctx->window);
    free(ctx->windowed);
    free(ctx->magnitude);
    free(ctx->split.realp);
    free(ctx->split.imagp);
    free(ctx);
}

int spectral_processor_bin_count(SpectralProcessorContext *ctx) {
    return ctx ? ctx->fft_size / 2 + 1 : 0;
}

void spectral_processor_process(SpectralProcessorContext *ctx,
                                const float *samples,
                                float *magnitude_db) {
    if (!ctx || !samples || !magnitude_db) return;

    int fft_size  = ctx->fft_size;
    int half      = fft_size / 2;
    int bin_count = half + 1;

    // Apply Hann window
    vDSP_vmul(samples, 1, ctx->window, 1, ctx->windowed, 1, (vDSP_Length)fft_size);

    // Pack real input into split-complex: N real → N/2 complex pairs
    vDSP_ctoz((DSPComplex *)ctx->windowed, 2, &ctx->split, 1, (vDSP_Length)half);

    // In-place forward real FFT
    vDSP_fft_zrip(ctx->fft_setup, &ctx->split, 1, ctx->log2n, FFT_FORWARD);

    // vDSP packs DC into realp[0] and Nyquist into imagp[0] after zrip.
    // Zero imagp[0] before zvabs so bin 0 is treated as real-only.
    float dc      = ctx->split.realp[0];
    float nyquist = ctx->split.imagp[0];
    ctx->split.imagp[0] = 0.0f;

    vDSP_zvabs(&ctx->split, 1, ctx->magnitude, 1, (vDSP_Length)half);

    ctx->magnitude[0]    = fabsf(dc);
    ctx->magnitude[half] = fabsf(nyquist);

    // Normalise: vDSP_fft_zrip output is 2x the standard DFT amplitude
    float scale = 1.0f / (float)fft_size;
    vDSP_vsmul(ctx->magnitude, 1, &scale, ctx->magnitude, 1, (vDSP_Length)bin_count);

    // Clamp to noise floor before log to prevent -inf in silence
    float noise_floor = 1e-7f;
    vDSP_vthr(ctx->magnitude, 1, &noise_floor, ctx->magnitude, 1, (vDSP_Length)bin_count);

    // 20 * log10(x) — amplitude dB (flag 0)
    float one = 1.0f;
    vDSP_vdbcon(ctx->magnitude, 1, &one, magnitude_db, 1, (vDSP_Length)bin_count, 0);
}

// ── Feature extraction ────────────────────────────────────────────────────────
// Both functions expect dB magnitudes from spectral_processor_process.
// They convert to linear internally to give perceptually meaningful results.

float spectral_processor_centroid(const float *magnitude_db,
                                  int bin_count,
                                  float bin_hz) {
    // Weighted mean frequency: Σ(f_i * mag_i) / Σ(mag_i), linear magnitudes
    float num = 0.0f, den = 0.0f;
    for (int i = 0; i < bin_count; i++) {
        float linear = powf(10.0f, magnitude_db[i] * 0.05f); // 10^(dB/20)
        num += (float)i * bin_hz * linear;
        den += linear;
    }
    return den > 1e-10f ? num / den : 0.0f;
}

float spectral_processor_odd_even_ratio(const float *magnitude_db,
                                        int bin_count,
                                        float fundamental_hz,
                                        float sample_rate) {
    if (fundamental_hz <= 0.0f) return 0.0f;

    // Reconstruct bin spacing from bin_count (bin_count = fft_size/2 + 1)
    float bin_hz  = sample_rate / (float)(2 * (bin_count - 1));
    float nyquist = sample_rate * 0.5f;
    float odd_sum = 0.0f, even_sum = 0.0f;

    for (int n = 1; (float)n * fundamental_hz < nyquist; n++) {
        int bin = (int)roundf((float)n * fundamental_hz / bin_hz);
        if (bin >= bin_count) break;

        float linear = powf(10.0f, magnitude_db[bin] * 0.05f);
        if (n & 1) odd_sum  += linear;
        else       even_sum += linear;
    }

    // Values > 1 mean odd harmonics dominate — normal for clarinet's cylindrical bore
    return even_sum > 1e-10f ? odd_sum / even_sum : 0.0f;
}

float spectral_processor_hps(const float *magnitude_db,
                              int bin_count,
                              float bin_hz,
                              int num_harmonics) {
    if (!magnitude_db || bin_count < 2 || bin_hz <= 0.0f) return 0.0f;
    if (num_harmonics < 2) num_harmonics = 2;

    // Search range: 80 Hz (low clarinet/bass clarinet) to 2 kHz (altissimo)
    int min_bin = (int)ceilf(80.0f  / bin_hz);
    int max_bin = (int)floorf(2000.0f / bin_hz);
    // With num_harmonics stages the highest usable bin is bin_count / num_harmonics
    if (max_bin >= bin_count / num_harmonics) max_bin = bin_count / num_harmonics - 1;
    if (min_bin < 1 || min_bin > max_bin) return 0.0f;

    float best_product = -1.0f;
    int   best_bin     =  0;

    for (int b = min_bin; b <= max_bin; b++) {
        float product = 1.0f;
        for (int h = 1; h <= num_harmonics; h++) {
            int idx = b * h;
            if (idx >= bin_count) { product = 0.0f; break; }
            // Convert dB → linear amplitude: 10^(dB/20)
            product *= powf(10.0f, magnitude_db[idx] * 0.05f);
        }
        if (product > best_product) {
            best_product = product;
            best_bin     = b;
        }
    }

    if (best_bin == 0) return 0.0f;
    // Reject if fundamental bin is below the silence floor
    if (magnitude_db[best_bin] < -70.0f) return 0.0f;

    return (float)best_bin * bin_hz;
}
