#include "BumblerEngine.h"

namespace bumbler {

void BumblerEngine::prepare(double sampleRate, int maxBlockSize) noexcept {
    mVoiceManager.prepare(sampleRate, maxBlockSize);
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
        // velocity interpreted as normalized pitch bend in [-1.0, 1.0]
        mVoiceManager.setPitchBend(velocity * 2.0f);
    }
}

void BumblerEngine::renderBlock(float* const* outputChannels, int numChannels, int numSamples, const ParameterSnapshot& params) noexcept {
    ScopedNoDenormals noDenormals;
    mVoiceManager.renderBlock(outputChannels, numChannels, numSamples, params);
}

} // namespace bumbler
