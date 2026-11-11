#include "PluginProcessor.h"
#include "PluginEditor.h"

void BigVoixBarMeter::paint(juce::Graphics& g)
{
    using namespace BigVoixColors;

    const auto area = getLocalBounds().toFloat();
    const float barY = showBestBracket ? (area.getY() + 14.0f) : (area.getY() + 6.0f);
    const float barH = 10.0f;
    const juce::Rectangle<float> trackRect(area.getX(), barY, area.getWidth() - 16.0f, barH);

    if (showBestBracket)
    {
        auto dbToX = [&trackRect](float db) {
            const float norm = std::clamp((db + 48.0f) / 48.0f, 0.0f, 1.0f);
            return trackRect.getX() + norm * trackRect.getWidth();
        };

        const float bestX1 = dbToX(-22.0f);
        const float bestX2 = dbToX(-14.0f);
        const float brakY  = barY - 3.0f;

        g.setColour(lavenderAccent);
        g.drawHorizontalLine(juce::roundToInt(brakY), bestX1, bestX2);
        g.drawVerticalLine(juce::roundToInt(bestX1), brakY, brakY + 3.0f);
        g.drawVerticalLine(juce::roundToInt(bestX2), brakY, brakY + 3.0f);

        g.setFont(makeFont(9.5f, true));
        g.drawText("BEST", juce::Rectangle<float>(bestX1 - 6.0f, area.getY() - 1.0f, (bestX2 - bestX1) + 12.0f, 11.0f),
                   juce::Justification::centred, false);
    }

    g.setColour(woodDarkBg.darker(0.4f));
    g.fillRoundedRectangle(trackRect, 4.0f);
    g.setColour(woodBorder);
    g.drawRoundedRectangle(trackRect, 4.0f, 1.2f);

    const float fillNorm = std::clamp((smoothDb + 48.0f) / 48.0f, 0.0f, 1.0f);
    if (fillNorm > 0.005f)
    {
        juce::ColourGradient grad(meterPurple, trackRect.getX(), barY,
                                  meterPink, trackRect.getRight(), barY, false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(trackRect.withWidth(trackRect.getWidth() * fillNorm), 4.0f);
    }

    const juce::Rectangle<float> clipRect(trackRect.getRight() + 4.0f, barY, 10.0f, barH);
    g.setColour(hasClipped ? clipRed : woodDarkBg);
    g.fillRoundedRectangle(clipRect, 3.0f);
    g.setColour(hasClipped ? juce::Colours::white : woodBorder);
    g.drawRoundedRectangle(clipRect, 3.0f, 1.0f);
}

BigVoixAudioProcessorEditor::BigVoixAudioProcessorEditor(BigVoixAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    using namespace BigVoixColors;
    using namespace BigVoixParamIDs;

    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(canvas);
    canvas.setInterceptsMouseClicks(false, true);

    addMouseListener(this, true);

    auto& apvts = audioProcessor.getAPVTS();

    canvas.addAndMakeVisible(lowCutBtn);
    lowCutAttachment = std::make_unique<ButtonAttachment>(apvts, lowCut, lowCutBtn);

    autoInputBtn.onClick = [this]()
    {
        if (audioProcessor.getTelemetry().autoInputListening.load(std::memory_order_relaxed))
            audioProcessor.cancelAutoInputAnalysis();
        else
            audioProcessor.startAutoInputAnalysis();
    };
    canvas.addAndMakeVisible(autoInputBtn);

    prevPresetBtn.onClick = [this]()
    {
        audioProcessor.loadPreviousPreset();
        presetNameBtn.setButtonText(audioProcessor.getCurrentPresetName());
    };
    nextPresetBtn.onClick = [this]()
    {
        audioProcessor.loadNextPreset();
        presetNameBtn.setButtonText(audioProcessor.getCurrentPresetName());
    };
    presetNameBtn.onClick = [this]() { showPresetsPopupMenu(); };
    saveDiskBtn.onClick   = [this]() { showSavePresetDialog(); };
    menuBurgerBtn.onClick = [this]() { showHamburgerMenu(); };

    saveDiskBtn.getProperties().set("isDiskIcon", true);
    menuBurgerBtn.getProperties().set("isBurgerIcon", true);

    canvas.addAndMakeVisible(prevPresetBtn);
    canvas.addAndMakeVisible(presetNameBtn);
    canvas.addAndMakeVisible(nextPresetBtn);
    canvas.addAndMakeVisible(saveDiskBtn);
    canvas.addAndMakeVisible(menuBurgerBtn);

    compKnob.configure(70.0, false, true);
    canvas.addAndMakeVisible(compKnob);
    compAttachment = std::make_unique<SliderAttachment>(apvts, comp, compKnob);

    eqTitleBtn.getProperties().set("isSectionHeader", true);
    canvas.addAndMakeVisible(eqTitleBtn);
    eqBypassAttachment = std::make_unique<ButtonAttachment>(apvts, eqBypass, eqTitleBtn);

    eqLowKnob.configure(0.0, true);
    eqMidKnob.configure(0.0, true);
    eqHiKnob.configure(0.0, true);
    eqAirKnob.configure(0.0, true);

    canvas.addAndMakeVisible(eqLowKnob);
    canvas.addAndMakeVisible(eqMidKnob);
    canvas.addAndMakeVisible(eqHiKnob);
    canvas.addAndMakeVisible(eqAirKnob);

    eqLowAtt = std::make_unique<SliderAttachment>(apvts, eqLowDb, eqLowKnob);
    eqMidAtt = std::make_unique<SliderAttachment>(apvts, eqMidDb, eqMidKnob);
    eqHiAtt  = std::make_unique<SliderAttachment>(apvts, eqHiDb,  eqHiKnob);
    eqAirAtt = std::make_unique<SliderAttachment>(apvts, eqAirDb, eqAirKnob);

    fxTitleBtn.getProperties().set("isSectionHeader", true);
    canvas.addAndMakeVisible(fxTitleBtn);
    fxBypassAttachment = std::make_unique<ButtonAttachment>(apvts, fxBypass, fxTitleBtn);

    fxDeEssKnob.configure(35.0);
    fxDriveKnob.configure(0.0);
    fxPhoneKnob.configure(0.0);
    fxDoublerKnob.configure(0.0);
    fxChorusKnob.configure(0.0);

    canvas.addAndMakeVisible(fxDeEssKnob);
    canvas.addAndMakeVisible(fxDriveKnob);
    canvas.addAndMakeVisible(fxPhoneKnob);
    canvas.addAndMakeVisible(fxDoublerKnob);
    canvas.addAndMakeVisible(fxChorusKnob);

    fxDeEssAtt   = std::make_unique<SliderAttachment>(apvts, fxDeEss,   fxDeEssKnob);
    fxDriveAtt   = std::make_unique<SliderAttachment>(apvts, fxDrive,   fxDriveKnob);
    fxPhoneAtt   = std::make_unique<SliderAttachment>(apvts, fxPhone,   fxPhoneKnob);
    fxDoublerAtt = std::make_unique<SliderAttachment>(apvts, fxDoubler, fxDoublerKnob);
    fxChorusAtt  = std::make_unique<SliderAttachment>(apvts, fxChorus,  fxChorusKnob);

    dlyTitleBtn.getProperties().set("isSectionHeader", true);
    canvas.addAndMakeVisible(dlyTitleBtn);
    dlyCutAttachment = std::make_unique<ButtonAttachment>(apvts, dlyCut, dlyTitleBtn);

    dlySendKnob.configure(0.0);
    dlyFeedKnob.configure(30.0);
    dlyFilterKnob.configure(50.0);
    dlyFeedKnob.getProperties().set("alwaysActive", true);
    dlyFilterKnob.getProperties().set("alwaysActive", true);

    canvas.addAndMakeVisible(dlySendKnob);
    canvas.addAndMakeVisible(dlyFeedKnob);
    canvas.addAndMakeVisible(dlyFilterKnob);

    dlySendAtt   = std::make_unique<SliderAttachment>(apvts, dlySend,   dlySendKnob);
    dlyFeedAtt   = std::make_unique<SliderAttachment>(apvts, dlyFeed,   dlyFeedKnob);
    dlyFilterAtt = std::make_unique<SliderAttachment>(apvts, dlyFilter, dlyFilterKnob);

    dlyTimePrevBtn.getProperties().set("isVioletTimeBtn", true);
    dlyTimeValBtn.getProperties().set("isVioletTimeBtn", true);
    dlyTimeNextBtn.getProperties().set("isVioletTimeBtn", true);

    dlyTimePrevBtn.onClick = [this]() { stepDelayTimeDivision(-1); };
    dlyTimeNextBtn.onClick = [this]() { stepDelayTimeDivision(+1); };
    dlyTimeValBtn.onClick  = [this]()
    {
        juce::PopupMenu m;
        for (size_t i = 0; i < BigVoixRhythm::kDelayDivisions.size(); ++i)
            m.addItem(static_cast<int>(i) + 1, BigVoixRhythm::kDelayDivisions[i].label);

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&dlyTimeValBtn),
                        [this](int res)
                        {
                            if (res > 0)
                            {
                                if (auto* param = audioProcessor.getAPVTS().getParameter(BigVoixParamIDs::dlyTimeDiv))
                                    param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(res - 1)));
                            }
                        });
    };

    canvas.addAndMakeVisible(dlyTimePrevBtn);
    canvas.addAndMakeVisible(dlyTimeValBtn);
    canvas.addAndMakeVisible(dlyTimeNextBtn);
    canvas.addAndMakeVisible(dlyPingPongBtn);
    dlyPingPongAttachment = std::make_unique<ButtonAttachment>(apvts, dlyPingPong, dlyPingPongBtn);

    revTitleBtn.getProperties().set("isSectionHeader", true);
    canvas.addAndMakeVisible(revTitleBtn);
    revCutAttachment = std::make_unique<ButtonAttachment>(apvts, revCut, revTitleBtn);

    revShortKnob.configure(0.0);
    revSmallKnob.configure(20.0);
    revBigKnob.configure(0.0);
    revLushKnob.configure(0.0);

    canvas.addAndMakeVisible(revShortKnob);
    canvas.addAndMakeVisible(revSmallKnob);
    canvas.addAndMakeVisible(revBigKnob);
    canvas.addAndMakeVisible(revLushKnob);

    revShortAtt = std::make_unique<SliderAttachment>(apvts, revShortSend, revShortKnob);
    revSmallAtt = std::make_unique<SliderAttachment>(apvts, revSmallSend, revSmallKnob);
    revBigAtt   = std::make_unique<SliderAttachment>(apvts, revBigSend,   revBigKnob);
    revLushAtt  = std::make_unique<SliderAttachment>(apvts, revLushSend,  revLushKnob);

    revShortCutBtn.getProperties().set("isReverbCutLabel", true);
    revSmallCutBtn.getProperties().set("isReverbCutLabel", true);
    revBigCutBtn.getProperties().set("isReverbCutLabel", true);
    revLushCutBtn.getProperties().set("isReverbCutLabel", true);

    canvas.addAndMakeVisible(revShortCutBtn);
    canvas.addAndMakeVisible(revSmallCutBtn);
    canvas.addAndMakeVisible(revBigCutBtn);
    canvas.addAndMakeVisible(revLushCutBtn);

    revShortCutAtt = std::make_unique<ButtonAttachment>(apvts, revShortCut, revShortCutBtn);
    revSmallCutAtt = std::make_unique<ButtonAttachment>(apvts, revSmallCut, revSmallCutBtn);
    revBigCutAtt   = std::make_unique<ButtonAttachment>(apvts, revBigCut,   revBigCutBtn);
    revLushCutAtt  = std::make_unique<ButtonAttachment>(apvts, revLushCut,  revLushCutBtn);

    inputMeter.onClipClicked  = [this]() { audioProcessor.clearInputClipFlag(); };
    outputMeter.onClipClicked = [this]() { audioProcessor.clearOutputClipFlag(); };
    canvas.addAndMakeVisible(inputMeter);
    canvas.addAndMakeVisible(outputMeter);

    inGainKnob.configure(0.0);
    outGainKnob.configure(0.0);
    inGainKnob.getProperties().set("alwaysActive", true);
    outGainKnob.getProperties().set("alwaysActive", true);

    canvas.addAndMakeVisible(inGainKnob);
    canvas.addAndMakeVisible(outGainKnob);

    inGainAtt  = std::make_unique<SliderAttachment>(apvts, inGainDb,  inGainKnob);
    outGainAtt = std::make_unique<SliderAttachment>(apvts, outGainDb, outGainKnob);

    canvas.setSize(kBaseWidth, kBaseHeight);
    applyCurrentScale();

    startTimerHz(60);
}

