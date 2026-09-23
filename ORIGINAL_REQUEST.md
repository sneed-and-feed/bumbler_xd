# Original User Request

## Initial Request — 2026-09-23T04:09:31Z

Build "Bumbler XD", a faithful audio synthesizer homage to the classic Wasp XT plugin, replicating its vintage digital/analog-modeled sound engine, signature multi-topology filters, modulation matrix, and raw analog-digital hybrid output character in a modern C++ / JUCE 8 VST3 and Standalone audio plugin.

Working directory: c:/Users/x/Documents/antigravity/bumbler_xd
Integrity mode: development

## Reference Material & Assets
- Parameter and architecture specifications matching the classic Wasp XT synth (Oscillators, 6-mode Filter, Dual LFOs, Amp/Filter/Mod Envelopes, Output Overdrive/Dual/Analog/W.Noise circuits).
- Reference UI layout: Skeuomorphic industrial rack panel with vintage knobs, LED mode buttons, horizontal mix slider, and dual green LCD ADSR displays.
- Codebase reference & inspiration: Inspect DSP engines, CMake build structures, and verification suites in `c:/Users/x/Documents/antigravity/braun_as-42` and `c:/Users/x/Documents/antigravity/braun_rb-26`.

## Requirements

### R1. Sound Generation & Oscillator Engine
Implement a polyphonic sound engine featuring three oscillators:
- OSC 1 & OSC 2: Coarse tuning (-3 to +3 octaves), fine tuning (-1 to +1 semitone), and selectable waveforms: Sawtooth, Square/Pulse, Sine, and Noise/Wavetable.
- OSC Mix: Continuous balance slider blending between OSC 1 and OSC 2.
- OSC 3: Auxiliary oscillator with selectable Square or Sawtooth waveform, and dedicated volume amount control (defaults to 0 / inaudible).
- Inter-oscillator modulation:
  - Ring Modulator: Modulates OSC 1 and OSC 2 to produce metallic/inharmonic sidebands.
  - Pulse Width (PW): Adjusts duty cycle when square waveform is selected on OSC 1 or OSC 2.
  - Frequency Modulation (FM): Modulates the frequency of OSC 2 using the output of OSC 1.
- Polyphony and Voice Management: Clean voice allocation, note-on/note-off tracking, and velocity sensitivity routing to Amplitude and Filter envelopes.

### R2. 6-Mode Filter Section
Implement the complete Wasp XT filter topology with cutoff frequency and resonance controls:
- 12 dB Lowpass Filter (LP)
- 24 dB Lowpass Filter (LP FAT)
- 12 dB Lowpass + Notch Cascade (LP+NT)
- Double Notch Filter (DBL.NT)
- 24 dB Bandpass Filter (BP)
- 24 dB Highpass Filter (HP)
- Filter modulation inputs: Keyboard tracking (KB.TRK) scaling cutoff with played note pitch, and bipolar Envelope Amount (ENV) scaling cutoff with the Filter ADSR.

### R3. Dual LFOs & Modulation Routing
Implement two independent multi-waveform LFOs with tempo sync and reset triggers:
- LFO 1: Sawtooth, square, sine, and noise waveforms; routable to OSC 1+2 pitch, Filter cutoff, or Pulse Width; featuring Delay, Tempo Sync, Key-trigger Phase Reset, Amount, and Rate controls.
- LFO 2: Sawtooth, square, sine, and noise waveforms; routable to OSC 1 pitch, OSC Mix ratio, or Master Amplitude; featuring Delay, Tempo Sync, Key-trigger Phase Reset, Amount, and Rate controls.
- MOD ENV: Dedicated attack-decay envelope with bipolar modulation amount, routable to PW, LFO 1 Amount, OSC 1 Level, or OSC 2 Pitch.
- Dual ADSR Envelopes: Independent 4-stage envelopes for Amplitude and Filter, including a Link toggle button to synchronize edits between both envelopes.

### R4. Vintage Output Coloring & Character Circuits
Implement the final output processing section:
- Distortion unit with on/off drive toggle, tone color shaping (muddy/dark to bright/fizzy), and drive amount.
- Dual Mode: Voice-doubling circuit with slight detuning to create thick unison/stereo width.
- Analog Mode: Emulates vintage hardware instability via subtle continuous pitch drift and free-running, non-synchronized oscillator phase upon note triggers.
- W.Noise Mode: Switchable oscillator noise generator between vintage fixed-table pseudo-random noise and true white noise.
- Master output volume control.

### R5. User Interface & DAW Parameter Management
- Full parameter management using JUCE `AudioProcessorValueTreeState` (APVTS) for sample-accurate automation, state saving/restoring, and preset compatibility.
- Intuitive, skeuomorphic user interface styled after the original Wasp XT hardware aesthetic (dark slate/blue chassis, vintage brushed-metal pointer knobs, green LCD ADSR graphs, horizontal mix fader, and red LED buttons).

## Verification Plan & Acceptance Criteria

### Verification Resources
- DSP unit test runner in `tests/` leveraging CMake/CTest.
- Automated offline audio rendering test script to validate frequency response curves and modulation characteristics.

### Acceptance Criteria

#### DSP Engine & Audio Verification
- [ ] C++ DSP test suite builds and passes headless unit tests under CTest with 0 test failures.
- [ ] Filter frequency response verification:
  - LP FAT demonstrates ~24 dB/oct roll-off above cutoff.
  - LP demonstrates ~12 dB/oct roll-off.
  - BP exhibits attenuation on both low and high ends around center frequency.
  - HP demonstrates ~24 dB/oct low-end roll-off below cutoff.
  - DBL.NT exhibits two distinct attenuation nulls.
- [ ] Inter-oscillator modulation verification:
  - Ring modulation produces expected sum and difference frequencies without DC offset runaway.
  - FM produces expected Bessel-distribution sideband harmonics when OSC 1 modulates OSC 2.
- [ ] Character circuits verification:
  - Enabling Dual mode measurably widens the stereo field and creates chorus beating between voice pairs.
  - Enabling Analog mode produces pitch drift variance across repeated identical MIDI note triggers.
  - Output Drive produces non-linear harmonic saturation and tone shaping responds to the Tone control.
- [ ] Real-time safety: Audio callback executes with no denormal numbers, no memory allocations, no lock contention, and no audio dropouts or clicks.

#### Build & Plugin Verification
- [ ] CMake project successfully configures and compiles both VST3 and Standalone targets on MSVC C++20 using JUCE 8.
- [ ] DAW state recall: Plugin correctly serializes and restores all parameters through APVTS XML/binary state.
- [ ] UI controls are bidirectionally connected to APVTS parameters without parameter feedback loops.

## Follow-up — 2026-09-23T05:20:13Z

The GitHub repository for this project is available at https://github.com/sneed-and-feed/bumbler_xd . Please configure git remote origin to this repository if appropriate, and keep it in mind for repository structure, CI/CD, and release packaging.

## Follow-up — 2026-09-23T11:00:01Z

Direct instruction from user: Do not micropoll. Ensure all subagents, crons, and monitoring loops avoid rapid status polling, tight loops, or high-frequency polling. Rely on reactive event notifications, tool completion wakeups, and wider monitoring intervals.


