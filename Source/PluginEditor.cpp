#include "PluginEditor.h"
#include "Params.h"

PinkSwanAudioProcessorEditor::PinkSwanAudioProcessorEditor(PinkSwanAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      presetBar(p.getPresetManager())
{
    setLookAndFeel(&mainLookAndFeel);
    lastSeenStateRevision = processorRef.getStateRevision();

    addAndMakeVisible(presetBar);

    setupRotarySlider(macroSlider, macroLabel, "MACRO", *this, 0.0);
    macroLabel.setColour(juce::Label::textColourId, pinkui::neonPink);

    setupRotarySlider(inputCutoffSlider, inputCutoffLabel, "In Cut", *this, 8000.0);
    setupRotarySlider(loopCutoffSlider, loopCutoffLabel, "Loop Cut", *this, 4000.0);
    setupRotarySlider(pitchSlider, pitchLabel, "Pitch", *this, 0.0);
    setupRotarySlider(pluckSlider, pluckLabel, juce::String::fromUTF8("H\u03B2"), *this, 0.0);
    setupRotarySlider(inharmSlider, inharmLabel, "Inharm", *this, 0.0);
    setupRotarySlider(decaySlider, decayLabel, "Feedbk", *this, 0.85);
    setupRotarySlider(crossFbSlider, crossFbLabel, "CrossFB", *this, 0.0);
    setupRotarySlider(selfModSlider, selfModLabel, "SelfMod", *this, 0.0);
    setupRotarySlider(dryWetSlider, dryWetLabel, "Dry/Wet", *this, 0.5);
    setupRotarySlider(wetGainSlider, wetGainLabel, "WetGain", *this, 1.0);
    setupRotarySlider(jitterSlider, jitterLabel, "Drift", *this, 0.0);

    swanPanicButton.onClick = [this] { processorRef.triggerPanic(); };
    addAndMakeVisible(swanPanicButton);

    polarityBox.addItemList({ "Positive", "Negative" }, 1);
    addAndMakeVisible(polarityBox);
    polarityLabel.setText("Polarity", juce::dontSendNotification);
    polarityLabel.setJustificationType(juce::Justification::centred);
    polarityLabel.setColour(juce::Label::textColourId, pinkui::textWhite);
    addAndMakeVisible(polarityLabel);

    gateLabel.setText("Gate (-48 dB)", juce::dontSendNotification);
    gateLabel.setJustificationType(juce::Justification::centred);
    gateLabel.setColour(juce::Label::textColourId, pinkui::textWhite);
    addAndMakeVisible(gateLabel);

    gateButton.setClickingTogglesState(true);
    gateButton.setButtonText("OFF");
    gateButton.onStateChange = [this] {
        gateButton.setButtonText(gateButton.getToggleState() ? "ON" : "OFF");
    };
    addAndMakeVisible(gateButton);

    drawerButton.setButtonText("MODULATION");
    drawerButton.onClick = [this] {
        isDrawerOpen = !isDrawerOpen;
        forceLfoUiRefresh = true;
        setSize(850, isDrawerOpen ? 830 : 480);
    };
    addAndMakeVisible(drawerButton);

    const juce::StringArray names = {
        "Pitch", "Feed", "InCut", "LoopC", juce::String::fromUTF8("H\u03B2"),
        "Inharm", "Cross", "Self", "Drift", "DryWet", "WetGain", "Macro", "LFO 1", "LFO 2"
    };
    const juce::StringArray m1Ids = {
        Params::m1_pitch, Params::m1_feed, Params::m1_incut, Params::m1_loopcut, Params::m1_hbeta,
        Params::m1_inharm, Params::m1_cross, Params::m1_self, Params::m1_drift, Params::m1_drywet,
        Params::m1_wetgain, Params::m1_macro
    };
    const juce::StringArray m2Ids = {
        Params::m2_pitch, Params::m2_feed, Params::m2_incut, Params::m2_loopcut, Params::m2_hbeta,
        Params::m2_inharm, Params::m2_cross, Params::m2_self, Params::m2_drift, Params::m2_drywet,
        Params::m2_wetgain, Params::m2_macro
    };

    for (int i = 0; i < 14; ++i)
    {
        matrixLabels[i].setText(names[i], juce::dontSendNotification);
        matrixLabels[i].setJustificationType(juce::Justification::centred);
        matrixLabels[i].setColour(juce::Label::textColourId, pinkui::textWhite);
        addChildComponent(matrixLabels[i]);
    }

    matrixReadoutLabel.setJustificationType(juce::Justification::centredRight);
    matrixReadoutLabel.setColour(juce::Label::textColourId, pinkui::softPink);
    matrixReadoutLabel.setFont(juce::Font(juce::FontOptions(12.5f)));
    addChildComponent(matrixReadoutLabel);

    auto setupMini = [this](juce::Slider& s, const juce::String& labelText) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setDoubleClickReturnValue(true, 0.0);
        s.setLookAndFeel(&matrixLookAndFeel);
        s.onValueChange = [this, &s, labelText] {
            const double v = s.getValue();
            const juce::String sign = v > 0.0005 ? "+" : "";
            matrixReadoutLabel.setText(labelText + ": " + sign + juce::String(v, 2), juce::dontSendNotification);
        };
        addChildComponent(s);
    };

    for (int i = 0; i < 12; ++i)
    {
        setupMini(m1Sliders[i], "LFO1 -> " + names[i]);
        m1Atts[i] = std::make_unique<SliderAttachment>(processorRef.apvts, m1Ids[i], m1Sliders[i]);

        setupMini(m2Sliders[i], "LFO2 -> " + names[i]);
        m2Atts[i] = std::make_unique<SliderAttachment>(processorRef.apvts, m2Ids[i], m2Sliders[i]);
    }

    setupMini(m2Sliders[12], "LFO2 -> LFO1");
    m2Atts[12] = std::make_unique<SliderAttachment>(processorRef.apvts, Params::m2_lfo1, m2Sliders[12]);

    setupMini(m1Sliders[13], "LFO1 -> LFO2");
    m1Atts[13] = std::make_unique<SliderAttachment>(processorRef.apvts, Params::m1_lfo2, m1Sliders[13]);

    auto styleBypass = [](juce::TextButton& b, const juce::String& txt) {
        b.setButtonText(txt);
        b.setColour(juce::TextButton::buttonColourId, pinkui::surfaceDark);
        b.setColour(juce::TextButton::buttonOnColourId, pinkui::dangerRed.withAlpha(0.25f));
        b.setColour(juce::TextButton::textColourOffId, pinkui::textWhite);
        b.setColour(juce::TextButton::textColourOnId, pinkui::dangerRed);
        b.setClickingTogglesState(true);
    };
    styleBypass(lfo1BypassButton, "BYP\n1");
    styleBypass(lfo2BypassButton, "BYP\n2");
    addChildComponent(lfo1BypassButton);
    addChildComponent(lfo2BypassButton);

    lfo1BypassAtt = std::make_unique<ButtonAttachment>(processorRef.apvts, Params::lfo1Bypass, lfo1BypassButton);
    lfo2BypassAtt = std::make_unique<ButtonAttachment>(processorRef.apvts, Params::lfo2Bypass, lfo2BypassButton);

    lfo1RowLabel.setText("LFO 1", juce::dontSendNotification);
    lfo1RowLabel.setJustificationType(juce::Justification::centredLeft);
    lfo1RowLabel.setColour(juce::Label::textColourId, pinkui::neonPink);
    lfo1RowLabel.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    addChildComponent(lfo1RowLabel);

    lfo2RowLabel.setText("LFO 2", juce::dontSendNotification);
    lfo2RowLabel.setJustificationType(juce::Justification::centredLeft);
    lfo2RowLabel.setColour(juce::Label::textColourId, pinkui::neonPink);
    lfo2RowLabel.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    addChildComponent(lfo2RowLabel);

    const juce::StringArray syncs = {
        "Free", "4 bars", "2 bars", "1 bar",
        "1/1", "1/1 T", "1/1 D",
        "1/2", "1/2 T", "1/2 D",
        "1/4", "1/4 T", "1/4 D",
        "1/8", "1/8 T", "1/8 D",
        "1/16", "1/16 T", "1/16 D",
        "5/4", "6/4", "7/4", "9/4", "18/4"
    };
    const juce::StringArray shapes = {
        "Sine", "Tri", "Saw Up", "Sqr", "Pulse", "Ramp",
        "Exp Rse", "S&H", "Smth Rnd", "Logistic", "Stairs", "Dbl Sine", "SOMA"
    };

    lfo1SyncBox.addItemList(syncs, 1);  addChildComponent(lfo1SyncBox);
    lfo1ShapeBox.addItemList(shapes, 1); addChildComponent(lfo1ShapeBox);
    lfo2SyncBox.addItemList(syncs, 1);  addChildComponent(lfo2SyncBox);
    lfo2ShapeBox.addItemList(shapes, 1); addChildComponent(lfo2ShapeBox);

    auto setupLfoKnob = [this](juce::Slider& s, juce::Label& nameLabel, juce::Label& valLabel,
                               const juce::String& name, double defaultVal) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setDoubleClickReturnValue(true, defaultVal);
        addChildComponent(s);

        nameLabel.setText(name, juce::dontSendNotification);
        nameLabel.setJustificationType(juce::Justification::centred);
        nameLabel.setColour(juce::Label::textColourId, pinkui::textWhite);
        addChildComponent(nameLabel);

        valLabel.setJustificationType(juce::Justification::centred);
        valLabel.setColour(juce::Label::textColourId, pinkui::textWhite);
        valLabel.setColour(juce::Label::backgroundColourId, pinkui::surfaceDark);
        valLabel.setColour(juce::Label::outlineColourId, pinkui::borderDark);
        addChildComponent(valLabel);
    };

    setupLfoKnob(lfo1RateSlider, lfo1RateNameLabel, lfo1RateValueLabel, "Rate", 1.0);
    setupLfoKnob(lfo1PhaseSlider, lfo1PhaseNameLabel, lfo1PhaseValueLabel, "Phase", 0.0);
    setupLfoKnob(lfo1PwSlider, lfo1PwNameLabel, lfo1PwValueLabel, "PW", 0.5);
    setupLfoKnob(lfo1SmoothSlider, lfo1SmoothNameLabel, lfo1SmoothValueLabel, "Smooth", 0.0);

    setupLfoKnob(lfo2RateSlider, lfo2RateNameLabel, lfo2RateValueLabel, "Rate", 1.0);
    setupLfoKnob(lfo2PhaseSlider, lfo2PhaseNameLabel, lfo2PhaseValueLabel, "Phase", 0.0);
    setupLfoKnob(lfo2PwSlider, lfo2PwNameLabel, lfo2PwValueLabel, "PW", 0.5);
    setupLfoKnob(lfo2SmoothSlider, lfo2SmoothNameLabel, lfo2SmoothValueLabel, "Smooth", 0.0);

    lfo1SyncAtt   = std::make_unique<ComboAttachment>(processorRef.apvts, Params::lfo1Sync, lfo1SyncBox);
    lfo1ShapeAtt  = std::make_unique<ComboAttachment>(processorRef.apvts, Params::lfo1Shape, lfo1ShapeBox);
    lfo1RateAtt   = std::make_unique<SliderAttachment>(processorRef.apvts, Params::lfo1Rate, lfo1RateSlider);
    lfo1PhaseAtt  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::lfo1Phase, lfo1PhaseSlider);
    lfo1PwAtt     = std::make_unique<SliderAttachment>(processorRef.apvts, Params::lfo1Pw, lfo1PwSlider);
    lfo1SmoothAtt = std::make_unique<SliderAttachment>(processorRef.apvts, Params::lfo1Smooth, lfo1SmoothSlider);

    lfo2SyncAtt   = std::make_unique<ComboAttachment>(processorRef.apvts, Params::lfo2Sync, lfo2SyncBox);
    lfo2ShapeAtt  = std::make_unique<ComboAttachment>(processorRef.apvts, Params::lfo2Shape, lfo2ShapeBox);
    lfo2RateAtt   = std::make_unique<SliderAttachment>(processorRef.apvts, Params::lfo2Rate, lfo2RateSlider);
    lfo2PhaseAtt  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::lfo2Phase, lfo2PhaseSlider);
    lfo2PwAtt     = std::make_unique<SliderAttachment>(processorRef.apvts, Params::lfo2Pw, lfo2PwSlider);
    lfo2SmoothAtt = std::make_unique<SliderAttachment>(processorRef.apvts, Params::lfo2Smooth, lfo2SmoothSlider);

    polarityAtt = std::make_unique<ComboAttachment>(processorRef.apvts, Params::polarity, polarityBox);
    gateAtt     = std::make_unique<ButtonAttachment>(processorRef.apvts, Params::gateEnabled, gateButton);
    gateButton.setButtonText(gateButton.getToggleState() ? "ON" : "OFF");

    mainAtts[0]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::pitch, pitchSlider);
    mainAtts[1]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::feedback, decaySlider);
    mainAtts[2]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::inputCutoff, inputCutoffSlider);
    mainAtts[3]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::loopCutoff, loopCutoffSlider);
    mainAtts[4]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::pluck, pluckSlider);
    mainAtts[5]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::inharm, inharmSlider);
    mainAtts[6]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::crossFb, crossFbSlider);
    mainAtts[7]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::selfMod, selfModSlider);
    mainAtts[8]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::jitter, jitterSlider);
    mainAtts[9]  = std::make_unique<SliderAttachment>(processorRef.apvts, Params::dryWet, dryWetSlider);
    mainAtts[10] = std::make_unique<SliderAttachment>(processorRef.apvts, Params::wetGain, wetGainSlider);
    mainAtts[11] = std::make_unique<SliderAttachment>(processorRef.apvts, Params::macro, macroSlider);

    setSize(850, 480);
    startTimerHz(30);
}

