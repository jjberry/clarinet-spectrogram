import Foundation

/// Spectral analysis results passed from DSP thread to render thread via MainActor.
struct SpectralData: Equatable {
    /// dB magnitude per FFT bin (fft_size/2 + 1 elements).
    var magnitudeDB: [Float] = []
    /// Spectral centroid in Hz.
    var centroid: Float = 0
    /// Ratio of odd-harmonic energy to even-harmonic energy.
    var oddEvenRatio: Float = 0
    /// Detected fundamental frequency in Hz (0 if undetected).
    var fundamental: Float = 0
    /// Monotonically increasing frame counter — use this for change detection
    /// rather than comparing magnitudeDB element-by-element.
    var frameIndex: Int = 0

    static let empty = SpectralData()

    static func == (lhs: SpectralData, rhs: SpectralData) -> Bool {
        lhs.frameIndex == rhs.frameIndex
    }
}