BigVoixAudioProcessorEditor::~BigVoixAudioProcessorEditor()
{
    stopTimer();
    removeMouseListener(this);
    setLookAndFeel(nullptr);
}

void BigVoixAudioProcessorEditor::applyCurrentScale()
{
    const float s = audioProcessor.getUiScaleFactor();
    canvas.setTransform(juce::AffineTransform::scale(s));
    setSize(juce::roundToInt(static_cast<float>(kBaseWidth)  * s),
            juce::roundToInt(static_cast<float>(kBaseHeight) * s));
}

void BigVoixAudioProcessorEditor::stepDelayTimeDivision(int delta)
{
    auto* raw = audioProcessor.getAPVTS().getRawParameterValue(BigVoixParamIDs::dlyTimeDiv);
    if (raw == nullptr) return;

    const int cur  = static_cast<int>(raw->load(std::memory_order_relaxed));
    const int next = std::clamp(cur + delta, 0, static_cast<int>(BigVoixRhythm::kDelayDivisions.size()) - 1);
    if (auto* param = audioProcessor.getAPVTS().getParameter(BigVoixParamIDs::dlyTimeDiv))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(next)));
}

void BigVoixAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    const auto ptInCanvas = e.getEventRelativeTo(&canvas).position;

    if (audioProcessor.getTelemetry().autoInputListening.load(std::memory_order_relaxed))
    {
        if (e.eventComponent != &autoInputBtn)
        {
            audioProcessor.cancelAutoInputAnalysis();
            return;
        }
    }

    if (characterMouthPath.contains(ptInCanvas))
    {
        if (auto* bypParam = audioProcessor.getAPVTS().getParameter(BigVoixParamIDs::globalBypass))
        {
            const bool currentlyBypassed = bypParam->getValue() > 0.5f;
            bypParam->setValueNotifyingHost(currentlyBypassed ? 0.0f : 1.0f);
            repaint();
        }
    }
}

