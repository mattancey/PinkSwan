#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <map>
#include "FactoryPresets.h"

namespace pinkpresets
{
class PresetManager
{
public:
    static constexpr int maxSlots = 4;

    explicit PresetManager(juce::AudioProcessorValueTreeState& a) : apvts(a)
    {
        slots[0] = snapshot();
        numSlots = 1;
        active = 0;
    }

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

    int getNumSlots() const
    {
        const juce::ScopedLock sl(lock);
        return numSlots;
    }

    int activeSlot() const
    {
        const juce::ScopedLock sl(lock);
        return active;
    }

    bool canCreateVariation() const
    {
        const juce::ScopedLock sl(lock);
        return numSlots < maxSlots;
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

        {
            const juce::ScopedLock sl(lock);
            slots[0] = snapshot();
            for (int i = 1; i < maxSlots; ++i)
                slots[(size_t)i].clear();
            numSlots = 1;
            active = 0;
        }

        apvts.state.setProperty("presetName", list[(size_t)index].name, nullptr);
        return true;
    }

    bool loadUser(const juce::String& name)
    {
        const auto xml = juce::XmlDocument::parse(fileFor(name));
        if (xml == nullptr || !xml->hasTagName("PinkSwanPreset"))
            return false;

        std::array<std::map<juce::String, float>, maxSlots> loadedSlots;
        int loadedCount = 0;
        int loadedActive = 0;

        if (const auto* vars = xml->getChildByName("VARIATIONS"))
        {
            loadedActive = vars->getIntAttribute("active", 0);
            for (const auto* slotXml : vars->getChildWithTagNameIterator("SLOT"))
            {
                if (loadedCount >= maxSlots)
                    break;

                for (const auto* e : slotXml->getChildWithTagNameIterator("PARAM"))
                    loadedSlots[(size_t)loadedCount][e->getStringAttribute("id")] = (float)e->getDoubleAttribute("value");
                ++loadedCount;
            }
        }

        if (loadedCount == 0)
        {
            for (const auto* e : xml->getChildWithTagNameIterator("PARAM"))
                loadedSlots[0][e->getStringAttribute("id")] = (float)e->getDoubleAttribute("value");
            loadedCount = 1;
            loadedActive = 0;
        }

        loadedActive = juce::jlimit(0, loadedCount - 1, loadedActive);
        apply(loadedSlots[(size_t)loadedActive]);

        {
            const juce::ScopedLock sl(lock);
            numSlots = loadedCount;
            active = loadedActive;
            for (int i = 0; i < maxSlots; ++i)
                slots[(size_t)i] = (i < numSlots) ? loadedSlots[(size_t)i] : std::map<juce::String, float>{};
            slots[(size_t)active] = snapshot();
        }

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

        std::array<std::map<juce::String, float>, maxSlots> slotsCopy;
        int countCopy = 1;
        int activeCopy = 0;
        const auto currentSnap = snapshot();

        {
            const juce::ScopedLock sl(lock);
            slots[(size_t)active] = currentSnap;
            slotsCopy = slots;
            countCopy = numSlots;
            activeCopy = active;
        }

        juce::XmlElement xml("PinkSwanPreset");
        xml.setAttribute("name", legal);

        // Paramètres du slot actif à la racine pour lisibilité
        for (const auto& kv : slotsCopy[(size_t)activeCopy])
        {
            auto* e = xml.createNewChildElement("PARAM");
            e->setAttribute("id", kv.first);
            e->setAttribute("value", (double)kv.second);
        }

        // Sauvegarde de toutes les variations A..D
        auto* vars = xml.createNewChildElement("VARIATIONS");
        vars->setAttribute("numSlots", countCopy);
        vars->setAttribute("active", activeCopy);

        for (int i = 0; i < countCopy; ++i)
        {
            auto* slotXml = vars->createNewChildElement("SLOT");
            slotXml->setAttribute("index", i);
            for (const auto& kv : slotsCopy[(size_t)i])
            {
                auto* e = slotXml->createNewChildElement("PARAM");
                e->setAttribute("id", kv.first);
                e->setAttribute("value", (double)kv.second);
            }
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

    // ---- Variations progressives A / B / C / D ----
    void switchSlot(int target)
    {
        std::map<juce::String, float> toApply;
        bool shouldApply = false;

        {
            const juce::ScopedLock sl(lock);
            if (target == active || target < 0 || target >= numSlots)
                return;

            slots[(size_t)active] = snapshot();
            active = target;
            toApply = slots[(size_t)active];
            shouldApply = true;
        }

        if (shouldApply)
            apply(toApply);
    }

    // Fige le slot en cours d'édition, crée le prochain slot disponible (B, C ou D) avec la copie, et bascule dessus
    bool createVariationFromActive()
    {
        const auto currentSnap = snapshot();
        const juce::ScopedLock sl(lock);

        if (numSlots >= maxSlots)
            return false;

        slots[(size_t)active] = currentSnap;
        const int newIdx = numSlots;
        slots[(size_t)newIdx] = currentSnap;
        ++numSlots;
        active = newIdx;
        return true;
    }

    // Supprime un slot (si > 1 variation), compacte les suivants et restaure le son si le slot actif est supprimé
    bool deleteVariation(int index)
    {
        std::map<juce::String, float> toApply;
        bool shouldApply = false;
        const auto currentSnap = snapshot();

        {
            const juce::ScopedLock sl(lock);
            if (numSlots <= 1 || index < 0 || index >= numSlots)
                return false;

            slots[(size_t)active] = currentSnap;

            for (int i = index; i < numSlots - 1; ++i)
                slots[(size_t)i] = std::move(slots[(size_t)(i + 1)]);
            slots[(size_t)(numSlots - 1)].clear();
            --numSlots;

            if (active == index)
            {
                active = juce::jmin(index, numSlots - 1);
                toApply = slots[(size_t)active];
                shouldApply = true;
            }
            else if (active > index)
            {
                --active;
            }
        }

        if (shouldApply)
            apply(toApply);

        return true;
    }

    // ---- Persistance dans le projet Ableton Live (getStateInformation / setStateInformation) ----
    void writeSlotsToState(juce::ValueTree& state) const
    {
        const auto activeSnap = snapshot();

        std::array<std::map<juce::String, float>, maxSlots> slotsCopy;
        int countCopy = 1;
        int activeCopy = 0;

        {
            const juce::ScopedLock sl(lock);
            slotsCopy = slots;
            countCopy = numSlots;
            activeCopy = active;
            slotsCopy[(size_t)activeCopy] = activeSnap;
        }

        auto existing = state.getChildWithName("SWAN_SLOTS");
        if (existing.isValid())
            state.removeChild(existing, nullptr);

        juce::ValueTree slotsTree("SWAN_SLOTS");
        slotsTree.setProperty("numSlots", countCopy, nullptr);
        slotsTree.setProperty("active", activeCopy, nullptr);

        for (int i = 0; i < countCopy; ++i)
        {
            juce::ValueTree slotTree("SLOT");
            slotTree.setProperty("index", i, nullptr);

            for (const auto& kv : slotsCopy[(size_t)i])
            {
                juce::ValueTree pTree("P");
                pTree.setProperty("id", kv.first, nullptr);
                pTree.setProperty("v", (double)kv.second, nullptr);
                slotTree.appendChild(pTree, nullptr);
            }
            slotsTree.appendChild(slotTree, nullptr);
        }

        state.appendChild(slotsTree, nullptr);
    }

    void loadSlotsFromState(const juce::ValueTree& state)
    {
        const auto currentSnap = snapshot();
        const auto slotsTree = state.getChildWithName("SWAN_SLOTS");

        const juce::ScopedLock sl(lock);
        if (!slotsTree.isValid())
        {
            numSlots = 1;
            active = 0;
            slots[0] = currentSnap;
            for (int i = 1; i < maxSlots; ++i)
                slots[(size_t)i].clear();
            return;
        }

        int loadedCount = 0;
        for (int i = 0; i < slotsTree.getNumChildren() && loadedCount < maxSlots; ++i)
        {
            const auto slotTree = slotsTree.getChild(i);
            if (!slotTree.hasType("SLOT"))
                continue;

            slots[(size_t)loadedCount].clear();
            for (int j = 0; j < slotTree.getNumChildren(); ++j)
            {
                const auto pTree = slotTree.getChild(j);
                if (pTree.hasType("P"))
                    slots[(size_t)loadedCount][pTree.getProperty("id").toString()] = (float)(double)pTree.getProperty("v");
            }
            ++loadedCount;
        }

        if (loadedCount <= 0)
        {
            numSlots = 1;
            active = 0;
            slots[0] = currentSnap;
            return;
        }

        numSlots = loadedCount;
        active = juce::jlimit(0, numSlots - 1, (int)slotsTree.getProperty("active", 0));
        for (int i = numSlots; i < maxSlots; ++i)
            slots[(size_t)i].clear();
        slots[(size_t)active] = currentSnap;
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
    mutable juce::CriticalSection lock;
    std::array<std::map<juce::String, float>, maxSlots> slots;
    int numSlots = 1;
    int active = 0;
};
} // namespace pinkpresets
