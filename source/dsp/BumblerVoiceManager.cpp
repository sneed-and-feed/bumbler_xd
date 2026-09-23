#include "BumblerVoiceManager.h"

namespace bumbler {

void BumblerVoiceManager::prepare(double sampleRate, int maxBlockSize) noexcept {
    mSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    mMaxBlockSize = (maxBlockSize > 0) ? maxBlockSize : 512;
    mSampleCounter = 0;
    mNoteTriggerCounter = 0;
    mLastAllocatedIndex = 0;
    mMonoScratchBuffer.fill(0.0f);
    mAnalogMode = false;

    for (size_t i = 0; i < mVoices.size(); ++i) {
        auto& voice = mVoices[i];
        voice.prepare(mSampleRate);
        voice.setFilterEnabled(mFilterEnabled);
        voice.setHostBpm(mHostBpm);
        voice.seedVoice(static_cast<uint32_t>(i));
    }
    mCharacterCircuits.prepare(mSampleRate, mMaxBlockSize);
    reset();
}

void BumblerVoiceManager::reset() noexcept {
    mSampleCounter = 0;
    mNoteTriggerCounter = 0;
    mSustainPedal = false;
    mPitchBendSemitones = 0.0f;
    for (auto& voice : mVoices) {
        voice.reset();
    }
    mCharacterCircuits.reset();
}

void BumblerVoiceManager::setPolyphonyLimit(int limit) noexcept {
    mMaxPolyphony = std::clamp(limit, 1, kMaxVoices);
    // If limit was decreased, force kill any voices beyond the new limit
    for (int i = mMaxPolyphony; i < kMaxVoices; ++i) {
        if (mVoices[static_cast<size_t>(i)].isActive()) {
            mVoices[static_cast<size_t>(i)].forceKill();
        }
    }
}

int BumblerVoiceManager::getNumActiveVoices() const noexcept {
    int activeCount = 0;
    for (int i = 0; i < mMaxPolyphony; ++i) {
        if (mVoices[static_cast<size_t>(i)].isActive()) {
            ++activeCount;
        }
    }
    return activeCount;
}

BumblerVoice* BumblerVoiceManager::allocateVoice(int midiNote) noexcept {
    // ------------------------------------------------------------------------
    // Tier 1: Same-Pitch Retriggering
    // If an active voice is already playing this note, retrigger that voice.
    // Prevents voice pileup during rapid repeated notes.
    // ------------------------------------------------------------------------
    for (int i = 0; i < mMaxPolyphony; ++i) {
        auto& v = mVoices[static_cast<size_t>(i)];
        if (v.isActive() && v.getMidiNote() == midiNote) {
            return &v;
        }
    }

    // ------------------------------------------------------------------------
    // Tier 2: Free / Inactive Voice
    // Search for an inactive voice using round-robin scan.
    // ------------------------------------------------------------------------
    for (int i = 0; i < mMaxPolyphony; ++i) {
        const int idx = (mLastAllocatedIndex + 1 + i) % mMaxPolyphony;
        if (!mVoices[static_cast<size_t>(idx)].isActive()) {
            mLastAllocatedIndex = idx;
            return &mVoices[static_cast<size_t>(idx)];
        }
    }

    // ------------------------------------------------------------------------
    // Tier 3: Releasing Voice Stealing
    // If all voices are active, prefer stealing a voice in the Release stage.
    // Steal the releasing voice with the oldest trigger sample.
    // ------------------------------------------------------------------------
    BumblerVoice* oldestReleasing = nullptr;
    uint64_t oldestReleaseSample = UINT64_MAX;
    for (int i = 0; i < mMaxPolyphony; ++i) {
        auto& v = mVoices[static_cast<size_t>(i)];
        if (v.isReleasing() && v.getTriggerSample() < oldestReleaseSample) {
            oldestReleaseSample = v.getTriggerSample();
            oldestReleasing = &v;
        }
    }
    if (oldestReleasing != nullptr) {
        mLastAllocatedIndex = static_cast<int>(oldestReleasing - &mVoices[0]);
        return oldestReleasing;
    }

    // ------------------------------------------------------------------------
    // Tier 4: Oldest Held Voice Stealing (LRU)
    // All voices are physically held. Steal the oldest note in the pool.
    // ------------------------------------------------------------------------
    BumblerVoice* oldestHeld = nullptr;
    uint64_t oldestHeldSample = UINT64_MAX;
    for (int i = 0; i < mMaxPolyphony; ++i) {
        auto& v = mVoices[static_cast<size_t>(i)];
        if (v.getTriggerSample() < oldestHeldSample) {
            oldestHeldSample = v.getTriggerSample();
            oldestHeld = &v;
        }
    }
    if (oldestHeld != nullptr) {
        mLastAllocatedIndex = static_cast<int>(oldestHeld - &mVoices[0]);
        return oldestHeld;
    }

    // Absolute fallback
    return &mVoices[0];
}

void BumblerVoiceManager::noteOn(int midiNote, float velocity) noexcept {
    if (velocity <= 0.0f) {
        noteOff(midiNote, 0.0f);
        return;
    }

    BumblerVoice* voice = allocateVoice(midiNote);
    if (voice != nullptr) {
        voice->setPitchBend(mPitchBendSemitones);
        voice->noteOn(midiNote, velocity, ++mNoteTriggerCounter, mAnalogMode);
    }
}

void BumblerVoiceManager::noteOff(int midiNote, float velocity) noexcept {
    for (int i = 0; i < mMaxPolyphony; ++i) {
        auto& v = mVoices[static_cast<size_t>(i)];
        if (v.isActive() && v.getMidiNote() == midiNote && v.isHeld()) {
            if (mSustainPedal) {
                v.setPedalLatched(true);
            } else {
                v.noteOff(velocity);
            }
        }
    }
}

void BumblerVoiceManager::setSustainPedal(bool pedalDown) noexcept {
    mSustainPedal = pedalDown;
    if (!mSustainPedal) {
        // Release all notes held by the sustain pedal
        for (int i = 0; i < mMaxPolyphony; ++i) {
            auto& v = mVoices[static_cast<size_t>(i)];
            if (v.isActive() && v.isPedalLatched()) {
                v.setPedalLatched(false);
                v.noteOff(0.0f);
            }
        }
    }
}

void BumblerVoiceManager::setPitchBend(float semitones) noexcept {
    mPitchBendSemitones = semitones;
    for (int i = 0; i < mMaxPolyphony; ++i) {
        mVoices[static_cast<size_t>(i)].setPitchBend(mPitchBendSemitones);
    }
}

void BumblerVoiceManager::allNotesOff(bool fastKill) noexcept {
    for (int i = 0; i < mMaxPolyphony; ++i) {
        if (fastKill) {
            mVoices[static_cast<size_t>(i)].forceKill();
        } else {
            mVoices[static_cast<size_t>(i)].noteOff(0.0f);
        }
    }
}

void BumblerVoiceManager::renderBlock(float* const* outputChannels, int numChannels, int numSamples, const ParameterSnapshot& params) noexcept {
    ScopedNoDenormals noDenormals;

    if (outputChannels == nullptr || numChannels < 1 || numSamples <= 0) return;

    mAnalogMode = (params.analogMode >= 0.5f);
    for (int i = 0; i < mMaxPolyphony; ++i) {
        mVoices[static_cast<size_t>(i)].setAnalogMode(mAnalogMode);
    }

    if (numChannels == 1) {
        float* outM = outputChannels[0];
        std::memset(outM, 0, static_cast<size_t>(numSamples) * sizeof(float));

        float* scratchR = mMonoScratchBuffer.data();
        const int safeSamples = std::min(numSamples, static_cast<int>(mMonoScratchBuffer.size()));
        std::memset(scratchR, 0, static_cast<size_t>(safeSamples) * sizeof(float));

        for (int i = 0; i < mMaxPolyphony; ++i) {
            auto& v = mVoices[static_cast<size_t>(i)];
            if (v.isActive()) {
                v.renderBlockAccumulate(outM, scratchR, safeSamples, params);
            }
        }

        mCharacterCircuits.processStereo(outM, scratchR, safeSamples, params);

        for (int s = 0; s < safeSamples; ++s) {
            outM[s] = flushDenormal(0.5f * (outM[s] + scratchR[s]));
        }

        mSampleCounter += static_cast<uint64_t>(numSamples);
        return;
    }

    float* outL = outputChannels[0];
    float* outR = outputChannels[1];

    std::memset(outL, 0, static_cast<size_t>(numSamples) * sizeof(float));
    std::memset(outR, 0, static_cast<size_t>(numSamples) * sizeof(float));

    // Render and accumulate active voices directly into the destination buffers
    for (int i = 0; i < mMaxPolyphony; ++i) {
        auto& v = mVoices[static_cast<size_t>(i)];
        if (v.isActive()) {
            v.renderBlockAccumulate(outL, outR, numSamples, params);
        }
    }

    // Process Character Circuits stereo output chain (Distortion, DualMode, DC Blocker, Master Volume)
    mCharacterCircuits.processStereo(outL, outR, numSamples, params);

    mSampleCounter += static_cast<uint64_t>(numSamples);
}

} // namespace bumbler
