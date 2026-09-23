# 🐝 Bumbler XD Clean-Room Homage Preset Collection

This collection provides **15 clean-room, royalty-free homage presets** for the **Bumbler XD** virtual analog synthesizer. Designed by sound designers and audio engineers, these patches capture the distinctive sonic character, raw aggressive filter bite, and organic analog warmth of the legendary EDP Wasp and its legacy digital incarnations—re-engineered from the ground up for Bumbler XD's zero-delay feedback (ZDF) state-variable filter engine and lock-free APVTS architecture.

---

## 📋 Preset Catalog

The 15 presets span 6 production categories:

| # | Preset Name | Category | Primary Filter Mode | Distinctive Features |
|---|---|---|---|---|
| **01** | [`Acid Squelch Bass`](Bass/Acid_Squelch_Bass.xml) | **Bass** | `LP24` (24 dB Lowpass) | High-resonance 303/Wasp squelch, snappy 220ms decay, saturating drive, dynamic velocity accent. |
| **02** | [`Sub Rumble Bass`](Bass/Sub_Rumble_Bass.xml) | **Bass** | `LP12` (12 dB Lowpass) | Sub-octave sine/saw foundation, warm lowpass slope, analog oscillator drift, solid mono center. |
| **03** | [`Reese Grime Bass`](Bass/Reese_Grime_Bass.xml) | **Bass** | `DBL.NT` (Double Notch) | Detuned dual-saw beat frequencies, sweeping comb notches via LFO 1, stereo dual-mode spread. |
| **04** | [`Piercing Sync Lead`](Lead/Piercing_Sync_Lead.xml) | **Lead** | `LP+NT` (12 dB LP + Notch) | Audio-rate FM modulation, mod envelope pitch dive, delayed sine vibrato, screaming drive presence. |
| **05** | [`Screamer Rave Stab`](Lead/Screamer_Rave_Stab.xml) | **Lead** | `BP24` (24 dB Bandpass) | 90s Dutch/UK rave stab, resonant bandpass formant, detuned saw/square, high-gain distortion bite. |
| **06** | [`Formant Vocal Lead`](Lead/Formant_Vocal_Lead.xml) | **Lead** | `LP+NT` (12 dB LP + Notch) | Vowel-like formant cascade, ModEnv modulating pulse width (PWM talking lead), stereo unison chorusing. |
| **07** | [`Celestial Choir Pad`](Pad/Celestial_Choir_Pad.xml) | **Pad** | `DBL.NT` (Double Notch) | 1.2s blooming swell, slow LFO PWM shimmer, gentle mix panning, lush wide dual-mode stereo field. |
| **08** | [`Dark Atmosphere Drone`](Pad/Dark_Atmosphere_Drone.xml) | **Pad** | `HP24` (24 dB Highpass) | Hollow highpass resonance, Sample & Hold random cutoff wander, inverted breathing envelope, vintage table noise. |
| **09** | [`Vintage String Machine`](Pad/Vintage_String_Machine.xml) | **Pad** | `LP24` (24 dB Lowpass) | 1970s analog string ensemble, detuned saws, slow attack swell, ensemble pitch chorusing via LFO 1. |
| **10** | [`Chime Glass Pluck`](Pluck/Chime_Glass_Pluck.xml) | **Pluck** | `BP24` (24 dB Bandpass) | Crystalline bell pluck, ring modulation metallic overtone, 1ms instant transient, pitch sparkle decay. |
| **11** | [`Retro Trance Pluck`](Pluck/Retro_Trance_Pluck.xml) | **Pluck** | `LP24` (24 dB Lowpass) | Eurodance/trance pluck, snappy 24dB filter plunge, sub-osc punch, velocity dynamics, stereo unison. |
| **12** | [`Electro Zap Kick`](Percussion/Electro_Zap_Kick.xml) | **Percussion** | `LP24` (24 dB Lowpass) | Heavy analog electronic kick, 45ms pitch laser plunge, transient FM click, saturated drive clipping. |
| **13** | [`Snappy Noise Snare`](Percussion/Snappy_Noise_Snare.xml) | **Percussion** | `BP24` (24 dB Bandpass) | Tuned sine body + white noise burst, BP24 filter snap, zero sustain, phase-locked transient circuit. |
| **14** | [`SciFi Laser Dive`](FX/SciFi_Laser_Dive.xml) | **FX** | `LP+NT` (12 dB LP + Notch) | Extreme pitch dive, high-depth audio FM screech, resonant filter plunge, aggressive distortion presence. |
| **15** | [`Cosmic Riser Sweep`](FX/Cosmic_Riser_Sweep.xml) | **FX** | `BP24` (24 dB Bandpass) | 3.5s rising filter sweep, ramp LFO stutter modulation, white noise build, stereo dual-mode expansion. |

---

## 🎧 Detailed Sound Design Profiles

### 1. Bass Section

