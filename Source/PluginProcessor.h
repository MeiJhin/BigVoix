#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace BigVoixParamIDs
{
    inline constexpr const char* globalBypass  = "global_bypass";
    inline constexpr const char* lowCut        = "low_cut";
    inline constexpr const char* inGainDb      = "in_gain_db";
    inline constexpr const char* outGainDb     = "out_gain_db";

    inline constexpr const char* comp          = "comp";

    inline constexpr const char* eqBypass      = "eq_bypass";
    inline constexpr const char* eqLowDb       = "eq_low_db";
    inline constexpr const char* eqMidDb       = "eq_mid_db";
    inline constexpr const char* eqHiDb        = "eq_hi_db";
    inline constexpr const char* eqAirDb       = "eq_air_db";

    inline constexpr const char* fxBypass      = "fx_bypass";
    inline constexpr const char* fxDeEss       = "fx_deess";
    inline constexpr const char* fxDrive       = "fx_drive";
    inline constexpr const char* fxPhone       = "fx_phone";
    inline constexpr const char* fxDoubler     = "fx_doubler";
    inline constexpr const char* fxChorus      = "fx_chorus";

    inline constexpr const char* dlyCut        = "dly_cut";
    inline constexpr const char* dlySend       = "dly_send";
    inline constexpr const char* dlyFeed       = "dly_feed";
    inline constexpr const char* dlyFilter     = "dly_filter";
    inline constexpr const char* dlyTimeDiv    = "dly_time_div";
    inline constexpr const char* dlyPingPong   = "dly_pingpong";

    inline constexpr const char* revCut        = "rev_cut";
    inline constexpr const char* revShortSend  = "rev_short_send";
    inline constexpr const char* revShortCut   = "rev_short_cut";
    inline constexpr const char* revSmallSend  = "rev_small_send";
    inline constexpr const char* revSmallCut   = "rev_small_cut";
    inline constexpr const char* revBigSend    = "rev_big_send";
    inline constexpr const char* revBigCut     = "rev_big_cut";
    inline constexpr const char* revLushSend   = "rev_lush_send";
    inline constexpr const char* revLushCut    = "rev_lush_cut";
}

namespace BigVoixRhythm
{
    struct DivisionInfo
    {
        const char* label;
        double quarterNoteMultiplier;
    };

    inline constexpr std::array<DivisionInfo, 21> kDelayDivisions {{
        { "1/32",   0.125 },
        { "1/16T",  0.16666666666666666 },
        { "1/32D",  0.1875 },
        { "1/16",   0.25 },
        { "1/8T",   0.33333333333333333 },
        { "1/16D",  0.375 },
        { "1/8",    0.5 },
        { "1/4T",   0.66666666666666667 },
        { "1/8D",   0.75 },
        { "1/4",    1.0 },
        { "1/2T",   1.33333333333333333 },
        { "1/4D",   1.5 },
        { "1/2",    2.0 },
        { "1/1T",   2.66666666666666667 },
        { "1/2D",   3.0 },
        { "1/1",    4.0 },
        { "2/1T",   5.33333333333333333 },
        { "1/1D",   6.0 },
        { "2/1",    8.0 },
        { "2/1D",   12.0 },
        { "4/1",    16.0 }
    }};

    inline juce::StringArray getDivisionStringArray()
    {
        juce::StringArray arr;
        for (const auto& d : kDelayDivisions)
            arr.add(d.label);
        return arr;
    }
}

struct BigVoixParamRefs
{
    std::atomic<float>* globalBypass = nullptr;
    std::atomic<float>* lowCut       = nullptr;
    std::atomic<float>* inGainDb     = nullptr;
    std::atomic<float>* outGainDb    = nullptr;

    std::atomic<float>* comp         = nullptr;

    std::atomic<float>* eqBypass     = nullptr;
    std::atomic<float>* eqLowDb      = nullptr;
    std::atomic<float>* eqMidDb      = nullptr;
    std::atomic<float>* eqHiDb       = nullptr;
    std::atomic<float>* eqAirDb      = nullptr;

    std::atomic<float>* fxBypass     = nullptr;
    std::atomic<float>* fxDeEss      = nullptr;
    std::atomic<float>* fxDrive      = nullptr;
    std::atomic<float>* fxPhone      = nullptr;
    std::atomic<float>* fxDoubler    = nullptr;
    std::atomic<float>* fxChorus     = nullptr;

    std::atomic<float>* dlyCut       = nullptr;
    std::atomic<float>* dlySend      = nullptr;
    std::atomic<float>* dlyFeed      = nullptr;
    std::atomic<float>* dlyFilter    = nullptr;
    std::atomic<float>* dlyTimeDiv   = nullptr;
    std::atomic<float>* dlyPingPong  = nullptr;

    std::atomic<float>* revCut       = nullptr;
    std::atomic<float>* revShortSend = nullptr;
    std::atomic<float>* revShortCut  = nullptr;
    std::atomic<float>* revSmallSend = nullptr;
    std::atomic<float>* revSmallCut  = nullptr;
    std::atomic<float>* revBigSend   = nullptr;
    std::atomic<float>* revBigCut    = nullptr;
    std::atomic<float>* revLushSend  = nullptr;
    std::atomic<float>* revLushCut   = nullptr;

