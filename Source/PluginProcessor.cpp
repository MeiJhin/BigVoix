#include "PluginProcessor.h"
#include "PluginEditor.h"

BigVoixAudioProcessor::BigVoixAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "BigVoixState", createParameterLayout())
{
    paramRefs.bind(apvts);
    buildFactoryPresets();

    const auto folder = getPresetsFolder();
    if (!folder.exists())
        folder.createDirectory();
}

bool BigVoixAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    return (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo());
}

void BigVoixAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    inputStage.prepare(sampleRate);
    vocalCompressor.prepare(sampleRate);
    vocalEQ.prepare(sampleRate);
    vocalFX.prepare(sampleRate);
    autoDucker.prepare(sampleRate, samplesPerBlock);
    delayModule.prepare(sampleRate);
    reverbRack.prepare(sampleRate, samplesPerBlock);
    outputStage.prepare(sampleRate);

    setLatencySamples(0);
}

void BigVoixAudioProcessor::releaseResources()
{
    inputStage.reset();
    vocalCompressor.reset();
    vocalEQ.reset();
    vocalFX.reset();
    autoDucker.reset();
    delayModule.reset();
    reverbRack.reset();
    outputStage.reset();
}

void BigVoixAudioProcessor::startAutoInputAnalysis() noexcept
{
    telemetry.autoInputProgress.store(0.0f, std::memory_order_relaxed);
    telemetry.autoInputListening.store(true, std::memory_order_relaxed);
    inputStage.startAutoInput();
}

void BigVoixAudioProcessor::cancelAutoInputAnalysis() noexcept
{
    inputStage.cancelAutoInput();
    telemetry.autoInputListening.store(false, std::memory_order_relaxed);
    telemetry.autoInputProgress.store(0.0f, std::memory_order_relaxed);
    telemetry.autoInputVoiceActive.store(false, std::memory_order_relaxed);
}

void BigVoixAudioProcessor::setUiScaleFactor(float newScale) noexcept
{
    uiScaleFactor.store(std::clamp(newScale, 0.66f, 1.50f), std::memory_order_relaxed);
}

void BigVoixAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numSamples = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1 && getTotalNumOutputChannels() >= 2)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    const float pk = buffer.getMagnitude(0, 0, numSamples);
    const float db = BigVoixDSP::gainToDb(pk);
    telemetry.inputPeakDb.store(db, std::memory_order_relaxed);
    telemetry.inputRmsDb.store(db - 3.0f, std::memory_order_relaxed);
    telemetry.outputPeakDb.store(db, std::memory_order_relaxed);
    telemetry.outputRmsDb.store(db - 3.0f, std::memory_order_relaxed);
    telemetry.compGainReductionDb.store(0.0f, std::memory_order_relaxed);
    telemetry.deEssGainReduction.store(0.0f, std::memory_order_relaxed);
}

void BigVoixAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalInCh  = getTotalNumInputChannels();
    const int totalOutCh = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    if (totalInCh == 1 && totalOutCh >= 2)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    for (int ch = 2; ch < totalOutCh; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (paramRefs.globalBypass->load(std::memory_order_relaxed) > 0.5f)
    {
        processBlockBypassed(buffer, midi);
        return;
    }

    double hostBpm = 120.0;
    if (auto* currentPlayHead = getPlayHead())
    {
        if (const auto pos = currentPlayHead->getPosition())
        {
            if (const auto bpmOpt = pos->getBpm())
                hostBpm = *bpmOpt;
        }
    }

    const bool lowCutOn = paramRefs.lowCut->load(std::memory_order_relaxed) > 0.5f;

    inputStage.process(
        buffer,
        paramRefs.inGainDb->load(std::memory_order_relaxed),
        lowCutOn,
        telemetry
    );

    float computedAutoInGainDb = 0.0f;
    if (inputStage.popCompletedAutoGainDb(computedAutoInGainDb))
    {
        setParameterPlainValue(BigVoixParamIDs::inGainDb, computedAutoInGainDb);
    }

    vocalCompressor.process(
        buffer,
        paramRefs.comp->load(std::memory_order_relaxed),
        telemetry
    );

    vocalEQ.process(
        buffer,
        paramRefs.eqBypass->load(std::memory_order_relaxed) > 0.5f,
        paramRefs.eqLowDb->load(std::memory_order_relaxed),
        paramRefs.eqMidDb->load(std::memory_order_relaxed),
        paramRefs.eqHiDb->load(std::memory_order_relaxed),
        paramRefs.eqAirDb->load(std::memory_order_relaxed),
        telemetry
    );

    int sleepingCount = 0;

    vocalFX.process(
        buffer,
        paramRefs.fxBypass->load(std::memory_order_relaxed) > 0.5f,
        paramRefs.fxDeEss->load(std::memory_order_relaxed),
        paramRefs.fxDrive->load(std::memory_order_relaxed),
        paramRefs.fxPhone->load(std::memory_order_relaxed),
        paramRefs.fxDoubler->load(std::memory_order_relaxed),
        paramRefs.fxChorus->load(std::memory_order_relaxed),
        telemetry,
        sleepingCount
    );

    autoDucker.analyzeVoiceBlock(buffer, telemetry);

    const int divIdx = std::clamp(
        static_cast<int>(paramRefs.dlyTimeDiv->load(std::memory_order_relaxed)),
        0, 20);
    const double qnMult = BigVoixRhythm::kDelayDivisions[static_cast<size_t>(divIdx)].quarterNoteMultiplier;

    delayModule.process(
        buffer,
        paramRefs.dlyCut->load(std::memory_order_relaxed) > 0.5f,
        paramRefs.dlySend->load(std::memory_order_relaxed),
        paramRefs.dlyFeed->load(std::memory_order_relaxed),
        paramRefs.dlyFilter->load(std::memory_order_relaxed),
        qnMult,
        paramRefs.dlyPingPong->load(std::memory_order_relaxed) > 0.5f,
        hostBpm,
        autoDucker,
        sleepingCount
    );

    reverbRack.process(
        buffer,
        paramRefs.revCut->load(std::memory_order_relaxed) > 0.5f,
        paramRefs.revShortSend->load(std::memory_order_relaxed),
        paramRefs.revShortCut->load(std::memory_order_relaxed) > 0.5f,
        paramRefs.revSmallSend->load(std::memory_order_relaxed),
        paramRefs.revSmallCut->load(std::memory_order_relaxed) > 0.5f,
        paramRefs.revBigSend->load(std::memory_order_relaxed),
        paramRefs.revBigCut->load(std::memory_order_relaxed) > 0.5f,
        paramRefs.revLushSend->load(std::memory_order_relaxed),
        paramRefs.revLushCut->load(std::memory_order_relaxed) > 0.5f,
        autoDucker,
        sleepingCount
    );

    telemetry.sleepingModulesCount.store(sleepingCount, std::memory_order_relaxed);

    outputStage.process(
        buffer,
        paramRefs.outGainDb->load(std::memory_order_relaxed),
        lowCutOn,
        telemetry
    );
}

void BigVoixAudioProcessor::setParameterPlainValue(const char* paramID, float plainValue)
{
    if (auto* p = apvts.getParameter(paramID))
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
}

