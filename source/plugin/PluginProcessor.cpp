#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetMigrator.h"
#include "parameters/MigratedPresets.h"

namespace bumbler {

BumblerAudioProcessor::BumblerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      mApvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Cache all 55 atomic parameter value pointers once during construction.
    // Audio thread reads directly from these pointers with std::memory_order_relaxed.
    mAtomicPointers.initialize(mApvts);
    loadPreset(0);
}

void BumblerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    mEngine.prepare(sampleRate, samplesPerBlock);
}

void BumblerAudioProcessor::releaseResources() {
    mEngine.reset();
}

bool BumblerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void BumblerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    // 1. Synchronize host tempo from DAW transport playhead
    if (auto* ph = getPlayHead()) {
        if (auto position = ph->getPosition()) {
            if (position->getBpm().hasValue()) {
                mEngine.setHostBpm(static_cast<float>(*position->getBpm()));
            }
        }
    }

    // 2. Process incoming MIDI events through engine
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            mEngine.processMidiEvent(0x90, msg.getNoteNumber(), msg.getFloatVelocity());
        } else if (msg.isNoteOff()) {
            mEngine.processMidiEvent(0x80, msg.getNoteNumber(), msg.getFloatVelocity());
        } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
            mEngine.processMidiEvent(0xB0, 123, 0.0f);
        } else if (msg.isPitchWheel()) {
            // ================================================================
            // 14-Bit MIDI Pitch Wheel Transformation:
            // Standard MIDI Pitch Bend encodes pitch deflection using two 7-bit
            // data bytes (LSB and MSB) combined into a 14-bit unsigned integer:
            //   - Domain: [0, 16383] (0x0000 to 0x3FFF)
            //   - Center: 8192 (0x2000, neutral position, no pitch bend)
            //
            // Mathematical Normalization Formula:
            //   normalized = static_cast<float>(pitchWheelValue - 8192) / 8192.0f
            // Maps the 14-bit domain [0, 16383] to bipolar float [-1.0f, +1.0f]:
            //   - Value 0     -> (0 - 8192) / 8192.0f = -1.0f (full pitch down)
            //   - Value 8192  -> (8192 - 8192) / 8192.0f =  0.0f (center / neutral)
            //   - Value 16383 -> (16383 - 8192) / 8192.0f ≈ +0.99988f (~+1.0f full up)
            //
            // Downstream Scaling (BumblerEngine::processMidiEvent):
            //   pitchBendSemitones = normalized * 2.0f
            // Maps [-1.0f, +1.0f] to standard +/-2.0 semitones pitch bend range.
            // ================================================================
            const float pitchBendVal = static_cast<float>(msg.getPitchWheelValue() - 8192) / 8192.0f;
            mEngine.processMidiEvent(0xE0, 0, pitchBendVal);
        } else if (msg.isController()) {
            if (msg.getControllerNumber() == 64) { // Sustain pedal
                mEngine.processMidiEvent(0xB0, 64, msg.getControllerValue() >= 64 ? 1.0f : 0.0f);
            } else if (msg.getControllerNumber() == 120 || msg.getControllerNumber() == 123) {
                mEngine.processMidiEvent(0xB0, msg.getControllerNumber(), 0.0f);
            }
        }
    }

    // 3. Extract lock-free parameter snapshot using cached atomic pointers (<15ns, 0 allocations, 0 lookups)
    const ParameterSnapshot snapshot = mAtomicPointers.loadSnapshot();

    // 4. Render synthesizer audio into buffer
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    float* const* channels = buffer.getArrayOfWritePointers();

    mEngine.renderBlock(channels, numChannels, numSamples, snapshot);
}

// ============================================================================
// Program / Preset Management
// ============================================================================
int BumblerAudioProcessor::getNumPrograms() {
    return static_cast<int>(getFactoryPresets().size());
}

int BumblerAudioProcessor::getCurrentProgram() {
    return mCurrentProgramIndex.load(std::memory_order_relaxed);
}

void BumblerAudioProcessor::setCurrentProgram(int index) {
    if (index >= 0 && index < getNumPrograms()) {
        loadPreset(index);
    }
}

const juce::String BumblerAudioProcessor::getProgramName(int index) {
    const auto& presets = getFactoryPresets();
    if (index >= 0 && index < static_cast<int>(presets.size())) {
        return presets[static_cast<size_t>(index)].name;
    }
    return {};
}

void BumblerAudioProcessor::changeProgramName(int, const juce::String&) {
    // Factory presets are read-only
}

