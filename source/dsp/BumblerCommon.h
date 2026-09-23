#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <vector>
#include <cstring>
#include <limits>

// Hardware denormal control intrinsics
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
#include <arm64intr.h>
#endif
#endif

namespace bumbler {

// ============================================================================
// Mathematical Constants
// ============================================================================
inline constexpr float kPi        = 3.14159265358979323846f;
inline constexpr float kTwoPi     = 6.28318530717958647692f;
inline constexpr float kHalfPi    = 1.57079632679489661923f;
inline constexpr float kInvTwoPi  = 0.15915494309189533577f;
inline constexpr float kSqrt2     = 1.41421356237309504880f;

// ============================================================================
// ScopedNoDenormals: Cross-Platform RAII Hardware FTZ/DAZ Guard
// Bit 15: FTZ (Flush-To-Zero), Bit 6: DAZ (Denormals-Are-Zero)
// ============================================================================
class ScopedNoDenormals {
public:
    ScopedNoDenormals() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        mOldMxcsr = _mm_getcsr();
        _mm_setcsr(mOldMxcsr | 0x8040);
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
        mOldFpcr = _ReadStatusReg(ARM64_FPCR);
        _WriteStatusReg(ARM64_FPCR, mOldFpcr | (1ULL << 24)); // Bit 24: FZ
#elif defined(__GNUC__) || defined(__clang__)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        mOldFpcr = fpcr;
        asm volatile("msr fpcr, %0" : : "r"(fpcr | (1ULL << 24)));
#endif
#endif
    }

    ~ScopedNoDenormals() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_setcsr(mOldMxcsr);
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
        _WriteStatusReg(ARM64_FPCR, mOldFpcr);
#elif defined(__GNUC__) || defined(__clang__)
        asm volatile("msr fpcr, %0" : : "r"(mOldFpcr));
#endif
#endif
    }

    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

private:
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    unsigned int mOldMxcsr { 0 };
#elif defined(__aarch64__) || defined(_M_ARM64)
    uint64_t mOldFpcr { 0 };
#else
    int mDummy { 0 };
#endif
};

// ============================================================================
// Bitwise IEEE 754 Sanity Checks (Immune to -ffast-math optimizations)
// ============================================================================
[[nodiscard]] inline bool isFiniteBitwise(float val) noexcept {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(float));
    return (bits & 0x7F800000u) != 0x7F800000u;
}

[[nodiscard]] inline bool isNanOrInfBitwise(float val) noexcept {
    return !isFiniteBitwise(val);
}

[[nodiscard]] inline float flushDenormal(float val) noexcept {
    if (isNanOrInfBitwise(val)) [[unlikely]] {
        return 0.0f;
    }
    return (std::abs(val) < 1.0e-15f) ? 0.0f : val;
}

// ============================================================================
// Phase & Frequency Helper Functions
// ============================================================================
[[nodiscard]] inline float wrap01(float phase) noexcept {
    phase -= std::floor(phase);
    if (phase >= 1.0f || phase < 0.0f) {
        phase = 0.0f;
    }
    return phase;
}

[[nodiscard]] inline float midiNoteToHz(float midiNote, float a4Freq = 440.0f) noexcept {
    return a4Freq * std::pow(2.0f, (midiNote - 69.0f) * (1.0f / 12.0f));
}

[[nodiscard]] inline float semitonesToRatio(float semitones) noexcept {
    return std::pow(2.0f, semitones * (1.0f / 12.0f));
}

[[nodiscard]] inline float centsToRatio(float cents) noexcept {
    return std::pow(2.0f, cents * (1.0f / 1200.0f));
}

[[nodiscard]] inline float dbToGain(float db) noexcept {
    return std::pow(10.0f, db * 0.05f);
}

[[nodiscard]] inline float gainToDb(float gain) noexcept {
    return (gain > 1.0e-5f) ? (20.0f * std::log10(gain)) : -100.0f;
}

// ============================================================================
// PolyBLEP Residual Function
// Returns residual correction for step discontinuity over [-dt, dt]
// ============================================================================
[[nodiscard]] inline float polyBlep(float t, float dt) noexcept {
    if (dt <= 0.0f) return 0.0f;
    if (t < dt) {
        const float x = t / dt;
        return x + x - x * x - 1.0f; // 2x - x^2 - 1
    }
    if (t > 1.0f - dt) {
        const float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f; // x^2 + 2x + 1
    }
    return 0.0f;
}

