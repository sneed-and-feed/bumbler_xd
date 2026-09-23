#include "BumblerEngine.h"
#include <cmath>
#include <algorithm>

namespace bumbler {

void BumblerEngine::prepare(double sampleRate, int maxBlockSize) noexcept {
    // Defensively validate sampleRate against non-finite, sub-audio, or ultrasonic extremes
    const double safeSampleRate = (!std::isfinite(sampleRate) || sampleRate <= 1000.0 || sampleRate > 384000.0)
                                      ? 48000.0
                                      : sampleRate;
    // Defensively clamp block size to standard real-time audio bounds [1, 8192]
    const int safeBlockSize = std::clamp(maxBlockSize, 1, 8192);

    mVoiceManager.prepare(safeSampleRate, safeBlockSize);
    mVoiceManager.setFilterEnabled(true);
}

void BumblerEngine::reset() noexcept {
    mVoiceManager.reset();
}

void BumblerEngine::processMidiEvent(int status, int noteNumber, float velocity) noexcept {
    const int command = status & 0xF0;
    if (command == 0x90) { // Note On
        if (velocity > 0.0f) {
            mVoiceManager.noteOn(noteNumber, velocity);
        } else {
            mVoiceManager.noteOff(noteNumber, 0.0f);
        }
    } else if (command == 0x80) { // Note Off
        mVoiceManager.noteOff(noteNumber, velocity);
    } else if (command == 0xB0) { // Control Change
        if (noteNumber == 64) { // Sustain Pedal
            mVoiceManager.setSustainPedal(velocity >= 0.5f);
        } else if (noteNumber == 120 || noteNumber == 123) { // All Sound/Notes Off
            mVoiceManager.allNotesOff(noteNumber == 120);
        }
    } else if (command == 0xE0) { // Pitch Bend
        // ====================================================================
        // 14-Bit MIDI Pitch Wheel Transformation & Scaling:
        // A raw MIDI pitch wheel event combines two 7-bit bytes into a 14-bit
        // unsigned integer value in [0, 16383]:
        //   - Minimum: 0 (0x0000) -> maximum downward pitch deflection
        //   - Center:  8192 (0x2000) -> neutral pitch (no deflection)
        //   - Maximum: 16383 (0x3FFF) -> maximum upward pitch deflection
        //
        // 1. Upstream Normalization (PluginProcessor):
        //      normalized = static_cast<float>(raw14bit - 8192) / 8192.0f
        //    Yields normalized bipolar value in [-1.0f, +1.0f]:
        //      - 0     -> (0 - 8192) / 8192 = -1.0f
        //      - 8192  -> (8192 - 8192) / 8192 = 0.0f
        //      - 16383 -> (16383 - 8192) / 8192 ≈ +0.99988f
        //    This normalized bipolar value is passed as 'velocity' into this method.
        //
        // 2. Semitone Range Scaling:
        //    The normalized bipolar value is multiplied by 2.0f to produce
        //    the standard synthesizer pitch bend range of +/- 2.0 semitones:
        //      semitones = normalized * 2.0f  (range: [-2.0f, +2.0f] semitones)
        // ====================================================================
        mVoiceManager.setPitchBend(velocity * 2.0f);
    }
}

void BumblerEngine::renderBlock(float* const* outputChannels, int numChannels, int numSamples, const ParameterSnapshot& params) noexcept {
    if (outputChannels == nullptr || numChannels < 1 || numSamples <= 0) return;
    ScopedNoDenormals noDenormals;
    mVoiceManager.renderBlock(outputChannels, numChannels, numSamples, params);
}

} // namespace bumbler
