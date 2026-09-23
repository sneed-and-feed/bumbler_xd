#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace bumbler {

BumblerAudioProcessorEditor::BumblerAudioProcessorEditor(BumblerAudioProcessor& p)
    : AudioProcessorEditor(&p), mProcessor(p)
{
    setLookAndFeel(&mLookAndFeel);

    setupUI();

    // Attach OSC mix fader
    mOscMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        mProcessor.getApvts(), ParamIDs::oscMix.getParamID(), mOscMixFader.getSlider());
    addAndMakeVisible(mOscMixFader);

    // Add and make visible LCD displays
    addAndMakeVisible(mFilterLcdDisplay);
    addAndMakeVisible(mAmpLcdDisplay);

    // Register APVTS parameter listeners for live LCD updates
    auto& apvts = mProcessor.getApvts();
    apvts.addParameterListener("filterAttack", this);
    apvts.addParameterListener("filterDecay", this);
    apvts.addParameterListener("filterSustain", this);
    apvts.addParameterListener("filterRelease", this);

    apvts.addParameterListener("filterEnvAmount", this);
    apvts.addParameterListener("envLink", this);

    apvts.addParameterListener("ampAttack", this);
    apvts.addParameterListener("ampDecay", this);
    apvts.addParameterListener("ampSustain", this);
    apvts.addParameterListener("ampRelease", this);

    // Initial LCD curve build
    updateLcdDisplays();

    // Listen to all mouse events on child components
    addMouseListener(this, true);

    // Standard rack-mount default size
    setSize(1120, 700);
    setResizable(true, true);
    setResizeLimits(960, 600, 1920, 1200);
}

BumblerAudioProcessorEditor::~BumblerAudioProcessorEditor() {
    removeMouseListener(this);
    setLookAndFeel(nullptr);

    // 1. Unregister APVTS parameter listeners
    auto& apvts = mProcessor.getApvts();
    apvts.removeParameterListener("filterAttack", this);
    apvts.removeParameterListener("filterDecay", this);
    apvts.removeParameterListener("filterSustain", this);
    apvts.removeParameterListener("filterRelease", this);

    apvts.removeParameterListener("filterEnvAmount", this);
    apvts.removeParameterListener("envLink", this);

    apvts.removeParameterListener("ampAttack", this);
    apvts.removeParameterListener("ampDecay", this);
    apvts.removeParameterListener("ampSustain", this);
    apvts.removeParameterListener("ampRelease", this);

    // 2. Explicitly tear down all attachments BEFORE controls are destroyed (RAII invariant)
    mOscMixAttachment.reset();
    for (auto& k : mKnobs)   k->attachment.reset();
    for (auto& b : mButtons) b->attachment.reset();
    for (auto& c : mCombos)  c->attachment.reset();

    // 3. Clear component vectors
    mKnobs.clear();
    mButtons.clear();
    mCombos.clear();
}

void BumblerAudioProcessorEditor::parameterChanged(const juce::String&, float) {
    // Coalesce updates to message thread with SafePointer to prevent use-after-free on editor close
    juce::Component::SafePointer<BumblerAudioProcessorEditor> safeThis(this);
    juce::MessageManager::callAsync([safeThis]() {
        if (safeThis != nullptr)
            safeThis->updateLcdDisplays();
    });
}

void BumblerAudioProcessorEditor::updateLcdDisplays() {
    auto& apvts = mProcessor.getApvts();
    auto getVal = [&apvts](const char* id) -> float {
        if (auto* p = apvts.getRawParameterValue(id)) {
            return p->load(std::memory_order_relaxed);
        }
        return 0.0f;
    };

    const float envAmt = getVal("filterEnvAmount");
    const bool isLinked = (getVal("envLink") > 0.5f);
    mFilterLcdDisplay.setFilterModulationStatus(envAmt, isLinked);
    if (isLinked) {
        // When linked, filter envelope curve mirrors amp envelope
        mFilterLcdDisplay.setEnvelopeParameters(
            getVal("ampAttack"),
            getVal("ampDecay"),
            getVal("ampSustain"),
            getVal("ampRelease")
        );
    } else {
        mFilterLcdDisplay.setEnvelopeParameters(
            getVal("filterAttack"),
            getVal("filterDecay"),
            getVal("filterSustain"),
            getVal("filterRelease")
        );
    }

    mAmpLcdDisplay.setEnvelopeParameters(
        getVal("ampAttack"),
        getVal("ampDecay"),
        getVal("ampSustain"),
        getVal("ampRelease")
    );
}