[[nodiscard]] inline float polyBlep4(float t, float dt) noexcept {
    if (dt <= 0.0f) return 0.0f;
    constexpr float c = 0.095f;
    constexpr float a1 = 2.0f - 8.0f * c;       // 1.24f
    constexpr float a3 = 16.0f * c - 2.0f;      // -0.48f
    constexpr float a4 = 1.0f - 9.0f * c;       // 0.145f

    if (t < dt) {
        const float x = t / dt;
        return -1.0f + x * (a1 + x * x * (a3 + a4 * x));
    }
    if (t < 2.0f * dt) {
        const float x = t / dt;
        const float u = 2.0f - x;
        const float u2 = u * u;
        return (-c) * (u2 * u2);
    }
    if (t > 1.0f - dt) {
        const float x = (t - 1.0f) / dt;
        return 1.0f + x * (a1 + x * x * (a3 - a4 * x));
    }
    if (t > 1.0f - 2.0f * dt) {
        const float x = (t - 1.0f) / dt;
        const float u = x + 2.0f;
        const float u2 = u * u;
        return c * (u2 * u2);
    }
    return 0.0f;
}

// ============================================================================
// Fast Precomputed Sine Lookup Table (2048 points, linear interpolation)
// Peak error < -118 dB, zero allocations, no transcendental calls in audio loop
// ============================================================================
class FastSinTable {
public:
    static constexpr size_t kTableSize = 2048;
    static constexpr size_t kMask = kTableSize - 1;

    static inline const std::array<float, kTableSize + 1> table = []() {
        std::array<float, kTableSize + 1> t {};
        for (size_t i = 0; i <= kTableSize; ++i) {
            t[i] = std::sin(static_cast<float>(i) * (kTwoPi / static_cast<float>(kTableSize)));
        }
        return t;
    }();

    [[nodiscard]] static inline float sin01(float phase01) noexcept {
        float p = wrap01(phase01);
        if (p >= 1.0f || p < 0.0f) p = 0.0f;
        float norm = p * static_cast<float>(kTableSize);
        if (norm >= static_cast<float>(kTableSize)) norm = 0.0f;
        const size_t idx = static_cast<size_t>(norm) & kMask;
        const float frac = norm - static_cast<float>(idx);
        return table[idx] + frac * (table[idx + 1] - table[idx]);
    }

    [[nodiscard]] static inline float sinRad(float angle) noexcept {
        return sin01(angle * kInvTwoPi);
    }
};

// ============================================================================
// One-Pole DC Blocker Filter
// Cutoff frequency configurable, defaults to ~10 Hz to prevent DC runaway
// ============================================================================
class OnePoleDCBlocker {
public:
    void reset() noexcept {
        mX1 = 0.0f;
        mY1 = 0.0f;
    }

    void setSampleRate(float sampleRate, float cutoffHz = 10.0f) noexcept {
        const float fs = (sampleRate > 100.0f) ? sampleRate : 48000.0f;
        mR = 1.0f - (kTwoPi * cutoffHz / fs);
        mR = std::clamp(mR, 0.90f, 0.9999f);
    }

    [[nodiscard]] inline float process(float x) noexcept {
        const float y = x - mX1 + mR * mY1;
        mX1 = flushDenormal(x);
        mY1 = flushDenormal(y);
        return y;
    }

private:
    float mR { 0.99857f };
    float mX1 { 0.0f };
    float mY1 { 0.0f };
};

// ============================================================================
// Synthesizer Enums
// ============================================================================
enum class OscWaveform : int {
    Saw = 0,
    Square = 1,
    Sine = 2,
    Noise = 3
};

using WaveformType = OscWaveform;

enum class Osc3Waveform : int {
    Square = 0,
    Saw = 1
};

enum class FilterMode : int {
    LP12 = 0,
    LP24 = 1,
    LP_NT = 2,
    DBL_NT = 3,
    BP24 = 4,
    HP24 = 5
};

enum class LfoWaveform : int {
    Saw = 0,
    Square = 1,
    Sine = 2,
    Noise = 3
};

enum class Lfo1Destination : int {
    Osc12Pitch = 0,
    FilterCutoff = 1,
    PulseWidth = 2
};

enum class Lfo2Destination : int {
    Osc1Pitch = 0,
    OscMix = 1,
    MasterAmp = 2
};

enum class ModEnvDestination : int {
    PulseWidth = 0,
    Lfo1Amount = 1,
    Osc1Level = 2,
    Osc2Pitch = 3
};

enum class WNoiseMode : int {
    Vintage = 0,
    White = 1
};

} // namespace bumbler