void BigVoixAudioProcessor::buildFactoryPresets()
{
    factoryPresets.clear();

    {
        BigVoixPresetData p;
        p.category = "Startup Vocal";
        p.name     = "Startup Vocal";
        p.comp = 70.0f; p.eqHiDb = 1.5f; p.eqAirDb = 2.0f; p.fxDeEss = 35.0f; p.revSmallSend = 20.0f;
        factoryPresets.push_back(p);
    }
    {
        BigVoixPresetData p;
        p.category = "Leads";
        p.name     = "Modern Pop Lead";
        p.comp = 74.0f; p.eqLowDb = -1.5f; p.eqMidDb = 1.5f; p.eqHiDb = 3.0f; p.eqAirDb = 5.0f;
        p.fxDeEss = 50.0f; p.fxDrive = 18.0f; p.fxDoubler = 22.0f;
        p.dlySend = 16.0f; p.dlyTimeDiv = 6; p.revSmallSend = 15.0f; p.revBigSend = 22.0f;
        factoryPresets.push_back(p);
    }
    {
        BigVoixPresetData p;
        p.category = "Leads";
        p.name     = "Rap Upfront Punch";
        p.comp = 84.0f; p.eqLowDb = 1.0f; p.eqMidDb = 2.5f; p.eqHiDb = 3.5f; p.eqAirDb = 3.0f;
        p.fxDeEss = 55.0f; p.fxDrive = 32.0f; p.fxDoubler = 12.0f;
        p.dlySend = 10.0f; p.dlyTimeDiv = 3; p.revShortSend = 25.0f; p.revSmallSend = 8.0f;
        factoryPresets.push_back(p);
    }
    {
        BigVoixPresetData p;
        p.category = "Leads";
        p.name     = "Intimate Velvet";
        p.comp = 62.0f; p.eqLowDb = 2.5f; p.eqMidDb = -1.0f; p.eqHiDb = 1.5f; p.eqAirDb = 6.5f;
        p.fxDeEss = 48.0f; p.fxDrive = 25.0f; p.revSmallSend = 18.0f; p.revLushSend = 28.0f;
        factoryPresets.push_back(p);
    }
    {
        BigVoixPresetData p;
        p.category = "Backs";
        p.name     = "Wide Stereo Harmonies";
        p.comp = 78.0f; p.eqLowDb = -4.5f; p.eqMidDb = -1.5f; p.eqHiDb = 2.0f; p.eqAirDb = 4.5f;
        p.fxDeEss = 65.0f; p.fxDoubler = 68.0f; p.fxChorus = 25.0f;
        p.revBigSend = 32.0f;
        factoryPresets.push_back(p);
    }
    {
        BigVoixPresetData p;
        p.category = "Adlibs";
        p.name     = "Cloudy Trap Adlib";
        p.comp = 80.0f; p.eqLowDb = -6.0f; p.eqHiDb = 3.0f; p.eqAirDb = 5.0f;
        p.fxDeEss = 55.0f; p.fxDrive = 35.0f; p.fxPhone = 42.0f; p.fxChorus = 35.0f;
        p.dlySend = 42.0f; p.dlyFeed = 55.0f; p.dlyTimeDiv = 8; p.revLushSend = 58.0f;
        factoryPresets.push_back(p);
    }
    {
        BigVoixPresetData p;
        p.category = "FX";
        p.name     = "Vintage Radio Intro";
        p.comp = 85.0f; p.fxDrive = 55.0f; p.fxPhone = 88.0f; p.revShortSend = 30.0f;
        factoryPresets.push_back(p);
    }
    {
        BigVoixPresetData p;
        p.category = "Stream & Video";
        p.name     = "Broadcast & Podcast Pro";
        p.comp = 72.0f; p.eqLowDb = 2.0f; p.eqMidDb = 2.0f; p.eqHiDb = 2.5f; p.eqAirDb = 2.0f;
        p.fxDeEss = 58.0f; p.revShortSend = 8.0f;
        factoryPresets.push_back(p);
    }
    {
        BigVoixPresetData p;
        p.category = "Music Styles";
        p.name     = "R&B Silk & Lush";
        p.comp = 68.0f; p.eqLowDb = 1.0f; p.eqHiDb = 2.5f; p.eqAirDb = 6.0f;
        p.fxDeEss = 52.0f; p.fxDrive = 20.0f; p.fxDoubler = 28.0f; p.fxChorus = 18.0f;
        p.dlySend = 24.0f; p.dlyTimeDiv = 9; p.revBigSend = 25.0f; p.revLushSend = 35.0f;
        factoryPresets.push_back(p);
    }
}

int BigVoixAudioProcessor::getNumPrograms()
{
    return std::max(1, static_cast<int>(factoryPresets.size()));
}

void BigVoixAudioProcessor::setCurrentProgram(int index)
{
    loadPresetByIndex(index);
}

const juce::String BigVoixAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < static_cast<int>(factoryPresets.size()))
        return factoryPresets[static_cast<size_t>(index)].name;
    return "Startup Vocal";
}

juce::String BigVoixAudioProcessor::getCurrentPresetName() const
{
    if (customLoadedPresetName.isNotEmpty())
        return customLoadedPresetName;

    if (currentPresetIndex >= 0 && currentPresetIndex < static_cast<int>(factoryPresets.size()))
        return factoryPresets[static_cast<size_t>(currentPresetIndex)].name;

    return "Startup Vocal";
}

