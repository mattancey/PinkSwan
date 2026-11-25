#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace pinkui
{
    inline const juce::Colour bgBlack      { 0xFF09090C };
    inline const juce::Colour surfaceDark  { 0xFF141419 };
    inline const juce::Colour borderDark   { 0xFF2B2B34 };
    inline const juce::Colour neonPink     { 0xFFFF3E9D };
    inline const juce::Colour softPink     { 0xFFFF8EC6 };
    inline const juce::Colour dimPink      { 0xFF6B2246 };
    inline const juce::Colour dangerRed    { 0xFFFF2A2A };
    inline const juce::Colour textWhite    { 0xFFF2F2F5 };
    inline const juce::Colour textMuted    { 0xFF9494A0 };
}

class PinkSwanLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PinkSwanLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, pinkui::bgBlack);
        setColour(juce::Slider::rotarySliderFillColourId, pinkui::neonPink);
        setColour(juce::Slider::rotarySliderOutlineColourId, pinkui::borderDark);
        setColour(juce::Slider::thumbColourId, pinkui::textWhite);
        setColour(juce::Slider::textBoxTextColourId, pinkui::textWhite);
        setColour(juce::Slider::textBoxBackgroundColourId, pinkui::surfaceDark);
        setColour(juce::Slider::textBoxOutlineColourId, pinkui::borderDark);
        setColour(juce::Slider::textBoxHighlightColourId, pinkui::neonPink.withAlpha(0.4f));

        setColour(juce::ComboBox::backgroundColourId, pinkui::surfaceDark);
        setColour(juce::ComboBox::textColourId, pinkui::textWhite);
        setColour(juce::ComboBox::outlineColourId, pinkui::borderDark);
        setColour(juce::ComboBox::arrowColourId, pinkui::neonPink);

        setColour(juce::PopupMenu::backgroundColourId, pinkui::surfaceDark);
        setColour(juce::PopupMenu::textColourId, pinkui::textWhite);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, pinkui::neonPink.withAlpha(0.28f));
        setColour(juce::PopupMenu::highlightedTextColourId, pinkui::textWhite);

        setColour(juce::TextButton::buttonColourId, pinkui::surfaceDark);
        setColour(juce::TextButton::buttonOnColourId, pinkui::neonPink.withAlpha(0.22f));
        setColour(juce::TextButton::textColourOffId, pinkui::textWhite);
        setColour(juce::TextButton::textColourOnId, pinkui::softPink);

        setColour(juce::Label::textColourId, pinkui::textWhite);
        setColour(juce::TextEditor::backgroundColourId, pinkui::surfaceDark);
        setColour(juce::TextEditor::textColourId, pinkui::textWhite);
        setColour(juce::TextEditor::outlineColourId, pinkui::neonPink);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float arcRadius = radius - 3.0f;
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Fond du potard
        g.setColour(pinkui::surfaceDark);
        g.fillEllipse(centre.x - arcRadius, centre.y - arcRadius, arcRadius * 2.0f, arcRadius * 2.0f);

        // Piste de fond
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(pinkui::borderDark);
        g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Arc actif (bipolaire si min < 0 < max, sinon depuis le début)
        const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
        const bool isBipolar = (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0);
        float startArcAngle = rotaryStartAngle;
        if (isBipolar)
        {
            const float zeroProp = (float)((0.0 - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum()));
            startArcAngle = rotaryStartAngle + zeroProp * (rotaryEndAngle - rotaryStartAngle);
        }

        if (std::abs(angle - startArcAngle) > 0.005f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                   juce::jmin(startArcAngle, angle),
                                   juce::jmax(startArcAngle, angle), true);
            g.setColour(accent);
            g.strokePath(valueArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Aiguille monochrome nette
        juce::Path pointer;
        const float innerR = arcRadius * 0.35f;
        const float outerR = arcRadius * 0.85f;
        pointer.startNewSubPath(centre.x + std::sin(angle) * innerR, centre.y - std::cos(angle) * innerR);
        pointer.lineTo(centre.x + std::sin(angle) * outerR, centre.y - std::cos(angle) * outerR);
        g.setColour(pinkui::textWhite);
        g.strokePath(pointer, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool toggled = button.getToggleState();

        auto fill = toggled ? pinkui::neonPink.withAlpha(0.22f) : backgroundColour;
        if (isButtonDown)
            fill = fill.brighter(0.2f);
        else if (isMouseOverButton)
            fill = fill.brighter(0.1f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 5.0f);

        g.setColour(toggled ? pinkui::neonPink : (isMouseOverButton ? pinkui::softPink : pinkui::borderDark));
        g.drawRoundedRectangle(bounds, 5.0f, toggled ? 1.5f : 1.0f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(1.0f);
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, 5.0f);

        g.setColour(box.hasKeyboardFocus(true) ? pinkui::neonPink : pinkui::borderDark);
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

        // Petite flèche minimaliste
        const float arrowX = (float)width - 14.0f;
        const float arrowY = (float)height * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.0f);
        arrow.lineTo(arrowX, arrowY + 2.5f);
        arrow.lineTo(arrowX + 4.0f, arrowY - 2.0f);
        g.setColour(pinkui::neonPink);
        g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

class MatrixLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float midAngle = rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle);

        const bool isActive = std::abs(slider.getValue()) > 0.001;

        // Cercle de fond
        g.setColour(pinkui::borderDark);
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.6f);

        // Arc bipolaire depuis midi
        if (isActive)
        {
            juce::Path arc;
            arc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                              juce::jmin(midAngle, angle),
                              juce::jmax(midAngle, angle), true);
            g.setColour(pinkui::neonPink);
            g.strokePath(arc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Thumb
        const float thumbRadius = juce::jmax(2.2f, radius * 0.18f);
        const float thumbDist = radius * 0.72f;
        const float thumbX = centre.x + std::sin(angle) * thumbDist;
        const float thumbY = centre.y - std::cos(angle) * thumbDist;
        g.setColour(isActive ? pinkui::softPink : pinkui::textMuted);
        g.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
    }
};