    void bind(juce::AudioProcessorValueTreeState& apvts) noexcept
    {
        globalBypass = apvts.getRawParameterValue(BigVoixParamIDs::globalBypass);
        lowCut       = apvts.getRawParameterValue(BigVoixParamIDs::lowCut);
        inGainDb     = apvts.getRawParameterValue(BigVoixParamIDs::inGainDb);
        outGainDb    = apvts.getRawParameterValue(BigVoixParamIDs::outGainDb);

        comp         = apvts.getRawParameterValue(BigVoixParamIDs::comp);

        eqBypass     = apvts.getRawParameterValue(BigVoixParamIDs::eqBypass);
        eqLowDb      = apvts.getRawParameterValue(BigVoixParamIDs::eqLowDb);
        eqMidDb      = apvts.getRawParameterValue(BigVoixParamIDs::eqMidDb);
        eqHiDb       = apvts.getRawParameterValue(BigVoixParamIDs::eqHiDb);
        eqAirDb      = apvts.getRawParameterValue(BigVoixParamIDs::eqAirDb);

        fxBypass     = apvts.getRawParameterValue(BigVoixParamIDs::fxBypass);
        fxDeEss      = apvts.getRawParameterValue(BigVoixParamIDs::fxDeEss);
        fxDrive      = apvts.getRawParameterValue(BigVoixParamIDs::fxDrive);
        fxPhone      = apvts.getRawParameterValue(BigVoixParamIDs::fxPhone);
        fxDoubler    = apvts.getRawParameterValue(BigVoixParamIDs::fxDoubler);
        fxChorus     = apvts.getRawParameterValue(BigVoixParamIDs::fxChorus);

        dlyCut       = apvts.getRawParameterValue(BigVoixParamIDs::dlyCut);
        dlySend      = apvts.getRawParameterValue(BigVoixParamIDs::dlySend);
        dlyFeed      = apvts.getRawParameterValue(BigVoixParamIDs::dlyFeed);
        dlyFilter    = apvts.getRawParameterValue(BigVoixParamIDs::dlyFilter);
        dlyTimeDiv   = apvts.getRawParameterValue(BigVoixParamIDs::dlyTimeDiv);
        dlyPingPong  = apvts.getRawParameterValue(BigVoixParamIDs::dlyPingPong);

        revCut       = apvts.getRawParameterValue(BigVoixParamIDs::revCut);
        revShortSend = apvts.getRawParameterValue(BigVoixParamIDs::revShortSend);
        revShortCut  = apvts.getRawParameterValue(BigVoixParamIDs::revShortCut);
        revSmallSend = apvts.getRawParameterValue(BigVoixParamIDs::revSmallSend);
        revSmallCut  = apvts.getRawParameterValue(BigVoixParamIDs::revSmallCut);
        revBigSend   = apvts.getRawParameterValue(BigVoixParamIDs::revBigSend);
        revBigCut    = apvts.getRawParameterValue(BigVoixParamIDs::revBigCut);
        revLushSend  = apvts.getRawParameterValue(BigVoixParamIDs::revLushSend);
        revLushCut   = apvts.getRawParameterValue(BigVoixParamIDs::revLushCut);
    }
};

struct BigVoixTelemetry
{
    std::atomic<float> inputRmsDb            { -100.0f };
    std::atomic<float> inputPeakDb           { -100.0f };
    std::atomic<float> outputRmsDb           { -100.0f };
    std::atomic<float> outputPeakDb          { -100.0f };

    std::atomic<bool>  inputClipped          { false };
    std::atomic<bool>  outputClipped         { false };

    std::atomic<bool>  autoInputListening    { false };
    std::atomic<float> autoInputProgress     { 0.0f };
    std::atomic<bool>  autoInputVoiceActive  { false };

    std::atomic<float> compGainReductionDb   { 0.0f };
    std::atomic<float> deEssGainReduction    { 0.0f };
    std::atomic<float> airDynGainReductionDb { 0.0f };
    std::atomic<float> duckingAttenuationDb  { 0.0f };

    std::atomic<int>   sleepingModulesCount  { 0 };
};

struct BigVoixPresetData
{
    juce::String category;
    juce::String name;

    float inGainDb      = 0.0f;
    bool  lowCut        = true;
    float comp          = 70.0f;

    bool  eqBypass      = false;
    float eqLowDb       = 0.0f;
    float eqMidDb       = 0.0f;
    float eqHiDb        = 0.0f;
    float eqAirDb       = 0.0f;

    bool  fxBypass      = false;
    float fxDeEss       = 35.0f;
    float fxDrive       = 0.0f;
    float fxPhone       = 0.0f;
    float fxDoubler     = 0.0f;
    float fxChorus      = 0.0f;

    bool  dlyCut        = false;
    float dlySend       = 0.0f;
    float dlyFeed       = 30.0f;
    float dlyFilter     = 50.0f;
    int   dlyTimeDiv    = 3;
    bool  dlyPingPong   = true;