void BigVoixAudioProcessor::loadPresetByIndex(int index)
{
    if (factoryPresets.empty())
        return;

    currentPresetIndex = std::clamp(index, 0, static_cast<int>(factoryPresets.size()) - 1);
    customLoadedPresetName.clear();
    const auto& p = factoryPresets[static_cast<size_t>(currentPresetIndex)];

    using namespace BigVoixParamIDs;
    setParameterPlainValue(lowCut,       p.lowCut ? 1.0f : 0.0f);
    setParameterPlainValue(comp,         p.comp);

    setParameterPlainValue(eqBypass,     p.eqBypass ? 1.0f : 0.0f);
    setParameterPlainValue(eqLowDb,      p.eqLowDb);
    setParameterPlainValue(eqMidDb,      p.eqMidDb);
    setParameterPlainValue(eqHiDb,       p.eqHiDb);
    setParameterPlainValue(eqAirDb,      p.eqAirDb);

    setParameterPlainValue(fxBypass,     p.fxBypass ? 1.0f : 0.0f);
    setParameterPlainValue(fxDeEss,      p.fxDeEss);
    setParameterPlainValue(fxDrive,      p.fxDrive);
    setParameterPlainValue(fxPhone,      p.fxPhone);
    setParameterPlainValue(fxDoubler,    p.fxDoubler);
    setParameterPlainValue(fxChorus,     p.fxChorus);

    setParameterPlainValue(dlyCut,       p.dlyCut ? 1.0f : 0.0f);
    setParameterPlainValue(dlySend,      p.dlySend);
    setParameterPlainValue(dlyFeed,      p.dlyFeed);
    setParameterPlainValue(dlyFilter,    p.dlyFilter);
    setParameterPlainValue(dlyTimeDiv,   static_cast<float>(p.dlyTimeDiv));
    setParameterPlainValue(dlyPingPong,  p.dlyPingPong ? 1.0f : 0.0f);

    setParameterPlainValue(revCut,       p.revCut ? 1.0f : 0.0f);
    setParameterPlainValue(revShortSend, p.revShortSend);
    setParameterPlainValue(revShortCut,  0.0f);
    setParameterPlainValue(revSmallSend, p.revSmallSend);
    setParameterPlainValue(revSmallCut,  0.0f);
    setParameterPlainValue(revBigSend,   p.revBigSend);
    setParameterPlainValue(revBigCut,    0.0f);
    setParameterPlainValue(revLushSend,  p.revLushSend);
    setParameterPlainValue(revLushCut,   0.0f);
}

void BigVoixAudioProcessor::loadNextPreset()
{
    if (factoryPresets.empty()) return;
    loadPresetByIndex((currentPresetIndex + 1) % static_cast<int>(factoryPresets.size()));
}

void BigVoixAudioProcessor::loadPreviousPreset()
{
    if (factoryPresets.empty()) return;
    const int n = static_cast<int>(factoryPresets.size());
    loadPresetByIndex((currentPresetIndex - 1 + n) % n);
}

juce::File BigVoixAudioProcessor::getPresetsFolder() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
               .getChildFile("BigVoix");
}

bool BigVoixAudioProcessor::saveUserPresetToFile(const juce::File& targetFile)
{
    auto state = apvts.copyState();
    if (std::unique_ptr<juce::XmlElement> xml = state.createXml())
    {
        const auto finalFile = targetFile.withFileExtension(".bigvoix_preset");
        xml->setAttribute("presetName", finalFile.getFileNameWithoutExtension());
        if (xml->writeTo(finalFile))
        {
            customLoadedPresetName = finalFile.getFileNameWithoutExtension();
            return true;
        }
    }
    return false;
}

bool BigVoixAudioProcessor::loadUserPresetFromFile(const juce::File& sourceFile)
{
    if (!sourceFile.existsAsFile())
        return false;

    if (auto xml = juce::XmlDocument::parse(sourceFile))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            customLoadedPresetName = sourceFile.getFileNameWithoutExtension();
            return true;
        }
    }
    return false;
}

void BigVoixAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (std::unique_ptr<juce::XmlElement> xml = state.createXml())
    {
        xml->setAttribute("pluginVersion", "1.0.0");
        xml->setAttribute("uiScaleFactor", static_cast<double>(getUiScaleFactor()));
        xml->setAttribute("presetIndex", currentPresetIndex);
        xml->setAttribute("customPresetName", customLoadedPresetName);
        copyXmlToBinary(*xml, destData);
    }
}

void BigVoixAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (std::unique_ptr<juce::XmlElement> xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            setUiScaleFactor(static_cast<float>(xml->getDoubleAttribute("uiScaleFactor", 1.0)));
            currentPresetIndex     = xml->getIntAttribute("presetIndex", 0);
            customLoadedPresetName = xml->getStringAttribute("customPresetName", {});
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
        }
    }
}

juce::AudioProcessorEditor* BigVoixAudioProcessor::createEditor()
{
    return new BigVoixAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BigVoixAudioProcessor();
}