void BigVoixAudioProcessorEditor::timerCallback()
{
    const auto& tel = audioProcessor.getTelemetry();
    auto& apvts     = audioProcessor.getAPVTS();

    inputMeter.updateLevel(tel.inputRmsDb.load(std::memory_order_relaxed),
                           tel.inputClipped.load(std::memory_order_relaxed));
    outputMeter.updateLevel(tel.outputRmsDb.load(std::memory_order_relaxed),
                            tel.outputClipped.load(std::memory_order_relaxed));

    const bool eqOff  = eqTitleBtn.getToggleState();
    const bool fxOff  = fxTitleBtn.getToggleState();
    const bool dlyOff = dlyTitleBtn.getToggleState() || (dlySendKnob.getValue() <= 0.15);
    const bool revOff = revTitleBtn.getToggleState();

    eqLowKnob.setSectionMuted(eqOff);
    eqMidKnob.setSectionMuted(eqOff);
    eqHiKnob.setSectionMuted(eqOff);
    eqAirKnob.setSectionMuted(eqOff);

    fxDeEssKnob.setSectionMuted(fxOff);
    fxDriveKnob.setSectionMuted(fxOff);
    fxPhoneKnob.setSectionMuted(fxOff);
    fxDoublerKnob.setSectionMuted(fxOff);
    fxChorusKnob.setSectionMuted(fxOff);

    dlySendKnob.setSectionMuted(dlyTitleBtn.getToggleState());
    dlyFeedKnob.setSectionMuted(dlyOff);
    dlyFilterKnob.setSectionMuted(dlyOff);

    revShortKnob.setSectionMuted(revOff || revShortCutBtn.getToggleState());
    revSmallKnob.setSectionMuted(revOff || revSmallCutBtn.getToggleState());
    revBigKnob.setSectionMuted(revOff   || revBigCutBtn.getToggleState());
    revLushKnob.setSectionMuted(revOff  || revLushCutBtn.getToggleState());

    const int divIdx = std::clamp(
        static_cast<int>(apvts.getRawParameterValue(BigVoixParamIDs::dlyTimeDiv)->load(std::memory_order_relaxed)),
        0, 20);
    const juce::String divLabel = BigVoixRhythm::kDelayDivisions[static_cast<size_t>(divIdx)].label;
    if (dlyTimeValBtn.getButtonText() != divLabel)
        dlyTimeValBtn.setButtonText(divLabel);

    const juce::String curPreset = audioProcessor.getCurrentPresetName();
    if (presetNameBtn.getButtonText() != curPreset)
        presetNameBtn.setButtonText(curPreset);

    repaint();
}

