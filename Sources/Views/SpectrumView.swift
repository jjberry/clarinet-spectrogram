import SwiftUI

/// Held / averaged spectrum for steady-state long-tone analysis.
/// EMA-smoothed current spectrum (green, filled) with optional reference overlay (blue, dashed).
struct SpectrumView: View {
    @EnvironmentObject var audioEngine: AudioEngine

    // dB display range
    private let dbFloor: Float = -100
    private let dbCeil:  Float = -10
    // Frequency display range
    private let maxHz:   Float = 10_000
    // Must match AudioEngine's DSPContext
    private let sampleRate: Float = 44_100
    private let fftSize:    Int   = 4_096
    // EMA weight for each new frame — lower → slower convergence
    private let alpha: Float = 0.08

    @State private var averaged:  [Float]   = []
    @State private var reference: [Float]?  = nil

    var body: some View {
        VStack(spacing: 0) {
            Canvas { context, size in
                draw(in: &context, size: size)
            }
            .onChange(of: audioEngine.spectralData.frameIndex) { _, _ in
                smooth(audioEngine.spectralData.magnitudeDB)
            }

            controlBar
                .frame(height: 28)
                .background(Color(white: 0.07))
        }
        .frame(maxWidth: .infinity)
        .frame(height: 220)
        .background(Color.black)
    }

    // MARK: - Coordinate helpers

    private func plotRect(for size: CGSize) -> CGRect {
        CGRect(x: 36, y: 6, width: size.width - 44, height: size.height - 26)
    }

    private func xFor(_ hz: Float, in r: CGRect) -> CGFloat {
        r.minX + CGFloat(hz / maxHz) * r.width
    }

    private func yFor(_ dB: Float, in r: CGRect) -> CGFloat {
        let t = CGFloat((dB - dbFloor) / (dbCeil - dbFloor))
        return r.maxY - t * r.height
    }

    // MARK: - Top-level draw

    private func draw(in context: inout GraphicsContext, size: CGSize) {
        let sd   = audioEngine.spectralData
        let plot = plotRect(for: size)

        drawGrid(in: &context, plot: plot)
        drawAxisLabels(in: &context, plot: plot)

        if let ref = reference {
            drawCurve(in: &context, bins: ref, plot: plot,
                      color: Color(red: 0.4, green: 0.6, blue: 1.0),
                      filled: false, dashed: true)
        }

        if !averaged.isEmpty {
            drawCurve(in: &context, bins: averaged, plot: plot,
                      color: Color(red: 0.2, green: 0.9, blue: 0.5),
                      filled: true, dashed: false)
        }

        if sd.fundamental > 0 {
            drawHarmonicMarkers(in: &context, fundamental: sd.fundamental, plot: plot)
        }

        if sd.centroid > 0 {
            drawCentroidMarker(in: &context, hz: sd.centroid, plot: plot)
        }

        drawMetrics(in: &context, sd: sd, size: size)
    }

    // MARK: - Grid and axes

    private func drawGrid(in context: inout GraphicsContext, plot: CGRect) {
        let dbTicks: [Float] = [-100, -80, -60, -40, -20]
        for db in dbTicks {
            let y = yFor(db, in: plot)
            var p = Path()
            p.move(to: CGPoint(x: plot.minX, y: y))
            p.addLine(to: CGPoint(x: plot.maxX, y: y))
            context.stroke(p, with: .color(Color(white: 0.14)))
        }
        var border = Path()
        border.addRect(plot)
        context.stroke(border, with: .color(Color(white: 0.28)), lineWidth: 0.5)
    }

    private func drawAxisLabels(in context: inout GraphicsContext, plot: CGRect) {
        let font  = Font.system(size: 9)
        let color = Color(white: 0.5)

        // dB axis (left)
        for (db, label) in [(-100, "-100"), (-80, "-80"), (-60, "-60"), (-40, "-40"), (-20, "-20")] as [(Float, String)] {
            context.draw(
                Text(label).font(font).foregroundColor(color),
                at: CGPoint(x: plot.minX - 4, y: yFor(db, in: plot)),
                anchor: .trailing
            )
        }

        // Hz axis (bottom)
        let hzLabels: [(Float, String)] = [
            (500, "500"), (1000, "1k"), (2000, "2k"), (3000, "3k"),
            (5000, "5k"), (7000, "7k"), (10000, "10k"),
        ]
        for (hz, label) in hzLabels {
            guard hz <= maxHz else { continue }
            let x = xFor(hz, in: plot)
            var tick = Path()
            tick.move(to: CGPoint(x: x, y: plot.maxY))
            tick.addLine(to: CGPoint(x: x, y: plot.maxY + 3))
            context.stroke(tick, with: .color(color))
            context.draw(
                Text(label).font(font).foregroundColor(color),
                at: CGPoint(x: x, y: plot.maxY + 4),
                anchor: .top
            )
        }
    }

    // MARK: - Spectrum curve

