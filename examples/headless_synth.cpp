#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <iomanip>

#include "BumblerEngine.h"
#include "ParameterSnapshot.h"
#include "parameters/PresetParameters.h"

// ============================================================================
// Bumbler XD — Headless Standalone CLI Synthesizer
// Clean C++20 demonstration of BumblerEngine without JUCE GUI
// ============================================================================

namespace {

#pragma pack(push, 1)
struct RiffWavHeader {
    char     riffTag[4]     {'R', 'I', 'F', 'F'};
    uint32_t fileSizeMinus8 {0};
    char     waveTag[4]     {'W', 'A', 'V', 'E'};

    // "fmt " Sub-chunk
    char     fmtTag[4]      {'f', 'm', 't', ' '};
    uint32_t fmtLength      {16};             // 16 bytes for standard PCM
    uint16_t audioFormat    {1};              // 1 = Linear PCM
    uint16_t numChannels    {2};              // 2 = Stereo
    uint32_t sampleRate     {44100};
    uint32_t byteRate       {44100 * 2 * 2};  // sampleRate * numChannels * bitsPerSample / 8
    uint16_t blockAlign     {4};              // numChannels * bitsPerSample / 8
    uint16_t bitsPerSample  {16};             // 16-bit signed PCM

    // "data" Sub-chunk
    char     dataTag[4]     {'d', 'a', 't', 'a'};
    uint32_t dataBytes      {0};
};
#pragma pack(pop)

bool writeWavFile(const std::string& filename,
                  const float* left,
                  const float* right,
                  uint32_t numSamples,
                  uint32_t sampleRate) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file for writing: " << filename << "\n";
        return false;
    }

    RiffWavHeader header;
    header.sampleRate = sampleRate;
    header.numChannels = 2;
    header.bitsPerSample = 16;
    header.blockAlign = static_cast<uint16_t>(header.numChannels * (header.bitsPerSample / 8));
    header.byteRate = header.sampleRate * header.blockAlign;
    header.dataBytes = numSamples * header.blockAlign;
    header.fileSizeMinus8 = 36 + header.dataBytes;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    std::vector<int16_t> pcmInterleaved(numSamples * 2);
    for (uint32_t i = 0; i < numSamples; ++i) {
        const float sampleL = std::clamp(left[i], -1.0f, 1.0f);
        const float sampleR = std::clamp(right[i], -1.0f, 1.0f);

        pcmInterleaved[i * 2]     = static_cast<int16_t>(std::round(sampleL * 32767.0f));
        pcmInterleaved[i * 2 + 1] = static_cast<int16_t>(std::round(sampleR * 32767.0f));
    }

    file.write(reinterpret_cast<const char*>(pcmInterleaved.data()),
               static_cast<std::streamsize>(pcmInterleaved.size() * sizeof(int16_t)));

    return file.good();
}

void printUsage(const char* progName) {
    std::cout << "======================================================================\n";
    std::cout << "       BUMBLER XD : HEADLESS SYNTHESIZER CLI EXAMPLE (JUCE-LESS)      \n";
    std::cout << "======================================================================\n";
    std::cout << "Usage: " << progName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --preset <0-4>        Factory preset index (default: 0)\n";
    std::cout << "  --note <0-127>        MIDI note number (default: 60 = Middle C)\n";
    std::cout << "  --duration <seconds>  Audio render duration in seconds (default: 2.0)\n";
    std::cout << "  --output <file.wav>   Destination WAV file path (default: 'output.wav')\n";
    std::cout << "  --help, -h            Show this help text\n\n";
    std::cout << "Available Factory Presets:\n";
    const auto& presets = bumbler::getFactoryPresets();
    for (size_t i = 0; i < presets.size(); ++i) {
        std::cout << "  [" << i << "] " << presets[i].name << " (" << presets[i].category << ")\n";
    }
    std::cout << "======================================================================\n";
}

} // namespace

