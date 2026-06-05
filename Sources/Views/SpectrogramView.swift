import SwiftUI

// MARK: - Color LUT

// Inferno-inspired colormap: black → deep purple → blue → cyan → yellow → white
// Precomputed at startup as a 256-entry lookup table.
private let colorLUT: [(r: UInt8, g: UInt8, b: UInt8)] = {
    let stops: [(t: Float, r: Float, g: Float, b: Float)] = [
        (0.00,   0,   0,   0),
        (0.15,  20,   0,  60),
        (0.35,  50,   0, 150),
        (0.50,   0,  80, 210),
        (0.65,   0, 200, 210),
        (0.80,  50, 230,  50),
        (0.90, 255, 210,   0),
        (1.00, 255, 255, 255),
    ]
    return (0...255).map { i in
        let t = Float(i) / 255.0
        for s in 1..<stops.count where t <= stops[s].t {
            let lo = stops[s - 1], hi = stops[s]
            let f = (t - lo.t) / (hi.t - lo.t)
            return (r: UInt8(lo.r + f * (hi.r - lo.r)),
                    g: UInt8(lo.g + f * (hi.g - lo.g)),
                    b: UInt8(lo.b + f * (hi.b - lo.b)))
        }
        let last = stops.last!
        return (r: UInt8(last.r), g: UInt8(last.g), b: UInt8(last.b))
    }
}()

// MARK: - SpectrogramBuffer

/// Accumulates FFT columns into a CGContext pixel buffer.
/// Oldest column is always at x=0; newest at x=maxColumns-1.
/// A new column shifts all existing pixels left by one and writes to the right edge.
private final class SpectrogramBuffer: ObservableObject {
    let maxColumns: Int
    let height: Int         // pixel rows in the backing image
    private let cgCtx: CGContext

    init(maxColumns: Int, height: Int) {
        self.maxColumns = maxColumns
        self.height     = height
        let cs = CGColorSpaceCreateDeviceRGB()
        cgCtx = CGContext(data: nil,
                          width: maxColumns,
                          height: height,
                          bitsPerComponent: 8,
                          bytesPerRow: maxColumns * 4,
                          space: cs,
                          bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
        cgCtx.setFillColor(CGColor(red: 0, green: 0, blue: 0, alpha: 1))
        cgCtx.fill(CGRect(x: 0, y: 0, width: maxColumns, height: height))
    }

    /// Push one FFT frame. Called on the main thread only.
    /// `maxBin` limits the displayed frequency range; pass `bins.count - 1` for full range.
    func push(magnitudeDB bins: [Float], dbFloor: Float, dbCeil: Float, maxBin: Int) {
        guard !bins.isEmpty,
              let base = cgCtx.data?.assumingMemoryBound(to: UInt8.self) else { return }

        let invRange  = 1.0 / (dbCeil - dbFloor)
        let topBin    = min(maxBin, bins.count - 1)
        let stride    = maxColumns * 4      // bytes per row in CGContext

        // Shift all rows left by one pixel (discard leftmost column)
        for r in 0..<height {
            let row = base.advanced(by: r * stride)
            memmove(row, row.advanced(by: 4), stride - 4)
        }

        // Write new column at the right edge (maxColumns - 1).
        // CGImage row 0 renders at the top in SwiftUI, so map r=0 → highest bin
        // so that high frequencies appear at top and low frequencies at bottom.
        let lastCol = maxColumns - 1
        for r in 0..<height {
            let binIdx = (height - 1 - r) * topBin / max(1, height - 1)
            let dB     = bins[min(binIdx, topBin)]
            let t      = max(0.0, min(1.0, (dB - dbFloor) * invRange))
            let lut    = colorLUT[Int(t * 255)]
            let i      = (r * maxColumns + lastCol) * 4
            base[i]     = lut.r
            base[i + 1] = lut.g
            base[i + 2] = lut.b
            base[i + 3] = 255
        }

        objectWillChange.send()
    }

    /// Returns a CGImage backed by the context's data. Cheap — no copy until next mutation.
    var image: CGImage? { cgCtx.makeImage() }
}

// MARK: - SpectrogramView

/// Scrolling waterfall spectrogram.
/// Each incoming FFT frame is one pixel column; time advances left to right.
struct SpectrogramView: View {
    @EnvironmentObject var audioEngine: AudioEngine

    private let dbFloor:    Float = -100
    private let dbCeil:     Float = -10
    private let fftSize: Int = 4_096

    @State private var limitHz: Float? = 10_000   // nil = full range

    @StateObject private var buffer = SpectrogramBuffer(maxColumns: 900, height: 512)

    private var maxBin: Int {
        let binCount = audioEngine.spectralData.magnitudeDB.count
        guard let hz = limitHz, hz > 0 else { return binCount - 1 }
        let binHz = audioEngine.sampleRate / Float(fftSize)
        return min(Int(hz / binHz), binCount - 1)
    }

    var body: some View {
        Canvas { context, size in
            if let cgImage = buffer.image {
                context.draw(
                    Image(decorative: cgImage, scale: 1.0),
                    in: CGRect(origin: .zero, size: size)
                )
            }
            drawHarmonicMarkers(in: &context, size: size)
        }
        .onChange(of: audioEngine.spectralData.frameIndex) { _, _ in
            let bins = audioEngine.spectralData.magnitudeDB
            buffer.push(magnitudeDB: bins, dbFloor: dbFloor, dbCeil: dbCeil, maxBin: maxBin)
        }
        .overlay(alignment: .topTrailing) {
            freqRangeToggle
                .padding(8)
        }
        .frame(maxWidth: .infinity)
        .frame(minHeight: 320, maxHeight: .infinity)
        .background(Color.black)
    }

    private func drawHarmonicMarkers(in context: inout GraphicsContext, size: CGSize) {
        let fundamental = audioEngine.spectralData.fundamental
        guard fundamental > 0 else { return }

        let displayMax = limitHz ?? (audioEngine.sampleRate / 2)
        guard displayMax > 0 else { return }

        for n in 1...16 {
            let freq = fundamental * Float(n)
            guard freq <= displayMax else { break }

            // Map frequency to Y: low freq at bottom, high at top
            let yFrac = CGFloat(freq / displayMax)
            let y     = size.height * (1.0 - yFrac)

            var path = Path()
            path.move(to:    CGPoint(x: 0,          y: y))
            path.addLine(to: CGPoint(x: size.width, y: y))

            let color: Color = n == 1 ? .yellow : .white
            context.stroke(path,
                           with: .color(color.opacity(n == 1 ? 0.8 : 0.35)),
                           lineWidth: n == 1 ? 1.5 : 0.75)
        }
    }

    private var freqRangeToggle: some View {
        HStack(spacing: 0) {
            toggleButton(label: "10 kHz", active: limitHz == 10_000) { limitHz = 10_000 }
            toggleButton(label: "Full",   active: limitHz == nil)    { limitHz = nil    }
        }
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.white.opacity(0.3), lineWidth: 1))
    }

    private func toggleButton(label: String, active: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(label)
                .font(.system(size: 26, weight: .medium))
                .foregroundColor(active ? .black : .white.opacity(0.7))
                .padding(.horizontal, 10)
                .padding(.vertical, 5)
                .background(active ? Color.white.opacity(0.85) : Color.white.opacity(0.1))
        }
        .buttonStyle(.plain)
    }
}
