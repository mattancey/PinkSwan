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

// Bouton Panic / Kill Tail : cygne facetté Stealth / Low-Poly en vue 3/4 avant gauche
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

        const auto borderCol = flashAlpha > 0.01f
            ? pinkui::neonPink.interpolatedWith(pinkui::textWhite, flashAlpha)
            : (shouldDrawButtonAsHighlighted ? pinkui::softPink : pinkui::neonPink);
        g.setColour(borderCol);
        g.drawRoundedRectangle(frame, corner, 1.8f);

        const auto c = frame.getCentre();
        const float s = side * 0.43f;
        const float grooveW = juce::jmax(1.0f, side * 0.016f);
        const float detailW = juce::jmax(0.8f, side * 0.012f);
        const juce::PathStrokeType grooveStroke(grooveW, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);
        const juce::PathStrokeType detailStroke(detailW, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);

        auto pt = [&](float nx, float ny) -> juce::Point<float> {
            return { c.x + nx * s, c.y + ny * s };
        };

        auto shade = [&](juce::Colour base) -> juce::Colour {
            return flashAlpha > 0.01f ? base.interpolatedWith(pinkui::softPink, flashAlpha * 0.45f) : base;
        };

        auto drawFacet = [&](std::initializer_list<juce::Point<float>> pts, juce::Colour fillCol) {
            juce::Path p;
            bool first = true;
            for (const auto& v : pts)
            {
                if (first) { p.startNewSubPath(v); first = false; }
                else       { p.lineTo(v); }
            }
            p.closeSubPath();
            g.setColour(shade(fillCol));
            g.fillPath(p);
            g.setColour(pinkui::bgBlack);
            g.strokePath(p, grooveStroke);
        };

        auto drawLine = [&](juce::Point<float> a, juce::Point<float> b, juce::Colour col, const juce::PathStrokeType& st) {
            juce::Path p;
            p.startNewSubPath(a);
            p.lineTo(b);
            g.setColour(col);
            g.strokePath(p, st);
        };

        const juce::Colour cBright  { 0xFFF4F4F8 };
        const juce::Colour cLight   { 0xFFE2E2EC };
        const juce::Colour cSilver  { 0xFFC0C0CE };
        const juce::Colour cMid     { 0xFF9C9CAC };
        const juce::Colour cSlate   { 0xFF787888 };
        const juce::Colour cShadow  { 0xFF525260 };

        // 1. Aile arrière gauche (perspective 3/4 avant gauche)
        drawFacet({ pt(-0.22f, 0.04f), pt(-0.44f, -0.10f), pt(-0.62f, -0.14f), pt(-0.50f, -0.02f),
                    pt(-0.70f, 0.02f), pt(-0.54f,  0.10f), pt(-0.68f,  0.16f), pt(-0.48f,  0.22f), pt(-0.28f, 0.14f) },
                  cShadow);
        drawLine(pt(-0.28f, 0.10f), pt(-0.62f, -0.14f), shade(cSilver), detailStroke);
        drawLine(pt(-0.32f, 0.14f), pt(-0.70f,  0.02f), shade(cSilver), detailStroke);
        drawLine(pt(-0.34f, 0.18f), pt(-0.68f,  0.16f), shade(cSilver), detailStroke);

        // 2. Aile principale droite (lames géométriques superposées)
        drawFacet({ pt( 0.00f, 0.06f), pt(0.22f, -0.08f), pt(0.52f, -0.32f), pt(0.34f, 0.04f), pt(0.14f, 0.20f) }, cBright);
        drawFacet({ pt( 0.52f,-0.32f), pt(0.78f, -0.26f), pt(0.54f, -0.14f), pt(0.34f, 0.04f) }, cSilver);
        drawFacet({ pt( 0.34f, 0.04f), pt(0.54f, -0.14f), pt(0.82f, -0.06f), pt(0.56f, 0.02f), pt(0.30f, 0.18f) }, cBright);
        drawFacet({ pt( 0.30f, 0.18f), pt(0.56f,  0.02f), pt(0.78f,  0.14f), pt(0.52f, 0.18f), pt(0.24f, 0.30f) }, cMid);
        drawFacet({ pt( 0.24f, 0.30f), pt(0.52f,  0.18f), pt(0.68f,  0.32f), pt(0.44f, 0.32f), pt(0.16f, 0.42f) }, cLight);
        drawFacet({ pt( 0.16f, 0.42f), pt(0.44f,  0.32f), pt(0.58f,  0.48f), pt(0.26f, 0.58f), pt(-0.02f, 0.62f) }, cSlate);
        drawFacet({ pt( 0.00f, 0.06f), pt(0.14f,  0.20f), pt(0.24f,  0.30f), pt(0.16f, 0.42f), pt(-0.02f, 0.62f), pt(-0.06f, 0.34f) }, cMid);

        drawLine(pt(0.22f, -0.08f), pt( 0.34f,  0.04f), pinkui::bgBlack, detailStroke);
        drawLine(pt(0.34f,  0.04f), pt( 0.78f, -0.26f), pinkui::bgBlack, detailStroke);
        drawLine(pt(0.34f,  0.04f), pt( 0.82f, -0.06f), pinkui::bgBlack, detailStroke);
        drawLine(pt(0.30f,  0.18f), pt( 0.78f,  0.14f), pinkui::bgBlack, detailStroke);
        drawLine(pt(0.24f,  0.30f), pt( 0.68f,  0.32f), pinkui::bgBlack, detailStroke);
        drawLine(pt(0.14f,  0.20f), pt(-0.06f,  0.34f), pinkui::bgBlack, detailStroke);

        // 3. Poitrail en proue (3/4 avant gauche)
        drawFacet({ pt( 0.00f, 0.06f), pt(-0.15f, 0.08f), pt(-0.34f, 0.30f), pt(-0.06f, 0.34f) }, cMid);
        drawFacet({ pt(-0.06f, 0.34f), pt(-0.34f, 0.30f), pt(-0.32f, 0.58f), pt(-0.20f, 0.66f), pt(-0.02f, 0.62f) }, cSlate);
        drawFacet({ pt(-0.15f, 0.08f), pt(-0.28f, 0.12f), pt(-0.52f, 0.24f), pt(-0.34f, 0.30f) }, cBright);
        drawFacet({ pt(-0.34f, 0.30f), pt(-0.52f, 0.24f), pt(-0.58f, 0.44f), pt(-0.32f, 0.58f) }, cLight);
        drawFacet({ pt(-0.32f, 0.58f), pt(-0.58f, 0.44f), pt(-0.42f, 0.62f), pt(-0.20f, 0.66f) }, cSilver);

        // 4. Cou en tension facetté
        drawFacet({ pt( 0.08f, -0.58f), pt(-0.05f, -0.54f), pt( 0.02f, -0.32f), pt( 0.14f, -0.36f) }, cSlate);
        drawFacet({ pt( 0.14f, -0.36f), pt( 0.02f, -0.32f), pt(-0.01f, -0.10f), pt( 0.12f, -0.14f) }, cSlate);
        drawFacet({ pt( 0.12f, -0.14f), pt(-0.01f, -0.10f), pt(-0.15f,  0.08f), pt( 0.00f,  0.06f) }, cMid);
        drawFacet({ pt(-0.05f, -0.54f), pt(-0.18f, -0.46f), pt(-0.08f, -0.26f), pt( 0.02f, -0.32f) }, cBright);
        drawFacet({ pt( 0.02f, -0.32f), pt(-0.08f, -0.26f), pt(-0.12f, -0.04f), pt(-0.01f, -0.10f) }, cLight);
        drawFacet({ pt(-0.01f, -0.10f), pt(-0.12f, -0.04f), pt(-0.28f,  0.12f), pt(-0.15f,  0.08f) }, cBright);

        drawLine(pt( 0.08f, -0.58f), pt( 0.02f, -0.32f), pinkui::bgBlack, detailStroke);
        drawLine(pt( 0.02f, -0.32f), pt(-0.18f, -0.46f), pinkui::bgBlack, detailStroke);
        drawLine(pt( 0.14f, -0.36f), pt(-0.01f, -0.10f), pinkui::bgBlack, detailStroke);
        drawLine(pt(-0.01f, -0.10f), pt(-0.08f, -0.26f), pinkui::bgBlack, detailStroke);
        drawLine(pt( 0.12f, -0.14f), pt(-0.15f,  0.08f), pinkui::bgBlack, detailStroke);

        // 5. Tête & bec poignard (tournés vers la gauche)
        drawFacet({ pt( 0.08f, -0.58f), pt( 0.00f, -0.72f), pt(-0.14f, -0.78f), pt(-0.28f, -0.62f), pt(-0.18f, -0.46f), pt(-0.05f, -0.54f) }, cMid);
        drawFacet({ pt(-0.14f, -0.78f), pt(-0.28f, -0.72f), pt(-0.40f, -0.60f), pt(-0.28f, -0.62f) }, cBright);
        drawFacet({ pt(-0.28f, -0.62f), pt(-0.40f, -0.60f), pt(-0.44f, -0.48f), pt(-0.36f, -0.44f), pt(-0.18f, -0.46f) }, cLight);
        drawFacet({ pt(-0.40f, -0.60f), pt(-0.62f, -0.44f), pt(-0.74f, -0.30f), pt(-0.44f, -0.48f) }, cMid);
        drawFacet({ pt(-0.44f, -0.48f), pt(-0.74f, -0.30f), pt(-0.46f, -0.38f), pt(-0.36f, -0.44f) }, cShadow);

        // Œil anguleux
        juce::Path eye;
        eye.startNewSubPath(pt(-0.29f, -0.58f));
        eye.lineTo(pt(-0.40f, -0.56f));
        eye.lineTo(pt(-0.36f, -0.51f));
        eye.lineTo(pt(-0.27f, -0.54f));
        eye.closeSubPath();
        g.setColour(pinkui::bgBlack);
        g.fillPath(eye);

        // 6. Sillage anguleux sur l'eau
        juce::Path wake1;
        wake1.startNewSubPath(pt( 0.76f, 0.66f));
        wake1.lineTo(pt( 0.18f, 0.66f));
        wake1.lineTo(pt(-0.20f, 0.72f));
        wake1.lineTo(pt(-0.68f, 0.66f));
        wake1.lineTo(pt(-0.84f, 0.62f));
        g.setColour(shade(cBright));
        g.strokePath(wake1, grooveStroke);

        juce::Path wake2;
        wake2.startNewSubPath(pt( 0.62f, 0.75f));
        wake2.lineTo(pt(-0.16f, 0.79f));
        wake2.lineTo(pt(-0.60f, 0.73f));
        g.setColour(shade(cMid));
        g.strokePath(wake2, detailStroke);

        drawLine(pt(0.32f, 0.84f), pt(-0.36f, 0.84f), shade(cShadow), detailStroke);
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