void BigVoixAudioProcessorEditor::paint(juce::Graphics& g)
{
    using namespace BigVoixColors;

    const float s = audioProcessor.getUiScaleFactor();
    g.addTransform(juce::AffineTransform::scale(s));

    const bool isGlobalBypassed = audioProcessor.getAPVTS()
                                      .getRawParameterValue(BigVoixParamIDs::globalBypass)
                                      ->load(std::memory_order_relaxed) > 0.5f;

    const juce::Rectangle<float> fullRect(0.0f, 0.0f, static_cast<float>(kBaseWidth), static_cast<float>(kBaseHeight));
    juce::ColourGradient mainBg(leftBgCenter, 220.0f, 260.0f, leftBgEdge.darker(0.35f), 900.0f, 520.0f, true);
    g.setGradientFill(mainBg);
    g.fillRect(fullRect);

    const auto compCenter = compKnob.getBounds().toFloat().getCentre();
    juce::ColourGradient compGlow(neonHalo.withAlpha(isGlobalBypassed ? 0.10f : 0.42f), compCenter.x, compCenter.y,
                                  neonHalo.withAlpha(0.0f), compCenter.x + 175.0f, compCenter.y, true);
    g.setGradientFill(compGlow);
    g.fillEllipse(compCenter.x - 175.0f, compCenter.y - 175.0f, 350.0f, 350.0f);

    const float compNorm = static_cast<float>(compKnob.getValue() * 0.01);
    const float compGrDb = audioProcessor.getTelemetry().compGainReductionDb.load(std::memory_order_relaxed);
    const juce::Rectangle<float> leftPanelRect(0.0f, 0.0f, 436.0f, static_cast<float>(kBaseHeight));

    BigVoixLookAndFeel::drawCartoonCharacter(g, leftPanelRect, compNorm, compGrDb,
                                             isGlobalBypassed, characterMouthPath);

    g.setColour(isGlobalBypassed ? textDisabled : juce::Colours::white);
    g.setFont(makeFont(30.0f, true, true));
    g.drawText("COMP", juce::Rectangle<float>(compCenter.x - 80.0f, static_cast<float>(compKnob.getBottom()) + 4.0f, 160.0f, 34.0f),
               juce::Justification::centred, false);

    BigVoixLookAndFeel::drawBeveledWoodStrip(g, eqStripRect,  !eqTitleBtn.getToggleState());
    BigVoixLookAndFeel::drawBeveledWoodStrip(g, fxStripRect,  !fxTitleBtn.getToggleState());
    BigVoixLookAndFeel::drawBeveledWoodStrip(g, dlyStripRect, !dlyTitleBtn.getToggleState());
    BigVoixLookAndFeel::drawBeveledWoodStrip(g, revStripRect, !revTitleBtn.getToggleState());

    BigVoixLookAndFeel::drawBeveledWoodStrip(g, logoStripRect, true);
    {
        const float waveCenterY = logoStripRect.getCentreY();
        const float startX      = logoStripRect.getX() + 24.0f;
        const float endX        = logoStripRect.getRight() - 24.0f;
        int barIndex = 0;

        g.setColour(waveformBg.withAlpha(0.75f));
        for (float wx = startX; wx < endX; wx += 6.0f, ++barIndex)
        {
            const float env = std::sin(static_cast<float>(barIndex) * 0.37f) * 0.5f
                            + std::cos(static_cast<float>(barIndex) * 0.83f) * 0.5f;
            const float barH = 6.0f + std::abs(env) * (logoStripRect.getHeight() - 14.0f);
            g.fillRoundedRectangle(wx, waveCenterY - barH * 0.5f, 3.0f, barH, 1.5f);
        }

        g.setFont(makeFont(34.0f, true, true));
        g.setColour(neonHalo.withAlpha(0.65f));
        g.drawText("BigVoix", logoStripRect.translated(0.0f, 1.5f), juce::Justification::centred, false);
        g.setColour(logoNeonCore);
        g.drawText("BigVoix", logoStripRect, juce::Justification::centred, false);
    }

    g.setFont(makeFont(12.5f, true));
    auto drawKnobLabel = [&g](const juce::Component& knob, const juce::String& text, bool active)
    {
        g.setColour(active ? lavenderAccent : textDisabled);
        g.drawText(text, juce::Rectangle<int>(knob.getX() - 8, knob.getBottom() - 2, knob.getWidth() + 16, 18),
                   juce::Justification::centred, false);
    };

    const bool eqOn = !eqTitleBtn.getToggleState();
    drawKnobLabel(eqLowKnob, "Low", eqOn);
    drawKnobLabel(eqMidKnob, "Mid", eqOn);
    drawKnobLabel(eqHiKnob,  "Hi",  eqOn);
    drawKnobLabel(eqAirKnob, "Air", eqOn);

    const bool fxOn = !fxTitleBtn.getToggleState();
    drawKnobLabel(fxDeEssKnob,   "De-ess",  fxOn);
    drawKnobLabel(fxDriveKnob,   "Drive",   fxOn);
    drawKnobLabel(fxPhoneKnob,   "Phone",   fxOn);
    drawKnobLabel(fxDoublerKnob, "Doubler", fxOn);
    drawKnobLabel(fxChorusKnob,  "Chorus",  fxOn);

    {
        const float deEssLedNorm = audioProcessor.getTelemetry().deEssGainReduction.load(std::memory_order_relaxed);
        const juce::Rectangle<float> ledBox(static_cast<float>(fxDeEssKnob.getRight()) - 4.0f,
                                            static_cast<float>(fxDeEssKnob.getY()) + 6.0f,
                                            8.5f, 8.5f);
        g.setColour(woodDarkBg);
        g.fillEllipse(ledBox);
        if (deEssLedNorm > 0.02f && fxOn)
        {
            g.setColour(meterPink.withAlpha(std::clamp(deEssLedNorm * 0.45f, 0.0f, 0.45f)));
            g.fillEllipse(ledBox.expanded(3.5f));
            g.setColour(meterPurple.interpolatedWith(meterPink, deEssLedNorm));
            g.fillEllipse(ledBox);
        }
        g.setColour(woodBorder);
        g.drawEllipse(ledBox, 1.0f);
    }

    const bool dlyOn = !dlyTitleBtn.getToggleState();
    drawKnobLabel(dlySendKnob, "Delay", dlyOn);

    g.setFont(makeFont(11.0f, true));
    g.setColour(dlyOn ? lavenderAccent : textDisabled);
    g.drawText("Feed",   dlyFeedKnob.getRight() + 2,   dlyFeedKnob.getY(),   48, dlyFeedKnob.getHeight(),   juce::Justification::centredLeft, false);
    g.drawText("Filter", dlyFilterKnob.getRight() + 2, dlyFilterKnob.getY(), 48, dlyFilterKnob.getHeight(), juce::Justification::centredLeft, false);
    g.drawText("Time",   dlyTimeValBtn.getX(),         dlyTimeValBtn.getBottom() + 1, dlyTimeValBtn.getWidth(), 14, juce::Justification::centred, false);

    g.setFont(makeFont(13.0f, true));
    g.setColour(lavenderAccent);
    g.drawText("IN",  inGainKnob.getRight() + 3, inGainKnob.getY(), 28, inGainKnob.getHeight(), juce::Justification::centredLeft, false);
    g.drawText("OUT", outGainKnob.getX() - 34,   outGainKnob.getY(), 32, outGainKnob.getHeight(), juce::Justification::centredRight, false);
}

void BigVoixAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    using namespace BigVoixColors;

    const float s = audioProcessor.getUiScaleFactor();
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(s));

    const auto& tel = audioProcessor.getTelemetry();
    if (tel.autoInputListening.load(std::memory_order_relaxed))
    {
        const float prog = tel.autoInputProgress.load(std::memory_order_relaxed);
        const bool voiceActive = tel.autoInputVoiceActive.load(std::memory_order_relaxed);
        const auto btnBounds = autoInputBtn.getBounds().toFloat();

        g.setColour(woodDarkBg.withAlpha(0.92f));
        g.fillRoundedRectangle(btnBounds, 5.0f);

        g.setColour(voiceActive ? knobActiveTop : knobInactiveTop);
        g.fillRoundedRectangle(btnBounds.withWidth(btnBounds.getWidth() * prog), 5.0f);

        g.setColour(lavenderAccent);
        g.drawRoundedRectangle(btnBounds, 5.0f, 1.5f);
        g.setFont(makeFont(11.0f, true));
        g.drawText(voiceActive ? ("Sing... " + juce::String(juce::roundToInt(prog * 100.0f)) + "%")
                               : "Paused (Sing!)",
                   btnBounds, juce::Justification::centred, false);
    }

    const bool isGlobalBypassed = audioProcessor.getAPVTS()
                                      .getRawParameterValue(BigVoixParamIDs::globalBypass)
                                      ->load(std::memory_order_relaxed) > 0.5f;
    if (isGlobalBypassed)
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRect(0, 48, kBaseWidth, kBaseHeight - 48);

        const juce::Rectangle<float> badge(360.0f, 268.0f, 320.0f, 36.0f);
        g.setColour(woodDarkBg.withAlpha(0.95f));
        g.fillRoundedRectangle(badge, 8.0f);
        g.setColour(knobHighlight);
        g.drawRoundedRectangle(badge, 8.0f, 1.5f);
        g.setColour(lavenderAccent);
        g.setFont(makeFont(13.5f, true));
        g.drawText("BYPASSED (Click mouth to wake)", badge, juce::Justification::centred, false);
    }

    g.restoreState();
}

