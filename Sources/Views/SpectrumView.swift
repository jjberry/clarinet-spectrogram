import SwiftUI

/// Held / averaged spectrum for steady-state long-tone analysis.
/// Optionally overlays a reference "good tone" capture.
struct SpectrumView: View {
    @EnvironmentObject var audioEngine: AudioEngine

    var body: some View {
        Canvas { _, _ in
            // TODO:
            //   - Draw current spectrum as filled curve
            //   - Overlay reference spectrum if captured
            //   - Mark harmonic positions for detected fundamental
            //   - Draw frequency axis with Hz labels and dB scale
        }
        .frame(maxWidth: .infinity)
        .frame(height: 220)
        .background(Color.black)
    }
}
