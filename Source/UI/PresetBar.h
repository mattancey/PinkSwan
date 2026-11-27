#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include "PinkSwanLookAndFeel.h"
#include "../Presets.h"

namespace pinkui
{
class SlotButton : public juce::Component, private juce::Timer
{
public:
    explicit SlotButton(const juce::String& labelText) : letter(labelText)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    ~SlotButton() override { stopTimer(); }

    void setActive(bool shouldBeActive)
    {
        if (active != shouldBeActive)
        {
            active = shouldBeActive;
            repaint();
        }
    }

    void setCanDelete(bool allowDelete)
    {
        if (canDelete != allowDelete)
        {
            canDelete = allowDelete;
            repaint();
        }
    }

    std::function<void()> onClick;
    std::function<void()> onLongPress;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        auto fill = active ? pinkui::neonPink.withAlpha(0.30f) : pinkui::surfaceDark;
        if (isPressed)
            fill = fill.brighter(0.15f);
        else if (isHovered)
            fill = fill.brighter(0.08f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 5.0f);

        // Jauge de suppression progressive pendant un appui long (> 140 ms jusqu'à 600 ms)
        if (canDelete && holdMs > graceMs)
        {
            const float prog = juce::jlimit(0.0f, 1.0f, (float)(holdMs - graceMs) / (float)(holdLimitMs - graceMs));
            auto bar = bounds.reduced(1.5f);
            bar.setWidth(bar.getWidth() * prog);
            g.setColour(pinkui::dangerRed.withAlpha(0.45f));
            g.fillRoundedRectangle(bar, 4.0f);
        }

        const auto border = (canDelete && holdMs > graceMs)
            ? pinkui::dangerRed
            : (active ? pinkui::neonPink : (isHovered ? pinkui::softPink : pinkui::borderDark));
        g.setColour(border);
        g.drawRoundedRectangle(bounds, 5.0f, active ? 1.6f : 1.0f);

        g.setColour(active ? pinkui::textWhite : pinkui::textMuted);
        g.setFont(juce::Font(juce::FontOptions(13.0f, active ? juce::Font::bold : juce::Font::plain)));
        g.drawFittedText(letter, getLocalBounds(), juce::Justification::centred, 1);
    }

    void mouseEnter(const juce::MouseEvent&) override { isHovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override
    {
        isHovered = false;
        cancelHold();
        repaint();
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        isPressed = true;
        longPressTriggered = false;
        holdMs = 0;
        if (canDelete)
            startTimer(tickMs);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isPressed && !getLocalBounds().contains(e.getPosition()))
        {
            cancelHold();
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        stopTimer();
        const bool triggerShort = isPressed && !longPressTriggered && getLocalBounds().contains(e.getPosition());
        isPressed = false;
        holdMs = 0;
        repaint();

        if (triggerShort && onClick)
            onClick();
    }

private:
    static constexpr int tickMs = 20;
    static constexpr int graceMs = 140;
    static constexpr int holdLimitMs = 600;

    void cancelHold()
    {
        stopTimer();
        isPressed = false;
        holdMs = 0;
    }

    void timerCallback() override
    {
        if (!isPressed || !canDelete)
        {
            cancelHold();
            repaint();
            return;
        }

        holdMs += tickMs;
        if (holdMs >= holdLimitMs)
        {
            longPressTriggered = true;
            cancelHold();
            repaint();
            if (onLongPress)
                onLongPress();
            return;
        }
        repaint();
    }

    juce::String letter;
    bool active = false;
    bool canDelete = false;
    bool isHovered = false;
    bo
