#include "PresetMigrator.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace bumbler {

// ============================================================================
// Parameter Orders & ID Definitions
// ============================================================================
static const char* const kWaspXtParams[55] = {
    // Oscillators (14)
    "osc1Waveform", "osc1Octave", "osc1Fine",
    "osc2Waveform", "osc2Octave", "osc2Fine",
    "oscMix", "osc3Waveform", "osc3Level",
    "ringModMix", "pulseWidth", "fmAmount",
    "velToAmp", "velToFilter",
    // Filter (5)
    "filterMode", "filterCutoff", "filterResonance", "filterKbTrack", "filterEnvAmount",
    // Envelopes (13)
    "ampAttack", "ampDecay", "ampSustain", "ampRelease",
    "filterAttack", "filterDecay", "filterSustain", "filterRelease", "envLink",
    "modAttack", "modDecay", "modAmount", "modTarget",
    // Dual LFOs (16)
    "lfo1Waveform", "lfo1Rate", "lfo1Delay", "lfo1Sync", "lfo1SyncDiv", "lfo1KeyReset", "lfo1Amount", "lfo1Target",
    "lfo2Waveform", "lfo2Rate", "lfo2Delay", "lfo2Sync", "lfo2SyncDiv", "lfo2KeyReset", "lfo2Amount", "lfo2Target",
    // Output & Character (7)
    "driveEnabled", "driveAmount", "driveTone", "dualMode", "analogMode", "wNoiseMode", "masterVolume"
};

static const char* const kClassicWaspParams[32] = {
    "osc1Waveform", "osc1Octave", "osc1Fine",
    "osc2Waveform", "osc2Octave", "osc2Fine", "oscMix",
    "ringModMix", "pulseWidth", "fmAmount",
    "velToAmp", "velToFilter",
    "filterMode", "filterCutoff", "filterResonance", "filterKbTrack", "filterEnvAmount",
    "ampAttack", "ampDecay", "ampSustain", "ampRelease",
    "filterAttack", "filterDecay", "filterSustain", "filterRelease", "envLink",
    "lfo1Waveform", "lfo1Rate", "lfo1Delay", "lfo1Amount", "lfo1Target",
    "masterVolume"
};

// 56 native parameters corresponding to Wasp XT Delphi form controls (Tags 0..55)
static const char* const kNativeWaspXtParams[56] = {
    /* 00 */ "osc1Waveform",         // Osc1ShapeSelect (0=Saw, 1=Square, 2=Sine, 3=Noise)
    /* 01 */ "osc1Octave",           // Osc1CoarseWheel
    /* 02 */ "osc1Fine",             // Osc1FineWheel
    /* 03 */ "osc2Waveform",         // Osc2ShapeSelect
    /* 04 */ "osc2Octave",           // Osc2CoarseWheel
    /* 05 */ "osc2Fine",             // Osc2FineWheel
    /* 06 */ "osc3Waveform",         // Osc3ShapeSelect (0=Square, 1=Saw)
    /* 07 */ "osc3Level",            // Osc3AmountWheel
    /* 08 */ "oscMix",               // OscMixSlider
    /* 09 */ "pulseWidth",           // PulseWidthWheel
    /* 10 */ "fmAmount",             // FmAmountWheel
    /* 11 */ "ringModMix",           // RingModBtn (0 or 1)
    /* 12 */ "ampAttack",            // AmpAttackWheel
    /* 13 */ "ampDecay",             // AmpDecayWheel
    /* 14 */ "ampSustain",           // AmpSustainLevelWheel
    /* 15 */ "ampRelease",           // AmpReleaseWheel
    /* 16 */ "filterAttack",         // FiltAttack
    /* 17 */ "filterDecay",          // FiltDecay
    /* 18 */ "filterSustain",        // FiltSustainLevel
    /* 19 */ "filterRelease",        // FiltRelease
    /* 20 */ "filterKbTrack",        // KbTrackWheel
    /* 21 */ "filterMode",           // FilterTypeSelect (0..5)
    /* 22 */ "filterCutoff",         // CutoffWheel
    /* 23 */ "filterResonance",      // ResonanceWheel
    /* 24 */ "filterEnvAmount",      // EnvAmtWheel
    /* 25 */ "lfo1Waveform",         // LFO1ShapeSelect (0..3)
    /* 26 */ "lfo1Target",           // LFO1DestSelect (0..2)
    /* 27 */ "lfo1Amount",           // LFO1AmountWheel
    /* 28 */ "lfo1Rate",             // LFO1FreqWheel
    /* 29 */ "lfo1Sync",             // LFO1SyncBtn
    /* 30 */ "lfo1KeyReset",         // LFO1ResetBtn
    /* 31 */ "lfo2Waveform",         // LFO2ShapeSelect (0..3)
    /* 32 */ "lfo2Target",           // LFO2DestSelect (0..2)
    /* 33 */ "lfo2Amount",           // LFO2AmountWheel
    /* 34 */ "lfo2Rate",             // LFO2FreqWheel
    /* 35 */ "lfo2Sync",             // LFO2SyncBtn
    /* 36 */ "lfo2KeyReset",         // LFO2ResetBtn
    /* 37 */ "driveEnabled",         // UseDistBtn
    /* 38 */ "driveAmount",          // DistDriveWheel
    /* 39 */ "driveTone",            // DistToneWheel
    /* 40 */ "dualMode",             // DualVoiceBtn
    /* 41 */ "velToFilter",          // VelocityFilterWheel
    /* 42 */ "analogMode",           // AnalogBtn
    /* 43 */ "modAttack",            // ModAttackWheel
    /* 44 */ "modDecay",             // ModDecayWheel
    /* 45 */ "modAmount",            // ModAmountWheel
    /* 46 */ "modDest1",             // ModDest1Btn (Filter Cutoff)
    /* 47 */ "modDest2",             // ModDest2Btn (Osc1 Pitch)
    /* 48 */ "modDest3",             // ModDest3Btn (Osc2 Pitch)
    /* 49 */ "modDest4",             // ModDest4Btn (Pulse Width)
    /* 50 */ "masterVolume",         // VolumeWheel
    /* 51 */ "aftertouch",           // AftertouchBtn
    /* 52 */ "lfo1Delay",            // LFO1DelayWheel
    /* 53 */ "lfo2Delay",            // LFO2DelayWheel
    /* 54 */ "velToAmp",             // VelocityAmpWheel
    /* 55 */ "wNoiseMode"            // WhiteNoiseBtn
};

