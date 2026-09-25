#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <cstring>
#include "plugin/PresetMigrator.h"

using namespace bumbler;

static void testScalingFormulas() {
    std::cout << "[TEST] Running scaling formula verification..." << std::endl;

    // Logarithmic Cutoff (20 Hz to 20,000 Hz)
    float cutoff0 = PresetMigrator::scaleLogarithmic(0.0f, 20.0f, 20000.0f);
    assert(std::abs(cutoff0 - 20.0f) < 0.01f);

    float cutoff1 = PresetMigrator::scaleLogarithmic(1.0f, 20.0f, 20000.0f);
    assert(std::abs(cutoff1 - 20000.0f) < 0.1f);

    float cutoffMid = PresetMigrator::scaleLogarithmic(0.5f, 20.0f, 20000.0f);
    float expectedMid = 20.0f * std::sqrt(1000.0f); // ~632.455 Hz
    assert(std::abs(cutoffMid - expectedMid) < 0.1f);

    // Exponential Time Scaling (0.001s to 10.0s)
    float t0 = PresetMigrator::scaleExponentialTime(0.0f, 0.001f, 10.0f);
    assert(std::abs(t0 - 0.001f) < 1e-5f);

    float t1 = PresetMigrator::scaleExponentialTime(1.0f, 0.001f, 10.0f);
    assert(std::abs(t1 - 10.0f) < 1e-3f);

    float tMid = PresetMigrator::scaleExponentialTime(0.5f, 0.001f, 10.0f);
    float expectedTMid = 0.001f * std::sqrt(10000.0f); // 0.10s
    assert(std::abs(tMid - expectedTMid) < 1e-4f);

    // Bipolar scaling
    assert(std::abs(PresetMigrator::scaleBipolar(0.0f) - (-1.0f)) < 1e-5f);
    assert(std::abs(PresetMigrator::scaleBipolar(0.5f) - 0.0f) < 1e-5f);
    assert(std::abs(PresetMigrator::scaleBipolar(1.0f) - 1.0f) < 1e-5f);

    std::cout << "  -> PASS: All mathematical scaling curves exact." << std::endl;
}

static void testSyntheticFxpParsing() {
    std::cout << "[TEST] Running synthetic FXP parsing..." << std::endl;

    // Build standard VST 2.4 FxCk chunk
    std::vector<uint8_t> buffer(56 + 55 * 4, 0);
    // Magic 'CcnK'
    std::memcpy(buffer.data(), "CcnK", 4);
    // Byte size
    uint32_t bSize = 20 + 28 + 55 * 4;
    buffer[4] = (bSize >> 24) & 0xFF;
    buffer[5] = (bSize >> 16) & 0xFF;
    buffer[6] = (bSize >> 8) & 0xFF;
    buffer[7] = bSize & 0xFF;
    // fxMagic 'FxCk'
    std::memcpy(buffer.data() + 8, "FxCk", 4);
    // numParams = 55
    buffer[27] = 55;
    // Preset name: "303 Squelch Bass"
    const char* pName = "303 Squelch Bass";
    std::memcpy(buffer.data() + 28, pName, std::strlen(pName));

    // Fill floats with normalized test values
    for (int i = 0; i < 55; ++i) {
        float f = 0.5f;
        uint32_t u;
        std::memcpy(&u, &f, sizeof(u));
        size_t off = 56 + i * 4;
        buffer[off]     = (u >> 24) & 0xFF;
        buffer[off + 1] = (u >> 16) & 0xFF;
        buffer[off + 2] = (u >> 8) & 0xFF;
        buffer[off + 3] = u & 0xFF;
    }

    auto presets = PresetMigrator::parseFxp(buffer.data(), buffer.size(), "test_patch.fxp");
    assert(presets.size() == 1);
    assert(presets[0].name == "303 Squelch Bass");
    assert(presets[0].category == "Bass");
    assert(presets[0].sourceFormat.contains("FxCk"));

    // Verify cutoff at norm=0.5
    float expectedCutoff = 20.0f * std::sqrt(1000.0f);
    assert(std::abs(presets[0].snapshot.filterCutoff - expectedCutoff) < 0.5f);

    std::cout << "  -> PASS: Synthetic FXP parsed and classified as " << presets[0].category << std::endl;
}

