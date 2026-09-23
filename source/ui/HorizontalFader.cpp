#include "HorizontalFader.h"
#include "BumblerLookAndFeel.h"

namespace bumbler {

HorizontalFader::HorizontalFader() {
    mSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(mSlider);

    mLeftLabel.setText("OSC 1", juce::dontSendNotification);
    mLeftLabel.setJustificationType(juce::Justification::centredLeft);
    mLeftLabel.setColour(juce::Label::textColourId, juce::Colour(BumblerColours::WaspYellow));
    mLeftLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addAndMakeVisible(mLeftLabel);

    mCenterLabel.setText("OSC MIX (50/50)", juce::dontSendNotification);
    mCenterLabel.setJustificationType(juce::Justification::centred);
    mCenterLabel.setColour(juce::Label::textColourId, juce::Colour(BumblerColours::SilkWhite));
    mCenterLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addAndMakeVisible(mCenterLabel);

    mRightLabel.setText("OSC 2", juce::dontSendNotification);
    mRightLabel.setJustificationType(juce::Justification::centredRight);
    mRightLabel.setColour(juce::Label::textColourId, juce::Colour(BumblerColours::WaspYellow));
    mRightLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addAndMakeVisible(mRightLabel);
}

void HorizontalFader::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // Dark inset cavity frame
    g.setColour(juce::Colour(BumblerColours::PanelInset));
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(juce::Colour(BumblerColours::BezelOutline));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void HorizontalFader::resized() {
    auto bounds = getLocalBounds().reduced(6, 2);
    const int labelHeight = 16;

    auto labelRow = bounds.removeFromTop(labelHeight);
    mLeftLabel.setBounds(labelRow.removeFromLeft(60));
    mRightLabel.setBounds(labelRow.removeFromRight(60));
    mCenterLabel.setBounds(labelRow);

    mSlider.setBounds(bounds.reduced(4, 0));
}

} // namespace bumbler