void BumblerAudioProcessorEditor::addKnob(const juce::ParameterID& paramId, const char* name,
                                         juce::Slider::SliderStyle style, const char* suffix)
{
    auto slot = std::make_unique<KnobSlot>();
    slot->paramId = paramId.getParamID();

    slot->slider.setSliderStyle(style);
    slot->slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    if (suffix[0] != '\0') {
        slot->slider.setTextValueSuffix(suffix);
    }
    addAndMakeVisible(slot->slider);

    slot->nameLabel.setText(name, juce::dontSendNotification);
    slot->nameLabel.setJustificationType(juce::Justification::centred);
    slot->nameLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    slot->nameLabel.setColour(juce::Label::textColourId, juce::Colour(BumblerColours::SilkWhite));
    addAndMakeVisible(slot->nameLabel);

    slot->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        mProcessor.getApvts(), slot->paramId, slot->slider);

    mKnobs.push_back(std::move(slot));
}

void BumblerAudioProcessorEditor::addButton(const juce::ParameterID& paramId, const char* text) {
    auto slot = std::make_unique<ButtonSlot>();
    slot->paramId = paramId.getParamID();

    slot->button.setButtonText(text);
    slot->button.setName(slot->paramId);
    addAndMakeVisible(slot->button);

    slot->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        mProcessor.getApvts(), slot->paramId, slot->button);

    mButtons.push_back(std::move(slot));
}

void BumblerAudioProcessorEditor::addCombo(const juce::ParameterID& paramId, const char* labelText, const juce::StringArray& choices) {
    auto slot = std::make_unique<ComboSlot>();
    slot->paramId = paramId.getParamID();

    slot->label.setText(labelText, juce::dontSendNotification);
    slot->label.setJustificationType(juce::Justification::centredLeft);
    slot->label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    slot->label.setColour(juce::Label::textColourId, juce::Colour(BumblerColours::SilkWhite));
    addAndMakeVisible(slot->label);

    slot->comboBox.addItemList(choices, 1);
    addAndMakeVisible(slot->comboBox);

    slot->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mProcessor.getApvts(), slot->paramId, slot->comboBox);

    mCombos.push_back(std::move(slot));
}

BumblerAudioProcessorEditor::KnobSlot* BumblerAudioProcessorEditor::findKnob(const juce::String& paramId) {
    for (auto& k : mKnobs) {
        if (k->paramId == paramId) return k.get();
    }
    return nullptr;
}

BumblerAudioProcessorEditor::ButtonSlot* BumblerAudioProcessorEditor::findButton(const juce::String& paramId) {
    for (auto& b : mButtons) {
        if (b->paramId == paramId) return b.get();
    }
    return nullptr;
}

BumblerAudioProcessorEditor::ComboSlot* BumblerAudioProcessorEditor::findCombo(const juce::String& paramId) {
    for (auto& c : mCombos) {
        if (c->paramId == paramId) return c.get();
    }
    return nullptr;
}

void BumblerAudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        // Check if clicked component is within mOscMixFader
        if (e.eventComponent == &mOscMixFader || mOscMixFader.isParentOf(e.eventComponent)
            || e.eventComponent == &mOscMixFader.getSlider() || mOscMixFader.getSlider().isParentOf(e.eventComponent)) {
            showFaderContextMenu(e.getScreenPosition());
            return;
        }
        // Check if clicked component is within any KnobSlot
        for (auto& slot : mKnobs) {
            if (e.eventComponent == &slot->slider || slot->slider.isParentOf(e.eventComponent)
                || e.eventComponent == &slot->nameLabel || slot->nameLabel.isParentOf(e.eventComponent)) {
                showKnobContextMenu(*slot, e.getScreenPosition());
                return;
            }
        }
    }
}