// ============================================================================
// Endianness & Buffer Reading Primitives
// ============================================================================
static inline uint32_t readBE32(const uint8_t* p) noexcept {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

static inline int32_t readBE32i(const uint8_t* p) noexcept {
    return static_cast<int32_t>(readBE32(p));
}

static inline float readBEFloat(const uint8_t* p) noexcept {
    uint32_t u = readBE32(p);
    float f = 0.0f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

static inline uint32_t readLE32(const uint8_t* p) noexcept {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

static inline float readLEFloat(const uint8_t* p) noexcept {
    uint32_t u = readLE32(p);
    float f = 0.0f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

static std::vector<float> extractFloats(const uint8_t* data, size_t size, bool littleEndian) {
    std::vector<float> out;
    const size_t count = size / 4;
    if (count == 0) return out;
    out.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        float f = littleEndian ? readLEFloat(data + i * 4) : readBEFloat(data + i * 4);
        if (!std::isfinite(f)) {
            out.clear();
            return out;
        }
        out.push_back(f);
    }
    return out;
}

// ============================================================================
// Mathematical Domain Scaling
// ============================================================================
float PresetMigrator::clamp(float val, float minVal, float maxVal) noexcept {
    if (std::isnan(val) || std::isinf(val)) return minVal;
    return std::max(minVal, std::min(maxVal, val));
}

float PresetMigrator::scaleLogarithmic(float norm, float minVal, float maxVal) noexcept {
    if (norm > 1.0f) return clamp(norm, minVal, maxVal);
    float n = clamp(norm, 0.0f, 1.0f);
    float ratio = maxVal / minVal;
    return clamp(minVal * std::pow(ratio, n), minVal, maxVal);
}

float PresetMigrator::scaleExponentialTime(float norm, float minS, float maxS) noexcept {
    if (norm > 1.0f) return clamp(norm, minS, maxS);
    float n = clamp(norm, 0.0f, 1.0f);
    float ratio = maxS / minS;
    return clamp(minS * std::pow(ratio, n), minS, maxS);
}

float PresetMigrator::scaleLfoRate(float norm, float minHz, float maxHz) noexcept {
    if (norm > 1.0f) return clamp(norm, minHz, maxHz);
    float n = clamp(norm, 0.0f, 1.0f);
    float ratio = maxHz / minHz;
    return clamp(minHz * std::pow(ratio, n), minHz, maxHz);
}

float PresetMigrator::scaleBipolar(float norm, float minVal, float maxVal) noexcept {
    if (norm < 0.0f) return clamp(norm, minVal, maxVal);
    return clamp(norm * 2.0f - 1.0f, minVal, maxVal);
}

float PresetMigrator::scaleDiscrete(float norm, int numSteps) noexcept {
    if (norm > 1.0f)
        return clamp(static_cast<float>(std::round(norm)), 0.0f, static_cast<float>(numSteps - 1));
    return clamp(static_cast<float>(std::round(norm * (numSteps - 1))), 0.0f, static_cast<float>(numSteps - 1));
}

float PresetMigrator::mapLegacyParamToApvts(const juce::String& pid, float rawVal) {
    if (pid == "osc1Waveform" || pid == "osc2Waveform") {
        return scaleDiscrete(rawVal, 4); // 0=Saw, 1=Square, 2=Sine, 3=Noise
    }
    if (pid == "osc1Octave" || pid == "osc2Octave") {
        if (rawVal >= -3.0f && rawVal <= 3.0f && (rawVal < 0.0f || rawVal == std::round(rawVal))) {
            return clamp(std::round(rawVal), -3.0f, 3.0f);
        }
        return clamp(static_cast<float>(std::round(rawVal * 6.0f - 3.0f)), -3.0f, 3.0f);
    }
    if (pid == "osc1Fine" || pid == "osc2Fine") {
        if (rawVal >= -1.0f && rawVal <= 1.0f && rawVal < 0.0f) return clamp(rawVal, -1.0f, 1.0f);
        return scaleBipolar(rawVal, -1.0f, 1.0f);
    }
    if (pid == "oscMix") return clamp(rawVal, 0.0f, 1.0f);
    if (pid == "osc3Waveform") return (rawVal >= 0.5f ? 1.0f : 0.0f);
    if (pid == "osc3Level" || pid == "ringModMix" || pid == "fmAmount" || pid == "velToAmp" || pid == "velToFilter") {
        return clamp(rawVal, 0.0f, 1.0f);
    }
    if (pid == "pulseWidth") {
        if (rawVal >= 0.01f && rawVal <= 0.99f) return clamp(rawVal, 0.01f, 0.99f);
        return clamp(0.01f + rawVal * 0.98f, 0.01f, 0.99f);
    }

    // Filter
    if (pid == "filterMode") return scaleDiscrete(rawVal, 6);
    if (pid == "filterCutoff") return scaleLogarithmic(rawVal, 20.0f, 20000.0f);
    if (pid == "filterResonance" || pid == "filterKbTrack") return clamp(rawVal, 0.0f, 1.0f);
    if (pid == "filterEnvAmount") return scaleBipolar(rawVal, -1.0f, 1.0f);

    // Envelopes
    if (pid == "ampAttack" || pid == "ampDecay" || pid == "ampRelease" ||
        pid == "filterAttack" || pid == "filterDecay" || pid == "filterRelease" ||
        pid == "modDecay") {
        return scaleExponentialTime(rawVal, 0.001f, 10.0f);
    }
    if (pid == "modAttack") return scaleExponentialTime(rawVal, 0.001f, 5.0f);
    if (pid == "ampSustain" || pid == "filterSustain") return clamp(rawVal, 0.0f, 1.0f);
    if (pid == "envLink") return (rawVal >= 0.5f ? 1.0f : 0.0f);
    if (pid == "modAmount") return scaleBipolar(rawVal, -1.0f, 1.0f);
    if (pid == "modTarget") return scaleDiscrete(rawVal, 4);

    // LFOs
    if (pid == "lfo1Waveform" || pid == "lfo2Waveform") return scaleDiscrete(rawVal, 4);
    if (pid == "lfo1Rate" || pid == "lfo2Rate") return scaleLfoRate(rawVal, 0.05f, 30.0f);
    if (pid == "lfo1Delay" || pid == "lfo2Delay") {
        return (rawVal > 1.0f ? clamp(rawVal, 0.0f, 5.0f) : clamp(rawVal * 5.0f, 0.0f, 5.0f));
    }
    if (pid == "lfo1Sync" || pid == "lfo2Sync" || pid == "lfo1KeyReset" || pid == "lfo2KeyReset") {
        return (rawVal >= 0.5f ? 1.0f : 0.0f);
    }
    if (pid == "lfo1SyncDiv" || pid == "lfo2SyncDiv") return scaleDiscrete(rawVal, 6);
    if (pid == "lfo1Amount" || pid == "lfo2Amount") return clamp(rawVal, 0.0f, 1.0f);
    if (pid == "lfo1Target" || pid == "lfo2Target") return scaleDiscrete(rawVal, 3);

    // Character & Output
    if (pid == "driveEnabled" || pid == "dualMode" || pid == "analogMode" || pid == "wNoiseMode") {
        return (rawVal >= 0.5f ? 1.0f : 0.0f);
    }
    if (pid == "driveAmount" || pid == "driveTone" || pid == "masterVolume") {
        return clamp(rawVal, 0.0f, 1.0f);
    }

    return rawVal;
}

ParameterSnapshot PresetMigrator::createDefaultSnapshot() {
    ParameterSnapshot s {};
    s.osc1Waveform  = 0.0f;
    s.osc1Octave    = 0.0f;
    s.osc1Fine      = 0.0f;
    s.osc2Waveform  = 0.0f;
    s.osc2Octave    = 0.0f;
    s.osc2Fine      = 0.0f;
    s.oscMix        = 0.5f;
    s.osc3Waveform  = 0.0f;
    s.osc3Level     = 0.0f;
    s.ringModMix    = 0.0f;
    s.pulseWidth    = 0.50f;
    s.fmAmount      = 0.0f;
    s.velToAmp      = 0.50f;
    s.velToFilter   = 0.50f;

    s.filterMode      = 1.0f;
    s.filterCutoff    = 1200.0f;
    s.filterResonance = 0.20f;
    s.filterKbTrack   = 0.50f;
    s.filterEnvAmount = 0.0f;

    s.ampAttack     = 0.01f;
    s.ampDecay      = 0.30f;
    s.ampSustain    = 0.80f;
    s.ampRelease    = 0.30f;
    s.filterAttack  = 0.05f;
    s.filterDecay   = 0.50f;
    s.filterSustain = 0.50f;
    s.filterRelease = 0.40f;
    s.envLink       = 0.0f;

    s.modAttack     = 0.05f;
    s.modDecay      = 0.50f;
    s.modAmount     = 0.0f;
    s.modTarget     = 0.0f;

    s.lfo1Waveform  = 2.0f;
    s.lfo1Rate      = 2.0f;
    s.lfo1Delay     = 0.0f;
    s.lfo1Sync      = 0.0f;
    s.lfo1SyncDiv   = 3.0f;
    s.lfo1KeyReset  = 1.0f;
    s.lfo1Amount    = 0.0f;
    s.lfo1Target    = 1.0f;

    s.lfo2Waveform  = 2.0f;
    s.lfo2Rate      = 1.0f;
    s.lfo2Delay     = 0.0f;
    s.lfo2Sync      = 0.0f;
    s.lfo2SyncDiv   = 4.0f;
    s.lfo2KeyReset  = 1.0f;
    s.lfo2Amount    = 0.0f;
    s.lfo2Target    = 0.0f;

    s.driveEnabled  = 0.0f;
    s.driveAmount   = 0.30f;
    s.driveTone     = 0.50f;
    s.dualMode      = 0.0f;
    s.analogMode    = 0.0f;
    s.wNoiseMode    = 0.0f;
    s.masterVolume  = 0.80f;
    return s;
}

static void applyParamToSnapshot(ParameterSnapshot& s, const juce::String& pid, float val) {
    if (pid == "osc1Waveform")  s.osc1Waveform = val;
    else if (pid == "osc1Octave")    s.osc1Octave = val;
    else if (pid == "osc1Fine")      s.osc1Fine = val;
    else if (pid == "osc2Waveform")  s.osc2Waveform = val;
    else if (pid == "osc2Octave")    s.osc2Octave = val;
    else if (pid == "osc2Fine")      s.osc2Fine = val;
    else if (pid == "oscMix")        s.oscMix = val;
    else if (pid == "osc3Waveform")  s.osc3Waveform = val;
    else if (pid == "osc3Level")     s.osc3Level = val;
    else if (pid == "ringModMix")    s.ringModMix = val;
    else if (pid == "pulseWidth")    s.pulseWidth = val;
    else if (pid == "fmAmount")      s.fmAmount = val;
    else if (pid == "velToAmp")      s.velToAmp = val;
    else if (pid == "velToFilter")   s.velToFilter = val;
    else if (pid == "filterMode")      s.filterMode = val;
    else if (pid == "filterCutoff")    s.filterCutoff = val;
    else if (pid == "filterResonance") s.filterResonance = val;
    else if (pid == "filterKbTrack")   s.filterKbTrack = val;
    else if (pid == "filterEnvAmount") s.filterEnvAmount = val;
    else if (pid == "ampAttack")     s.ampAttack = val;
    else if (pid == "ampDecay")      s.ampDecay = val;
    else if (pid == "ampSustain")    s.ampSustain = val;
    else if (pid == "ampRelease")    s.ampRelease = val;
    else if (pid == "filterAttack")  s.filterAttack = val;
    else if (pid == "filterDecay")   s.filterDecay = val;
    else if (pid == "filterSustain") s.filterSustain = val;
    else if (pid == "filterRelease") s.filterRelease = val;
    else if (pid == "envLink")       s.envLink = val;
    else if (pid == "modAttack")     s.modAttack = val;
    else if (pid == "modDecay")      s.modDecay = val;
    else if (pid == "modAmount")     s.modAmount = val;
    else if (pid == "modTarget")     s.modTarget = val;
    else if (pid == "lfo1Waveform")  s.lfo1Waveform = val;
    else if (pid == "lfo1Rate")      s.lfo1Rate = val;
    else if (pid == "lfo1Delay")     s.lfo1Delay = val;
    else if (pid == "lfo1Sync")      s.lfo1Sync = val;
    else if (pid == "lfo1SyncDiv")   s.lfo1SyncDiv = val;
    else if (pid == "lfo1KeyReset")  s.lfo1KeyReset = val;
    else if (pid == "lfo1Amount")    s.lfo1Amount = val;
    else if (pid == "lfo1Target")    s.lfo1Target = val;
    else if (pid == "lfo2Waveform")  s.lfo2Waveform = val;
    else if (pid == "lfo2Rate")      s.lfo2Rate = val;
    else if (pid == "lfo2Delay")     s.lfo2Delay = val;
    else if (pid == "lfo2Sync")      s.lfo2Sync = val;
    else if (pid == "lfo2SyncDiv")   s.lfo2SyncDiv = val;
    else if (pid == "lfo2KeyReset")  s.lfo2KeyReset = val;
    else if (pid == "lfo2Amount")    s.lfo2Amount = val;
    else if (pid == "lfo2Target")    s.lfo2Target = val;
    else if (pid == "driveEnabled")  s.driveEnabled = val;
    else if (pid == "driveAmount")   s.driveAmount = val;
    else if (pid == "driveTone")     s.driveTone = val;
    else if (pid == "dualMode")      s.dualMode = val;
    else if (pid == "analogMode")    s.analogMode = val;
    else if (pid == "wNoiseMode")    s.wNoiseMode = val;
    else if (pid == "masterVolume")  s.masterVolume = val;
}

ParameterSnapshot PresetMigrator::mapFloatsToSnapshot(const std::vector<float>& floats, bool isClassicWasp32) {
    ParameterSnapshot snap = createDefaultSnapshot();
    const size_t numParams = isClassicWasp32 ? 32 : 55;
    const char* const* paramList = isClassicWasp32 ? kClassicWaspParams : kWaspXtParams;

    for (size_t i = 0; i < numParams && i < floats.size(); ++i) {
        juce::String pid = paramList[i];
        float mappedVal = mapLegacyParamToApvts(pid, floats[i]);
        applyParamToSnapshot(snap, pid, mappedVal);
    }
    return snap;
}

ParameterSnapshot PresetMigrator::mapNativeWaspFloatsToSnapshot(const std::vector<float>& floats, uint8_t flags) {
    ParameterSnapshot snap = createDefaultSnapshot();
    for (size_t i = 0; i < 56 && i < floats.size(); ++i) {
        juce::String pid = kNativeWaspXtParams[i];
        float rawVal = floats[i];
        if (pid.startsWith("modDest") || pid == "aftertouch") {
            continue;
        }
        float mappedVal = mapLegacyParamToApvts(pid, rawVal);
        applyParamToSnapshot(snap, pid, mappedVal);
    }

    // Mod Destination Buttons (Tags 46..49)
    // 46: ModDest1Btn (Filter Cutoff) -> target 0
    // 47: ModDest2Btn (Osc1 Pitch)    -> target 1
    // 48: ModDest3Btn (Osc2 Pitch)    -> target 1
    // 49: ModDest4Btn (Pulse Width)   -> target 2
    if (floats.size() > 49) {
        if (floats[46] >= 0.5f) {
            snap.modTarget = 0.0f; // Filter
        } else if (floats[47] >= 0.5f || floats[48] >= 0.5f) {
            snap.modTarget = 1.0f; // Pitch
        } else if (floats[49] >= 0.5f) {
            snap.modTarget = 2.0f; // Pulse Width
        }
    }

    // Flags: Bit 0 = envLink (AmpCoupleFltBtn)
    if (flags & 0x01) {
        snap.envLink = 1.0f;
    }

    return snap;
}

std::map<juce::String, float> PresetMigrator::snapshotToMap(const ParameterSnapshot& s) {
    std::map<juce::String, float> m;
    m["osc1Waveform"]  = s.osc1Waveform;
    m["osc1Octave"]    = s.osc1Octave;
    m["osc1Fine"]      = s.osc1Fine;
    m["osc2Waveform"]  = s.osc2Waveform;
    m["osc2Octave"]    = s.osc2Octave;
    m["osc2Fine"]      = s.osc2Fine;
    m["oscMix"]        = s.oscMix;
    m["osc3Waveform"]  = s.osc3Waveform;
    m["osc3Level"]     = s.osc3Level;
    m["ringModMix"]    = s.ringModMix;
    m["pulseWidth"]    = s.pulseWidth;
    m["fmAmount"]      = s.fmAmount;
    m["velToAmp"]      = s.velToAmp;
    m["velToFilter"]   = s.velToFilter;

    m["filterMode"]      = s.filterMode;
    m["filterCutoff"]    = s.filterCutoff;
    m["filterResonance"] = s.filterResonance;
    m["filterKbTrack"]   = s.filterKbTrack;
    m["filterEnvAmount"] = s.filterEnvAmount;

    m["ampAttack"]     = s.ampAttack;
    m["ampDecay"]      = s.ampDecay;
    m["ampSustain"]    = s.ampSustain;
    m["ampRelease"]    = s.ampRelease;
    m["filterAttack"]  = s.filterAttack;
    m["filterDecay"]   = s.filterDecay;
    m["filterSustain"] = s.filterSustain;
    m["filterRelease"] = s.filterRelease;
    m["envLink"]       = s.envLink;

    m["modAttack"]     = s.modAttack;
    m["modDecay"]      = s.modDecay;
    m["modAmount"]     = s.modAmount;
    m["modTarget"]     = s.modTarget;

    m["lfo1Waveform"]  = s.lfo1Waveform;
    m["lfo1Rate"]      = s.lfo1Rate;
    m["lfo1Delay"]     = s.lfo1Delay;
    m["lfo1Sync"]      = s.lfo1Sync;
    m["lfo1SyncDiv"]   = s.lfo1SyncDiv;
    m["lfo1KeyReset"]  = s.lfo1KeyReset;
    m["lfo1Amount"]    = s.lfo1Amount;
    m["lfo1Target"]    = s.lfo1Target;

    m["lfo2Waveform"]  = s.lfo2Waveform;
    m["lfo2Rate"]      = s.lfo2Rate;
    m["lfo2Delay"]     = s.lfo2Delay;
    m["lfo2Sync"]      = s.lfo2Sync;
    m["lfo2SyncDiv"]   = s.lfo2SyncDiv;
    m["lfo2KeyReset"]  = s.lfo2KeyReset;
    m["lfo2Amount"]    = s.lfo2Amount;
    m["lfo2Target"]    = s.lfo2Target;

    m["driveEnabled"]  = s.driveEnabled;
    m["driveAmount"]   = s.driveAmount;
    m["driveTone"]     = s.driveTone;
    m["dualMode"]      = s.dualMode;
    m["analogMode"]    = s.analogMode;
    m["wNoiseMode"]    = s.wNoiseMode;
    m["masterVolume"]  = s.masterVolume;
    return m;
}

juce::String PresetMigrator::inferCategory(const juce::String& name, const ParameterSnapshot& s) {
    juce::String nl = name.toLowerCase();
    if (nl.contains("bass") || nl.contains("sub") || nl.contains("303") || nl.contains("reese") || nl.contains("acid"))
        return "Bass";
    if (nl.contains("lead") || nl.contains("sync") || nl.contains("stab") || nl.contains("screamer") || nl.contains("hook"))
        return "Lead";
    if (nl.contains("pad") || nl.contains("choir") || nl.contains("string") || nl.contains("swell") || nl.contains("drone") || nl.contains("ambient"))
        return "Pad";
    if (nl.contains("pluck") || nl.contains("bell") || nl.contains("chime") || nl.contains("harp") || nl.contains("staccato"))
        return "Pluck";
    if (nl.contains("kick") || nl.contains("snare") || nl.contains("perc") || nl.contains("drum") || nl.contains("zap") || nl.contains("clap"))
        return "Percussion";
    if (nl.contains("fx") || nl.contains("laser") || nl.contains("noise") || nl.contains("sweep") || nl.contains("rise") || nl.contains("down"))
        return "FX";

    if (s.osc1Octave <= -1.0f && s.filterCutoff < 600.0f && s.ampAttack < 0.05f) return "Bass";
    if (s.ampAttack >= 0.40f || s.ampRelease >= 1.50f) return "Pad";
    if (s.ampSustain < 0.20f && s.ampDecay < 0.35f) return "Pluck";
    if (s.modTarget == 3.0f && s.modDecay < 0.10f) return "Percussion";
    return (s.filterCutoff > 2000.0f ? "Lead" : "Synth");
}

juce::String MigratedPreset::toXml() const {
    auto root = std::make_unique<juce::XmlElement>("BumblerXD");
    root->setAttribute("schemaVersion", 1);
    root->setAttribute("name", name);
    root->setAttribute("category", category);
    root->setAttribute("sourceFormat", sourceFormat);

    for (const char* pid : kWaspXtParams) {
        auto* paramEl = root->createNewChildElement("PARAM");
        paramEl->setAttribute("id", pid);
        float val = 0.0f;
        auto it = parameterMap.find(pid);
        if (it != parameterMap.end()) {
            val = it->second;
        }
        paramEl->setAttribute("value", static_cast<double>(val));
    }

    return root->toString();
}

// ============================================================================
// Format Parsers
// ============================================================================
std::vector<MigratedPreset> PresetMigrator::parseFxp(const void* dataPtr, size_t sizeBytes, const juce::String& filename) {
    std::vector<MigratedPreset> out;
    if (dataPtr == nullptr || sizeBytes < 28) return out;

    const uint8_t* data = static_cast<const uint8_t*>(dataPtr);
    if (std::memcmp(data, "CcnK", 4) != 0) return out;

    char fxMagic[5] = {0};
    std::memcpy(fxMagic, data + 8, 4);

    if (std::strcmp(fxMagic, "FxCk") == 0) {
        // Regular Float Preset
        int32_t numParams = readBE32i(data + 24);
        char prName[29] = {0};
        std::memcpy(prName, data + 28, 28);
        juce::String name = juce::String::fromUTF8(prName).trim();
        if (name.isEmpty() && filename.isNotEmpty()) {
            name = juce::File::createFileWithoutCheckingPath(filename).getFileNameWithoutExtension();
        }
        if (name.isEmpty()) name = "Migrated Preset";

        const size_t floatStart = 56;
        if (floatStart + numParams * 4 <= sizeBytes) {
            std::vector<float> floats;
            floats.reserve(numParams);
            for (int i = 0; i < numParams; ++i) {
                floats.push_back(readBEFloat(data + floatStart + i * 4));
            }

            MigratedPreset p;
            p.name = name;
            p.sourceFormat = "VST 2.4 FXP (FxCk)";
            p.sourceFilePath = filename;
            p.snapshot = mapFloatsToSnapshot(floats, numParams <= 36);
            p.parameterMap = snapshotToMap(p.snapshot);
            p.category = inferCategory(name, p.snapshot);
            out.push_back(p);
        }
    } else if (std::strcmp(fxMagic, "FPCh") == 0) {
        // Opaque Chunk Preset
        char prName[29] = {0};
        std::memcpy(prName, data + 28, 28);
        juce::String name = juce::String::fromUTF8(prName).trim();
        if (name.isEmpty() && filename.isNotEmpty()) {
            name = juce::File::createFileWithoutCheckingPath(filename).getFileNameWithoutExtension();
        }
        if (name.isEmpty()) name = "Migrated Preset";

        if (sizeBytes >= 60) {
            uint32_t chunkSize = readBE32(data + 56);
            const uint8_t* chunk = data + 60;
            size_t availableChunk = std::min(static_cast<size_t>(chunkSize), sizeBytes - 60);

            auto floats = extractFloats(chunk, availableChunk, true);
            if (floats.size() < 10) floats = extractFloats(chunk, availableChunk, false);

            if (floats.empty() && availableChunk >= 32) {
                for (size_t i = 0; i < std::min(availableChunk, size_t(55)); ++i) {
                    floats.push_back(chunk[i] / 255.0f);
                }
            }

            if (!floats.empty()) {
                MigratedPreset p;
                p.name = name;
                p.sourceFormat = "VST 2.4 FXP (FPCh)";
                p.sourceFilePath = filename;
                p.snapshot = mapFloatsToSnapshot(floats, floats.size() <= 36);
                p.parameterMap = snapshotToMap(p.snapshot);
                p.category = inferCategory(name, p.snapshot);
                out.push_back(p);
            }
        }
    } else if (std::strcmp(fxMagic, "FxBk") == 0) {
        // Regular Bank with multiple programs
        int32_t numPrograms = readBE32i(data + 20);
        int32_t numParams = (sizeBytes >= 160) ? readBE32i(data + 156) : 55;
        if (numParams <= 0 || numParams > 256) numParams = 55;

        size_t offset = 160;
        for (int pIdx = 0; pIdx < numPrograms; ++pIdx) {
            if (offset + 28 + numParams * 4 > sizeBytes) break;

            char prName[29] = {0};
            std::memcpy(prName, data + offset, 28);
            juce::String name = juce::String::fromUTF8(prName).trim();
            offset += 28;

            if (name.isEmpty() && filename.isNotEmpty()) {
                name = juce::File::createFileWithoutCheckingPath(filename).getFileNameWithoutExtension()
                     + "_" + juce::String::formatted("%02d", pIdx + 1);
            }
            if (name.isEmpty()) name = juce::String::formatted("Patch %02d", pIdx + 1);

            std::vector<float> floats;
            floats.reserve(numParams);
            for (int i = 0; i < numParams; ++i) {
                floats.push_back(readBEFloat(data + offset + i * 4));
            }
            offset += numParams * 4;

            MigratedPreset mp;
            mp.name = name;
            mp.sourceFormat = "VST 2.4 FXB (FxBk)";
            mp.sourceFilePath = filename;
            mp.snapshot = mapFloatsToSnapshot(floats, numParams <= 36);
            mp.parameterMap = snapshotToMap(mp.snapshot);
            mp.category = inferCategory(name, mp.snapshot);
            out.push_back(mp);
        }
    } else if (std::strcmp(fxMagic, "FBCh") == 0) {
        // Opaque Chunk Bank
        juce::String baseName = filename.isNotEmpty() ?
            juce::File::createFileWithoutCheckingPath(filename).getFileNameWithoutExtension() : "Migrated Bank";
        if (sizeBytes >= 160) {
            uint32_t chunkSize = readBE32(data + 156);
            const uint8_t* chunk = data + 160;
            size_t availableChunk = std::min(static_cast<size_t>(chunkSize), sizeBytes - 160);

            auto floats = extractFloats(chunk, availableChunk, true);
            if (floats.empty()) floats = extractFloats(chunk, availableChunk, false);

            if (!floats.empty()) {
                MigratedPreset p;
                p.name = baseName;
                p.sourceFormat = "VST 2.4 FXB (FBCh)";
                p.sourceFilePath = filename;
                p.snapshot = mapFloatsToSnapshot(floats, floats.size() <= 36);
                p.parameterMap = snapshotToMap(p.snapshot);
                p.category = inferCategory(baseName, p.snapshot);
                out.push_back(p);
            }
        }
    }
    return out;
}

static std::vector<MigratedPreset> parseNativeWaspChunk(const uint8_t* chunk, size_t chunkLen, const juce::String& defaultName, const juce::String& filename) {
    std::vector<MigratedPreset> out;
    if (chunk == nullptr || chunkLen < 32) return out;

    juce::String pName = defaultName.isNotEmpty() ? defaultName :
        (filename.isNotEmpty() ? juce::File::createFileWithoutCheckingPath(filename).getFileNameWithoutExtension() : "Migrated Preset");

    // 1. Check for embedded CcnK anywhere
    for (size_t j = 0; j + 28 <= chunkLen; ++j) {
        if (std::memcmp(chunk + j, "CcnK", 4) == 0) {
            auto sub = PresetMigrator::parseFxp(chunk + j, chunkLen - j, filename);
            if (!sub.empty()) {
                for (auto& p : sub) {
                    if (p.name.isEmpty() || p.name.startsWith("Patch ") || p.name == "Migrated Preset")
                        p.name = pName;
                }
                return sub;
            }
        }
    }

    // 2. Native FL Studio Wasp XT chunk (Delphi TFruityPlug SaveRestoreState):
    // Format: 4-byte version (10..13, e.g. 0x0D), followed by 224 bytes (56 floats), followed by 1 byte flags
    if (chunkLen >= 229) {
        uint32_t version = readLE32(chunk);
        if (version >= 1 && version <= 32) {
            std::vector<float> nativeFloats;
            nativeFloats.reserve(56);
            for (size_t i = 0; i < 56; ++i) {
                nativeFloats.push_back(readLEFloat(chunk + 4 + i * 4));
            }
            uint8_t flags = chunk[4 + 224];
            MigratedPreset p;
            p.name = pName;
            p.sourceFormat = "FL Studio Wasp XT Native (State v" + juce::String(version) + ")";
            p.sourceFilePath = filename;
            p.snapshot = PresetMigrator::mapNativeWaspFloatsToSnapshot(nativeFloats, flags);
            p.parameterMap = PresetMigrator::snapshotToMap(p.snapshot);
            p.category = PresetMigrator::inferCategory(pName, p.snapshot);
            out.push_back(p);
            return out;
        }
    }

    // 3. Native FL Studio older version: 164 bytes (41 floats) or 208 bytes (52 floats)
    if (chunkLen >= 169) {
        uint32_t version = readLE32(chunk);
        if (version >= 1 && version <= 32) {
            size_t numFloats = (chunkLen >= 213) ? 52 : 41;
            std::vector<float> nativeFloats;
            nativeFloats.reserve(numFloats);
            for (size_t i = 0; i < numFloats; ++i) {
                nativeFloats.push_back(readLEFloat(chunk + 4 + i * 4));
            }
            uint8_t flags = (chunkLen >= 4 + numFloats * 4 + 1) ? chunk[4 + numFloats * 4] : 0;
            MigratedPreset p;
            p.name = pName;
            p.sourceFormat = "FL Studio Wasp XT Native (State v" + juce::String(version) + ")";
            p.sourceFilePath = filename;
            p.snapshot = PresetMigrator::mapNativeWaspFloatsToSnapshot(nativeFloats, flags);
            p.parameterMap = PresetMigrator::snapshotToMap(p.snapshot);
            p.category = PresetMigrator::inferCategory(pName, p.snapshot);
            out.push_back(p);
            return out;
        }
    }

    // 4. Raw 56 or 55 floats at offset 0
    if (chunkLen >= 55 * 4) {
        auto floats = extractFloats(chunk, chunkLen, true);
        if (floats.empty()) floats = extractFloats(chunk, chunkLen, false);
        if (floats.size() >= 55) {
            MigratedPreset p;
            p.name = pName;
            p.sourceFormat = "FL Studio FST (Raw Floats)";
            p.sourceFilePath = filename;
            p.snapshot = (floats.size() >= 56) ?
                PresetMigrator::mapNativeWaspFloatsToSnapshot(floats, 0) :
                PresetMigrator::mapFloatsToSnapshot(floats, false);
            p.parameterMap = PresetMigrator::snapshotToMap(p.snapshot);
            p.category = PresetMigrator::inferCategory(pName, p.snapshot);
            out.push_back(p);
            return out;
        }
    }

    // 5. Multi-offset scanning for 32..56 normalized floats in range [-3.5, 3.5]
    for (size_t off = 0; off + 32 * 4 <= chunkLen && off <= 64; off += 4) {
        size_t count = (chunkLen - off) / 4;
        if (count > 56) count = 56;
        size_t validCount = 0;
        std::vector<float> candidate;
        candidate.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            float f = readLEFloat(chunk + off + i * 4);
            if (std::isfinite(f) && f >= -3.5f && f <= 3.5f) {
                validCount++;
            }
            candidate.push_back(f);
        }
        if (count >= 32 && validCount >= (count * 85) / 100) {
            MigratedPreset p;
            p.name = pName;
            p.sourceFormat = "FL Studio FST (Scanned Block)";
            p.sourceFilePath = filename;
            p.snapshot = (candidate.size() >= 56) ?
                PresetMigrator::mapNativeWaspFloatsToSnapshot(candidate, 0) :
                PresetMigrator::mapFloatsToSnapshot(candidate, candidate.size() <= 36);
            p.parameterMap = PresetMigrator::snapshotToMap(p.snapshot);
            p.category = PresetMigrator::inferCategory(pName, p.snapshot);
            out.push_back(p);
            return out;
        }
    }

    // 6. Byte knob values fallback (0..255)
    if (chunkLen >= 32) {
        std::vector<float> byteFloats;
        for (size_t i = 0; i < std::min(chunkLen, size_t(56)); ++i) {
            byteFloats.push_back(chunk[i] / 255.0f);
        }
        MigratedPreset p;
        p.name = pName;
        p.sourceFormat = "FL Studio FST (Raw Bytes)";
        p.sourceFilePath = filename;
        p.snapshot = (byteFloats.size() >= 56) ?
            PresetMigrator::mapNativeWaspFloatsToSnapshot(byteFloats, 0) :
            PresetMigrator::mapFloatsToSnapshot(byteFloats, byteFloats.size() <= 36);
        p.parameterMap = PresetMigrator::snapshotToMap(p.snapshot);
        p.category = PresetMigrator::inferCategory(pName, p.snapshot);
        out.push_back(p);
        return out;
    }

    return out;
}

std::vector<MigratedPreset> PresetMigrator::parseFst(const void* dataPtr, size_t sizeBytes, const juce::String& filename) {
    std::vector<MigratedPreset> out;
    if (dataPtr == nullptr || sizeBytes < 8) return out;

    const uint8_t* data = static_cast<const uint8_t*>(dataPtr);
    juce::String baseName = filename.isNotEmpty() ?
        juce::File::createFileWithoutCheckingPath(filename).getFileNameWithoutExtension() : "Migrated FST";

    // 1. Direct embedded VST CcnK check
    for (size_t i = 0; i + 28 <= sizeBytes; ++i) {
        if (std::memcmp(data + i, "CcnK", 4) == 0) {
            auto fxpOut = parseFxp(data + i, sizeBytes - i, filename);
            if (!fxpOut.empty()) return fxpOut;
        }
    }

    // 2. FLhd / FLdt Stream
    for (size_t i = 0; i + 4 <= sizeBytes; ++i) {
        if (std::memcmp(data + i, "FLhd", 4) == 0) {
            auto flpOut = parseFlp(data + i, sizeBytes - i, filename);
            if (!flpOut.empty()) return flpOut;
        }
    }

    // 3. RIFF Container
    if (std::memcmp(data, "RIFF", 4) == 0 && sizeBytes >= 12) {
        size_t pos = 12;
        while (pos + 8 <= sizeBytes) {
            char chunkId[5] = {0};
            std::memcpy(chunkId, data + pos, 4);
            uint32_t chunkLen = readLE32(data + pos + 4);
            pos += 8;

            if (pos + chunkLen > sizeBytes) break;
            const uint8_t* chunkPayload = data + pos;

            auto sub = parseNativeWaspChunk(chunkPayload, chunkLen, baseName, filename);
            if (!sub.empty()) return sub;

            pos += (chunkLen + 1) & ~1; // 2-byte alignment
        }
    }

    // 4. Raw file scan fallback
    auto fallback = parseNativeWaspChunk(data, sizeBytes, baseName, filename);
    if (!fallback.empty()) return fallback;

    return out;
}

static std::pair<uint32_t, size_t> readVarint(const uint8_t* buf, size_t pos, size_t maxLen) {
    uint32_t result = 0;
    int shift = 0;
    while (pos < maxLen) {
        uint8_t b = buf[pos++];
        result |= (uint32_t(b & 0x7F) << shift);
        shift += 7;
        if ((b & 0x80) == 0) break;
    }
    return {result, pos};
}

std::vector<MigratedPreset> PresetMigrator::parseFlp(const void* dataPtr, size_t sizeBytes, const juce::String& filename) {
    std::vector<MigratedPreset> out;
    if (dataPtr == nullptr || sizeBytes < 16) return out;

    const uint8_t* data = static_cast<const uint8_t*>(dataPtr);
    size_t flhdPos = 0;
    bool foundFlhd = false;

    for (size_t i = 0; i + 4 <= sizeBytes; ++i) {
        if (std::memcmp(data + i, "FLhd", 4) == 0) {
            flhdPos = i;
            foundFlhd = true;
            break;
        }
    }
    if (!foundFlhd) return out;

    size_t pos = flhdPos;
    uint32_t headerLen = readLE32(data + pos + 4);
    pos += 8 + headerLen;

    if (pos + 8 > sizeBytes) return out;

    // Scan for FLdt
    size_t fldtPos = 0;
    bool foundFldt = false;
    for (size_t i = pos; i + 4 <= sizeBytes; ++i) {
        if (std::memcmp(data + i, "FLdt", 4) == 0) {
            fldtPos = i;
            foundFldt = true;
            break;
        }
    }
    if (!foundFldt) return out;

    uint32_t fldtLen = readLE32(data + fldtPos + 4);
    pos = fldtPos + 8;
    const size_t endPos = std::min(sizeBytes, pos + fldtLen);

    juce::String currentChanName;
    juce::String currentPluginName;
    int chanCounter = 0;
    const bool isFstFile = filename.toLowerCase().endsWith(".fst");
    const juce::String fileStem = filename.isNotEmpty() ?
        juce::File::createFileWithoutCheckingPath(filename).getFileNameWithoutExtension() : "";

    while (pos < endPos) {
        uint8_t cmd = data[pos++];
        if (cmd < 64) {
            if (pos >= endPos) break;
            pos++; // 1 byte
            if (cmd == 0) { // FLP_NewChannel
                chanCounter++;
                currentChanName = "Channel " + juce::String(chanCounter);
                currentPluginName = {};
            }
        } else if (cmd < 128) {
            pos += 2;
        } else if (cmd < 192) {
            pos += 4;
        } else {
            auto [chunkLen, newPos] = readVarint(data, pos, endPos);
            pos = newPos;
            if (pos + chunkLen > endPos) break;

            const uint8_t* chunkBytes = data + pos;
            pos += chunkLen;

            // String events:
            // 192 = Channel Title / Name
            // 196 = Plugin Name / Sample Path
            // 201 = PluginID.InternalName (e.g. "Wasp XT", "Wasp", "Fruity Wrapper")
            // 203 = PluginID.Name (e.g. "Wasp XT")
            if (cmd == 192 || cmd == 196 || cmd == 201 || cmd == 203) {
                juce::String strVal = juce::String::fromUTF8(reinterpret_cast<const char*>(chunkBytes), static_cast<int>(chunkLen)).trim();
                while (strVal.endsWithChar('\0')) strVal = strVal.dropLastCharacters(1);
                if (strVal.isNotEmpty()) {
                    if (cmd == 192 && (currentChanName.isEmpty() || currentChanName.startsWith("Channel ")))
                        currentChanName = strVal;
                    if (cmd == 196 || cmd == 201 || cmd == 203)
                        currentPluginName = strVal;
                }
            } else if (cmd == 213 || cmd == 197 || cmd == 212) {
                // Plugin Data event (213=PluginID.Data, 197=FLP_PluginData, 212=Wrapper)
                bool isWasp = currentPluginName.toLowerCase().contains("wasp") ||
                              currentChanName.toLowerCase().contains("wasp") ||
                              filename.toLowerCase().contains("wasp") ||
                              isFstFile;

                juce::String pName;
                if (isFstFile && fileStem.isNotEmpty() && !fileStem.startsWith("test_") && !fileStem.equalsIgnoreCase("Migrated FST")) {
                    pName = fileStem;
                } else if (currentChanName.isNotEmpty() && !currentChanName.startsWith("Channel ")) {
                    pName = currentChanName;
                } else if (currentPluginName.isNotEmpty()) {
                    pName = currentPluginName;
                } else {
                    pName = juce::String::formatted("Wasp_Chan_%d", chanCounter > 0 ? chanCounter : 1);
                }

                auto sub = parseNativeWaspChunk(chunkBytes, chunkLen, pName, filename);
                if (!sub.empty()) {
                    if (isWasp || !filename.toLowerCase().endsWith(".flp")) {
                        if (filename.toLowerCase().endsWith(".flp")) {
                            for (auto& p : sub) {
                                p.sourceFormat = "FL Studio FLP Event 0x" + juce::String::toHexString(cmd).toUpperCase();
                            }
                        }
                        out.insert(out.end(), sub.begin(), sub.end());
                    }
                }
            }
        }
    }
    return out;
}

std::vector<MigratedPreset> PresetMigrator::parseXml(const juce::String& xmlText, const juce::String& filename) {
    std::vector<MigratedPreset> out;
    auto xml = juce::XmlDocument::parse(xmlText);
    if (xml == nullptr) return out;

    if (!xml->hasTagName("BumblerXD") && !xml->hasTagName("Parameters")) return out;

    MigratedPreset p;
    p.name = xml->getStringAttribute("name", juce::File::createFileWithoutCheckingPath(filename).getFileNameWithoutExtension());
    if (p.name.isEmpty()) p.name = "Imported Preset";
    p.category = xml->getStringAttribute("category", "Synth");
    p.sourceFormat = xml->getStringAttribute("sourceFormat", "Bumbler XD APVTS XML");
    p.sourceFilePath = filename;
    p.snapshot = createDefaultSnapshot();

    for (auto* child : xml->getChildIterator()) {
        if (child->hasTagName("PARAM")) {
            juce::String pid = child->getStringAttribute("id");
            float val = static_cast<float>(child->getDoubleAttribute("value", 0.0));
            applyParamToSnapshot(p.snapshot, pid, val);
            p.parameterMap[pid] = val;
        }
    }
    if (p.category == "Synth") {
        p.category = inferCategory(p.name, p.snapshot);
    }
    out.push_back(p);
    return out;
}

// ============================================================================
// High-Level Migration Workflows
// ============================================================================
std::vector<MigratedPreset> PresetMigrator::migrateFile(const juce::File& file) {
    if (!file.existsAsFile()) return {};

    juce::MemoryBlock mb;
    if (!file.loadFileAsData(mb) || mb.getSize() == 0) return {};

    const juce::String ext = file.getFileExtension().toLowerCase();
    const juce::String path = file.getFullPathName();

    if (ext == ".xml") {
        return parseXml(mb.toString(), path);
    }
    if (ext == ".fxp" || ext == ".fxb") {
        return parseFxp(mb.getData(), mb.getSize(), path);
    }
    if (ext == ".fst") {
        return parseFst(mb.getData(), mb.getSize(), path);
    }
    if (ext == ".flp") {
        return parseFlp(mb.getData(), mb.getSize(), path);
    }

    // Magic inspection fallback
    if (mb.getSize() >= 4) {
        const uint8_t* d = static_cast<const uint8_t*>(mb.getData());
        if (std::memcmp(d, "CcnK", 4) == 0) return parseFxp(d, mb.getSize(), path);
        if (std::memcmp(d, "RIFF", 4) == 0) return parseFst(d, mb.getSize(), path);
        if (std::memcmp(d, "FLhd", 4) == 0) return parseFlp(d, mb.getSize(), path);
        if (mb.toString().trimStart().startsWith("<")) return parseXml(mb.toString(), path);
    }

    return {};
}

std::vector<MigratedPreset> PresetMigrator::migrateFiles(const juce::Array<juce::File>& files) {
    std::vector<MigratedPreset> all;
    for (const auto& f : files) {
        if (f.isDirectory()) {
            auto sub = migrateDirectory(f, true);
            all.insert(all.end(), sub.begin(), sub.end());
        } else {
            auto sub = migrateFile(f);
            all.insert(all.end(), sub.begin(), sub.end());
        }
    }
    return all;
}

std::vector<MigratedPreset> PresetMigrator::migrateDirectory(const juce::File& dir, bool recursive) {
    std::vector<MigratedPreset> all;
    if (!dir.isDirectory()) return all;

    juce::Array<juce::File> foundFiles;
    dir.findChildFiles(foundFiles, juce::File::findFiles, recursive, "*.fxp;*.fxb;*.fst;*.flp;*.xml");

    for (const auto& f : foundFiles) {
        auto sub = migrateFile(f);
        all.insert(all.end(), sub.begin(), sub.end());
    }
    return all;
}

// ============================================================================
// Library Storage & File System
// ============================================================================
juce::File PresetMigrator::getUserPresetDirectory() {
    juce::File docDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    juce::File presetDir = docDir.getChildFile("Bumbler XD").getChildFile("Presets").getChildFile("Migrated");
    if (!presetDir.exists()) {
        presetDir.createDirectory();
    }
    return presetDir;
}

bool PresetMigrator::savePresetToXml(const MigratedPreset& preset, const juce::File& destinationFile) {
    destinationFile.getParentDirectory().createDirectory();
    return destinationFile.replaceWithText(preset.toXml(), false, false, "UTF-8");
}

juce::File PresetMigrator::saveToUserLibrary(const MigratedPreset& preset) {
    juce::File baseDir = getUserPresetDirectory();
    juce::File categoryDir = baseDir.getChildFile(preset.category.isNotEmpty() ? preset.category : "Synth");
    categoryDir.createDirectory();

    juce::String safeName = preset.name;
    for (juce::juce_wchar c : "<>:\"/\\|?*") {
        safeName = safeName.replaceCharacter(c, '_');
    }
    safeName = safeName.trim();
    if (safeName.isEmpty()) safeName = "Migrated_Patch";

    juce::File targetFile = categoryDir.getChildFile(safeName + ".xml");
    savePresetToXml(preset, targetFile);
    return targetFile;
}

juce::Array<juce::File> PresetMigrator::scanUserPresets() {
    juce::Array<juce::File> files;
    juce::File baseDir = getUserPresetDirectory();
    if (baseDir.isDirectory()) {
        baseDir.findChildFiles(files, juce::File::findFiles, true, "*.xml");
    }
    return files;
}

} // namespace bumbler