void BumblerAudioProcessor::applyParameterSnapshot(const ParameterSnapshot& p) {
    auto setParam = [this](const juce::ParameterID& id, float value) {
        if (auto* param = mApvts.getParameter(id.getParamID())) {
            const auto range = mApvts.getParameterRange(id.getParamID());
            param->setValueNotifyingHost(range.convertTo0to1(value));
        }
    };

    // Oscillators
    setParam(ParamIDs::osc1Waveform,  p.osc1Waveform);
    setParam(ParamIDs::osc1Octave,    p.osc1Octave);
    setParam(ParamIDs::osc1Fine,      p.osc1Fine);
    setParam(ParamIDs::osc2Waveform,  p.osc2Waveform);
    setParam(ParamIDs::osc2Octave,    p.osc2Octave);
    setParam(ParamIDs::osc2Fine,      p.osc2Fine);
    setParam(ParamIDs::oscMix,        p.oscMix);
    setParam(ParamIDs::osc3Waveform,  p.osc3Waveform);
    setParam(ParamIDs::osc3Level,     p.osc3Level);
    setParam(ParamIDs::ringModMix,    p.ringModMix);
    setParam(ParamIDs::pulseWidth,    p.pulseWidth);
    setParam(ParamIDs::fmAmount,      p.fmAmount);
    setParam(ParamIDs::velToAmp,      p.velToAmp);
    setParam(ParamIDs::velToFilter,   p.velToFilter);

    // Filter
    setParam(ParamIDs::filterMode,      p.filterMode);
    setParam(ParamIDs::filterCutoff,    p.filterCutoff);
    setParam(ParamIDs::filterResonance, p.filterResonance);
    setParam(ParamIDs::filterKbTrack,   p.filterKbTrack);
    setParam(ParamIDs::filterEnvAmount, p.filterEnvAmount);

    // Envelopes
    setParam(ParamIDs::ampAttack,     p.ampAttack);
    setParam(ParamIDs::ampDecay,      p.ampDecay);
    setParam(ParamIDs::ampSustain,    p.ampSustain);
    setParam(ParamIDs::ampRelease,    p.ampRelease);
    setParam(ParamIDs::filterAttack,  p.filterAttack);
    setParam(ParamIDs::filterDecay,   p.filterDecay);
    setParam(ParamIDs::filterSustain, p.filterSustain);
    setParam(ParamIDs::filterRelease, p.filterRelease);
    setParam(ParamIDs::envLink,       p.envLink);
    setParam(ParamIDs::modAttack,     p.modAttack);
    setParam(ParamIDs::modDecay,      p.modDecay);
    setParam(ParamIDs::modAmount,     p.modAmount);
    setParam(ParamIDs::modTarget,     p.modTarget);

    // LFOs
    setParam(ParamIDs::lfo1Waveform, p.lfo1Waveform);
    setParam(ParamIDs::lfo1Rate,     p.lfo1Rate);
    setParam(ParamIDs::lfo1Delay,    p.lfo1Delay);
    setParam(ParamIDs::lfo1Sync,     p.lfo1Sync);
    setParam(ParamIDs::lfo1SyncDiv,  p.lfo1SyncDiv);
    setParam(ParamIDs::lfo1KeyReset, p.lfo1KeyReset);
    setParam(ParamIDs::lfo1Amount,   p.lfo1Amount);
    setParam(ParamIDs::lfo1Target,   p.lfo1Target);

    setParam(ParamIDs::lfo2Waveform, p.lfo2Waveform);
    setParam(ParamIDs::lfo2Rate,     p.lfo2Rate);
    setParam(ParamIDs::lfo2Delay,    p.lfo2Delay);
    setParam(ParamIDs::lfo2Sync,     p.lfo2Sync);
    setParam(ParamIDs::lfo2SyncDiv,  p.lfo2SyncDiv);
    setParam(ParamIDs::lfo2KeyReset, p.lfo2KeyReset);
    setParam(ParamIDs::lfo2Amount,   p.lfo2Amount);
    setParam(ParamIDs::lfo2Target,   p.lfo2Target);

    // Character & Output
    setParam(ParamIDs::driveEnabled, p.driveEnabled);
    setParam(ParamIDs::driveAmount,  p.driveAmount);
    setParam(ParamIDs::driveTone,    p.driveTone);
    setParam(ParamIDs::dualMode,     p.dualMode);
    setParam(ParamIDs::analogMode,   p.analogMode);
    setParam(ParamIDs::wNoiseMode,   p.wNoiseMode);
    setParam(ParamIDs::masterVolume, p.masterVolume);
}