#### `Acid Squelch Bass`
- **Oscillators:** Osc 1 set to Sawtooth (-1 octave) mixed with Osc 2 Square (-1 octave, detuned +3 cents) and a dedicated sub-oscillator square wave.
- **Filter:** 4-pole 24 dB/oct Lowpass (`LP24`) with cutoff tuned to 340 Hz and resonance boosted to 82% (`0.82`). The filter envelope amount is set to `+0.80`, providing the signature acidic snap.
- **Velocity Routing:** Velocity to Filter (`velToFilter = 0.85`) allows accents to violently drive the filter into self-oscillating squelch.
- **Character Circuits:** Distortion Drive enabled at 48% with tone pushed to 65% for analog harmonic saturation.

#### `Sub Rumble Bass`
- **Oscillators:** Pure Sine fundamental on Osc 1 (-2 octaves) with a subtle Sawtooth layer on Osc 2 (-1 octave) at 15% mix and 35% sub-oscillator level.
- **Filter:** 2-pole 12 dB/oct Lowpass (`LP12`) set to 220 Hz with low resonance (`0.25`) for smooth low-end retention.
- **Character Circuits:** Saturation set to 30% with a dark tone (`0.35`) to warm up low frequencies without introducing harsh upper harmonics. Dual Mode is disabled (`0.0`) to guarantee phase-coherent mono sub bass.

#### `Reese Grime Bass`
- **Oscillators:** Dual Sawtooth oscillators centered at -1 octave, aggressively detuned against each other (-10 cents on Osc 1, +10 cents on Osc 2) generating natural phase beating.
- **Filter:** Double Notch (`DBL.NT`, mode 3) at 950 Hz with 68% resonance. LFO 1 slowly modulates the filter cutoff frequency at 0.45 Hz, creating moving comb-filtering notches.
- **Character Circuits:** Dual Mode enabled (`dualMode = 1.0`) for wide stereo spreading; Drive pushed to 55% for aggressive modern bass music grit.

---

### 2. Lead Section

