#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "FactoryPresets.h"

namespace pinkpresets
{
class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& a) : apvts(a) {}

    juce::StringArray factoryNames() const
    {
        juce::StringArray names;
        for (const auto& p : factoryPresets())
            names.add(p.name);
        return names;
    }

    juce::StringArray userNames() const
    {
        juce::StringArray names;
        for (const auto& f : folder().findChildFiles(juce::File::findFiles, false, "*.xml"))
            names.add(f.getFileNameWithoutExtension());
        names.sort(true);
        return names;
    }

    juce::String currentName() const
    {
        return apvts.state.getProperty("presetName", "Init").toString();
    }

    bool loadFactory(int index)
    {
        const auto& list = factoryPresets();
        if (index < 0 || index >= (int)list.size())
            return false;

        std::map<juce::String, float> values;
        for (const auto& pv : list[(size_t)index].values)
            values[pv.id] = pv.value;

        apply(values);
        apvts.state.setProperty("presetName", list[(size_t)index].name, nullptr);
        return true;
    }

    bool loadUser(const juce::String& name)
    {
        const auto xml = juce::XmlDocument::parse(fileFor(name));
        if (xml == nullptr || !xml->hasTagName("PinkSwanPreset"))
            return false;

        std::map<juce::String, float> values;
        for (const auto* e : xml->getChildWithTagNameIterator("PARAM"))
            values[e->getStringAttribute("id")] = (float)e->getDoubleAttribute("value");

        apply(values);
        apvts.state.setProperty("presetName", name, nullptr);
        return true;
    }

    bool saveUser(const juce::String& name)
    {
        const auto legal = juce::File::createLegalFileName(name.trim());
        if (legal.isEmpty())
            return false;

        const auto dir = folder();
        if (!dir.createDirectory().wasOk())
            return false;

        juce::XmlElement xml("PinkSwanPreset");
        xml.setAttribute("name", legal);
        for (const auto& kv : snapshot())
        {
            auto* e = xml.createNewChildElement("PARAM");
            e->setAttribute("id", kv.first);
            e->setAttribute("value", (double)kv.second);
        }

        if (!dir.getChildFile(legal + ".xml").replaceWithText(xml.toString()))
            return false;

        apvts.state.setProperty("presetName", legal, nullptr);
        return true;
    }

    bool deleteUser(const juce::String& name)
    {
        const bool deleted = fileFor(name).deleteFile();
        if (deleted && currentName() == name)
            apvts.state.setProperty("presetName", "Init", nullptr);
        return deleted;
    }

    // ---- A/B (comparaison instantanée durant la session) ----
    int activeSlot() const noexcept { return active; }

    void switchSlot(int target)
    {
        if (target == active || target < 0 || target > 1)
            return;

        slot[active] = snapshot();
        slotValid[active] = true;

        if (slotValid[target])
        {
            apply(slot[target]);
            apvts.state.setProperty("presetName", slotName[target], nullptr);
        }
        else
        {
            slot[target] = slot[active];
            slotName[target] = currentName();
            slotValid[target] = true;
        }

        slotName[active] = currentName();
        active = target;
    }

    void copyToOtherSlot()
    {
        const int other = 1 - active;
        slot[other] = snapshot();
        slotName[other] = currentName();
        slotValid[other] = true;
    }

private:
    static juce::File folder()
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        base = base.getChildFile("Application Support");
       #endif
        return base.getChildFile("PinkSwan").getChildFile("Presets");
    }

    static juce::File fileFor(const juce::String& name)
    {
        return folder().getChildFile(juce::File::createLegalFileName(name) + ".xml");
    }

    std::map<juce::String, float> snapshot() const
    {
        std::map<juce::String, float> values;
        for (auto* p : apvts.processor.getParameters())
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
                values[rp->paramID] = rp->convertFrom0to1(rp->getValue());
        }
        return values;
    }

    void apply(const std::map<juce::String, float>& values)
    {
        for (auto* p : apvts.processor.getParameters())
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
            {
                const auto it = values.find(rp->paramID);
                const float real = (it != values.end())
                    ? it->second
                    : rp->convertFrom0to1(rp->getDefaultValue());

                rp->beginChangeGesture();
                rp->setValueNotifyingHost(rp->convertTo0to1(real));
                rp->endChangeGesture();
            }
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::map<juce::String, float> slot[2];
    juce::String slotName[2] { "Init", "Init" };
    bool slotValid[2] = { false, false };
    int active = 0;
};
} // namespace pinkpresets