void BumblerAudioProcessorEditor::showKnobContextMenu(KnobSlot& slot, juce::Point<int> screenPos) {
    auto* param = mProcessor.getApvts().getParameter(slot.paramId);
    if (param != nullptr) {
        if (auto* hContext = getHostContext()) {
            if (auto hostMenu = hContext->getContextMenuForParameter(param)) {
                const auto localPos = getLocalPoint(nullptr, screenPos);
                hostMenu->showNativeMenu(localPos);
                return;
            }
        }
    }

    auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param);

    juce::PopupMenu menu;
    const juce::String currentValueStr = slot.slider.getTextFromValue(slot.slider.getValue());
    const juce::String headerText = (slot.nameLabel.getText().isNotEmpty() ? slot.nameLabel.getText() : slot.paramId)
                                  + "  (" + currentValueStr + ")";
    menu.addSectionHeader(headerText);
    menu.addSeparator();

    juce::String defaultText;
    double defaultDenormVal = 0.0;
    if (rangedParam != nullptr) {
        defaultDenormVal = static_cast<double>(rangedParam->getNormalisableRange().convertFrom0to1(rangedParam->getDefaultValue()));
        defaultText = rangedParam->getText(rangedParam->getDefaultValue(), 1024);
        if (defaultText.isEmpty()) {
            defaultText = slot.slider.getTextFromValue(defaultDenormVal);
        }
    } else {
        defaultDenormVal = slot.slider.getMinimum();
        defaultText = slot.slider.getTextFromValue(defaultDenormVal);
    }

    menu.addItem(1, "Reset to Default (" + defaultText + ")");
    menu.addItem(2, "Set to Minimum (" + slot.slider.getTextFromValue(slot.slider.getMinimum()) + ")");
    menu.addItem(3, "Set to Maximum (" + slot.slider.getTextFromValue(slot.slider.getMaximum()) + ")");
    menu.addSeparator();
    menu.addItem(4, "Set to Exact Value...");

    juce::Component::SafePointer<BumblerAudioProcessorEditor> safeThis(this);
    const juce::String paramId = slot.paramId;
    const juce::String paramName = slot.nameLabel.getText().isNotEmpty() ? slot.nameLabel.getText() : slot.paramId;

    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1))
            .withTargetComponent(&slot.slider)
            .withParentComponent(this),
        [safeThis, paramId, paramName, defaultDenormVal](int result) {
            if (safeThis == nullptr || result <= 0) {
                return;
            }

            auto* currentSlot = safeThis->findKnob(paramId);
            if (currentSlot == nullptr) {
                return;
            }

            if (result == 1) {
                currentSlot->slider.setValue(defaultDenormVal, juce::sendNotificationSync);
            } else if (result == 2) {
                currentSlot->slider.setValue(currentSlot->slider.getMinimum(), juce::sendNotificationSync);
            } else if (result == 3) {
                currentSlot->slider.setValue(currentSlot->slider.getMaximum(), juce::sendNotificationSync);
            } else if (result == 4) {
                auto* alert = new juce::AlertWindow("Set Exact Value",
                                                    "Enter value for " + paramName + ":",
                                                    juce::AlertWindow::NoIcon,
                                                    safeThis.getComponent());
                alert->addTextEditor("val", currentSlot->slider.getTextFromValue(currentSlot->slider.getValue()), "Value:");
                alert->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

                if (auto* te = alert->getTextEditor("val")) {
                    te->selectAll();
                }

                alert->enterModalState(true, juce::ModalCallbackFunction::create(
                    [safeThis, alert, paramId](int modalResult) {
                        if (safeThis == nullptr || modalResult != 1) {
                            return;
                        }

                        auto* s = safeThis->findKnob(paramId);
                        if (s == nullptr) {
                            return;
                        }

                        const juce::String entered = alert->getTextEditorContents("val");
                        const double val = s->slider.getValueFromText(entered);
                        s->slider.setValue(val, juce::sendNotificationSync);
                    }), true);
            }
        });
}

void BumblerAudioProcessorEditor::showFaderContextMenu(juce::Point<int> screenPos) {
    auto* param = mProcessor.getApvts().getParameter(ParamIDs::oscMix.getParamID());
    if (param != nullptr) {
        if (auto* hContext = getHostContext()) {
            if (auto hostMenu = hContext->getContextMenuForParameter(param)) {
                const auto localPos = getLocalPoint(nullptr, screenPos);
                hostMenu->showNativeMenu(localPos);
                return;
            }
        }
    }

    auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param);

    juce::PopupMenu menu;
    auto& slider = mOscMixFader.getSlider();
    const juce::String currentValueStr = slider.getTextFromValue(slider.getValue());
    const juce::String headerText = "OSC Mix  (" + currentValueStr + ")";
    menu.addSectionHeader(headerText);
    menu.addSeparator();

    juce::String defaultText;
    double defaultDenormVal = 0.5;
    if (rangedParam != nullptr) {
        defaultDenormVal = static_cast<double>(rangedParam->getNormalisableRange().convertFrom0to1(rangedParam->getDefaultValue()));
        defaultText = rangedParam->getText(rangedParam->getDefaultValue(), 1024);
        if (defaultText.isEmpty()) {
            defaultText = slider.getTextFromValue(defaultDenormVal);
        }
    } else {
        defaultDenormVal = slider.getMinimum();
        defaultText = slider.getTextFromValue(defaultDenormVal);
    }

    menu.addItem(1, "Reset to Default (" + defaultText + ")");
    menu.addItem(2, "Set to Minimum (" + slider.getTextFromValue(slider.getMinimum()) + ")");
    menu.addItem(3, "Set to Maximum (" + slider.getTextFromValue(slider.getMaximum()) + ")");
    menu.addSeparator();
    menu.addItem(4, "Set to Exact Value...");

    juce::Component::SafePointer<BumblerAudioProcessorEditor> safeThis(this);

    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1))
            .withTargetComponent(&slider)
            .withParentComponent(this),
        [safeThis, defaultDenormVal](int result) {
            if (safeThis == nullptr || result <= 0) {
                return;
            }

            auto& s = safeThis->mOscMixFader.getSlider();

            if (result == 1) {
                s.setValue(defaultDenormVal, juce::sendNotificationSync);
            } else if (result == 2) {
                s.setValue(s.getMinimum(), juce::sendNotificationSync);
            } else if (result == 3) {
                s.setValue(s.getMaximum(), juce::sendNotificationSync);
            } else if (result == 4) {
                auto* alert = new juce::AlertWindow("Set Exact Value",
                                                    "Enter value for OSC Mix:",
                                                    juce::AlertWindow::NoIcon,
                                                    safeThis.getComponent());
                alert->addTextEditor("val", s.getTextFromValue(s.getValue()), "Value:");
                alert->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

                if (auto* te = alert->getTextEditor("val")) {
                    te->selectAll();
                }

                alert->enterModalState(true, juce::ModalCallbackFunction::create(
                    [safeThis, alert](int modalResult) {
                        if (safeThis == nullptr || modalResult != 1) {
                            return;
                        }

                        auto& faderSlider = safeThis->mOscMixFader.getSlider();
                        const juce::String entered = alert->getTextEditorContents("val");
                        const double val = faderSlider.getValueFromText(entered);
                        faderSlider.setValue(val, juce::sendNotificationSync);
                    }), true);
            }
        });
}