PinkSwanAudioProcessorEditor::~PinkSwanAudioProcessorEditor()
{
    stopTimer();

    for (int i = 0; i < 14; ++i)
    {
        m1Sliders[i].setLookAndFeel(nullptr);
        m2Sliders[i].setLookAndFeel(nullptr);
    }
    setLookAndFeel(nullptr);
}

void PinkSwanAudioProcessorEditor::timerCallback()
{
    const uint32_t rev = processorRef.getStateRevision();
    if (rev != lastSeenStateRevision)
    {
        lastSeenStateRevision = rev;
        presetBar.syncFromManager();
    }

    // Alerte visuelle sur CrossFB (uniquement si la couleur change)
    const float fb = processorRef.apvts.getRawParameterValue(Params::feedback)->load();
    const float crossFb = processorRef.apvts.getRawParameterValue(Params::crossFb)->load();
    juce::Colour targetCrossCol = pinkui::neonPink;
    if (fb > 0.5f && crossFb > 0.3f)
    {
        if (crossFb < 0.6f)
            targetCrossCol = pinkui::neonPink.interpolatedWith(pinkui::dangerRed, (crossFb - 0.3f) / 0.3f);
        else
            targetCrossCol = pinkui::dangerRed;
    }
    if (targetCrossCol != lastCrossFbColour)
    {
        lastCrossFbColour = targetCrossCol;
        crossFbSlider.setColour(juce::Slider::rotarySliderFillColourId, targetCrossCol);
        crossFbSlider.repaint();
    }

    if (!isDrawerOpen)
        return;

    const float r1 = (float)lfo1RateSlider.getValue();
    const float p1 = (float)lfo1PhaseSlider.getValue();
    const float w1 = (float)lfo1PwSlider.getValue();
    const float s1 = (float)lfo1SmoothSlider.getValue();
    const int sync1 = lfo1SyncBox.getSelectedItemIndex();
    const bool byp1 = lfo1BypassButton.getToggleState();

    const float r2 = (float)lfo2RateSlider.getValue();
    const float p2 = (float)lfo2PhaseSlider.getValue();
    const float w2 = (float)lfo2PwSlider.getValue();
    const float s2 = (float)lfo2SmoothSlider.getValue();
    const int sync2 = lfo2SyncBox.getSelectedItemIndex();
    const bool byp2 = lfo2BypassButton.getToggleState();

    const auto deg = juce::String::fromUTF8("\u00B0");

    if (forceLfoUiRefresh || r1 != lastLfo1Rate)   { lfo1RateValueLabel.setText(juce::String(r1, 2), juce::dontSendNotification);   lastLfo1Rate = r1; }
    if (forceLfoUiRefresh || p1 != lastLfo1Phase)  { lfo1PhaseValueLabel.setText(juce::String(p1, 0) + deg, juce::dontSendNotification); lastLfo1Phase = p1; }
    if (forceLfoUiRefresh || w1 != lastLfo1Pw)     { lfo1PwValueLabel.setText(juce::String(w1, 2), juce::dontSendNotification);     lastLfo1Pw = w1; }
    if (forceLfoUiRefresh || s1 != lastLfo1Smooth) { lfo1SmoothValueLabel.setText(juce::String(s1, 2), juce::dontSendNotification); lastLfo1Smooth = s1; }

    if (forceLfoUiRefresh || r2 != lastLfo2Rate)   { lfo2RateValueLabel.setText(juce::String(r2, 2), juce::dontSendNotification);   lastLfo2Rate = r2; }
    if (forceLfoUiRefresh || p2 != lastLfo2Phase)  { lfo2PhaseValueLabel.setText(juce::String(p2, 0) + deg, juce::dontSendNotification); lastLfo2Phase = p2; }
    if (forceLfoUiRefresh || w2 != lastLfo2Pw)     { lfo2PwValueLabel.setText(juce::String(w2, 2), juce::dontSendNotification);     lastLfo2Pw = w2; }
    if (forceLfoUiRefresh || s2 != lastLfo2Smooth) { lfo2SmoothValueLabel.setText(juce::String(s2, 2), juce::dontSendNotification); lastLfo2Smooth = s2; }

    if (forceLfoUiRefresh || sync1 != lastLfo1Sync || byp1 != lastLfo1Bypass)
    {
        const bool rateActive = (!byp1) && (sync1 == 0);
        const float rowAlpha = byp1 ? 0.38f : 1.0f;
        lfo1RateSlider.setEnabled(rateActive);
        lfo1RateSlider.setAlpha(rateActive ? 1.0f : 0.35f);
        lfo1RateNameLabel.setAlpha(rateActive ? 1.0f : 0.35f);
        lfo1RateValueLabel.setAlpha(rateActive ? 1.0f : 0.35f);
        lfo1SyncBox.setAlpha(rowAlpha);
        lfo1ShapeBox.setAlpha(rowAlpha);
        lfo1PhaseSlider.setAlpha(rowAlpha);
        lfo1PwSlider.setAlpha(rowAlpha);
        lfo1SmoothSlider.setAlpha(rowAlpha);
        lastLfo1Sync = sync1;
        lastLfo1Bypass = byp1;
    }

    if (forceLfoUiRefresh || sync2 != lastLfo2Sync || byp2 != lastLfo2Bypass)
    {
        const bool rateActive = (!byp2) && (sync2 == 0);
        const float rowAlpha = byp2 ? 0.38f : 1.0f;
        lfo2RateSlider.setEnabled(rateActive);
        lfo2RateSlider.setAlpha(rateActive ? 1.0f : 0.35f);
        lfo2RateNameLabel.setAlpha(rateActive ? 1.0f : 0.35f);
        lfo2RateValueLabel.setAlpha(rateActive ? 1.0f : 0.35f);
        lfo2SyncBox.setAlpha(rowAlpha);
        lfo2ShapeBox.setAlpha(rowAlpha);
        lfo2PhaseSlider.setAlpha(rowAlpha);
        lfo2PwSlider.setAlpha(rowAlpha);
        lfo2SmoothSlider.setAlpha(rowAlpha);
        lastLfo2Sync = sync2;
        lastLfo2Bypass = byp2;
    }

    forceLfoUiRefresh = false;
}

void PinkSwanAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(pinkui::bgBlack);

    // En-tête : "PinkSwan" en un seul mot à gauche, PresetBar à droite
    auto header = getLocalBounds().reduced(10, 8).removeFromTop(32);
    auto titleArea = header.removeFromLeft(155);
    g.setColour(pinkui::textWhite);
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    g.drawFittedText("Pink", titleArea.withWidth(56), juce::Justification::centredLeft, 1);
    g.setColour(pinkui::neonPink);
    g.drawFittedText("Swan", titleArea.withTrimmedLeft(54), juce::Justification::centredLeft, 1);

    // Ligne séparatrice sous le header
    g.setColour(pinkui::borderDark);
    g.drawHorizontalLine(46, 10.0f, (float)(getWidth() - 10));

    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(40);
    if (isDrawerOpen)
        area.removeFromBottom(350);
    area.removeFromBottom(30);

    auto leftArea = area.removeFromLeft(150);
    g.setColour(pinkui::borderDark);
    g.drawVerticalLine(leftArea.getRight() + 10, (float)area.getY() + 4.0f, (float)area.getBottom() - 4.0f);

    area.removeFromLeft(20);
    const float colWidth = area.getWidth() / 4.0f;
    g.drawVerticalLine((int)(area.getX() + colWidth),     (float)area.getY() + 4.0f, (float)area.getBottom() - 4.0f);
    g.drawVerticalLine((int)(area.getX() + colWidth * 2), (float)area.getY() + 4.0f, (float)area.getBottom() - 4.0f);
    g.drawVerticalLine((int)(area.getX() + colWidth * 3), (float)area.getY() + 4.0f, (float)area.getBottom() - 4.0f);

    if (isDrawerOpen)
    {
        g.setColour(pinkui::borderDark);
        g.drawHorizontalLine(getHeight() - 355, 10.0f, (float)(getWidth() - 10));
    }
}

