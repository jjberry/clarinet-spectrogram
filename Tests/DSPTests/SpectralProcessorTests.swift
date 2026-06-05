import XCTest

final class SpectralProcessorTests: XCTestCase {

    // Test constants — 4096-point FFT at 44100 Hz is the intended production config
    static let fftSize: Int32  = 4096
    static let sampleRate: Float = 44100
    static let binHz: Float = sampleRate / Float(fftSize)   // ≈ 10.77 Hz / bin

    private var ctx: OpaquePointer!

    override func setUp() {
        super.setUp()
        ctx = spectral_processor_create(Self.fftSize)
        XCTAssertNotNil(ctx, "Failed to create SpectralProcessorContext")
    }

    override func tearDown() {
        spectral_processor_destroy(ctx)
        ctx = nil
        super.tearDown()
    }

    // MARK: - Helpers

    private func sine(frequency: Float, amplitude: Float = 1) -> [Float] {
        (0..<Int(Self.fftSize)).map {
            amplitude * sin(2 * .pi * frequency * Float($0) / Self.sampleRate)
        }
    }

    private func process(_ samples: [Float]) -> [Float] {
        var result = [Float](repeating: 0, count: Int(spectral_processor_bin_count(ctx)))
        samples.withUnsafeBufferPointer { s in
            result.withUnsafeMutableBufferPointer { r in
                spectral_processor_process(ctx, s.baseAddress, r.baseAddress)
            }
        }
        return result
    }

    private func centroid(of bins: [Float]) -> Float {
        bins.withUnsafeBufferPointer {
            spectral_processor_centroid($0.baseAddress, Int32($0.count), Self.binHz)
        }
    }

    private func oddEvenRatio(of bins: [Float], fundamental: Float) -> Float {
        bins.withUnsafeBufferPointer {
            spectral_processor_odd_even_ratio($0.baseAddress, Int32($0.count),
                                              fundamental, Self.sampleRate)
        }
    }

    private func peakBin(in bins: [Float]) -> Int {
        bins.indices.max(by: { bins[$0] < bins[$1] })!
    }

    // MARK: - Create / Destroy

    func testCreateAcceptsPowersOfTwo() {
        for size in [256, 512, 1024, 2048, 4096, 8192] {
            let c = spectral_processor_create(Int32(size))
            XCTAssertNotNil(c, "Expected success for size \(size)")
            spectral_processor_destroy(c)
        }
    }

    func testCreateRejectsInvalidSizes() {
        for size: Int32 in [0, -1, 100, 1000, 3000] {
            XCTAssertNil(spectral_processor_create(size), "Expected nil for size \(size)")
        }
    }

    func testBinCountIsFftSizeOverTwoPlusOne() {
        XCTAssertEqual(spectral_processor_bin_count(ctx), Self.fftSize / 2 + 1)
    }

    // MARK: - Process: output shape and scaling

    func testSilenceProducesNoiseFloor() {
        let bins = process([Float](repeating: 0, count: Int(Self.fftSize)))
        // Noise floor = 20 * log10(1e-7) = -140 dB; allow ±1 dB for float rounding
        XCTAssertTrue(bins.allSatisfy { $0 <= -139 },
                      "All bins should be at noise floor (~-140 dB) for silence")
    }

    func testPureTonePeakIsAtExpectedBin() {
        let freq: Float = 1000
        let bins = process(sine(frequency: freq))
        let expected = Int(freq / Self.binHz)
        XCTAssertLessThanOrEqual(abs(peakBin(in: bins) - expected), 1,
            "Peak bin \(peakBin(in: bins)) should be within ±1 of expected bin \(expected)")
    }

    func testHigherFrequencyYieldsHigherPeakBin() {
        let lo = peakBin(in: process(sine(frequency: 440)))
        let hi = peakBin(in: process(sine(frequency: 880)))
        XCTAssertLessThan(lo, hi)
    }

    func testPureToneIsAtLeast40dBAboveNoiseFloor() {
        let bins = process(sine(frequency: 1000))
        let headroom = bins.max()! - (-140)
        XCTAssertGreaterThan(headroom, 40,
            "Unit-amplitude tone should be well above noise floor")
    }

    // MARK: - Spectral centroid

    func testCentroidOfPureToneIsNearToneFrequency() {
        let freq: Float = 1000
        XCTAssertEqual(centroid(of: process(sine(frequency: freq))), freq, accuracy: 100,
                       "Centroid of a 1 kHz tone should be within 100 Hz")
    }

    func testCentroidIncreasesMonotonicallyWithFrequency() {
        let c440  = centroid(of: process(sine(frequency: 440)))
        let c1000 = centroid(of: process(sine(frequency: 1000)))
        let c2000 = centroid(of: process(sine(frequency: 2000)))
        XCTAssertLessThan(c440, c1000)
        XCTAssertLessThan(c1000, c2000)
    }

    // MARK: - Odd/even harmonic ratio

    /// Clarinet's cylindrical bore produces strongly odd-dominant spectra; ratio >> 1.
    func testOddOnlySignalHasHighRatio() {
        let fundamental: Float = 200
        var signal = [Float](repeating: 0, count: Int(Self.fftSize))
        var n = 1
        while Float(n) * fundamental < Self.sampleRate / 2 {
            for i in signal.indices {
                signal[i] += sin(2 * .pi * Float(n) * fundamental * Float(i) / Self.sampleRate)
            }
            n += 2
        }
        XCTAssertGreaterThan(oddEvenRatio(of: process(signal), fundamental: fundamental), 5)
    }

    func testEvenOnlySignalHasLowRatio() {
        let fundamental: Float = 200
        var signal = [Float](repeating: 0, count: Int(Self.fftSize))
        var n = 2
        while Float(n) * fundamental < Self.sampleRate / 2 {
            for i in signal.indices {
                signal[i] += sin(2 * .pi * Float(n) * fundamental * Float(i) / Self.sampleRate)
            }
            n += 2
        }
        XCTAssertLessThan(oddEvenRatio(of: process(signal), fundamental: fundamental), 0.5)
    }

    /// Simulated clarinet spectrum: odd harmonics at 1/n amplitude, even at 0.3/n.
    /// Ratio should be > 1 — odd harmonics dominate.
    func testClarinetLikeSpectrumHasOddDominance() {
        let fundamental: Float = 233  // B♭3
        var signal = [Float](repeating: 0, count: Int(Self.fftSize))
        for n in 1...10 {
            let freq = Float(n) * fundamental
            guard freq < Self.sampleRate / 2 else { break }
            let amplitude: Float = n % 2 == 1 ? 1.0 / Float(n) : 0.3 / Float(n)
            for i in signal.indices {
                signal[i] += amplitude * sin(2 * .pi * freq * Float(i) / Self.sampleRate)
            }
        }
        XCTAssertGreaterThan(oddEvenRatio(of: process(signal), fundamental: fundamental), 1)
    }
}