    private func drawCurve(in context: inout GraphicsContext,
                            bins: [Float], plot: CGRect,
                            color: Color, filled: Bool, dashed: Bool) {
        let binHz  = sampleRate / Float(fftSize)
        let maxBin = min(bins.count - 1, Int(maxHz / binHz))
        guard maxBin > 0 else { return }

        var outline = Path()
        var firstX = CGFloat(0), lastX = CGFloat(0)

        for bin in 0...maxBin {
            let x = xFor(Float(bin) * binHz, in: plot)
            let y = max(plot.minY, min(plot.maxY, yFor(bins[bin], in: plot)))
            if bin == 0 { outline.move(to: CGPoint(x: x, y: y)); firstX = x }
            else        { outline.addLine(to: CGPoint(x: x, y: y)) }
            lastX = x
        }

        if filled {
            var area = outline
            area.addLine(to: CGPoint(x: lastX,  y: plot.maxY))
            area.addLine(to: CGPoint(x: firstX, y: plot.maxY))
            area.closeSubpath()

            context.fill(area, with: .linearGradient(
                Gradient(stops: [
                    .init(color: color.opacity(0.45), location: 0),
                    .init(color: color.opacity(0.00), location: 1),
                ]),
                startPoint: CGPoint(x: 0, y: plot.minY),
                endPoint:   CGPoint(x: 0, y: plot.maxY)
            ))
        }

        context.stroke(outline,
                       with: .color(color),
                       style: StrokeStyle(lineWidth: 1.5,
                                          dash: dashed ? [5, 3] : []))
    }

    // MARK: - Markers

    /// Thin vertical line at each harmonic; odd harmonics orange, even purple.
    private func drawHarmonicMarkers(in context: inout GraphicsContext,
                                      fundamental: Float, plot: CGRect) {
        var n = 1
        while Float(n) * fundamental <= maxHz {
            let x     = xFor(Float(n) * fundamental, in: plot)
            let color = n.isMultiple(of: 2)
                ? Color(red: 0.8, green: 0.3, blue: 1.0, opacity: 0.45)
                : Color(red: 1.0, green: 0.5, blue: 0.2, opacity: 0.55)
            var line = Path()
            line.move(to: CGPoint(x: x, y: plot.minY))
            line.addLine(to: CGPoint(x: x, y: plot.maxY))
            context.stroke(line, with: .color(color), lineWidth: 1)
            n += 1
        }
    }

    /// Dashed yellow line at the spectral centroid frequency.
    private func drawCentroidMarker(in context: inout GraphicsContext,
                                     hz: Float, plot: CGRect) {
        guard hz > 0, hz <= maxHz else { return }
        let x = xFor(hz, in: plot)
        var line = Path()
        line.move(to: CGPoint(x: x, y: plot.minY))
        line.addLine(to: CGPoint(x: x, y: plot.maxY))
        context.stroke(line, with: .color(.yellow.opacity(0.65)),
                        style: StrokeStyle(lineWidth: 1, dash: [3, 3]))
    }

    // MARK: - Metrics overlay (top-right corner)

    private func drawMetrics(in context: inout GraphicsContext,
                              sd: SpectralData, size: CGSize) {
        let font = Font.system(size: 10).monospacedDigit()
        var y: CGFloat = 10

        if sd.centroid > 0 {
            context.draw(
                Text("C \(Int(sd.centroid)) Hz").font(font).foregroundColor(.yellow),
                at: CGPoint(x: size.width - 8, y: y), anchor: .topTrailing
            )
            y += 14
        }
        if sd.oddEvenRatio > 0 {
            context.draw(
                Text(String(format: "O/E %.2f", sd.oddEvenRatio))
                    .font(font)
                    .foregroundColor(Color(red: 0.8, green: 0.5, blue: 1.0)),
                at: CGPoint(x: size.width - 8, y: y), anchor: .topTrailing
            )
        }
    }

    // MARK: - State

    private func smooth(_ newBins: [Float]) {
        guard !newBins.isEmpty else { return }
        if averaged.count != newBins.count { averaged = newBins; return }
        let beta = 1.0 - alpha
        for i in averaged.indices {
            averaged[i] = alpha * newBins[i] + beta * averaged[i]
        }
    }

    private func captureReference() {
        guard !averaged.isEmpty else { return }
        reference = averaged
    }

    // MARK: - Control bar

    private var controlBar: some View {
        HStack(spacing: 16) {
            Button(action: captureReference) {
                Label("Capture Reference", systemImage: "pin.fill")
                    .font(.caption)
            }
            .buttonStyle(.plain)
            .foregroundColor(.cyan)
            .disabled(averaged.isEmpty)

            if reference != nil {
                Button("Clear", action: { reference = nil })
                    .font(.caption)
                    .buttonStyle(.plain)
                    .foregroundColor(Color.orange.opacity(0.8))
            }

            Spacer()
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 4)
    }
}