    bool  revCut        = false;
    float revShortSend  = 0.0f;
    float revSmallSend  = 0.0f;
    float revBigSend    = 0.0f;
    float revLushSend   = 0.0f;
    float outGainDb     = 0.0f;
};

#include "DSP/BigVoixCoreDSP.h"
#include "DSP/BigVoixSpaceDSP.h"

class BigVoixAudioProcessor final : public juce::AudioProcessor
{
public:
    BigVoixAudioProcessor();
    ~BigVoixAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return apvts.getParameter(BigVoixParamIDs::globalBypass);
    }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPresetIndex; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void startAutoInputAnalysis() noexcept;
    void cancelAutoInputAnalysis() noexcept;
    void clearInputClipFlag() noexcept  { telemetry.inputClipped.store(false, std::memory_order_relaxed); }
    void clearOutputClipFlag() noexcept { telemetry.outputClipped.store(false, std::memory_order_relaxed); }

    const std::vector<BigVoixPresetData>& getFactoryPresets() const noexcept { return factoryPresets; }
    void loadPresetByIndex(int index);
    void loadNextPreset();
    void loadPreviousPreset();
    juce::String getCurrentPresetName() const;
    juce::File getPresetsFolder() const;
    bool saveUserPresetToFile(const juce::File& targetFile);
    bool loadUserPresetFromFile(const juce::File& sourceFile);

    float getUiScaleFactor() const noexcept { return uiScaleFactor.load(std::memory_order_relaxed); }
    void setUiScaleFactor(float newScale) noexcept;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    const BigVoixTelemetry& getTelemetry() const noexcept { return telemetry; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using namespace BigVoixParamIDs;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        auto pctString = [](float v, int) { return juce::String(juce::roundToInt(v)) + " %"; };
        auto dbString  = [](float v, int) {
            return (v > 0.05f ? "+" : "") + juce::String(v, 1) + " dB";
        };

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ globalBypass, 1 }, "Global Bypass", false));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ lowCut, 1 }, "Low Cut (90Hz In / 100Hz Out)", true));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ inGainDb, 1 }, "IN Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ outGainDb, 1 }, "OUT Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ comp, 1 }, "COMP",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 70.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ eqBypass, 1 }, "EQ Section Bypass", false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ eqLowDb, 1 }, "EQ Low",
            juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ eqMidDb, 1 }, "EQ Mid",
            juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ eqHiDb, 1 }, "EQ Hi",
            juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ eqAirDb, 1 }, "EQ Air (Dynamic)",
            juce::NormalisableRange<float>(-15.0f, 15.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbString)));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ fxBypass, 1 }, "FX Section Bypass", false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ fxDeEss, 1 }, "De-ess",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ fxDrive, 1 }, "Drive",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ fxPhone, 1 }, "Phone",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ fxDoubler, 1 }, "Doubler",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ fxChorus, 1 }, "Chorus",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ dlyCut, 1 }, "DLY Output Cut", false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ dlySend, 1 }, "Delay Send",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ dlyFeed, 1 }, "Delay Feed",
            juce::NormalisableRange<float>(0.0f, 95.0f, 0.1f), 30.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ dlyFilter, 1 }, "Delay Filter",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ dlyTimeDiv, 1 }, "Delay Time Division",
            BigVoixRhythm::getDivisionStringArray(), 3));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ dlyPingPong, 1 }, "Delay Ping-Pong", true));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ revCut, 1 }, "REV Global Output Cut", false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ revShortSend, 1 }, "Short Reverb Send",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ revShortCut, 1 }, "Short Reverb Cut", false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ revSmallSend, 1 }, "Small Reverb Send",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ revSmallCut, 1 }, "Small Reverb Cut", false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ revBigSend, 1 }, "Big Reverb Send",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ revBigCut, 1 }, "Big Reverb Cut", false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ revLushSend, 1 }, "Lush Reverb Send",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctString)));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ revLushCut, 1 }, "Lush Reverb Cut", false));

        return { params.begin(), params.end() };
    }

private:
    void buildFactoryPresets();
    void setParameterPlainValue(const char* paramID, float plainValue);

    juce::AudioProcessorValueTreeState apvts;
    BigVoixParamRefs paramRefs;
    BigVoixTelemetry telemetry;

    std::vector<BigVoixPresetData> factoryPresets;
    int currentPresetIndex = 0;
    juce::String customLoadedPresetName;
    std::atomic<float> uiScaleFactor { 1.0f };

    BigVoixDSP::InputStageModule     inputStage;
    BigVoixDSP::ThreeStageVocalComp  vocalCompressor;
    BigVoixDSP::VocalEQ4Band         vocalEQ;
    BigVoixDSP::VocalFXRack          vocalFX;
    BigVoixDSP::VocalAutoDucker      autoDucker;
    BigVoixDSP::DuckedPingPongDelay  delayModule;
    BigVoixDSP::ParallelReverbsRack  reverbRack;
    BigVoixDSP::OutputStageModule    outputStage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BigVoixAudioProcessor)
};
