import Foundation

/// Spectral analysis results passed from DSP thread to render thread.
/// Shared state is exchanged via double-buffer or atomic swap — no locks on DSP thread.
struct SpectralData {
    /// dB magnitude per FFT bin (fft_size/2 + 1 elements).
    var magnitudeDB: [Float] = []
    /// Spectral centroid in Hz.
    var centroid: Float = 0
    /// Ratio of odd-harmonic energy to even-harmonic energy.
    var oddEvenRatio: Float = 0
    /// Detected fundamental frequency in Hz (0 if undetected).
    var fundamental: Float = 0

    static let empty = SpectralData()
}
