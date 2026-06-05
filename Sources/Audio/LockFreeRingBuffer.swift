import Foundation

/// Single-producer single-consumer lock-free ring buffer for audio samples.
/// Capacity must be a power of two.
final class LockFreeRingBuffer {
    private let capacity: Int
    private let mask: Int
    private var buffer: [Float]
    // writeIndex owned by audio thread; readIndex owned by DSP thread
    private var writeIndex: Int = 0
    private var readIndex: Int = 0

    init(capacity: Int) {
        precondition(capacity > 0 && capacity & (capacity - 1) == 0,
                     "Capacity must be a power of two")
        self.capacity = capacity
        self.mask = capacity - 1
        self.buffer = [Float](repeating: 0, count: capacity)
    }

    /// Called on the audio (producer) thread.
    func write(_ samples: UnsafePointer<Float>, count: Int) {
        // TODO: implement with atomic store/load for head/tail indices
    }

    /// Called on the DSP (consumer) thread. Returns number of samples actually read.
    @discardableResult
    func read(into output: UnsafeMutablePointer<Float>, count: Int) -> Int {
        // TODO: implement with atomic store/load for head/tail indices
        return 0
    }

    var availableToRead: Int {
        (writeIndex - readIndex) & mask
    }
}
