# Clarinet Spectrogram & DSP Tools

## Project Overview

A suite of tools for real-time acoustic analysis and DSP processing of clarinet tone, motivated by:

- Visual feedback for long-tone practice and vocal tract voicing work
- Research into the acoustic signatures of good clarinet tone quality
- Creative DSP processing and modular synthesis integration

The project spans three related deliverables that share a common signal processing core:

1. **macOS spectrogram app** — real-time practice and research tool
1. **JUCE audio plugin** — spectral processing/enhancement as VST3/AU for Bitwig
1. **VCV Rack module** — CV-driven spectral analysis for modular integration

-----

## Scientific Background

### Key acoustic concepts

- Clarinet vocal tract **impedance matching**: skilled players tune a vocal tract impedance maximum ~150 Hz above the played note frequency (Wolfe et al., UNSW)
- **Tongue position / voicing** controls the posterior oral cavity volume, shifting the vocal tract impedance spectrum — this is the primary mechanism behind “open” vs “pinched” tone
- Clarinet’s **cylindrical bore** produces odd-harmonic dominance; even harmonic content is diagnostically meaningful for tone quality
- **Source-filter model** applies: vocal tract resonances shape the spectral envelope of the radiated sound — harmonics aligning with resonances are efficiently radiated

### Key researchers

- **Joe Wolfe / UNSW Music Acoustics group** — vocal tract impedance in wind instruments, clarinet/saxophone, real-time feedback systems. Many papers freely available at phys.unsw.edu.au/jw/
- **Joshua Gardner / ASU** — ultrasound imaging of tongue position during clarinet playing; showed tongue descends monotonically with pitch in upper registers
- **Brad Story / University of Arizona** — vocal tract area functions, resonator modeling
- **Ingo Titze** — source-filter interaction, resonance tuning in singers (directly analogous to clarinet voicing)
- **Arthur Benade** — foundational wind instrument acoustics; *Fundamentals of Musical Acoustics*

### Diagnostically useful spectral features

These are the measurements the research literature identifies as most relevant to tone quality:

|Feature                       |Relevance                                                                       |
|------------------------------|--------------------------------------------------------------------------------|
|**Spectral centroid**         |Tracks brightness/focus; shifts with voicing changes                            |
|**Odd/even harmonic ratio**   |Core timbral dimension of clarinet tone; deviations indicate reed/voicing issues|
|**Harmonic envelope shape**   |Direct acoustic signature of vocal tract resonance alignment                    |
|**Harmonic series regularity**|“Pinched” tone shows uneven or suppressed upper partials                        |

-----

## macOS Spectrogram App

### Architecture

```
Microphone → AVAudioEngine tap → Lock-free ring buffer
                                        ↓
                              C/vDSP FFT module (real-time thread)
                                        ↓
                         Spectral feature extraction
                                        ↓
                              SwiftUI / Metal render
```

### Audio capture

- Use **AVAudioEngine** with an input node tap
- Delivers PCM buffers on a real-time thread — **never block this thread**
- Use a **lock-free ring buffer** to hand data to the FFT/render thread

### FFT / signal processing (C module)

- Use **Apple Accelerate / vDSP** — zero dependencies, SIMD-optimized, ideal for Apple Silicon
- Key functions: `vDSP_fft_zrip` for real FFT, `vDSP_vdbcon` for dB conversion
- Apply **Hann window** before FFT (`vDSP_hann_window`)
- Compute magnitude from complex output, convert to dB scale
- Expose as a clean C API callable from Swift via bridging header

### Rendering

- **Start with SwiftUI Canvas** for rapid iteration
- **Upgrade to Metal** if performance demands it (scrolling texture approach — update rightmost column per frame)
- Two view modes:
  - **Scrolling waterfall** — real-time feedback during playing
  - **Held/averaged spectrum** — steady-state view for long-tone analysis

### UI features

- Reference tone capture — lock a “good tone” spectrum and overlay subsequent attempts
- Derived metrics displayed in real time: spectral centroid, odd/even ratio
- Frequency axis with harmonic markers for current fundamental
- dB scale with adjustable range

### Key parameters to expose

- FFT size (trade-off: frequency resolution vs latency) — start with 4096
- Overlap (50% is standard for spectrograms)
- Averaging window length for held spectrum view
- Color mapping for waterfall (perceptually uniform colormap preferred)

### Threading model

```
Audio thread:  [mic callback] → [ring buffer write]
DSP thread:    [ring buffer read] → [window] → [FFT] → [feature extraction] → [shared state]
Render thread: [read shared state] → [draw]
```

Shared state between DSP and render thread should be protected with a simple double-buffer or atomic swap — avoid locks on the DSP thread.

-----

## JUCE Plugin

### Concept

A spectrally-aware clarinet tone processor that uses knowledge of clarinet acoustics to shape the sound in musically meaningful ways:

- Harmonic envelope shaping (boost/cut specific harmonics relative to the fundamental)
- Odd/even harmonic ratio control
- Spectral centroid targeting
- “Resonance enhance” — identify and reinforce the strongest harmonic cluster

### Platform targets

- VST3 + AU (runs in Bitwig Studio and other DAWs)
- Potentially also a standalone app

### Relevant JUCE classes

- `juce::AudioProcessor` — main plugin class
- `juce::dsp::FFT` — or use vDSP directly for Apple builds
- `juce::AudioProcessorValueTreeState` — parameter management

-----

## VCV Rack Module

### Concept

Expose spectral analysis features as CV outputs, bridging acoustic clarinet performance into the modular world:

- CV out: spectral centroid (normalized 0-10V)
- CV out: odd/even harmonic ratio
- CV out: RMS / amplitude envelope
- CV out: “resonance match” signal — peaks when harmonic alignment is strong
- Audio out: spectrally processed clarinet signal

### Notes

- VCV plugin API is C++; `process()` called per audio block
- CV scaling convention: 0-10V unipolar, ±5V bipolar
- The core DSP code from the macOS app can be ported directly

-----

## Development Phasing

### Phase 1 — macOS spectrogram app

1. AVAudioEngine mic tap → PCM buffers to console
1. C vDSP FFT module with windowing, verify output
1. Basic scrolling spectrogram in SwiftUI Canvas
1. Add held/averaged spectrum view
1. Add derived metrics (centroid, odd/even ratio)
1. Reference tone capture and overlay
1. Polish: color mapping, axis labels, Metal if needed

### Phase 2 — JUCE plugin

1. JUCE project setup, VST3/AU targets
1. Port FFT/feature extraction from Phase 1
1. Harmonic envelope shaping processor
1. UI with spectrum display + parameter controls
1. Test in Bitwig

### Phase 3 — VCV Rack module

1. VCV SDK setup
1. Port DSP core
1. CV output scaling
1. Panel design

-----

## Reference Links

- UNSW Music Acoustics (Wolfe group): <https://www.phys.unsw.edu.au/music/>
- Wolfe publications (many free PDFs): <https://www.phys.unsw.edu.au/jw/pubs.html>
- Gardner / ASU clarinet research: <https://music.asu.edu/content/joshua-gardner>
- VCV Rack plugin SDK: <https://vcvrack.com/manual/PluginDevelopmentTutorial>
- JUCE framework: <https://juce.com>
- Apple vDSP reference: <https://developer.apple.com/documentation/accelerate/vdsp>