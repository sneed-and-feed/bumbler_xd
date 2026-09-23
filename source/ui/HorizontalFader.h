#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bumbler {

class HorizontalFader : public juce::Component {
public:
    HorizontalFader();
    ~HorizontalFader() override = default;

    juce::Slider& getSlider() noexcept { return mSlider; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Slider mSlider;
    juce::Label  mLeftLabel;
    juce::Label  mCenterLabel;
    juce::Label  mRightLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HorizontalFader)
};

} // namespace bumbler
