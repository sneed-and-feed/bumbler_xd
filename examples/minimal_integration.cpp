#include <iostream>
#include <array>
#include <cmath>
#include <algorithm>
#include "BumblerEngine.h"
#include "ParameterSnapshot.h"
#include "parameters/PresetParameters.h"

int main() {
    // 1. Instantiate the headless C++20 DSP engine
    bumbler::BumblerEngine engine;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    // 2. Prepare engine at 48 kHz with 512 max block size
    engine.prepare(sampleRate, blockSize);
    engine.reset();

    // 3. Load a factory preset ParameterSnapshot (Preset 0: Acid Bass)
    const auto& presets = bumbler::getFactoryPresets();
    const bumbler::ParameterSnapshot params = presets[0].params;

    // 4. Trigger MIDI Note On: Note 60 (Middle C), Velocity 0.8
    engine.processMidiEvent(0x90, 60, 0.8f);

    // 5. Render 512 stereo samples into float arrays
    std::array<float, blockSize> leftChannel{};
    std::array<float, blockSize> rightChannel{};
    float* outputChannels[2] = { leftChannel.data(), rightChannel.data() };

    engine.renderBlock(outputChannels, 2, blockSize, params);

    // 6. Verify audio output (non-silent, finite samples)
    float peak = 0.0f;
    bool hasNonZero = false;
    bool allFinite = true;

    for (int i = 0; i < blockSize; ++i) {
        const float l = leftChannel[i];
        const float r = rightChannel[i];
        if (!std::isfinite(l) || !std::isfinite(r)) allFinite = false;
        if (std::abs(l) > 1e-5f || std::abs(r) > 1e-5f) hasNonZero = true;
        peak = std::max({peak, std::abs(l), std::abs(r)});
    }

    if (allFinite && hasNonZero) {
        std::cout << "[SUCCESS] BumblerEngine rendered " << blockSize 
                  << " stereo samples. Peak: " << peak << " (" << presets[0].name << ")\n";
        return 0;
    }

    std::cerr << "[FAILURE] Audio output verification failed.\n";
    return 1;
}
