#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PinkSwanLookAndFeel.h"

inline void setupRotarySlider(juce::Slider& s, juce::Label& l, const juce::String& name,
                              juce::Component& parent, double defaultValue)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    s.setDoubleClickReturnValue(true, defaultValue);
    parent.addAndMakeVisible(s);

    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, pinkui::textWhite);
    parent.addAndMakeVisible(l);
}

class CustomComboBox : public juce::ComboBox
{
public:
    void showPopup() override
    {
        juce::PopupMenu m;
        m.setLookAndFeel(&getLookAndFeel());
        for (int i = 1; i <= getNumItems(); ++i)
            m.addItem(i, getItemText(i - 1), true, i == getSelectedId());

        auto options = juce::PopupMenu::Options()
            .withTargetComponent(this)
            .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::upwards)
            .withMinimumWidth(getWidth())
            .withMaximumNumColumns(1);

        juce::Component::SafePointer<CustomComboBox> safeThis(this);
        m.showMenuAsync(options, [safeThis](int result) {
            if (safeThis == nullptr)
                return;
            safeThis->hidePopup();
            if (result > 0)
            {
                juce::MessageManager::callAsync([safeThis, result] {
                    if (safeThis != nullptr)
                        safeThis->setSelectedId(result, juce::sendNotificationSync);
                });
            }
        });
    }
};

// Bouton Panic / Kill Tail : cygne monochrome Stealth / Origami en vue de 3/4 avant
class SwanPanicButton : public juce::Button, private juce::Timer
{
public:
    SwanPanicButton() : juce::Button("SwanPanic")
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Panic: Kill resonance tail");
    }

    ~SwanPanicButton() override { stopTimer(); }

    void triggerFlash()
    {
        flashAlpha = 1.0f;
        startTimerHz(60);
        repaint();
    }

protected:
    void clicked() override
    {
        triggerFlash();
        juce::Button::clicked();
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        const float side = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto frame = bounds.withSizeKeepingCentre(side, side);
        constexpr float corner = 4.0f;

        // Fond noir + flash éventuel lors du clic Panic
        g.setColour(pinkui::bgBlack);
        g.fillRoundedRectangle(frame, corner);

        if (flashAlpha > 0.01f)
        {
            g.setColour(pinkui::neonPink.withAlpha(flashAlpha * 0.38f));
            g.fillRoundedRectangle(frame, corner);
        }
        else if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
        {
            g.setColour(pinkui::textWhite.withAlpha(shouldDrawButtonAsDown ? 0.14f : 0.06f));
            g.fillRoundedRectangle(frame, corner);
        }

        // Cadre rose néon net
        const auto borderCol = flashAlpha > 0.01f
            ? pinkui::neonPink.interpolatedWith(pinkui::textWhite, flashAlpha)
            : (shouldDrawButtonAsHighlighted ? pinkui::softPink : pinkui::neonPink);
        g.setColour(borderCol);
        g.drawRoundedRectangle(frame, corner, 1.8f);

        // Géométrie Stealth / Origami en vue 3/4 avant
        const auto c = frame.getCentre();
        const float s = side * 0.41f;
        const float cy = c.y + 0.05f * s;
        const auto swanCol = flashAlpha > 0.01f
            ? pinkui::textWhite.interpolatedWith(pinkui::softPink, flashAlpha * 0.5f)
            : pinkui::textWhite;

        auto pt = [&](float nx, float ny) -> juce::Point<float> {
            return { c.x + nx * s, cy + ny * s };
        };

        juce::Path swan;
        swan.startNewSubPath(pt(-0.14f, -0.74f)); // Sommet du crâne
        swan.lineTo(pt(-0.36f, -0.62f));          // Arcade tendue
        swan.lineTo(pt(-0.68f, -0.36f));          // Pointe du bec poignard
        swan.lineTo(pt(-0.34f, -0.44f));          // Entaille sous le bec
        swan.lineTo(pt(-0.20f, -0.38f));          // Gorge
        swan.lineTo(pt(-0.12f, -0.16f));          // Pli intérieur du cou en Z
        swan.lineTo(pt(-0.54f,  0.16f));          // Proue anguleuse du poitrail
        swan.lineTo(pt(-0.38f,  0.48f));          // Base avant sur l'eau
        swan.lineTo(pt( 0.24f,  0.48f));          // Ligne de quille
        swan.lineTo(pt( 0.80f,  0.16f));          // Pointe de queue arrière
        swan.lineTo(pt( 0.50f, -0.20f));          // Pointe haute de l'aile furtive
        swan.lineTo(pt( 0.08f,  0.08f));          // Creux de l'aile
        swan.lineTo(pt( 0.12f, -0.52f));          // Arête arrière tendue du cou
        swan.closeSubPath();

        g.setColour(swanCol);
        g.fillPath(swan);

        // Découpes négatives tranchantes (arête de poitrail + 2 lames parallèles d'aile)
        const float cutWidth = juce::jmax(1.5f, side * 0.028f);
        const juce::PathStrokeType sharpStroke(cutWidth, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);

        juce::Path wingCuts;
        wingCuts.startNewSubPath(pt(-0.54f, 0.16f));
        wingCuts.lineTo(pt(-0.02f, 0.24f));
        wingCuts.lineTo(pt( 0.58f, -0.06f));

        wingCuts.startNewSubPath(pt( 0.06f, 0.38f));
        wingCuts.lineTo(pt( 0.74f, 0.06f));

        // Fente d'œil diagonale pour le regard agressif
        wingCuts.startNewSubPath(pt(-0.32f, -0.57f));
        wingCuts.lineTo(pt(-0.15f, -0.53f));

        g.setColour(pinkui::bgBlack);
        g.strokePath(wingCuts, sharpStroke);

        // Ligne d'eau brisée en V sous la silhouette
        juce::Path water;
        water.startNewSubPath(pt(-0.72f, 0.57f));
        water.lineTo(pt(-0.38f, 0.63f));
        water.lineTo(pt( 0.76f, 0.57f));

        g.setColour(swanCol.withAlpha(0.65f));
        g.strokePath(water, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }

private:
    void timerCallback() override
    {
        flashAlpha *= 0.80f;
        if (flashAlpha < 0.02f)
        {
            flashAlpha = 0.0f;
            stopTimer();
        }
        repaint();
    }

    float flashAlpha = 0.0f;
};
