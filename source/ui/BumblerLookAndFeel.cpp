#include "BumblerLookAndFeel.h"
#include <cmath>

namespace bumbler {

BumblerLookAndFeel::BumblerLookAndFeel() {
    applyThemeColours();
}

void BumblerLookAndFeel::applyThemeColours() {
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(BumblerColours::ChassisDark));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(BumblerColours::PanelInset));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(BumblerColours::BezelOutline));
    setColour(juce::ComboBox::textColourId, juce::Colour(BumblerColours::SilkWhite));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(BumblerColours::WaspYellow));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(BumblerColours::ChassisDark));
    setColour(juce::PopupMenu::textColourId, juce::Colour(BumblerColours::SilkWhite));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(BumblerColours::BezelHighlight));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(BumblerColours::WaspYellow));
    setColour(juce::Label::textColourId, juce::Colour(BumblerColours::SilkWhite));
}

BumblerLookAndFeel::KnobTier BumblerLookAndFeel::getTierForBounds(int width, int height) noexcept {
    const int sz = juce::jmin(width, height);
    if (sz < 40) return KnobTier::Trim;
    if (sz > 58) return KnobTier::Hero;
    return KnobTier::Standard;
}

void BumblerLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPosProportional, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused(slider);
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height));

    const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const float radius = (size - 6.0f) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // 1. Recessed track groove (background arc)
    const float trackWidth = (radius > 22.0f) ? 3.0f : 2.0f;
    juce::Path trackPath;
    trackPath.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(BumblerColours::KnobTrackBg));
    g.strokePath(trackPath, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. Proportional active value arc in Wasp Yellow
    if (sliderPosProportional > 0.001f) {
        juce::Path activeTrack;
        activeTrack.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(BumblerColours::WaspYellow));
        g.strokePath(activeTrack, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3. Knob body outer shadow and knurled perimeter
    const float capRadius = radius - trackWidth - 2.5f;
    const float capRx = centreX - capRadius;
    const float capRy = centreY - capRadius;
    const float capRw = capRadius * 2.0f;

    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(capRx + 1.0f, capRy + 2.0f, capRw, capRw);

    // Knurled rim
    g.setColour(juce::Colour(BumblerColours::KnobRim));
    g.fillEllipse(capRx - 1.0f, capRy - 1.0f, capRw + 2.0f, capRw + 2.0f);

    // 4. Aluminum cap with brushed gradient
    juce::ColourGradient capGradient(
        juce::Colour(BumblerColours::KnobCapLight), centreX, capRy,
        juce::Colour(BumblerColours::KnobCapDark), centreX, capRy + capRw,
        false);
    g.setGradientFill(capGradient);
    g.fillEllipse(capRx, capRy, capRw, capRw);

    // Bezel ring outline
    g.setColour(juce::Colour(BumblerColours::BezelHighlight).withAlpha(0.4f));
    g.drawEllipse(capRx, capRy, capRw, capRw, 1.0f);

    // 5. Razor-sharp Fluorescent Yellow Pointer Needle
    juce::Path needle;
    const float needleLength = capRadius * 0.78f;
    const float needleThickness = (radius > 22.0f) ? 2.5f : 1.8f;
    needle.startNewSubPath(centreX, centreY);
    needle.lineTo(centreX, centreY - needleLength);

    g.setColour(juce::Colour(BumblerColours::WaspBrightYellow));
    g.strokePath(needle, juce::PathStrokeType(needleThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 juce::AffineTransform::rotation(angle, centreX, centreY));

    // Center cap pip
    g.setColour(juce::Colour(BumblerColours::KnobCapDark));
    g.fillEllipse(centreX - 2.5f, centreY - 2.5f, 5.0f, 5.0f);
}

void BumblerLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float minSliderPos, float maxSliderPos,
                                         const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos, style, slider);

    auto trackArea = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                            static_cast<float>(width), static_cast<float>(height)).reduced(4.0f, 8.0f);

    // Trough recessed slot
    const float slotHeight = 6.0f;
    auto slotArea = juce::Rectangle<float>(trackArea.getX(), trackArea.getCentreY() - slotHeight * 0.5f,
                                          trackArea.getWidth(), slotHeight);

    g.setColour(juce::Colour(BumblerColours::PanelInset));
    g.fillRoundedRectangle(slotArea, 3.0f);

    g.setColour(juce::Colour(BumblerColours::BezelOutline));
    g.drawRoundedRectangle(slotArea, 3.0f, 1.0f);

    // Center detent tick mark
    const float centreX = trackArea.getCentreX();
    g.setColour(juce::Colour(BumblerColours::TextMuted));
    g.drawLine(centreX, trackArea.getY() - 2.0f, centreX, trackArea.getY() + 4.0f, 1.0f);
    g.drawLine(centreX, trackArea.getBottom() - 4.0f, centreX, trackArea.getBottom() + 2.0f, 1.0f);

    // Thumb fader cap (brushed aluminum block with center indicator)
    const float thumbWidth = 24.0f;
    const float thumbHeight = trackArea.getHeight() + 4.0f;
    const float thumbX = juce::jlimit(trackArea.getX(), trackArea.getRight() - thumbWidth,
                                      sliderPos - thumbWidth * 0.5f);
    const float thumbY = trackArea.getCentreY() - thumbHeight * 0.5f;

    auto thumbArea = juce::Rectangle<float>(thumbX, thumbY, thumbWidth, thumbHeight);

    // Thumb shadow
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(thumbArea.translated(1.0f, 2.0f), 3.0f);

    // Thumb gradient
    juce::ColourGradient thumbGrad(
        juce::Colour(BumblerColours::KnobCapLight), thumbArea.getX(), thumbArea.getY(),
        juce::Colour(BumblerColours::KnobCapDark), thumbArea.getRight(), thumbArea.getBottom(),
        false);
    g.setGradientFill(thumbGrad);
    g.fillRoundedRectangle(thumbArea, 3.0f);

    g.setColour(juce::Colour(BumblerColours::BezelHighlight));
    g.drawRoundedRectangle(thumbArea, 3.0f, 1.0f);

    // Thumb grip ribs
    g.setColour(juce::Colour(BumblerColours::KnobCapDark));
    for (float ribY = thumbArea.getCentreY() - 6.0f; ribY <= thumbArea.getCentreY() + 6.0f; ribY += 4.0f) {
        g.drawLine(thumbArea.getX() + 4.0f, ribY, thumbArea.getRight() - 4.0f, ribY, 1.0f);
    }

    // Thumb center indicator line (Wasp Yellow)
    g.setColour(juce::Colour(BumblerColours::WaspYellow));
    g.drawLine(thumbArea.getCentreX(), thumbArea.getY() + 2.0f, thumbArea.getCentreX(), thumbArea.getBottom() - 2.0f, 2.0f);
}

void BumblerLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const int tickWidth = 20;
    const int margin = 4;
    const int tickX = margin;
    const int tickY = (button.getHeight() - tickWidth) / 2;

    drawTickBox(g, button, static_cast<float>(tickX), static_cast<float>(tickY),
                static_cast<float>(tickWidth), static_cast<float>(tickWidth),
                button.getToggleState(), button.isEnabled(),
                shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    // Draw text label next to LED
    const int textX = tickX + tickWidth + 6;
    const int textW = button.getWidth() - textX - margin;
    if (textW > 0 && button.getButtonText().isNotEmpty()) {
        g.setColour(button.getToggleState() ? juce::Colour(BumblerColours::WaspYellow)
                                            : juce::Colour(BumblerColours::SilkWhite));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawFittedText(button.getButtonText(), textX, 0, textW, button.getHeight(),
                         juce::Justification::centredLeft, 1);
    }
}

void BumblerLookAndFeel::drawTickBox(juce::Graphics& g, juce::Component& component,
                                    float x, float y, float w, float h,
                                    bool ticked, bool isEnabled,
                                    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(isEnabled, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    auto bounds = juce::Rectangle<float>(x, y, w, h);
    const float radius = juce::jmin(w, h) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();

    // Metallic outer bezel
    g.setColour(juce::Colour(BumblerColours::LedRim));
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    g.setColour(juce::Colour(BumblerColours::BezelHighlight).withAlpha(0.6f));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    const float ledR = radius - 2.5f;

    // Determine LED color based on component name / ID
    uint32_t onColour = BumblerColours::LedGreenOn;
    uint32_t glowColour = BumblerColours::LedGreenGlow;

    juce::String name = component.getName().toLowerCase();
    if (name.contains("drive") || name.contains("red")) {
        onColour = BumblerColours::LedRedOn;
        glowColour = BumblerColours::LedRedGlow;
    } else if (name.contains("sync") || name.contains("amber")) {
        onColour = BumblerColours::LedAmberOn;
        glowColour = BumblerColours::LedAmberGlow;
    }

    if (ticked) {
        // Multi-tier bloom halo
        juce::ColourGradient glowGrad(
            juce::Colour(glowColour), cx, cy,
            juce::Colour(glowColour).withAlpha(0.0f), cx, cy + ledR * 2.2f,
            true);
        g.setGradientFill(glowGrad);
        g.fillEllipse(cx - ledR * 2.0f, cy - ledR * 2.0f, ledR * 4.0f, ledR * 4.0f);

        // Solid vibrant core
        g.setColour(juce::Colour(onColour));
        g.fillEllipse(cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f);

        // Off-center specular highlight
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.fillEllipse(cx - ledR * 0.4f, cy - ledR * 0.5f, ledR * 0.5f, ledR * 0.35f);
    } else {
        // Dark unlit lens
        g.setColour(juce::Colour(BumblerColours::LedOff));
        g.fillEllipse(cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f);

        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawEllipse(cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f, 1.0f);
    }
}

void BumblerLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                     int buttonX, int buttonY, int buttonW, int buttonH,
                                     juce::ComboBox& box)
{
    juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH, box);
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

    // Dark inset background
    g.setColour(juce::Colour(BumblerColours::PanelInset));
    g.fillRoundedRectangle(bounds, 3.0f);

    // Bezel outline
    g.setColour(juce::Colour(BumblerColours::BezelOutline));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    // Dropdown arrow indicator (Wasp Yellow triangle)
    const float arrowX = static_cast<float>(width) - 14.0f;
    const float arrowY = static_cast<float>(height) * 0.5f - 2.0f;
    juce::Path arrow;
    arrow.addTriangle(arrowX, arrowY, arrowX + 8.0f, arrowY, arrowX + 4.0f, arrowY + 5.0f);
    g.setColour(juce::Colour(BumblerColours::WaspYellow));
    g.fillPath(arrow);
}

void BumblerLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    g.fillAll(juce::Colour(BumblerColours::ChassisDark));
    g.setColour(juce::Colour(BumblerColours::BezelHighlight));
    g.drawRect(0, 0, width, height, 1);
}

void BumblerLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label) {
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited()) {
        auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font(getLabelFont(label));
        g.setFont(font);

        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
        auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / font.getHeight())),
                         label.getMinimumHorizontalScale());
    }
}

juce::Font BumblerLookAndFeel::getLabelFont(juce::Label&) {
    return juce::FontOptions(11.0f, juce::Font::bold);
}

} // namespace bumbler