void BigVoixAudioProcessorEditor::resized()
{
    lowCutBtn.setBounds(448, 10, 86, 28);
    autoInputBtn.setBounds(540, 10, 96, 28);
    prevPresetBtn.setBounds(644, 10, 28, 28);
    presetNameBtn.setBounds(674, 10, 224, 28);
    nextPresetBtn.setBounds(900, 10, 28, 28);
    saveDiskBtn.setBounds(934, 10, 44, 28);
    menuBurgerBtn.setBounds(982, 10, 38, 28);

    compKnob.setBounds(178, 155, 216, 216);

    const float rightX = 448.0f;
    const float rightW = 572.0f;

    eqStripRect   = { rightX,  50.0f, rightW, 96.0f };
    fxStripRect   = { rightX, 154.0f, rightW, 96.0f };
    logoStripRect = { rightX, 258.0f, rightW, 56.0f };
    dlyStripRect  = { rightX, 322.0f, rightW, 96.0f };
    revStripRect  = { rightX, 426.0f, rightW, 96.0f };

    eqTitleBtn.setBounds(460, 82, 62, 32);
    const int eqStartX  = 542;
    const int eqSpacing = 114;
    eqLowKnob.setBounds(eqStartX + 0 * eqSpacing, 56, 66, 66);
    eqMidKnob.setBounds(eqStartX + 1 * eqSpacing, 56, 66, 66);
    eqHiKnob.setBounds (eqStartX + 2 * eqSpacing, 56, 66, 66);
    eqAirKnob.setBounds(eqStartX + 3 * eqSpacing, 56, 66, 66);

    fxTitleBtn.setBounds(460, 186, 62, 32);
    const int fxStartX  = 534;
    const int fxSpacing = 94;
    fxDeEssKnob.setBounds  (fxStartX + 0 * fxSpacing, 160, 64, 64);
    fxDriveKnob.setBounds  (fxStartX + 1 * fxSpacing, 160, 64, 64);
    fxPhoneKnob.setBounds  (fxStartX + 2 * fxSpacing, 160, 64, 64);
    fxDoublerKnob.setBounds(fxStartX + 3 * fxSpacing, 160, 64, 64);
    fxChorusKnob.setBounds (fxStartX + 4 * fxSpacing, 160, 64, 64);

    dlyTitleBtn.setBounds(460, 354, 62, 32);
    dlySendKnob.setBounds(538, 328, 66, 66);
    dlyFeedKnob.setBounds(624, 328, 40, 40);
    dlyFilterKnob.setBounds(624, 372, 40, 40);

    dlyTimePrevBtn.setBounds(726, 350, 26, 28);
    dlyTimeValBtn.setBounds (754, 350, 72, 28);
    dlyTimeNextBtn.setBounds(828, 350, 26, 28);
    dlyPingPongBtn.setBounds(874, 350, 128, 28);

    revTitleBtn.setBounds(460, 458, 62, 32);
    const int revStartX  = 542;
    const int revSpacing = 114;
    BigVoixKnob* revKnobs[4] = { &revShortKnob, &revSmallKnob, &revBigKnob, &revLushKnob };
    juce::ToggleButton* revCutBtns[4] = { &revShortCutBtn, &revSmallCutBtn, &revBigCutBtn, &revLushCutBtn };

    for (int i = 0; i < 4; ++i)
    {
        const int kx = revStartX + i * revSpacing;
        revKnobs[i]->setBounds(kx, 430, 64, 64);
        revCutBtns[i]->setBounds(kx - 6, 494, 76, 24);
    }

    inputMeter.setBounds(24, 536, 210, 28);
    inGainKnob.setBounds(242, 530, 44, 44);

    outGainKnob.setBounds(748, 530, 44, 44);
    outputMeter.setBounds(800, 542, 210, 22);
}

