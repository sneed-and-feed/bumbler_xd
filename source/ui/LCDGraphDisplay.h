#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bumbler {

class LCDGraphDisplay : public juce::Component {
public:
    enum class DisplayType {
        FilterEnvelope,
        AmpEnvelope
    };

    explicit LCDGraphDisplay(DisplayType type);
    ~LCDGraphDisplay() override = default;

    void setEnvelopeParameters(float attackSec, float decaySec, float sustainLevel, float releaseSec);
    void setFilterModulationStatus(float envAmount, bool isLinked);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buildEnvelopePath(juce::Rectangle<float> area);

    DisplayType mType;
    float mAttack  { 0.05f };
    float mDecay   { 0.50f };
    float mSustain { 0.50f };
    float mRelease { 0.40f };

    float mEnvAmount { 1.0f };
    bool  mIsLinked  { false };

    juce::Path mCurvePath;
    juce::Path mFillPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LCDGraphDisplay)
};

} // namespace bumbler
