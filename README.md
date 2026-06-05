# ClariScope

Real-time spectrogram and acoustic analysis tools for clarinet tone research and practice. Motivated by vocal tract voicing work and the acoustic research of Joe Wolfe's group at UNSW.

## Features

- **Scrolling waterfall spectrogram** — real-time visual feedback during playing
- **Averaged spectrum view** — steady-state analysis for long-tone practice
- **HPS pitch detection** — Harmonic Product Spectrum fundamental estimator
- **Harmonic overlays** — fundamental and partials marked on both views
- **Spectral centroid** and **odd/even harmonic ratio** metrics displayed live
- **Reference tone capture** — lock a "good tone" spectrum and overlay subsequent attempts
- **Frequency range toggle** — zoom to 10 kHz or show full range

## Background

Clarinet's cylindrical bore produces odd-harmonic dominance. The odd/even harmonic ratio is a direct diagnostic of tone quality, and skilled players tune a vocal tract impedance maximum ~150 Hz above the played note (Wolfe et al., UNSW). ClariScope makes these features visible in real time.

Key references:
- Wolfe group (UNSW Music Acoustics): https://www.phys.unsw.edu.au/music/
- Gardner / ASU — ultrasound imaging of tongue position in clarinet playing

## Architecture

```
Microphone → AVAudioEngine tap → Lock-free ring buffer
                                         ↓
                               C/vDSP FFT module (real-time thread)
                                         ↓
                          HPS pitch · centroid · odd/even ratio
                                         ↓
                               SwiftUI / Canvas render
```

The DSP core (`Sources/DSP/`) is plain C using Apple Accelerate/vDSP — portable to the planned JUCE plugin and VCV Rack module.

## Requirements

- macOS 14+
- Xcode 15+
- [XcodeGen](https://github.com/yonaskolb/XcodeGen): `brew install xcodegen`

## Building

```sh
xcodegen generate
open ClariScope.xcodeproj
```

Then press **Cmd+R** in Xcode. Grant microphone access when prompted.

## Roadmap

- [x] Phase 1 — macOS spectrogram app
- [ ] Phase 2 — JUCE VST3/AU plugin for Bitwig
- [ ] Phase 3 — VCV Rack module with CV outputs

## License

MIT — see [LICENSE](LICENSE).