void PinkSwanAudioProcessorEditor::resized()
{
    auto full = getLocalBounds();

    // Header : titre "PinkSwan" à gauche (165 px), PresetBar à droite
    auto header = full.reduced(10, 8).removeFromTop(30);
    header.removeFromLeft(170);
    presetBar.setBounds(header);

    auto area = full.reduced(10);
    area.removeFromTop(42);
    if (isDrawerOpen)
        area.removeFromBottom(350);

    drawerButton.setBounds(full.removeFromBottom(26).reduced(10, 2));
    area.removeFromBottom(30);

    // Colonne de gauche : MACRO + Polarity + Gate (-48 dB)
    auto leftArea = area.removeFromLeft(150).reduced(10, 6);
    macroLabel.setBounds(leftArea.removeFromTop(24));
    macroSlider.setBounds(leftArea.removeFromTop(125));
    leftArea.removeFromTop(14);
    polarityLabel.setBounds(leftArea.removeFromTop(20));
    polarityBox.setBounds(leftArea.removeFromTop(28));
    leftArea.removeFromTop(14);
    gateLabel.setBounds(leftArea.removeFromTop(20));
    gateButton.setBounds(leftArea.removeFromTop(28).reduced(18, 0));

    area.removeFromLeft(20);
    const int cols = 4;
    const int colWidth = area.getWidth() / cols;
    const int rowHeight = area.getHeight() / 3;

    auto place = [&](juce::Slider& s, juce::Label& l, int col, int row) {
        juce::Rectangle<int> cell(area.getX() + col * colWidth, area.getY() + row * rowHeight, colWidth, rowHeight);
        cell = cell.reduced(10, 6);
        l.setBounds(cell.removeFromTop(20));
        s.setBounds(cell);
    };

    place(inputCutoffSlider, inputCutoffLabel, 0, 0);
    place(loopCutoffSlider, loopCutoffLabel, 0, 1);
    place(pitchSlider, pitchLabel, 0, 2);

    place(pluckSlider, pluckLabel, 1, 0);
    place(inharmSlider, inharmLabel, 1, 1);

    // Cadre central bas : icône Cygne 3/4 avant (bouton Panic)
    juce::Rectangle<int> swanCell(area.getX() + 1 * colWidth, area.getY() + 2 * rowHeight, colWidth, rowHeight);
    swanCell = swanCell.reduced(10);
    const int swanSize = juce::jmin(swanCell.getWidth(), swanCell.getHeight()) - 8;
    swanPanicButton.setBounds(swanCell.withSizeKeepingCentre(swanSize, swanSize));

    place(decaySlider, decayLabel, 2, 0);
    place(crossFbSlider, crossFbLabel, 2, 1);
    place(selfModSlider, selfModLabel, 2, 2);

    place(dryWetSlider, dryWetLabel, 3, 0);
    place(wetGainSlider, wetGainLabel, 3, 1);
    place(jitterSlider, jitterLabel, 3, 2);

    drawerButton.setButtonText(isDrawerOpen ? "MODULATION  -" : "MODULATION  +");

    if (isDrawerOpen)
    {
        auto drawerArea = full.removeFromBottom(348).reduced(10, 6);
        drawerArea.removeFromBottom(24);

        auto topDrawerRow = drawerArea.removeFromTop(20);
        matrixReadoutLabel.setBounds(topDrawerRow);
        matrixReadoutLabel.setVisible(true);

        auto matrixArea = drawerArea.removeFromTop(108);
        const int mCols = 14;

        auto labelArea = matrixArea.removeFromLeft(44);
        lfo1BypassButton.setBounds(labelArea.getX(), matrixArea.getY() + 2, 38, 50);
        lfo1BypassButton.setVisible(true);
        lfo2BypassButton.setBounds(labelArea.getX(), matrixArea.getY() + 56, 38, 50);
        lfo2BypassButton.setVisible(true);

        const int mWidth = matrixArea.getWidth() / mCols;
        const int mRowHeight = matrixArea.getHeight() / 2;

        for (int i = 0; i < mCols; ++i)
        {
            matrixLabels[i].setBounds(matrixArea.getX() + i * mWidth, topDrawerRow.getY(), mWidth, 20);
            matrixLabels[i].setVisible(true);
        }

        // Garde la lecture de valeur en haut à droite si la ligne de titres est au même niveau
        matrixReadoutLabel.setBounds(full.getWidth() - 250, topDrawerRow.getY() - 20, 240, 18);

        for (int i = 0; i < 12; ++i)
        {
            m1Sliders[i].setBounds(matrixArea.getX() + i * mWidth, matrixArea.getY(), mWidth, mRowHeight);
            m1Sliders[i].setVisible(true);
            m2Sliders[i].setBounds(matrixArea.getX() + i * mWidth, matrixArea.getY() + mRowHeight, mWidth, mRowHeight);
            m2Sliders[i].setVisible(true);
        }

        m1Sliders[12].setVisible(false);
        m1Sliders[13].setBounds(matrixArea.getX() + 13 * mWidth, matrixArea.getY(), mWidth, mRowHeight);
        m1Sliders[13].setVisible(true);

        m2Sliders[12].setBounds(matrixArea.getX() + 12 * mWidth, matrixArea.getY() + mRowHeight, mWidth, mRowHeight);
        m2Sliders[12].setVisible(true);
        m2Sliders[13].setVisible(false);

        drawerArea.removeFromTop(14);

        auto layoutLfoRow = [&](juce::Rectangle<int> row,
                                juce::Label& rowLabel,
                                CustomComboBox& syncBox, CustomComboBox& shapeBox,
                                juce::Slider& rateS, juce::Label& rateNL, juce::Label& rateVL,
                                juce::Slider& phaseS, juce::Label& phaseNL, juce::Label& phaseVL,
                                juce::Slider& pwS, juce::Label& pwNL, juce::Label& pwVL,
                                juce::Slider& smS, juce::Label& smNL, juce::Label& smVL)
        {
            auto titleCol = row.removeFromLeft(56);
            rowLabel.setBounds(titleCol.getX(), titleCol.getY() + 30, 52, 22);
            rowLabel.setVisible(true);

            syncBox.setBounds(row.removeFromLeft(110).reduced(4, 26));
            syncBox.setVisible(true);
            shapeBox.setBounds(row.removeFromLeft(110).reduced(4, 26));
            shapeBox.setVisible(true);

            auto placeLfoKnob = [&](juce::Slider& s, juce::Label& nl, juce::Label& vl) {
                auto box = row.removeFromLeft(130).reduced(6, 0);
                auto left = box.removeFromLeft(52);
                nl.setBounds(left.removeFromTop(24)); nl.setVisible(true);
                vl.setBounds(left.removeFromTop(22).reduced(0, 2)); vl.setVisible(true);
                s.setBounds(box); s.setVisible(true);
            };

            placeLfoKnob(rateS, rateNL, rateVL);
            placeLfoKnob(phaseS, phaseNL, phaseVL);
            placeLfoKnob(pwS, pwNL, pwVL);
            placeLfoKnob(smS, smNL, smVL);
        };

        layoutLfoRow(drawerArea.removeFromTop(88), lfo1RowLabel, lfo1SyncBox, lfo1ShapeBox,
                     lfo1RateSlider, lfo1RateNameLabel, lfo1RateValueLabel,
                     lfo1PhaseSlider, lfo1PhaseNameLabel, lfo1PhaseValueLabel,
                     lfo1PwSlider, lfo1PwNameLabel, lfo1PwValueLabel,
                     lfo1SmoothSlider, lfo1SmoothNameLabel, lfo1SmoothValueLabel);

        drawerArea.removeFromTop(6);

        layoutLfoRow(drawerArea.removeFromTop(88), lfo2RowLabel, lfo2SyncBox, lfo2ShapeBox,
                     lfo2RateSlider, lfo2RateNameLabel, lfo2RateValueLabel,
                     lfo2PhaseSlider, lfo2PhaseNameLabel, lfo2PhaseValueLabel,
                     lfo2PwSlider, lfo2PwNameLabel, lfo2PwValueLabel,
                     lfo2SmoothSlider, lfo2SmoothNameLabel, lfo2SmoothValueLabel);
    }
    else
    {
        matrixReadoutLabel.setVisible(false);
        for (int i = 0; i < 14; ++i)
        {
            matrixLabels[i].setVisible(false);
            m1Sliders[i].setVisible(false);
            m2Sliders[i].setVisible(false);
        }

        lfo1BypassButton.setVisible(false);
        lfo2BypassButton.setVisible(false);
        lfo1RowLabel.setVisible(false);
        lfo2RowLabel.setVisible(false);

        lfo1SyncBox.setVisible(false);  lfo1ShapeBox.setVisible(false);
        lfo1RateSlider.setVisible(false);  lfo1RateNameLabel.setVisible(false);  lfo1RateValueLabel.setVisible(false);
        lfo1PhaseSlider.setVisible(false); lfo1PhaseNameLabel.setVisible(false); lfo1PhaseValueLabel.setVisible(false);
        lfo1PwSlider.setVisible(false);    lfo1PwNameLabel.setVisible(false);    lfo1PwValueLabel.setVisible(false);
        lfo1SmoothSlider.setVisible(false);lfo1SmoothNameLabel.setVisible(false);lfo1SmoothValueLabel.setVisible(false);

        lfo2SyncBox.setVisible(false);  lfo2ShapeBox.setVisible(false);
        lfo2RateSlider.setVisible(false);  lfo2RateNameLabel.setVisible(false);  lfo2RateValueLabel.setVisible(false);
        lfo2PhaseSlider.setVisible(false); lfo2PhaseNameLabel.setVisible(false); lfo2PhaseValueLabel.setVisible(false);
        lfo2PwSlider.setVisible(false);    lfo2PwNameLabel.setVisible(false);    lfo2PwValueLabel.setVisible(false);
        lfo2SmoothSlider.setVisible(false);lfo2SmoothNameLabel.setVisible(false);lfo2SmoothValueLabel.setVisible(false);
    }
}