void BigVoixAudioProcessorEditor::showPresetsPopupMenu()
{
    juce::PopupMenu mainMenu;
    const auto& presets = audioProcessor.getFactoryPresets();

    static const juce::StringArray categories {
        "Startup Vocal", "Leads", "Backs", "Adlibs", "FX", "Stream & Video", "Music Styles"
    };

    for (const auto& cat : categories)
    {
        juce::PopupMenu sub;
        for (size_t i = 0; i < presets.size(); ++i)
        {
            if (presets[i].category == cat)
            {
                const bool isCurrent = (static_cast<int>(i) == audioProcessor.getCurrentProgram());
                sub.addItem(static_cast<int>(i) + 1, presets[i].name, true, isCurrent);
            }
        }
        if (sub.getNumItems() > 0)
            mainMenu.addSubMenu(cat, sub);
    }

    juce::Array<juce::File> userFiles;
    audioProcessor.getPresetsFolder().findChildFiles(userFiles, juce::File::findFiles, false, "*.bigvoix_preset");
    if (!userFiles.isEmpty())
    {
        mainMenu.addSeparator();
        juce::PopupMenu userSub;
        for (int i = 0; i < userFiles.size(); ++i)
            userSub.addItem(1000 + i, userFiles[i].getFileNameWithoutExtension());
        mainMenu.addSubMenu("User Presets", userSub);
    }

    mainMenu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetNameBtn),
                           [this, userFiles](int result)
                           {
                               if (result >= 1 && result < 1000)
                               {
                                   audioProcessor.loadPresetByIndex(result - 1);
                                   presetNameBtn.setButtonText(audioProcessor.getCurrentPresetName());
                               }
                               else if (result >= 1000 && (result - 1000) < userFiles.size())
                               {
                                   audioProcessor.loadUserPresetFromFile(userFiles[result - 1000]);
                                   presetNameBtn.setButtonText(audioProcessor.getCurrentPresetName());
                               }
                           });
}

