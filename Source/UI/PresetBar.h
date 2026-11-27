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
            setTooltip(canDelete
                ? "Click: edit variation " + letter + " | Hold: delete"
                : "Active variation (" + letter + ")");
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
    bool isPressed = false;
    bool longPressTriggered = false;
    int holdMs = 0;
};

class PresetBar : public juce::Component
{
public:
    explicit PresetBar(pinkpresets::PresetManager& m)
        : manager(m),
          slotButtons { SlotButton("A"), SlotButton("B"), SlotButton("C"), SlotButton("D") }
    {
        prevButton.setButtonText("<");
        nextButton.setButtonText(">");
        saveButton.setButtonText("SAVE");
        deleteButton.setButtonText("DEL");
        copyButton.setButtonText("COPY");

        addAndMakeVisible(prevButton);
        addAndMakeVisible(nextButton);
        addAndMakeVisible(saveButton);
        addAndMakeVisible(deleteButton);
        addAndMakeVisible(combo);
        addAndMakeVisible(copyButton);

        for (int i = 0; i < pinkpresets::PresetManager::maxSlots; ++i)
        {
            addChildComponent(slotButtons[(size_t)i]);
            slotButtons[(size_t)i].onClick = [this, i] {
                manager.switchSlot(i);
                refresh();
            };
            slotButtons[(size_t)i].onLongPress = [this, i] {
                if (manager.deleteVariation(i))
                    refresh();
            };
        }

        prevButton.onClick = [this] { stepPreset(-1); };
        nextButton.onClick = [this] { stepPreset(1); };
        saveButton.onClick = [this] { promptSave(); };
        deleteButton.onClick = [this] {
            if (userNames.contains(manager.currentName()))
            {
                manager.deleteUser(manager.currentName());
                refresh();
            }
        };

        combo.onChange = [this] {
            const int id = combo.getSelectedId();
            if (id >= 1000)
                manager.loadUser(userNames[id - 1000]);
            else if (id >= 1)
                manager.loadFactory(id - 1);
            else
                return;

            refresh();
        };

        copyButton.onClick = [this] {
            if (manager.createVariationFromActive())
                refresh();
        };

        refresh();
    }

    void syncFromManager() { refresh(); }

    void resized() override
    {
        auto r = getLocalBounds();
        copyButton.setBounds(r.removeFromRight(46));
        r.removeFromRight(6);

        const int visibleSlots = juce::jlimit(1, pinkpresets::PresetManager::maxSlots, manager.getNumSlots());
        for (int i = pinkpresets::PresetManager::maxSlots - 1; i >= 0; --i)
        {
            if (i < visibleSlots)
            {
                slotButtons[(size_t)i].setBounds(r.removeFromRight(26));
                if (i > 0)
                    r.removeFromRight(3);
            }
        }
        r.removeFromRight(8);

        deleteButton.setBounds(r.removeFromRight(40)); r.removeFromRight(4);
        saveButton.setBounds(r.removeFromRight(44));   r.removeFromRight(8);

        prevButton.setBounds(r.removeFromLeft(24));  r.removeFromLeft(4);
        nextButton.setBounds(r.removeFromRight(24)); r.removeFromRight(4);
        combo.setBounds(r);
    }

private:
    void refresh()
    {
        combo.clear(juce::dontSendNotification);
        factoryNames = manager.factoryNames();
        userNames = manager.userNames();

        combo.addSectionHeading("FACTORY");
        for (int i = 0; i < factoryNames.size(); ++i)
            combo.addItem(factoryNames[i], i + 1);

        if (userNames.size() > 0)
        {
            combo.addSectionHeading("USER");
            for (int i = 0; i < userNames.size(); ++i)
                combo.addItem(userNames[i], 1000 + i);
        }

        const auto cur = manager.currentName();
        combo.setText(cur, juce::dontSendNotification);
        deleteButton.setEnabled(userNames.contains(cur));

        const int count = manager.getNumSlots();
        const int activeIdx = manager.activeSlot();
        const bool canDel = count > 1;

        for (int i = 0; i < pinkpresets::PresetManager::maxSlots; ++i)
        {
            auto& btn = slotButtons[(size_t)i];
            const bool vis = i < count;
            btn.setVisible(vis);
            if (vis)
            {
                btn.setActive(i == activeIdx);
                btn.setCanDelete(canDel);
            }
        }

        copyButton.setEnabled(manager.canCreateVariation());
        copyButton.setAlpha(manager.canCreateVariation() ? 1.0f : 0.42f);

        resized();
        repaint();
    }

    void stepPreset(int direction)
    {
        juce::StringArray all(factoryNames);
        all.addArray(userNames);
        if (all.isEmpty())
            return;

        const int current = all.indexOf(manager.currentName());
        const int next = (current < 0)
            ? (direction > 0 ? 0 : all.size() - 1)
            : (current + direction + all.size()) % all.size();

        if (next < factoryNames.size())
            manager.loadFactory(next);
        else
            manager.loadUser(all[next]);

        refresh();
    }

    void promptSave()
    {
        auto* window = new juce::AlertWindow("SAVE PRESET", "Preset name:", juce::MessageBoxIconType::NoIcon);
        window->setLookAndFeel(&getLookAndFeel());
        window->addTextEditor("name", manager.currentName());
        window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PresetBar> safe(this);
        window->enterModalState(true, juce::ModalCallbackFunction::create([safe, window](int result) {
            if (result != 1 || safe == nullptr)
                return;
            const auto name = window->getTextEditorContents("name").trim();
            if (name.isNotEmpty() && safe->manager.saveUser(name))
                safe->refresh();
        }), true);
    }

    pinkpresets::PresetManager& manager;
    juce::StringArray factoryNames, userNames;
    juce::ComboBox combo;
    juce::TextButton prevButton, nextButton, saveButton, deleteButton, copyButton;
    std::array<SlotButton, pinkpresets::PresetManager::maxSlots> slotButtons;
};
} // namespace pinkui
