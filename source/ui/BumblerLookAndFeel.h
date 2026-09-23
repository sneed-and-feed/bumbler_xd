#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bumbler {

struct BumblerColours {
    // Chassis & Panel Palette
    static constexpr uint32_t ChassisDark         = 0xFF141820; // Deep midnight slate
    static constexpr uint32_t ChassisPanel        = 0xFF1B222D; // Brushed textured panel
    static constexpr uint32_t PanelInset          = 0xFF12161E; // Inset cavity background
    static constexpr uint32_t BezelOutline        = 0xFF2A3445; // Precision bezel rim
    static constexpr uint32_t BezelHighlight      = 0xFF3D4B63; // Specular top bevel

    // Wasp XT Signature Silk-Screen
    static constexpr uint32_t WaspYellow          = 0xFFF5C542; // Classic Wasp golden yellow
    static constexpr uint32_t WaspBrightYellow    = 0xFFFFD700; // Fluorescent yellow accent
    static constexpr uint32_t SilkWhite           = 0xFFECEFF4; // High-contrast silk labels
    static constexpr uint32_t TextMuted           = 0xFF7E8C9F; // Secondary unit markings
    static constexpr uint32_t AccentCyan          = 0xFF64B5F6; // Secondary modulation accent

    // Knob Styling
    static constexpr uint32_t KnobCapLight        = 0xFF353D4C; // Aluminum top specular
    static constexpr uint32_t KnobCapDark         = 0xFF202530; // Dark shadow edge
    static constexpr uint32_t KnobRim             = 0xFF434E61; // Knurled outer ring
    static constexpr uint32_t KnobIndicator       = 0xFFF5C542; // Yellow pointer needle
    static constexpr uint32_t KnobTrackBg         = 0xFF161A22; // Inset track groove
    static constexpr uint32_t KnobTrackFill       = 0xFFF5C542; // Active calibration arc

    // LED Indicators
    static constexpr uint32_t LedRedOn            = 0xFFFF3B30;
    static constexpr uint32_t LedRedGlow          = 0x80FF3B30;
    static constexpr uint32_t LedGreenOn          = 0xFF34C759;
    static constexpr uint32_t LedGreenGlow        = 0x8034C759;
    static constexpr uint32_t LedAmberOn          = 0xFFFF9500;
    static constexpr uint32_t LedAmberGlow        = 0x80FF9500;
    static constexpr uint32_t LedOff              = 0xFF1B2026;
    static constexpr uint32_t LedRim              = 0xFF3A4454;

    // Green LCD Display Palette
    static constexpr uint32_t LcdBg               = 0xFF0A140D; // Deep olive LCD backlight
    static constexpr uint32_t LcdBezel            = 0xFF162419; // Bezel frame
    static constexpr uint32_t LcdGrid             = 0xFF122316; // Matrix grid lines
    static constexpr uint32_t LcdPhosphorBright   = 0xFF39FF74; // Vibrant phosphor green
    static constexpr uint32_t LcdPhosphorGlow     = 0x3339FF74; // Vector bloom fill
    static constexpr uint32_t LcdText             = 0xFF52FFA0; // Digital readout
};

class BumblerLookAndFeel : public juce::LookAndFeel_V4 {
public:
    enum class KnobTier {
        Trim,       // < 40px (Octave, Fine)
        Standard,   // 40-60px (General controls)
        Hero        // > 60px (Filter Cutoff, Master Volume)
    };

    BumblerLookAndFeel();
    ~BumblerLookAndFeel() override = default;

    static KnobTier getTierForBounds(int width, int height) noexcept;

    // Rotary Slider Overrides
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    // Linear Slider Overrides
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    // LED Button Overrides
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawTickBox(juce::Graphics& g, juce::Component& component,
                     float x, float y, float w, float h,
                     bool ticked, bool isEnabled,
                     bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // ComboBox & PopupMenu Overrides
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

    // Label & Typography Overrides
    void drawLabel(juce::Graphics& g, juce::Label& label) override;
    juce::Font getLabelFont(juce::Label&) override;

private:
    void applyThemeColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BumblerLookAndFeel)
};

} // namespace bumbler