void BumblerAudioProcessorEditor::setupUI() {
    // Presets ComboBox
    mPresetLabel.setText("PRESET:", juce::dontSendNotification);
    mPresetLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    mPresetLabel.setColour(juce::Label::textColourId, juce::Colour(BumblerColours::WaspYellow));
    addAndMakeVisible(mPresetLabel);

    for (int i = 0; i < mProcessor.getNumPrograms(); ++i) {
        mPresetComboBox.addItem(mProcessor.getProgramName(i), i + 1);
    }
    mPresetComboBox.setSelectedId(mProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    mPresetComboBox.onChange = [this]() {
        const int selected = mPresetComboBox.getSelectedId() - 1;
        if (selected >= 0 && selected < mProcessor.getNumPrograms()) {
            mProcessor.setCurrentProgram(selected);
            updateLcdDisplays();
        }
    };
    addAndMakeVisible(mPresetComboBox);

    // 1. Oscillators Section (14 params)
    addCombo(ParamIDs::osc1Waveform, "OSC 1", getOscWaveformChoices());
    addKnob(ParamIDs::osc1Octave, "OCT", juce::Slider::RotaryHorizontalVerticalDrag, " oct");
    addKnob(ParamIDs::osc1Fine, "FINE", juce::Slider::RotaryHorizontalVerticalDrag, " st");

    addCombo(ParamIDs::osc2Waveform, "OSC 2", getOscWaveformChoices());
    addKnob(ParamIDs::osc2Octave, "OCT", juce::Slider::RotaryHorizontalVerticalDrag, " oct");
    addKnob(ParamIDs::osc2Fine, "FINE", juce::Slider::RotaryHorizontalVerticalDrag, " st");

    addCombo(ParamIDs::osc3Waveform, "AUX SUB", getOsc3WaveformChoices());
    addKnob(ParamIDs::osc3Level, "SUB LVL");

    addKnob(ParamIDs::ringModMix, "RING MOD");
    addKnob(ParamIDs::pulseWidth, "PW");
    addKnob(ParamIDs::fmAmount, "FM");
    addKnob(ParamIDs::velToAmp, "VEL>AMP");
    addKnob(ParamIDs::velToFilter, "VEL>FLT");

    // 2. Filter Section (5 params)
    addCombo(ParamIDs::filterMode, "MODE", getFilterModeChoices());
    addKnob(ParamIDs::filterCutoff, "CUTOFF", juce::Slider::RotaryHorizontalVerticalDrag, " Hz");
    addKnob(ParamIDs::filterResonance, "RESO");
    addKnob(ParamIDs::filterKbTrack, "KB TRK");
    addKnob(ParamIDs::filterEnvAmount, "ENV AMT");

    // 3. Envelopes (13 params)
    addKnob(ParamIDs::filterAttack, "A (FLT)", juce::Slider::RotaryHorizontalVerticalDrag, " s");
    addKnob(ParamIDs::filterDecay, "D (FLT)", juce::Slider::RotaryHorizontalVerticalDrag, " s");
    addKnob(ParamIDs::filterSustain, "S (FLT)");
    addKnob(ParamIDs::filterRelease, "R (FLT)", juce::Slider::RotaryHorizontalVerticalDrag, " s");

    addButton(ParamIDs::envLink, "LINK ENVS");

    addKnob(ParamIDs::ampAttack, "A (AMP)", juce::Slider::RotaryHorizontalVerticalDrag, " s");
    addKnob(ParamIDs::ampDecay, "D (AMP)", juce::Slider::RotaryHorizontalVerticalDrag, " s");
    addKnob(ParamIDs::ampSustain, "S (AMP)");
    addKnob(ParamIDs::ampRelease, "R (AMP)", juce::Slider::RotaryHorizontalVerticalDrag, " s");

    addKnob(ParamIDs::modAttack, "A (MOD)", juce::Slider::RotaryHorizontalVerticalDrag, " s");
    addKnob(ParamIDs::modDecay, "D (MOD)", juce::Slider::RotaryHorizontalVerticalDrag, " s");
    addKnob(ParamIDs::modAmount, "MOD AMT");
    addCombo(ParamIDs::modTarget, "TARGET", getModTargetChoices());

    // 4. Dual LFOs (16 params)
    addCombo(ParamIDs::lfo1Waveform, "LFO 1", getLfoWaveformChoices());
    addKnob(ParamIDs::lfo1Rate, "RATE", juce::Slider::RotaryHorizontalVerticalDrag, " Hz");
    addKnob(ParamIDs::lfo1Delay, "DELAY", juce::Slider::RotaryHorizontalVerticalDrag, " s");
    addButton(ParamIDs::lfo1Sync, "SYNC");
    addCombo(ParamIDs::lfo1SyncDiv, "DIV", getLfoSyncDivChoices());
    addButton(ParamIDs::lfo1KeyReset, "RESET");
    addKnob(ParamIDs::lfo1Amount, "AMOUNT");
    addCombo(ParamIDs::lfo1Target, "DEST", getLfo1TargetChoices());

    addCombo(ParamIDs::lfo2Waveform, "LFO 2", getLfoWaveformChoices());
    addKnob(ParamIDs::lfo2Rate, "RATE", juce::Slider::RotaryHorizontalVerticalDrag, " Hz");
    addKnob(ParamIDs::lfo2Delay, "DELAY", juce::Slider::RotaryHorizontalVerticalDrag, " s");
    addButton(ParamIDs::lfo2Sync, "SYNC");
    addCombo(ParamIDs::lfo2SyncDiv, "DIV", getLfoSyncDivChoices());
    addButton(ParamIDs::lfo2KeyReset, "RESET");
    addKnob(ParamIDs::lfo2Amount, "AMOUNT");
    addCombo(ParamIDs::lfo2Target, "DEST", getLfo2TargetChoices());

    // 5. Character & Output (7 params)
    addButton(ParamIDs::driveEnabled, "DRIVE");
    addKnob(ParamIDs::driveAmount, "DRIVE AMT");
    addKnob(ParamIDs::driveTone, "TONE");
    addButton(ParamIDs::dualMode, "DUAL");
    addButton(ParamIDs::analogMode, "ANALOG");
    addCombo(ParamIDs::wNoiseMode, "NOISE", getNoiseModeChoices());
    addKnob(ParamIDs::masterVolume, "VOLUME");
}

void BumblerAudioProcessorEditor::paint(juce::Graphics& g) {
    // 1. Chassis Background (Dark Midnight Slate)
    g.fillAll(juce::Colour(BumblerColours::ChassisDark));

    // 2. Header Panel
    auto headerArea = juce::Rectangle<float>(10.0f, 10.0f, static_cast<float>(getWidth() - 20), 50.0f);
    g.setColour(juce::Colour(BumblerColours::ChassisPanel));
    g.fillRoundedRectangle(headerArea, 4.0f);
    g.setColour(juce::Colour(BumblerColours::BezelOutline));
    g.drawRoundedRectangle(headerArea, 4.0f, 1.0f);

    // Logo & Subtitle
    g.setColour(juce::Colour(BumblerColours::WaspYellow));
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("BUMBLER XD", 25, 12, 170, 26, juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    g.setColour(juce::Colour(BumblerColours::TextMuted));
    g.drawText("VINTAGE WASP XT HOMAGE | JUCE 8", 25, 38, 220, 16, juce::Justification::centredLeft);

    // 3. Section Decks Outline & Titles
    auto drawDeck = [&](juce::Rectangle<int> r, const char* title) {
        auto rf = r.toFloat();
        g.setColour(juce::Colour(BumblerColours::ChassisPanel));
        g.fillRoundedRectangle(rf, 4.0f);
        g.setColour(juce::Colour(BumblerColours::BezelOutline));
        g.drawRoundedRectangle(rf, 4.0f, 1.0f);

        // Header strip
        auto titleArea = rf.removeFromTop(20.0f);
        g.setColour(juce::Colour(BumblerColours::PanelInset));
        g.fillRoundedRectangle(titleArea, 4.0f);

        g.setColour(juce::Colour(BumblerColours::WaspYellow));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, titleArea.reduced(8.0f, 0.0f), juce::Justification::centredLeft);
    };

    const int totalW = getWidth();
    const int totalH = getHeight();
    const int pad = 10;
    const int headerH = 65;
    const int contentH = totalH - headerH - pad;
    const int topDeckH = static_cast<int>(contentH * 0.48f);
    const int botDeckH = contentH - topDeckH - pad;

    const int leftTopW = static_cast<int>((totalW - 3 * pad) * 0.63f);
    const int rightTopW = totalW - leftTopW - 3 * pad;

    const int leftBotW = static_cast<int>((totalW - 3 * pad) * 0.50f);
    const int rightBotW = totalW - leftBotW - 3 * pad;

    // Deck 1: Oscillators & Voice Management
    drawDeck({ pad, headerH, leftTopW, topDeckH }, "OSCILLATORS & MIXER");

    // Deck 2: 6-Mode Filter
    drawDeck({ pad * 2 + leftTopW, headerH, rightTopW, topDeckH }, "6-MODE WASP FILTER");

    // Deck 3: Dual LFOs & Mod Env
    drawDeck({ pad, headerH + topDeckH + pad, leftBotW, botDeckH }, "DUAL LFOS & MOD ENV");

    // Deck 4: Envelopes & Vector Displays
    drawDeck({ pad * 2 + leftBotW, headerH + topDeckH + pad, rightBotW, botDeckH }, "ENVELOPES & VECTOR LCD");
}

void BumblerAudioProcessorEditor::resized() {
    const int totalW = getWidth();
    const int totalH = getHeight();
    const int pad = 10;

    // Header Controls Layout (Y = 10 to 60)
    mPresetLabel.setBounds(200, 24, 60, 22);
    mPresetComboBox.setBounds(265, 24, 120, 22);

    if (auto* b = findButton("driveEnabled")) b->button.setBounds(400, 23, 75, 24);
    if (auto* k = findKnob("driveAmount")) {
        k->slider.setBounds(480, 14, 38, 38);
        k->nameLabel.setBounds(474, 48, 50, 12);
    }
    if (auto* k = findKnob("driveTone")) {
        k->slider.setBounds(535, 14, 38, 38);
        k->nameLabel.setBounds(529, 48, 50, 12);
    }

    if (auto* b = findButton("dualMode"))   b->button.setBounds(595, 23, 70, 24);
    if (auto* b = findButton("analogMode")) b->button.setBounds(670, 23, 80, 24);

    if (auto* c = findCombo("wNoiseMode")) {
        c->label.setBounds(760, 14, 60, 14);
        c->comboBox.setBounds(760, 30, 95, 22);
    }

    if (auto* k = findKnob("masterVolume")) {
        k->slider.setBounds(totalW - 140, 12, 42, 42);
        k->nameLabel.setBounds(totalW - 146, 50, 54, 12);
    }

    // Geometry partitions
    const int headerH = 65;
    const int contentH = totalH - headerH - pad;
    const int topDeckH = static_cast<int>(contentH * 0.48f);
    const int botDeckH = contentH - topDeckH - pad;

    const int leftTopW = static_cast<int>((totalW - 3 * pad) * 0.63f);
    const int rightTopW = totalW - leftTopW - 3 * pad;

    const int leftBotW = static_cast<int>((totalW - 3 * pad) * 0.50f);
    const int rightBotW = totalW - leftBotW - 3 * pad;

    // ------------------------------------------------------------------------
    // Deck 1: Oscillators & Voice Management
    // ------------------------------------------------------------------------
    auto deck1 = juce::Rectangle<int>(pad, headerH + 24, leftTopW, topDeckH - 28).reduced(8, 4);

    // Row 1: OSC 1 & OSC 2
    auto oscRow = deck1.removeFromTop(44);
    if (auto* c = findCombo("osc1Waveform")) {
        c->label.setBounds(oscRow.getX(), oscRow.getY(), 45, 14);
        c->comboBox.setBounds(oscRow.getX() + 45, oscRow.getY() + 2, 75, 20);
    }
    if (auto* k = findKnob("osc1Octave")) {
        k->slider.setBounds(oscRow.getX() + 128, oscRow.getY() - 4, 34, 34);
        k->nameLabel.setBounds(oscRow.getX() + 124, oscRow.getY() + 28, 42, 12);
    }
    if (auto* k = findKnob("osc1Fine")) {
        k->slider.setBounds(oscRow.getX() + 172, oscRow.getY() - 4, 34, 34);
        k->nameLabel.setBounds(oscRow.getX() + 168, oscRow.getY() + 28, 42, 12);
    }

    // OSC 2
    const int osc2X = oscRow.getX() + 225;
    if (auto* c = findCombo("osc2Waveform")) {
        c->label.setBounds(osc2X, oscRow.getY(), 45, 14);
        c->comboBox.setBounds(osc2X + 45, oscRow.getY() + 2, 75, 20);
    }
    if (auto* k = findKnob("osc2Octave")) {
        k->slider.setBounds(osc2X + 128, oscRow.getY() - 4, 34, 34);
        k->nameLabel.setBounds(osc2X + 124, oscRow.getY() + 28, 42, 12);
    }
    if (auto* k = findKnob("osc2Fine")) {
        k->slider.setBounds(osc2X + 172, oscRow.getY() - 4, 34, 34);
        k->nameLabel.setBounds(osc2X + 168, oscRow.getY() + 28, 42, 12);
    }

    // Row 2: Sub-Osc & Inter-Mod
    deck1.removeFromTop(8);
    auto modRow = deck1.removeFromTop(48);
    if (auto* c = findCombo("osc3Waveform")) {
        c->label.setBounds(modRow.getX(), modRow.getY(), 55, 14);
        c->comboBox.setBounds(modRow.getX() + 55, modRow.getY() + 2, 65, 20);
    }
    if (auto* k = findKnob("osc3Level")) {
        k->slider.setBounds(modRow.getX() + 128, modRow.getY(), 34, 34);
        k->nameLabel.setBounds(modRow.getX() + 122, modRow.getY() + 32, 46, 12);
    }

    auto placeKnobInRow = [&](const char* id, int xOffset) {
        if (auto* k = findKnob(id)) {
            k->slider.setBounds(modRow.getX() + xOffset, modRow.getY(), 34, 34);
            k->nameLabel.setBounds(modRow.getX() + xOffset - 6, modRow.getY() + 32, 46, 12);
        }
    };
    placeKnobInRow("ringModMix", 185);
    placeKnobInRow("fmAmount", 240);
    placeKnobInRow("pulseWidth", 295);
    placeKnobInRow("velToAmp", 350);
    placeKnobInRow("velToFilter", 405);

    // Row 3: Horizontal Mix Crossfader
    deck1.removeFromTop(8);
    mOscMixFader.setBounds(deck1.removeFromTop(46).reduced(12, 0));

    // ------------------------------------------------------------------------
    // Deck 2: 6-Mode Filter Section
    // ------------------------------------------------------------------------
    auto deck2 = juce::Rectangle<int>(pad * 2 + leftTopW, headerH + 24, rightTopW, topDeckH - 28).reduced(8, 4);

    if (auto* c = findCombo("filterMode")) {
        c->label.setBounds(deck2.getX() + 8, deck2.getY() + 4, 45, 14);
        c->comboBox.setBounds(deck2.getX() + 55, deck2.getY() + 2, 90, 20);
    }

    // Hero Cutoff Knob (64x64)
    if (auto* k = findKnob("filterCutoff")) {
        k->slider.setBounds(deck2.getX() + 180, deck2.getY() + 4, 64, 64);
        k->nameLabel.setBounds(deck2.getX() + 175, deck2.getY() + 70, 74, 14);
    }

    // Secondary knobs row
    const int fltRowY = deck2.getY() + 90;
    auto placeFltKnob = [&](const char* id, int x) {
        if (auto* k = findKnob(id)) {
            k->slider.setBounds(deck2.getX() + x, fltRowY, 40, 40);
            k->nameLabel.setBounds(deck2.getX() + x - 5, fltRowY + 42, 50, 12);
        }
    };
    placeFltKnob("filterResonance", 20);
    placeFltKnob("filterKbTrack", 90);
    placeFltKnob("filterEnvAmount", 160);

    // ------------------------------------------------------------------------
    // Deck 3: Dual LFOs & Mod Env
    // ------------------------------------------------------------------------
    auto deck3 = juce::Rectangle<int>(pad, headerH + topDeckH + pad + 24, leftBotW, botDeckH - 28).reduced(6, 4);

    // LFO 1 Row
    auto lfo1Row = deck3.removeFromTop(50);
    if (auto* c = findCombo("lfo1Waveform")) {
        c->label.setBounds(lfo1Row.getX(), lfo1Row.getY(), 40, 14);
        c->comboBox.setBounds(lfo1Row.getX() + 42, lfo1Row.getY() + 2, 60, 20);
    }
    if (auto* k = findKnob("lfo1Rate")) {
        k->slider.setBounds(lfo1Row.getX() + 110, lfo1Row.getY(), 34, 34);
        k->nameLabel.setBounds(lfo1Row.getX() + 104, lfo1Row.getY() + 32, 46, 12);
    }
    if (auto* k = findKnob("lfo1Delay")) {
        k->slider.setBounds(lfo1Row.getX() + 155, lfo1Row.getY(), 34, 34);
        k->nameLabel.setBounds(lfo1Row.getX() + 149, lfo1Row.getY() + 32, 46, 12);
    }
    if (auto* b = findButton("lfo1Sync")) b->button.setBounds(lfo1Row.getX() + 205, lfo1Row.getY() + 6, 65, 20);
    if (auto* c = findCombo("lfo1SyncDiv")) {
        c->comboBox.setBounds(lfo1Row.getX() + 275, lfo1Row.getY() + 6, 52, 20);
    }
    if (auto* b = findButton("lfo1KeyReset")) b->button.setBounds(lfo1Row.getX() + 335, lfo1Row.getY() + 6, 70, 20);
    if (auto* k = findKnob("lfo1Amount")) {
        k->slider.setBounds(lfo1Row.getX() + 415, lfo1Row.getY(), 34, 34);
        k->nameLabel.setBounds(lfo1Row.getX() + 409, lfo1Row.getY() + 32, 46, 12);
    }
    if (auto* c = findCombo("lfo1Target")) {
        c->comboBox.setBounds(lfo1Row.getX() + 465, lfo1Row.getY() + 6, 60, 20);
    }

    // LFO 2 Row
    deck3.removeFromTop(6);
    auto lfo2Row = deck3.removeFromTop(50);
    if (auto* c = findCombo("lfo2Waveform")) {
        c->label.setBounds(lfo2Row.getX(), lfo2Row.getY(), 40, 14);
        c->comboBox.setBounds(lfo2Row.getX() + 42, lfo2Row.getY() + 2, 60, 20);
    }
    if (auto* k = findKnob("lfo2Rate")) {
        k->slider.setBounds(lfo2Row.getX() + 110, lfo2Row.getY(), 34, 34);
        k->nameLabel.setBounds(lfo2Row.getX() + 104, lfo2Row.getY() + 32, 46, 12);
    }
    if (auto* k = findKnob("lfo2Delay")) {
        k->slider.setBounds(lfo2Row.getX() + 155, lfo2Row.getY(), 34, 34);
        k->nameLabel.setBounds(lfo2Row.getX() + 149, lfo2Row.getY() + 32, 46, 12);
    }
    if (auto* b = findButton("lfo2Sync")) b->button.setBounds(lfo2Row.getX() + 205, lfo2Row.getY() + 6, 65, 20);
    if (auto* c = findCombo("lfo2SyncDiv")) {
        c->comboBox.setBounds(lfo2Row.getX() + 275, lfo2Row.getY() + 6, 52, 20);
    }
    if (auto* b = findButton("lfo2KeyReset")) b->button.setBounds(lfo2Row.getX() + 335, lfo2Row.getY() + 6, 70, 20);
    if (auto* k = findKnob("lfo2Amount")) {
        k->slider.setBounds(lfo2Row.getX() + 415, lfo2Row.getY(), 34, 34);
        k->nameLabel.setBounds(lfo2Row.getX() + 409, lfo2Row.getY() + 32, 46, 12);
    }
    if (auto* c = findCombo("lfo2Target")) {
        c->comboBox.setBounds(lfo2Row.getX() + 465, lfo2Row.getY() + 6, 60, 20);
    }

    // Mod Env Row
    deck3.removeFromTop(6);
    auto modEnvRow = deck3.removeFromTop(50);
    if (auto* k = findKnob("modAttack")) {
        k->slider.setBounds(modEnvRow.getX() + 20, modEnvRow.getY(), 34, 34);
        k->nameLabel.setBounds(modEnvRow.getX() + 14, modEnvRow.getY() + 32, 46, 12);
    }
    if (auto* k = findKnob("modDecay")) {
        k->slider.setBounds(modEnvRow.getX() + 85, modEnvRow.getY(), 34, 34);
        k->nameLabel.setBounds(modEnvRow.getX() + 79, modEnvRow.getY() + 32, 46, 12);
    }
    if (auto* k = findKnob("modAmount")) {
        k->slider.setBounds(modEnvRow.getX() + 150, modEnvRow.getY(), 34, 34);
        k->nameLabel.setBounds(modEnvRow.getX() + 142, modEnvRow.getY() + 32, 50, 12);
    }
    if (auto* c = findCombo("modTarget")) {
        c->label.setBounds(modEnvRow.getX() + 215, modEnvRow.getY() + 6, 50, 14);
        c->comboBox.setBounds(modEnvRow.getX() + 270, modEnvRow.getY() + 6, 85, 20);
    }

    // ------------------------------------------------------------------------
    // Deck 4: Envelopes & Dual Vector LCDs
    // ------------------------------------------------------------------------
    auto deck4 = juce::Rectangle<int>(pad * 2 + leftBotW, headerH + topDeckH + pad + 24, rightBotW, botDeckH - 28).reduced(6, 4);

    // Envelopes control row: Filter ADSR, Link button, Amp ADSR
    auto envKnobRow = deck4.removeFromTop(48);

    auto placeAdsrKnob = [&](const char* id, int x) {
        if (auto* k = findKnob(id)) {
            k->slider.setBounds(envKnobRow.getX() + x, envKnobRow.getY(), 32, 32);
            k->nameLabel.setBounds(envKnobRow.getX() + x - 6, envKnobRow.getY() + 30, 44, 12);
        }
    };

    // Filter ADSR knobs
    placeAdsrKnob("filterAttack", 10);
    placeAdsrKnob("filterDecay", 55);
    placeAdsrKnob("filterSustain", 100);
    placeAdsrKnob("filterRelease", 145);

    // Link Button
    if (auto* b = findButton("envLink")) {
        b->button.setBounds(envKnobRow.getX() + 195, envKnobRow.getY() + 4, 90, 22);
    }

    // Amp ADSR knobs
    placeAdsrKnob("ampAttack", 295);
    placeAdsrKnob("ampDecay", 340);
    placeAdsrKnob("ampSustain", 385);
    placeAdsrKnob("ampRelease", 430);

    // Dual Vector LCD screens side by side
    deck4.removeFromTop(6);
    const int lcdWidth = (deck4.getWidth() - 8) / 2;
    mFilterLcdDisplay.setBounds(deck4.getX(), deck4.getY(), lcdWidth, deck4.getHeight());
    mAmpLcdDisplay.setBounds(deck4.getX() + lcdWidth + 8, deck4.getY(), lcdWidth, deck4.getHeight());
}

} // namespace bumbler