#### `Piercing Sync Lead`
- **Oscillators:** Osc 1 Sawtooth (32') modulating Osc 2 Sawtooth (+1 octave, detuned +8 cents) with 35% FM depth and 15% Ring Modulation.
- **Filter:** Lowpass + Notch cascade (`LP+NT`, mode 2) at 2600 Hz.
- **Modulation:** Modulation Envelope set to a 5ms attack and 250ms decay routed to Osc 2 pitch (`modTarget = 3.0`), creating an aggressive laser attack dive. LFO 1 introduces delayed vibrato (0.3s delay, 5.5 Hz rate).

#### `Screamer Rave Stab`
- **Oscillators:** Punchy Square (35% pulse width) blended with a detuned Saw (+15 cents).
- **Filter:** 24 dB Bandpass (`BP24`, mode 4) centered at 1850 Hz with 72% resonance.
- **Character Circuits:** High-gain distortion (Drive Amount `0.65`, Tone `0.85`) accentuating the screaming 2 kHz–5 kHz presence range characteristic of classic rave anthems.

#### `Formant Vocal Lead`
- **Oscillators:** Dual Square oscillators (Osc 1 at 0 oct, Osc 2 at +1 oct).
- **Filter:** Lowpass + Notch (`LP+NT`, mode 2) at 1400 Hz with high resonance (`0.75`), mimicking human vocal tract resonances.
- **Modulation:** Mod Envelope modulates Pulse Width from 40ms attack to 450ms decay, creating an articulate "talking/vocal" sweep on every note trigger.

---

### 3. Pad & Atmosphere Section

#### `Celestial Choir Pad`
- **Oscillators:** Detuned Square and Saw waves (-6 cents / +6 cents) with Saw sub-oscillator support.
- **Filter:** Double Notch (`DBL.NT`, mode 3) with slow 1.2s attack and 2.5s decay.
- **Modulation:** Dual LFOs operating at non-harmonic rates (LFO 1 at 0.28 Hz sweeping Pulse Width; LFO 2 at 0.18 Hz panning Osc Mix) create non-repeating organic timbral movement.

#### `Dark Atmosphere Drone`
- **Oscillators:** Sub-octave Saw (-2 octaves) mixed with vintage noise table bed, 35% sub-oscillator, and 20% ring modulation.
- **Filter:** 24 dB Highpass (`HP24`, mode 5) with inverted envelope modulation (`filterEnvAmount = -0.25`), creating an eerie "breathing" hollow hollow sound.
- **Modulation:** LFO 1 in Sample & Hold noise mode (`lfo1Waveform = 3.0`) randomly modulates cutoff frequency; infinite sustain (`ampSustain = 1.0`) allows evolving background beds.

#### `Vintage String Machine`
- **Oscillators:** Dual Sawtooth oscillators with symmetrical detuning (-8 cents / +8 cents).
- **Filter:** 24 dB Lowpass (`LP24`) at 3200 Hz with low resonance (`0.30`) and 85% keyboard tracking.
- **Modulation:** LFO 1 set to 5.8 Hz sine wave modulating pitch by 8%, accurately reproducing the BBD ensemble chorus effect of vintage European string synthesizers.

---

### 4. Pluck Section

#### `Chime Glass Pluck`
- **Oscillators:** Fundamental Sine wave on Osc 1 mixed with +2 octave Saw on Osc 2 and 40% Ring Modulation for bright metallic overtones.
- **Filter:** Resonant Bandpass (`BP24`, mode 4) at 2200 Hz with 1ms attack and 260ms decay.
- **Modulation:** Mod Envelope creates a micro pitch transient dive (150ms decay) for an authentic percussive strike.

#### `Retro Trance Pluck`
- **Oscillators:** Saw + Square (+10 cents detuned) with square sub-oscillator.
- **Filter:** 24 dB Lowpass (`LP24`) with base cutoff at 750 Hz and a deep 70% envelope snap (`filterEnvAmount = 0.70`).
- **Dynamics:** Velocity to Filter (`0.75`) and Velocity to Amp (`0.70`) allow dynamic velocity-sensitive arpeggios.

---

### 5. Percussion Section

#### `Electro Zap Kick`
- **Oscillators:** Dual tuned Sine oscillators (-1 octave).
- **Filter & Envelopes:** 1ms amp attack, 180ms decay, zero sustain. Mod Envelope decays in 45ms with 90% modulation depth directly into Osc 2 pitch (`modTarget = 3.0`), creating the classic analog zap punch.
- **Distortion:** Heavy saturation (Drive Amount `0.60`) clips the transient into a solid, mix-cutting thump. Phase-locked analog mode is disabled (`analogMode = 0.0`) for absolute hit-to-hit consistency.

#### `Snappy Noise Snare`
- **Oscillators:** Tuned Sine body (0 octave) blended with 70% White Noise (`wNoiseMode = 1.0`).
- **Filter:** 24 dB Bandpass (`BP24`) centered at 1250 Hz with 1ms attack and 120ms decay.

---

### 6. Special Effects (FX) Section

#### `SciFi Laser Dive`
- **Oscillators:** High Saw (+1 octave) and Square with 55% audio-rate Frequency Modulation (FM) and 35% Ring Modulation.
- **Modulation:** Mod Envelope set to a massive 650ms decay routing 95% modulation into oscillator pitch, producing a classic sci-fi laser plunge.

#### `Cosmic Riser Sweep`
- **Oscillators:** Saw mixed with 60% White Noise and 20% Ring Modulation.
- **Filter:** Rising Bandpass (`BP24`) with a 3.5-second envelope attack and Sawtooth LFO stutter modulation (8.0 Hz rate) for dramatic track build-ups and EDM drops.

---

## 📥 How to Load Presets

### Option 1: Direct DAW State / APVTS Preset Recall
Bumbler XD presets use the JUCE APVTS-compatible XML format (`<BumblerXD ...>`). You can load presets directly from your DAW:

#### FL Studio
1. Open **Bumbler XD** in the Channel Rack.
2. Click the plugin wrapper menu (gear / arrow icon in top left of plugin window).
3. Select **"Load state..."** or **"Presets" -> "Load preset..."**
4. Browse to any `.xml` preset file in `presets/homage/`.

#### Ableton Live
1. Insert **Bumbler XD.vst3** on a MIDI track.
2. In the VST3 device title bar, click the **Folder icon** (Load Preset) or right-click the plugin name and select **"Load Preset (.xml)"**.
3. Select the target preset.

#### REAPER
1. Open the **FX window** containing Bumbler XD.
2. Click the **"+"** button next to the preset dropdown menu in the upper banner.
3. Select **"Import patch/bank..."** or **"Load preset from file..."** and select the `.xml` file.

#### Bitwig Studio / Studio One / Cubase
1. In the plugin wrapper header, locate the preset management icon.
2. Select **"Import VST3 Preset / XML State"**.

---

### Option 2: Installing Presets into System Folders

To have presets appear automatically in host preset browsers, copy the category subfolders into your system user preset directory:

#### Windows
```text
%USERPROFILE%\Documents\Bumbler XD\Presets\
  ├── Bass\
  ├── Lead\
  ├── Pad\
  ├── Pluck\
  ├── Percussion\
  └── FX\
```

Or for VST3 host-managed presets:
```text
%APPDATA%\VST3 Presets\Bumbler Audio\Bumbler XD\
```

#### macOS
```text
~/Library/Audio/Presets/Bumbler Audio/Bumbler XD/
```

#### Linux
```text
~/.vst3/presets/Bumbler Audio/Bumbler XD/
```

---

## ⚖️ Legal & Clean-Room Architecture

All 15 presets in this directory are **100% clean-room, royalty-free acoustic homages**:
- **Zero Binary Code:** No copyrighted binary chunks, proprietary sample tables, or decompiled code from third-party plugins were used.
- **Independent Synthesis:** All parameter configurations were authored from first principles using standard subtractive analog synthesis techniques (saw/square waves, zero-delay feedback state variable filters, linear/exponential envelopes).
- **Royalty-Free License:** These presets may be used freely in any commercial or non-commercial musical work, sound library, or soundtrack without attribution or licensing fees.