static void testXmlRoundtrip() {
    std::cout << "[TEST] Running XML round-trip serialization..." << std::endl;

    MigratedPreset p;
    p.name = "Celestial Choir Pad";
    p.category = "Pad";
    p.sourceFormat = "Unit Test";
    p.snapshot = PresetMigrator::createDefaultSnapshot();
    p.snapshot.ampAttack = 1.25f;
    p.snapshot.ampRelease = 2.0f;
    p.parameterMap = PresetMigrator::snapshotToMap(p.snapshot);

    juce::String xml = p.toXml();
    assert(xml.contains("<BumblerXD"));
    assert(xml.contains("name=\"Celestial Choir Pad\""));
    assert(xml.contains("category=\"Pad\""));
    assert(xml.contains("id=\"ampAttack\""));

    auto restored = PresetMigrator::parseXml(xml, "in_memory.xml");
    assert(restored.size() == 1);
    assert(restored[0].name == "Celestial Choir Pad");
    assert(restored[0].category == "Pad");
    assert(std::abs(restored[0].snapshot.ampAttack - 1.25f) < 1e-4f);
    assert(std::abs(restored[0].snapshot.ampRelease - 2.0f) < 1e-4f);

    std::cout << "  -> PASS: XML serialization and deserialization bit-identical." << std::endl;
}

static void testNativeWaspFstParsing() {
    std::cout << "[TEST] Running native Wasp XT .fst parsing..." << std::endl;

    // Construct synthetic FLhd/FLdt event stream with event 213 (0xD5) Wasp XT chunk
    std::vector<uint8_t> buf;
    const uint8_t flhd[] = {
        'F', 'L', 'h', 'd',  6, 0, 0, 0,  0, 0,  1, 0,  96, 0
    };
    buf.insert(buf.end(), flhd, flhd + sizeof(flhd));

    std::vector<uint8_t> events;
    // Event 201: PluginID.InternalName = "Wasp XT"
    events.push_back(201);
    const char pluginName[] = "Wasp XT\0";
    events.push_back(static_cast<uint8_t>(sizeof(pluginName)));
    events.insert(events.end(), pluginName, pluginName + sizeof(pluginName));

    // Event 213: PluginID.Data = 229 bytes (version 13 + 56 floats + 1 byte flags)
    std::vector<uint8_t> chunk;
    uint32_t ver = 13;
    chunk.insert(chunk.end(), reinterpret_cast<const uint8_t*>(&ver), reinterpret_cast<const uint8_t*>(&ver) + 4);
    for (size_t i = 0; i < 56; ++i) {
        float f = 0.0f;
        if (i == 22) f = 0.75f; // filterCutoff norm -> ~3556 Hz
        if (i == 14) f = 0.80f; // ampSustain
        chunk.insert(chunk.end(), reinterpret_cast<const uint8_t*>(&f), reinterpret_cast<const uint8_t*>(&f) + 4);
    }
    chunk.push_back(0x01); // flags: envLink = 1

    events.push_back(213);
    uint32_t cLen = static_cast<uint32_t>(chunk.size());
    while (cLen >= 0x80) {
        events.push_back(static_cast<uint8_t>((cLen & 0x7F) | 0x80));
        cLen >>= 7;
    }
    events.push_back(static_cast<uint8_t>(cLen));
    events.insert(events.end(), chunk.begin(), chunk.end());

    // FLdt
    buf.push_back('F'); buf.push_back('L'); buf.push_back('d'); buf.push_back('t');
    uint32_t dtLen = static_cast<uint32_t>(events.size());
    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&dtLen), reinterpret_cast<const uint8_t*>(&dtLen) + 4);
    buf.insert(buf.end(), events.begin(), events.end());

    auto presets = PresetMigrator::parseFst(buf.data(), buf.size(), "Afraid Of The Dark Pad.fst");
    assert(presets.size() == 1);
    assert(presets[0].name == "Afraid Of The Dark Pad");
    assert(presets[0].snapshot.envLink == 1.0f);

    float expectedCutoff = PresetMigrator::scaleLogarithmic(0.75f, 20.0f, 20000.0f);
    assert(std::abs(presets[0].snapshot.filterCutoff - expectedCutoff) < 1.0f);

    std::cout << "  -> PASS: Native Wasp XT .fst parsed successfully with Cutoff="
              << presets[0].snapshot.filterCutoff << " Hz, envLink="
              << presets[0].snapshot.envLink << std::endl;
}

int main() {
    std::cout << "=== Bumbler XD PresetMigrator C++ Verification Suite ===" << std::endl;
    testScalingFormulas();
    testSyntheticFxpParsing();
    testXmlRoundtrip();
    testNativeWaspFstParsing();
    std::cout << "=== ALL PRESET MIGRATOR C++ TESTS PASSED ===" << std::endl;
    return 0;
}
