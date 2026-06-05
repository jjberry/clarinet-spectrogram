import SwiftUI

struct ContentView: View {
    @EnvironmentObject var audioEngine: AudioEngine

    var body: some View {
        VStack(spacing: 0) {
            SpectrogramView()
            Divider()
            SpectrumView()
        }
        .frame(minWidth: 900, minHeight: 700)
        .background(Color.black)
        .onAppear { audioEngine.start() }
        .onDisappear { audioEngine.stop() }
    }
}