void BumblerAudioProcessor::loadPreset(int index) {
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return;

    mCurrentProgramIndex.store(index, std::memory_order_relaxed);
    applyParameterSnapshot(presets[static_cast<size_t>(index)].params);
}

void BumblerAudioProcessor::loadHomagePreset(int index) {
    const auto& presets = getMigratedPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return;

    mCurrentProgramIndex.store(5 + index, std::memory_order_relaxed);
    applyParameterSnapshot(presets[static_cast<size_t>(index)].params);
}

void BumblerAudioProcessor::loadMigratedSnapshot(const ParameterSnapshot& p) {
    mCurrentProgramIndex.store(-1, std::memory_order_relaxed);
    applyParameterSnapshot(p);
}

bool BumblerAudioProcessor::loadXmlPresetFile(const juce::File& xmlFile) {
    if (!xmlFile.existsAsFile())
        return false;

    juce::String xml = xmlFile.loadFileAsString();
    auto presets = PresetMigrator::parseXml(xml, xmlFile.getFullPathName());
    if (!presets.empty()) {
        loadMigratedSnapshot(presets[0].snapshot);
        return true;
    }
    return false;
}

// ============================================================================
// State Recall & Defensive Serialization
// ============================================================================
void BumblerAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = mApvts.copyState();
    state.setProperty("schemaVersion", 1, nullptr);
    state.setProperty("programIndex", mCurrentProgramIndex.load(std::memory_order_relaxed), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        copyXmlToBinary(*xml, destData);
    }
}

void BumblerAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr) {
        // Fallback: attempt to parse as plain UTF-8 text XML
        juce::String xmlString = juce::String::createStringFromData(data, sizeInBytes);
        xmlState = juce::XmlDocument::parse(xmlString);
    }

    if (xmlState == nullptr || (!xmlState->hasTagName(mApvts.state.getType()) && !xmlState->hasTagName("BumblerXD")))
        return;

    if (xmlState->hasTagName("BumblerXD"))
        xmlState->setTagName(mApvts.state.getType());

    auto vt = juce::ValueTree::fromXml(*xmlState);
    if (!vt.isValid())
        return;

    // Defensive NaN/Inf & range sanitization on every restored parameter node
    for (int i = 0; i < vt.getNumChildren(); ++i) {
        auto child = vt.getChild(i);
        if (child.hasType("PARAM")) {
            const auto paramId = child.getProperty("id").toString();
            if (auto* rParam = mApvts.getParameter(paramId)) {
                float val = static_cast<float>(child.getProperty("value"));
                const auto range = mApvts.getParameterRange(paramId);
                if (std::isnan(val) || std::isinf(val)) {
                    val = range.convertFrom0to1(rParam->getDefaultValue());
                    child.setProperty("value", val, nullptr);
                } else if (val < range.start || val > range.end) {
                    val = juce::jlimit(range.start, range.end, val);
                    child.setProperty("value", val, nullptr);
                }
            }
        }
    }

    // Defensive check on root properties (e.g. attribute-based XML chunks)
    for (int i = 0; i < vt.getNumProperties(); ++i) {
        auto propName = vt.getPropertyName(i);
        if (propName.toString() != "schemaVersion" && propName.toString() != "programIndex") {
            if (auto* rParam = mApvts.getParameter(propName.toString())) {
                float val = static_cast<float>(vt.getProperty(propName));
                const auto range = mApvts.getParameterRange(propName.toString());
                if (std::isnan(val) || std::isinf(val)) {
                    val = range.convertFrom0to1(rParam->getDefaultValue());
                    vt.setProperty(propName, val, nullptr);
                    rParam->setValueNotifyingHost(rParam->getDefaultValue());
                } else if (val < range.start || val > range.end) {
                    val = juce::jlimit(range.start, range.end, val);
                    vt.setProperty(propName, val, nullptr);
                    rParam->setValueNotifyingHost(range.convertTo0to1(val));
                } else {
                    rParam->setValueNotifyingHost(range.convertTo0to1(val));
                }
            }
        }
    }

    if (vt.getNumChildren() > 0) {
        mApvts.replaceState(vt);
    }

    if (vt.hasProperty("programIndex")) {
        const int pIdx = static_cast<int>(vt.getProperty("programIndex"));
        if (pIdx >= 0 && pIdx < getNumPrograms()) {
            mCurrentProgramIndex.store(pIdx, std::memory_order_relaxed);
        }
    }
}

juce::AudioProcessorEditor* BumblerAudioProcessor::createEditor() {
    return new BumblerAudioProcessorEditor(*this);
}

} // namespace bumbler

// JUCE Plugin Entry Point Export
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new bumbler::BumblerAudioProcessor();
}
