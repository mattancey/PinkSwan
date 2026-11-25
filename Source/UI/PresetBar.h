#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PinkSwanLookAndFeel.h"
#include "../Presets.h"

namespace pinkui
{
class PresetBar : public juce::Component
{
public:
    explicit PresetBar(pinkpresets::PresetManager& m) : manager(m)
    {
        prevButton.setButtonText("<");
        nextButton.setButtonText(">");
        saveButton.setButtonText("SAVE");
        deleteButton.setButtonText("DEL");
        aButton.setButtonText("A");
        bButton.setButtonText("B");
        copyButton.setButtonText("COPY");

        addAndMakeVisible(prevButton);
        addAndMakeVisible(nextButton);
        addAndMakeVisible(saveButton);
        addAndMakeVisible(deleteButton);
        addAndMakeVisible(combo);

        for (auto* b : { &aButton, &bButton })
        {
            b->setClickingTogglesState(true);
            b->setRadioGroupId(4242);
            addAndMakeVisible(*b);
        }
        addAndMakeVisible(copyButton);

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

        aButton.onClick = [this] {
            if (aButton.getToggleState())
            {
                manager.switchSlot(0);
                refresh();
            }
        };
        bButton.onClick = [this] {
            if (bButton.getToggleState())
            {
                manager.switchSlot(1);
                refresh();
            }
        };
        copyButton.onClick = [this] { manager.copyToOtherSlot(); };

        refresh();
    }

    void syncFromManager() { refresh(); }

    void resized() override
    {
        auto r = getLocalBounds();
        copyButton.setBounds(r.removeFromRight(48)); r.removeFromRight(4);
        bButton.setBounds(r.removeFromRight(28));    r.removeFromRight(3);
        aButton.setBounds(r.removeFromRight(28));    r.removeFromRight(8);
        deleteButton.setBounds(r.removeFromRight(42)); r.removeFromRight(4);
        saveButton.setBounds(r.removeFromRight(46));   r.removeFromRight(8);

        prevButton.setBounds(r.removeFromLeft(26));  r.removeFromLeft(4);
        nextButton.setBounds(r.removeFromRight(26)); r.removeFromRight(4);
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

        aButton.setToggleState(manager.activeSlot() == 0, juce::dontSendNotification);
        bButton.setToggleState(manager.activeSlot() == 1, juce::dontSendNotification);
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
    juce::TextButton prevButton, nextButton, saveButton, deleteButton, aButton, bButton, copyButton;
};
} // namespace pinkui
