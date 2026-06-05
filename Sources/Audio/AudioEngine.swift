import AVFoundation

/// Sendable wrapper around the C FFT context so the DSP task can own it
/// without crossing MainActor isolation.
private final class DSPContext: @unchecked Sendable {
    let ptr: OpaquePointer
    let fftSize: Int
    let hopSize: Int
    let binCount: Int

    init(fftSize: Int) {
        guard let p = spectral_processor_create(Int32(fftSize)) else {
            fatalError("spectral_processor_create failed")
        }
        ptr      = p
        self.fftSize  = fftSize
        hopSize  = fftSize / 2
        binCount = Int(spectral_processor_bin_count(p))
    }

    deinit { spectral_processor_destroy(ptr) }
}

/// Drives the full audio pipeline: mic tap → ring buffer → DSP task → published SpectralData.
@MainActor
final class AudioEngine: ObservableObject {

    @Published private(set) var spectralData = SpectralData()
    @Published private(set) var sampleRate: Float = 44_100

    private let avEngine   = AVAudioEngine()
    private let ringBuffer = LockFreeRingBuffer(capacity: 16384)
    private let dsp        = DSPContext(fftSize: 4096)
    private var dspTask: Task<Void, Never>?

    func start() {
        AVCaptureDevice.requestAccess(for: .audio) { [weak self] granted in
            guard granted else {
                print("[AudioEngine] Microphone access denied")
                return
            }
            Task { @MainActor [weak self] in self?.beginCapture() }
        }
    }

    func stop() {
        avEngine.inputNode.removeTap(onBus: 0)
        avEngine.stop()
        dspTask?.cancel()
        dspTask = nil
    }

    // MARK: - Private

    private func beginCapture() {
        let inputNode = avEngine.inputNode
        let format    = inputNode.outputFormat(forBus: 0)
        sampleRate    = Float(format.sampleRate)

        inputNode.installTap(onBus: 0, bufferSize: 1024, format: format,
                             block: Self.makeTapBlock(buf: ringBuffer))

        do {
            try avEngine.start()
        } catch {
            print("[AudioEngine] Failed to start: \(error)")
            return
        }

        launchDSPTask(buffer: ringBuffer, context: dsp, sampleRate: sampleRate)
    }

    // Defined nonisolated so the returned closure has no actor isolation —
    // AVFAudio calls it on a real-time thread, not the main actor.
    private nonisolated static func makeTapBlock(buf: LockFreeRingBuffer) -> AVAudioNodeTapBlock {
        { buffer, _ in
            guard let data = buffer.floatChannelData?[0] else { return }
            buf.write(data, count: Int(buffer.frameLength))
        }
    }

    private func launchDSPTask(buffer: LockFreeRingBuffer,
                               context: DSPContext,
                               sampleRate: Float) {
        dspTask = Task.detached(priority: .high) { [weak self] in
            var prevHalf    = [Float](repeating: 0, count: context.hopSize)
            var window      = [Float](repeating: 0, count: context.fftSize)
            var magnitudeDB = [Float](repeating: 0, count: context.binCount)
            let binHz       = sampleRate / Float(context.fftSize)
            var frameIndex  = 0

            while !Task.isCancelled {
                // Block until a full hop of new samples is available
                guard buffer.availableToRead >= context.hopSize else {
                    try? await Task.sleep(nanoseconds: 1_000_000)  // 1 ms
                    continue
                }

                // Read new hop into a local buffer
                var newHalf = [Float](repeating: 0, count: context.hopSize)
                _ = newHalf.withUnsafeMutableBufferPointer { ptr in
                    buffer.read(into: ptr.baseAddress!, count: context.hopSize)
                }

                // Assemble 50%-overlapping window: [previous hop | new hop]
                window.replaceSubrange(0..<context.hopSize, with: prevHalf)
                window.replaceSubrange(context.hopSize..<context.fftSize, with: newHalf)
                prevHalf = newHalf

                // FFT → dB magnitude spectrum
                window.withUnsafeBufferPointer { wPtr in
                    magnitudeDB.withUnsafeMutableBufferPointer { mPtr in
                        spectral_processor_process(context.ptr,
                                                   wPtr.baseAddress,
                                                   mPtr.baseAddress)
                    }
                }

                // Feature extraction
                let centroidHz = magnitudeDB.withUnsafeBufferPointer {
                    spectral_processor_centroid($0.baseAddress,
                                                Int32($0.count),
                                                binHz)
                }

                let fundamentalHz = magnitudeDB.withUnsafeBufferPointer {
                    spectral_processor_hps($0.baseAddress,
                                           Int32($0.count),
                                           binHz,
                                           5)
                }

                let oddEvenRatio = magnitudeDB.withUnsafeBufferPointer {
                    spectral_processor_odd_even_ratio($0.baseAddress,
                                                      Int32($0.count),
                                                      fundamentalHz,
                                                      sampleRate)
                }

                let data = SpectralData(magnitudeDB: magnitudeDB,
                                        centroid: centroidHz,
                                        oddEvenRatio: oddEvenRatio,
                                        fundamental: fundamentalHz,
                                        frameIndex: frameIndex)
                frameIndex += 1

                await MainActor.run { [weak self] in
                    self?.spectralData = data
                }
            }
        }
    }
}
