#include "LCDGraphDisplay.h"
#include "BumblerLookAndFeel.h"
#include <cmath>

namespace bumbler {

LCDGraphDisplay::LCDGraphDisplay(DisplayType type)
    : mType(type)
{
}

void LCDGraphDisplay::setEnvelopeParameters(float attackSec, float decaySec, float sustainLevel, float releaseSec) {
    mAttack  = juce::jlimit(0.001f, 10.0f, attackSec);
    mDecay   = juce::jlimit(0.001f, 10.0f, decaySec);
    mSustain = juce::jlimit(0.0f, 1.0f, sustainLevel);
    mRelease = juce::jlimit(0.001f, 10.0f, releaseSec);

    buildEnvelopePath(getLocalBounds().toFloat());
    repaint();
}

void LCDGraphDisplay::setFilterModulationStatus(float envAmount, bool isLinked) {
    mEnvAmount = envAmount;
    mIsLinked = isLinked;
    repaint();
}

void LCDGraphDisplay::resized() {
    buildEnvelopePath(getLocalBounds().toFloat());
}

void LCDGraphDisplay::buildEnvelopePath(juce::Rectangle<float> area) {
    mCurvePath.clear();
    mFillPath.clear();

    if (area.isEmpty())
        return;

    const float padX = 8.0f;
    const float headerH = 16.0f;
    const float footerH = 16.0f;
    const float plotX = area.getX() + padX;
    const float plotW = area.getWidth() - 2.0f * padX;
    const float plotY = area.getY() + headerH;
    const float plotH = area.getHeight() - headerH - footerH;

    const float yBase = plotY + plotH;
    const float yPeak = plotY + 2.0f;
    const float hUsable = yBase - yPeak;
    const float ySus = yBase - mSustain * hUsable;

    // Piecewise fractional weights mapping times into balanced visual widths
    const float logA = std::log10(1.0f + 9.0f * (mAttack / 10.0f));
    const float logD = std::log10(1.0f + 9.0f * (mDecay / 10.0f));
    const float logS = 0.25f; // Constant representation for sustain stage
    const float logR = std::log10(1.0f + 9.0f * (mRelease / 10.0f));

    const float totalLog = logA + logD + logS + logR;
    const float wA = juce::jmax(12.0f, (logA / totalLog) * plotW);
    const float wD = juce::jmax(12.0f, (logD / totalLog) * plotW);
    const float wS = juce::jmax(15.0f, (logS / totalLog) * plotW);
    const float wR = juce::jmax(12.0f, plotW - (wA + wD + wS));

    const float x0 = plotX;
    const float x1 = x0 + wA;
    const float x2 = x1 + wD;
    const float x3 = x2 + wS;
    const float x4 = x3 + wR;

    // Build piecewise exponential ADSR curve
    mCurvePath.startNewSubPath(x0, yBase);

    // 1. Attack curve: steep exponential rise to peak
    mCurvePath.quadraticTo(x0 + wA * 0.2f, yPeak + hUsable * 0.15f, x1, yPeak);

    // 2. Decay curve: exponential fall to sustain level
    mCurvePath.quadraticTo(x1 + wD * 0.35f, ySus - (yBase - ySus) * 0.25f, x2, ySus);

    // 3. Sustain hold: horizontal line across sustain window
    mCurvePath.lineTo(x3, ySus);

    // 4. Release curve: exponential fall from sustain to baseline
    mCurvePath.quadraticTo(x3 + wR * 0.35f, yBase - (yBase - ySus) * 0.35f, x4, yBase);

    // Build closed fill path for vector bloom
    mFillPath = mCurvePath;
    mFillPath.lineTo(x4, yBase);
    mFillPath.lineTo(x0, yBase);
    mFillPath.closeSubPath();
}

void LCDGraphDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // 1. Outer LCD Bezel
    g.setColour(juce::Colour(BumblerColours::LcdBezel));
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(juce::Colour(BumblerColours::BezelHighlight).withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // 2. LCD Olive Screen Background
    auto screenBounds = bounds.reduced(3.0f);
    g.setColour(juce::Colour(BumblerColours::LcdBg));
    g.fillRoundedRectangle(screenBounds, 2.0f);

    // 3. Dot-matrix Graticule Grid Lines
    g.setColour(juce::Colour(BumblerColours::LcdGrid));
    const float gridStepX = screenBounds.getWidth() / 8.0f;
    for (int i = 1; i < 8; ++i) {
        const float gx = screenBounds.getX() + i * gridStepX;
        g.drawVerticalLine(static_cast<int>(gx), screenBounds.getY(), screenBounds.getBottom());
    }

    const float gridStepY = screenBounds.getHeight() / 4.0f;
    for (int j = 1; j < 4; ++j) {
        const float gy = screenBounds.getY() + j * gridStepY;
        g.drawHorizontalLine(static_cast<int>(gy), screenBounds.getX(), screenBounds.getRight());
    }

    // 4. Phosphor Bloom Glow Fill
    juce::ColourGradient glowGradient(
        juce::Colour(BumblerColours::LcdPhosphorGlow), screenBounds.getX(), screenBounds.getY(),
        juce::Colours::transparentBlack, screenBounds.getX(), screenBounds.getBottom(),
        false);
    g.setGradientFill(glowGradient);
    g.fillPath(mFillPath);

    // 5. Razor-sharp Phosphor Vector Line
    g.setColour(juce::Colour(BumblerColours::LcdPhosphorBright));
    g.strokePath(mCurvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 6. Header Alphanumeric Tag
    g.setColour(juce::Colour(BumblerColours::LcdText));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    juce::String headerStr;
    if (mType == DisplayType::FilterEnvelope) {
        if (mIsLinked) {
            headerStr = "FILTER ENVELOPE [LINKED TO AMP]";
        } else if (std::abs(mEnvAmount) < 0.01f) {
            headerStr = "FILTER ENVELOPE [AMT: 0% — INACTIVE]";
        } else {
            headerStr = juce::String::formatted("FILTER ENVELOPE [AMT: %+.0f%%]", static_cast<double>(mEnvAmount * 100.0f));
        }
    } else {
        headerStr = "AMPLIFIER ENVELOPE [VCA]";
    }
    g.drawText(headerStr,
               juce::Rectangle<float>(screenBounds.getX() + 6.0f, screenBounds.getY() + 3.0f,
                                      screenBounds.getWidth() - 12.0f, 12.0f),
               juce::Justification::centredLeft);

    // 7. Footer Parameters Readout
    g.setFont(juce::FontOptions(9.5f, juce::Font::plain));
    const juce::String readouts = juce::String::formatted(
        "A:%.2fs  D:%.2fs  S:%d%%  R:%.2fs",
        mAttack, mDecay, static_cast<int>(mSustain * 100.0f + 0.5f), mRelease);
    g.drawText(readouts,
               juce::Rectangle<float>(screenBounds.getX() + 6.0f, screenBounds.getBottom() - 14.0f,
                                      screenBounds.getWidth() - 12.0f, 12.0f),
               juce::Justification::centredRight);
}

} // namespace bumbler