int main(int argc, char* argv[]) {
    int presetIndex = 0;
    int noteNumber = 60;
    double durationSec = 2.0;
    std::string outputPath = "output.wav";

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--preset" && i + 1 < argc) {
            presetIndex = std::atoi(argv[++i]);
        } else if (arg == "--note" && i + 1 < argc) {
            noteNumber = std::atoi(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            durationSec = std::atof(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            outputPath = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Validate parameters
    const auto& presets = bumbler::getFactoryPresets();
    if (presetIndex < 0 || presetIndex >= static_cast<int>(presets.size())) {
        std::cerr << "Error: Invalid preset index " << presetIndex << ". Must be between 0 and "
                  << (presets.size() - 1) << ".\n";
        return 1;
    }

    if (noteNumber < 0 || noteNumber > 127) {
        std::cerr << "Error: Invalid MIDI note " << noteNumber << ". Must be between 0 and 127.\n";
        return 1;
    }

    if (durationSec <= 0.01 || durationSec > 600.0) {
        std::cerr << "Error: Invalid duration " << durationSec << " seconds. Must be between 0.01 and 600.0.\n";
        return 1;
    }

    const auto& selectedPreset = presets[static_cast<size_t>(presetIndex)];
    const bumbler::ParameterSnapshot params = selectedPreset.params;

    constexpr double kSampleRate = 44100.0;
    constexpr int kBlockSize = 512;

    std::cout << "----------------------------------------------------------------------\n";
    std::cout << "Rendering Bumbler XD Audio Block:\n";
    std::cout << "  Preset:      [" << presetIndex << "] " << selectedPreset.name 
              << " (" << selectedPreset.category << ")\n";
    std::cout << "  MIDI Note:   " << noteNumber << "\n";
    std::cout << "  Duration:    " << durationSec << " seconds\n";
    std::cout << "  Sample Rate: " << static_cast<int>(kSampleRate) << " Hz\n";
    std::cout << "  Output File: " << outputPath << "\n";
    std::cout << "----------------------------------------------------------------------\n";

    // Initialize BumblerEngine (Pure headless C++20 DSP engine)
    bumbler::BumblerEngine engine;
    engine.prepare(kSampleRate, kBlockSize);
    engine.reset();

    // Determine timing for noteOn and release tail
    const uint32_t totalSamples = static_cast<uint32_t>(durationSec * kSampleRate);
    const double noteOnDuration = (durationSec > 0.5) ? std::max(0.1, durationSec - 0.5) : (durationSec * 0.75);
    const uint32_t noteOffSample = static_cast<uint32_t>(noteOnDuration * kSampleRate);

    std::vector<float> leftChannel(totalSamples, 0.0f);
    std::vector<float> rightChannel(totalSamples, 0.0f);

    std::vector<float> blockL(kBlockSize, 0.0f);
    std::vector<float> blockR(kBlockSize, 0.0f);
    float* channels[2] = { blockL.data(), blockR.data() };

    // Trigger initial Note On event (velocity = 0.85)
    engine.processMidiEvent(0x90, noteNumber, 0.85f);
    bool noteIsActive = true;

    uint32_t currentSample = 0;
    while (currentSample < totalSamples) {
        const uint32_t remaining = totalSamples - currentSample;
        const int samplesToProcess = static_cast<int>(std::min(static_cast<uint32_t>(kBlockSize), remaining));

        // Trigger Note Off when the gate period finishes to capture the release tail
        if (noteIsActive && (currentSample + static_cast<uint32_t>(samplesToProcess) >= noteOffSample)) {
            engine.processMidiEvent(0x80, noteNumber, 0.0f);
            noteIsActive = false;
        }

        std::fill(blockL.begin(), blockL.end(), 0.0f);
        std::fill(blockR.begin(), blockR.end(), 0.0f);

        engine.renderBlock(channels, 2, samplesToProcess, params);

        for (int i = 0; i < samplesToProcess; ++i) {
            leftChannel[currentSample + i]  = blockL[static_cast<size_t>(i)];
            rightChannel[currentSample + i] = blockR[static_cast<size_t>(i)];
        }

        currentSample += static_cast<uint32_t>(samplesToProcess);
    }

    // Compute basic audio metrics (Peak and RMS)
    float peak = 0.0f;
    double sumSq = 0.0;
    for (uint32_t i = 0; i < totalSamples; ++i) {
        const float absL = std::abs(leftChannel[i]);
        const float absR = std::abs(rightChannel[i]);
        if (absL > peak) peak = absL;
        if (absR > peak) peak = absR;
        sumSq += static_cast<double>(absL) * static_cast<double>(absL);
        sumSq += static_cast<double>(absR) * static_cast<double>(absR);
    }
    const double rms = std::sqrt(sumSq / (totalSamples * 2));
    const double peakDb = (peak > 1.0e-9f) ? (20.0 * std::log10(peak)) : -180.0;
    const double rmsDb  = (rms > 1.0e-9)   ? (20.0 * std::log10(rms))  : -180.0;

    std::cout << "Rendering Complete:\n";
    std::cout << "  Rendered Samples: " << totalSamples << " per channel\n";
    std::cout << "  Peak Amplitude:   " << std::fixed << std::setprecision(4) << peak 
              << " (" << std::setprecision(2) << peakDb << " dBFS)\n";
    std::cout << "  RMS Energy:       " << std::setprecision(4) << rms 
              << " (" << std::setprecision(2) << rmsDb << " dBFS)\n";

    // Write to standard 16-bit PCM stereo WAV file
    if (!writeWavFile(outputPath, leftChannel.data(), rightChannel.data(), totalSamples, static_cast<uint32_t>(kSampleRate))) {
        std::cerr << "Failed to write WAV file to: " << outputPath << "\n";
        return 1;
    }

    std::cout << "Successfully exported audio to: " << outputPath << "\n";
    return 0;
}
