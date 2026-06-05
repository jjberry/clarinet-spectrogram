import AVFoundation
import Combine

final class AudioEngine: ObservableObject {
    private let engine = AVAudioEngine()
    private let ringBuffer = LockFreeRingBuffer(capacity: 16384)

    @Published private(set) var spectralData = SpectralData()

    func start() {
        // TODO: configure input node tap → ringBuffer.write, start engine
        // Real-time thread: never allocate, never lock
    }

    func stop() {
        engine.inputNode.removeTap(onBus: 0)
        engine.stop()
    }
}