void BigVoixAudioProcessorEditor::showSavePresetDialog()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save BigVoix Preset",
        audioProcessor.getPresetsFolder().getChildFile(audioProcessor.getCurrentPresetName() + ".bigvoix_preset"),
        "*.bigvoix_preset"
    );

    constexpr int chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f != juce::File{})
        {
            audioProcessor.saveUserPresetToFile(f);
            presetNameBtn.setButtonText(audioProcessor.getCurrentPresetName());
        }
    });
}

void BigVoixAudioProcessorEditor::showHamburgerMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("PRESETS & FILES");
    menu.addItem(1, "Load preset from file (.bigvoix_preset)...");
    menu.addItem(2, "Open presets folder (Documents/BigVoix)");
    menu.addItem(3, "Rescan presets folder");

    menu.addSeparator();
    menu.addSectionHeader("UI SCALE (66% - 150%)");

    struct ScaleOption { int id; const char* label; float factor; };
    static constexpr std::array<ScaleOption, 6> kScales {{
        { 10, "66 %",  0.66f },
        { 11, "75 %",  0.75f },
        { 12, "100 %", 1.00f },
        { 13, "125 %", 1.25f },
        { 14, "135 %", 1.35f },
        { 15, "150 %", 1.50f }
    }};

    const float curScale = audioProcessor.getUiScaleFactor();
    for (const auto& sc : kScales)
        menu.addItem(sc.id, sc.label, true, std::abs(curScale - sc.factor) < 0.03f);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuBurgerBtn),
                       [this](int result)
                       {
                           if (result == 1)
                           {
                               fileChooser = std::make_unique<juce::FileChooser>(
                                   "Load BigVoix Preset",
                                   audioProcessor.getPresetsFolder(),
                                   "*.bigvoix_preset"
                               );
                               constexpr int openFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
                               fileChooser->launchAsync(openFlags, [this](const juce::FileChooser& fc)
                               {
                                   const auto f = fc.getResult();
                                   if (f.existsAsFile())
                                   {
                                       audioProcessor.loadUserPresetFromFile(f);
                                       presetNameBtn.setButtonText(audioProcessor.getCurrentPresetName());
                                   }
                               });
                           }
                           else if (result == 2)
                           {
                               audioProcessor.getPresetsFolder().revealToUser();
                           }
                           else if (result >= 10 && result <= 15)
                           {
                               static constexpr float factors[6] = { 0.66f, 0.75f, 1.00f, 1.25f, 1.35f, 1.50f };
                               audioProcessor.setUiScaleFactor(factors[result - 10]);
                               applyCurrentScale();
                           }
                       });
}
