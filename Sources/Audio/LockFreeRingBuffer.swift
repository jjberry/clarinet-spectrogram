import Foundation

/// Swift wrapper around the C lock-free SPSC ring buffer.
/// Marked @unchecked Sendable so it can cross actor boundaries safely —
/// thread safety is guaranteed by the C acquire/release atomics.
final class LockFreeRingBuffer: @unchecked Sendable {
    private let buf: OpaquePointer

    init(capacity: Int) {
        precondition(capacity > 0 && capacity & (capacity - 1) == 0,
                     "Capacity must be a power of two")
        guard let b = rb_create(Int32(capacity)) else {
            fatalError("rb_create failed — allocation error")
        }
        buf = b
    }

    deinit { rb_destroy(buf) }

    /// Called on the audio (producer) thread.
    func write(_ samples: UnsafePointer<Float>, count: Int) {
        rb_write(buf, samples, Int32(count))
    }

    /// Called on the DSP (consumer) thread. Returns samples actually read.
    @discardableResult
    func read(into output: UnsafeMutablePointer<Float>, count: Int) -> Int {
        Int(rb_read(buf, output, Int32(count)))
    }

    var availableToRead: Int {
        Int(rb_available_to_read(buf))
    }
}
