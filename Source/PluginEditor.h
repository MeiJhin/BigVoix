#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "GUI/BigVoixLookAndFeel.h"

class BigVoixBarMeter final : public juce::Component
{
public:
    explicit BigVoixBarMeter(bool isInputMeterWithBestRange)
        : showBestBracket(isInputMeterWithBestRange)
    {
        setRepaintsOnMouseActivity(true);
    }

    void updateLevel(float rmsDb, bool clipped) noexcept
    {
        smoothDb = (rmsDb > smoothDb) ? (0.5f * smoothDb + 0.5f * rmsDb)
                                      : std::max(rmsDb, smoothDb - 0.8f);
        hasClipped = clipped;
        repaint();
    }

    std::function<void()> onClipClicked;

    void mouseDown(const juce::MouseEvent&) override
    {
        if (onClipClicked)
            onClipClicked();
    }

    void paint(juce::Graphics& g) override;

private:
    bool  showBestBracket = false;
    float smoothDb        = -100.0f;
    bool  hasClipped      = false;
};

class BigVoixAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit BigVoixAudioProcessorEditor(BigVoixAudioProcessor&);
    ~BigVoixAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void applyCurrentScale();
    void showPresetsPopupMenu();
    void showSavePresetDialog();
    void showHamburgerMenu();
    void stepDelayTimeDivision(int delta);

    static constexpr int kBaseWidth  = 1040;
    static constexpr int kBaseHeight = 585;

    BigVoixAudioProcessor& audioProcessor;
    BigVoixLookAndFeel     lookAndFeel;
    juce::Component        canvas;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Path characterMouthPath;

    juce::ToggleButton lowCutBtn     { "Low cut" };
    juce::TextButton   autoInputBtn  { "Auto input" };
    juce::TextButton   prevPresetBtn { "<" };
    juce::TextButton   presetNameBtn { "Startup Vocal" };
    juce::TextButton   nextPresetBtn { ">" };
    juce::TextButton   saveDiskBtn   { "Save" };
    juce::TextButton   menuBurgerBtn { "=" };

    std::unique_ptr<ButtonAttachment> lowCutAttachment;

    BigVoixKnob compKnob;
    std::unique_ptr<SliderAttachment> compAttachment;

    juce::ToggleButton eqTitleBtn { "EQ" };
    BigVoixKnob        eqLowKnob, eqMidKnob, eqHiKnob, eqAirKnob;
    std::unique_ptr<ButtonAttachment> eqBypassAttachment;
    std::unique_ptr<SliderAttachment> eqLowAtt, eqMidAtt, eqHiAtt, eqAirAtt;

    juce::ToggleButton fxTitleBtn { "FX" };
    BigVoixKnob        fxDeEssKnob, fxDriveKnob, fxPhoneKnob, fxDoublerKnob, fxChorusKnob;
    std::unique_ptr<ButtonAttachment> fxBypassAttachment;
    std::unique_ptr<SliderAttachment> fxDeEssAtt, fxDriveAtt, fxPhoneAtt, fxDoublerAtt, fxChorusAtt;

    juce::ToggleButton dlyTitleBtn    { "DLY" };
    BigVoixKnob        dlySendKnob, dlyFeedKnob, dlyFilterKnob;
    juce::TextButton   dlyTimePrevBtn { "<" };
    juce::TextButton   dlyTimeValBtn  { "1/16" };
    juce::TextButton   dlyTimeNextBtn { ">" };
    juce::ToggleButton dlyPingPongBtn { "Ping-pong" };

    std::unique_ptr<ButtonAttachment> dlyCutAttachment, dlyPingPongAttachment;
    std::unique_ptr<SliderAttachment> dlySendAtt, dlyFeedAtt, dlyFilterAtt;

    juce::ToggleButton revTitleBtn    { "REV" };
    BigVoixKnob        revShortKnob, revSmallKnob, revBigKnob, revLushKnob;
    juce::ToggleButton revShortCutBtn { "Short" };
    juce::ToggleButton revSmallCutBtn { "Small" };
    juce::ToggleButton revBigCutBtn   { "Big" };
    juce::ToggleButton revLushCutBtn  { "Lush" };

    std::unique_ptr<ButtonAttachment> revCutAttachment;
    std::unique_ptr<ButtonAttachment> revShortCutAtt, revSmallCutAtt, revBigCutAtt, revLushCutAtt;
    std::unique_ptr<SliderAttachment> revShortAtt, revSmallAtt, revBigAtt, revLushAtt;

    BigVoixBarMeter inputMeter  { true };
    BigVoixBarMeter outputMeter { false };
    BigVoixKnob     inGainKnob;
    BigVoixKnob     outGainKnob;
    std::unique_ptr<SliderAttachment> inGainAtt, outGainAtt;

    juce::Rectangle<float> eqStripRect;
    juce::Rectangle<float> fxStripRect;
    juce::Rectangle<float> logoStripRect;
    juce::Rectangle<float> dlyStripRect;
    juce::Rectangle<float> revStripRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BigVoixAudioProcessorEditor)
};

