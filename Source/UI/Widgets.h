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

// Bouton Panic / Kill Tail avec silhouette minimaliste monochrome d'un cygne en vue de 3/4 avant
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

        // Fond noir + flash éventuel lors du clic Panic
        g.setColour(pinkui::bgBlack);
        g.fillRoundedRectangle(frame, 8.0f);

        if (flashAlpha > 0.01f)
        {
            g.setColour(pinkui::neonPink.withAlpha(flashAlpha * 0.35f));
            g.fillRoundedRectangle(frame, 8.0f);
        }
        else if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
        {
            g.setColour(pinkui::textWhite.withAlpha(shouldDrawButtonAsDown ? 0.14f : 0.06f));
            g.fillRoundedRectangle(frame, 8.0f);
        }

        // Cadre rose néon
        const auto borderCol = flashAlpha > 0.01f
            ? pinkui::neonPink.interpolatedWith(pinkui::textWhite, flashAlpha)
            : (shouldDrawButtonAsHighlighted ? pinkui::softPink : pinkui::neonPink);
        g.setColour(borderCol);
        g.drawRoundedRectangle(frame, 8.0f, 1.8f);

        // Dessin vectoriel minimaliste monochrome du cygne en vue 3/4 avant
        const auto c = frame.getCentre();
        const float s = side * 0.34f;
        const auto swanCol = flashAlpha > 0.01f
            ? pinkui::textWhite.interpolatedWith(pinkui::softPink, flashAlpha * 0.5f)
            : pinkui::textWhite;

        // Silhouette corps + cou en S + tête tournée 3/4 avant vers la gauche
        juce::Path swan;
        swan.startNewSubPath(c.x - 0.12f * s, c.y + 0.22f * s);
        // Montée du cou (côté arrière) vers la nuque
        swan.cubicTo(c.x + 0.08f * s, c.y - 0.05f * s,
                     c.x + 0.12f * s, c.y - 0.58f * s,
                     c.x - 0.12f * s, c.y - 0.68f * s);
        // Sommet de la tête -> bec pointant 3/4 avant-gauche
        swan.cubicTo(c.x - 0.26f * s, c.y - 0.72f * s,
                     c.x - 0.38f * s, c.y - 0.56f * s,
                     c.x - 0.54f * s, c.y - 0.42f * s);
        // Dessous du bec -> gorge
        swan.cubicTo(c.x - 0.40f * s, c.y - 0.38f * s,
                     c.x - 0.30f * s, c.y - 0.44f * s,
                     c.x - 0.22f * s, c.y - 0.38f * s);
        // Courbe intérieure du cou descendant vers le poitrail bombé (3/4 face)
        swan.cubicTo(c.x - 0.12f * s, c.y - 0.20f * s,
                     c.x - 0.54f * s, c.y + 0.04f * s,
                     c.x - 0.48f * s, c.y + 0.36f * s);
        // Base du poitrail sur l'eau vers la queue à droite
        swan.cubicTo(c.x - 0.34f * s, c.y + 0.58f * s,
                     c.x + 0.28f * s, c.y + 0.56f * s,
                     c.x + 0.74f * s, c.y + 0.20f * s);
        // Aile droite relevée (perspective 3/4 avant) rejoignant la base du cou
        swan.cubicTo(c.x + 0.48f * s, c.y + 0.04f * s,
                     c.x + 0.24f * s, c.y - 0.02f * s,
                     c.x - 0.12f * s, c.y + 0.22f * s);
        swan.closeSubPath();

        g.setColour(swanCol);
        g.fillPath(swan);

        // Ligne d'aile intérieure minimaliste (découpe négative)
        juce::Path wingLine;
        wingLine.startNewSubPath(c.x - 0.08f * s, c.y + 0.32f * s);
        wingLine.quadraticTo(c.x + 0.20f * s, c.y + 0.12f * s,
                             c.x + 0.54f * s, c.y + 0.22f * s);
        g.setColour(pinkui::bgBlack);
        g.strokePath(wingLine, juce::PathStrokeType(juce::jmax(1.4f, side * 0.024f),
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        // Ligne d'eau minimaliste sous le cygne
        juce::Path water;
        water.startNewSubPath(c.x - 0.66f * s, c.y + 0.64f * s);
        water.quadraticTo(c.x, c.y + 0.69f * s, c.x + 0.68f * s, c.y + 0.64f * s);
        g.setColour(swanCol.withAlpha(0.55f));
        g.strokePath(water, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
