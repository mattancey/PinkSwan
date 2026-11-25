#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/PinkSwanLookAndFeel.h"
#include "UI/Widgets.h"
#include "UI/PresetBar.h"

class PinkSwanAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit PinkSwanAudioProcessorEditor(PinkSwanAudioProcessor&);
    ~PinkSwanAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    PinkSwanAudioProcessor& processorRef;
    bool isDrawerOpen = false;
    uint32_t lastSeenStateRevision = 0;

    PinkSwanLookAndFeel mainLookAndFeel;
    MatrixLookAndFeel matrixLookAndFeel;

    pinkui::PresetBar presetBar;

    juce::Slider macroSlider, pitchSlider, decaySlider, pluckSlider, inharmSlider;
    juce::Slider inputCutoffSlider, loopCutoffSlider, dryWetSlider, jitterSlider;
    juce::Slider crossFbSlider, selfModSlider, wetGainSlider;

    juce::Label macroLabel, pitchLabel, decayLabel, pluckLabel, inharmLabel;
    juce::Label inputCutoffLabel, loopCutoffLabel, dryWetLabel, jitterLabel;
    juce::Label crossFbLabel, selfModLabel, wetGainLabel;

    juce::ComboBox polarityBox;
    juce::Label polarityLabel;

    juce::TextButton gateButton;
    juce::Label gateLabel;

    SwanPanicButton swanPanicButton;

    juce::TextButton drawerButton { "MODULATION" };

    juce::Slider m1Sliders[14], m2Sliders[14];
    juce::Label matrixLabels[14];
    juce::Label matrixReadoutLabel;

    juce::TextButton lfo1BypassButton, lfo2BypassButton;

    CustomComboBox lfo1SyncBox, lfo1ShapeBox, lfo2SyncBox, lfo2ShapeBox;
    juce::Slider lfo1RateSlider, lfo1PhaseSlider, lfo1PwSlider, lfo1SmoothSlider;
    juce::Slider lfo2RateSlider, lfo2PhaseSlider, lfo2PwSlider, lfo2SmoothSlider;

    juce::Label lfo1RateNameLabel, lfo1PhaseNameLabel, lfo1PwNameLabel, lfo1SmoothNameLabel;
    juce::Label lfo2RateNameLabel, lfo2PhaseNameLabel, lfo2PwNameLabel, lfo2SmoothNameLabel;

    juce::Label lfo1RateValueLabel, lfo1PhaseValueLabel, lfo1PwValueLabel, lfo1SmoothValueLabel;
    juce::Label lfo2RateValueLabel, lfo2PhaseValueLabel, lfo2PwValueLabel, lfo2SmoothValueLabel;

    juce::Label lfo1RowLabel, lfo2RowLabel;

    juce::Colour lastCrossFbColour { pinkui::neonPink };
    float lastLfo1Rate = -1.0f, lastLfo1Phase = -1.0f, lastLfo1Pw = -1.0f, lastLfo1Smooth = -1.0f;
    float lastLfo2Rate = -1.0f, lastLfo2Phase = -1.0f, lastLfo2Pw = -1.0f, lastLfo2Smooth = -1.0f;
    int lastLfo1Sync = -1, lastLfo2Sync = -1;
    bool lastLfo1Bypass = false, lastLfo2Bypass = false;
    bool forceLfoUiRefresh = true;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> mainAtts[12], m1Atts[14], m2Atts[14];
    std::unique_ptr<ComboAttachment> polarityAtt;
    std::unique_ptr<ButtonAttachment> gateAtt;
    std::unique_ptr<ComboAttachment> lfo1SyncAtt, lfo1ShapeAtt, lfo2SyncAtt, lfo2ShapeAtt;
    std::unique_ptr<SliderAttachment> lfo1RateAtt, lfo1PhaseAtt, lfo1PwAtt, lfo1SmoothAtt;
    std::unique_ptr<SliderAttachment> lfo2RateAtt, lfo2PhaseAtt, lfo2PwAtt, lfo2SmoothAtt;
    std::unique_ptr<ButtonAttachment> lfo1BypassAtt, lfo2BypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkSwanAudioProcessorEditor)
};
