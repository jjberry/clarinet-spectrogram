import SwiftUI

/// Scrolling waterfall spectrogram — one column per FFT frame, time scrolls left to right.
struct SpectrogramView: View {
    @EnvironmentObject var audioEngine: AudioEngine

    var body: some View {
        Canvas { _, _ in
            // TODO: maintain a circular texture of columns; draw each frame as a vertical
            // strip with color mapped from dB magnitude using a perceptually uniform colormap.
            // Switch to Metal (MTKView) if SwiftUI Canvas throughput proves insufficient.
        }
        .frame(maxWidth: .infinity)
        .frame(height: 320)
        .background(Color.black)
    }
}
