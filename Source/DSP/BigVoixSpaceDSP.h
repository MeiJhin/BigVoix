#pragma once

#include "BigVoixCoreDSP.h"

namespace BigVoixDSP
{
    class VocalAutoDucker
    {
    public:
        void prepare(double sampleRate, int maxBlockSize) noexcept
        {
            sr = sampleRate;
            attCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.005f));
            relCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.320f));
            duckGainBuffer.assign(static_cast<size_t>(std::max(1, maxBlockSize)), 1.0f);
            reset();
        }

        void reset() noexcept
        {
            envLinear = 0.0f;
            std::fill(duckGainBuffer.begin(), duckGainBuffer.end(), 1.0f);
        }

        template <typename TelemetryType>
        void analyzeVoiceBlock(const juce::AudioBuffer<float>& voiceBuffer,
                               TelemetryType& telemetry) noexcept
        {
            const int numSamples  = voiceBuffer.getNumSamples();
            const int numChannels = voiceBuffer.getNumChannels();
            if (static_cast<size_t>(numSamples) > duckGainBuffer.size())
                duckGainBuffer.resize(static_cast<size_t>(numSamples), 1.0f);

            const float* chL = voiceBuffer.getReadPointer(0);
            const float* chR = numChannels > 1 ? voiceBuffer.getReadPointer(1) : chL;

            float maxAttenDb = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float peak = std::max(std::abs(chL[i]), std::abs(chR[i]));
                envLinear = (peak > envLinear)
                          ? (attCoeff * envLinear + (1.0f - attCoeff) * peak)
                          : (relCoeff * envLinear + (1.0f - relCoeff) * peak);

                const float envDb   = gainToDb(envLinear);
                const float attenDb = std::min(7.5f, computeSoftKneeGrDb(envDb, -34.0f, 3.8f, 6.0f));
                if (attenDb > maxAttenDb)
                    maxAttenDb = attenDb;

                duckGainBuffer[static_cast<size_t>(i)] = dbToGain(-attenDb);
            }

            telemetry.duckingAttenuationDb.store(maxAttenDb, std::memory_order_relaxed);
        }

        inline float getDuckingLinearAt(int sampleIndex) const noexcept
        {
            return duckGainBuffer[static_cast<size_t>(sampleIndex)];
        }

    private:
        double sr        = 44100.0;
        float  attCoeff  = 0.99f;
        float  relCoeff  = 0.9995f;
        float  envLinear = 0.0f;
        std::vector<float> duckGainBuffer;
    };

    class DuckedPingPongDelay
    {
    public:
        void prepare(double sampleRate) noexcept
        {
            sr = sampleRate;

            bufSize = 2097152;
            bufMask = bufSize - 1;
            delayLineL.assign(static_cast<size_t>(bufSize), 0.0f);
            delayLineR.assign(static_cast<size_t>(bufSize), 0.0f);

            sendSmoothed.reset(sr, 0.025);
            feedSmoothed.reset(sr, 0.025);
            delaySamplesSmoothed.reset(sr, 0.060);
            cutGateSmoothed.reset(sr, 0.005);

            sendSmoothed.setCurrentAndTargetValue(0.0f);
            feedSmoothed.setCurrentAndTargetValue(0.30f);
            delaySamplesSmoothed.setCurrentAndTargetValue(static_cast<float>(0.125 * sr));
            cutGateSmoothed.setCurrentAndTargetValue(1.0f);

            loopHpf.setHighPass(220.0f, 0.707f, sr);
            loopLpf.setLowPass(3800.0f, 0.707f, sr);
            sleepTracker.prepare(sr);

            reset();
        }

        void reset() noexcept
        {
            std::fill(delayLineL.begin(), delayLineL.end(), 0.0f);
            std::fill(delayLineR.begin(), delayLineR.end(), 0.0f);
            writePos = 0;
            loopHpf.reset();
            loopLpf.reset();
            sleepTracker.reset();
        }

        void process(juce::AudioBuffer<float>& buffer,
                     bool dlyOutputCut,
                     float dlySendPct,
                     float dlyFeedPct,
                     float dlyFilterPct,
                     double quarterNoteMultiplier,
                     bool pingPongEnabled,
                     double hostBpm,
                     const VocalAutoDucker& ducker,
                     int& sleepingModulesCounter) noexcept
        {
            const int numSamples  = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();
            float* chL = buffer.getWritePointer(0);
            float* chR = numChannels > 1 ? buffer.getWritePointer(1) : chL;

            const float inPeak = std::max(buffer.getMagnitude(0, 0, numSamples),
                                          numChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : 0.0f);

            const float targetSend = std::clamp(dlySendPct * 0.01f, 0.0f, 1.0f);
            const bool sendActive  = (targetSend > 0.001f);

            if (sleepTracker.checkShouldSkipBlock(sendActive, inPeak))
            {
                ++sleepingModulesCounter;
                return;
            }

            const double safeBpm = std::clamp(hostBpm, 30.0, 300.0);
            const double quarterNoteSec = 60.0 / safeBpm;
            const double delaySec = quarterNoteSec * quarterNoteMultiplier;
            const float targetDelaySamples = std::clamp(
                static_cast<float>(delaySec * sr),
                32.0f,
                static_cast<float>(bufSize - 16));

            sendSmoothed.setTargetValue(targetSend * 0.70f);
            feedSmoothed.setTargetValue(std::clamp(dlyFeedPct * 0.01f, 0.0f, 0.95f));
            delaySamplesSmoothed.setTargetValue(targetDelaySamples);
            cutGateSmoothed.setTargetValue(dlyOutputCut ? 0.0f : 1.0f);

            const float fNorm  = std::clamp(dlyFilterPct * 0.01f, 0.0f, 1.0f);
            const float hpFreq = 140.0f + 560.0f * fNorm;
            const float lpFreq = 8500.0f * std::pow(1900.0f / 8500.0f, fNorm);
            loopHpf.setHighPass(hpFreq, 0.75f, sr);
            loopLpf.setLowPass(lpFreq, 0.75f, sr);

            float maxTailPeak = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float dryL     = chL[i];
                const float dryR     = chR[i];
                const float sendGain = sendSmoothed.getNextValue();
                const float fbGain   = feedSmoothed.getNextValue();
                const float dlySamps = delaySamplesSmoothed.getNextValue();
                const float cutGate  = cutGateSmoothed.getNextValue();
                const float duckGain = ducker.getDuckingLinearAt(i);

                const float tapL = readDelayHermite4Pt(delayLineL, writePos, dlySamps, bufMask);
                const float tapR = readDelayHermite4Pt(delayLineR, writePos, dlySamps, bufMask);

                const float tailAbs = std::max(std::abs(tapL), std::abs(tapR));
                if (tailAbs > maxTailPeak)
                    maxTailPeak = tailAbs;

                const float monoInSend = 0.5f * (dryL + dryR) * sendGain;

                float nextInL = 0.0f;
                float nextInR = 0.0f;

                if (pingPongEnabled)
                {
                    nextInL = monoInSend + tapR * fbGain;
                    nextInR = tapL * fbGain;
                }
                else
                {
                    nextInL = (dryL * sendGain) + tapL * fbGain;
                    nextInR = (dryR * sendGain) + tapR * fbGain;
                }

                delayLineL[static_cast<size_t>(writePos)] = loopLpf.processSample(0, loopHpf.processSample(0, nextInL));
                delayLineR[static_cast<size_t>(writePos)] = loopLpf.processSample(1, loopHpf.processSample(1, nextInR));
                writePos = (writePos + 1) & bufMask;

                const float outGain = cutGate * duckGain;
                chL[i] = dryL + tapL * outGain;
                chR[i] = dryR + tapR * outGain;
            }

            sleepTracker.updateAfterBlock(sendActive, maxTailPeak * (dlyOutputCut ? 0.0f : 1.0f), numSamples);
        }

    private:
        double sr      = 44100.0;
        int    bufSize = 2097152;
        int    bufMask = 2097151;
        int    writePos = 0;

        std::vector<float> delayLineL;
        std::vector<float> delayLineR;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sendSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySamplesSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> cutGateSmoothed;

        ZeroLatBiquad    loopHpf;
        ZeroLatBiquad    loopLpf;
        AutoSleepTracker sleepTracker;
    };

    class ParallelReverbsRack
    {
    public:
        void prepare(double sampleRate, int maxBlockSize) noexcept
        {
            sr = sampleRate;
            const float fSr = static_cast<float>(sr);

            shortBufSize = 16384;
            shortBufMask = shortBufSize - 1;
            shortLineL.assign(static_cast<size_t>(shortBufSize), 0.0f);
            shortLineR.assign(static_cast<size_t>(shortBufSize), 0.0f);
            shortWritePos = 0;
            shortDampLpf.setCutoff(5200.0f, sr);

            juce::dsp::ProcessSpec spec{};
            spec.sampleRate       = sr;
            spec.maximumBlockSize = static_cast<juce::uint32>(std::max(1, maxBlockSize));
            spec.numChannels      = 2;

            for (size_t m = 0; m < 4; ++m)
            {
                sendSmoothed[m].reset(sr, 0.025);
                cutSmoothed[m].reset(sr, 0.005);
                sendSmoothed[m].setCurrentAndTargetValue(0.0f);
                cutSmoothed[m].setCurrentAndTargetValue(1.0f);

                postHpf[m].setCutoff(190.0f, sr);
                sleepTrackers[m].prepare(sr);
                scratchBuffers[m].setSize(2, std::max(1, maxBlockSize), false, true, false);
            }

            smallReverb.prepare(spec);
            bigReverb.prepare(spec);
            lushReverb.prepare(spec);

            {
                juce::dsp::Reverb::Parameters pSmall;
                pSmall.roomSize = 0.46f;
                pSmall.damping  = 0.42f;
                pSmall.wetLevel = 1.0f;
                pSmall.dryLevel = 0.0f;
                pSmall.width    = 0.95f;
                smallReverb.setParameters(pSmall);

                juce::dsp::Reverb::Parameters pBig;
                pBig.roomSize = 0.78f;
                pBig.damping  = 0.30f;
                pBig.wetLevel = 1.0f;
                pBig.dryLevel = 0.0f;
                pBig.width    = 1.0f;
                bigReverb.setParameters(pBig);

                juce::dsp::Reverb::Parameters pLush;
                pLush.roomSize = 0.91f;
                pLush.damping  = 0.18f;
                pLush.wetLevel = 1.0f;
                pLush.dryLevel = 0.0f;
                pLush.width    = 1.0f;
                lushReverb.setParameters(pLush);
            }

            lushModBufSize = 8192;
            lushModBufMask = lushModBufSize - 1;
            lushModL.assign(static_cast<size_t>(lushModBufSize), 0.0f);
            lushModR.assign(static_cast<size_t>(lushModBufSize), 0.0f);
            lushModWritePos = 0;
            lushLfoInc      = juce::MathConstants<float>::twoPi * 0.55f / fSr;

            reset();
        }

        void reset() noexcept
        {
            std::fill(shortLineL.begin(), shortLineL.end(), 0.0f);
            std::fill(shortLineR.begin(), shortLineR.end(), 0.0f);
            std::fill(lushModL.begin(), lushModL.end(), 0.0f);
            std::fill(lushModR.begin(), lushModR.end(), 0.0f);
            shortWritePos   = 0;
            lushModWritePos = 0;
            lushLfoPhase    = 0.0f;

            shortDampLpf.reset();
            smallReverb.reset();
            bigReverb.reset();
            lushReverb.reset();

            for (size_t m = 0; m < 4; ++m)
            {
                postHpf[m].reset();
                sleepTrackers[m].reset();
                scratchBuffers[m].clear();
            }
        }

        void process(juce::AudioBuffer<float>& buffer,
                     bool globalRevCut,
                     float shortSendPct, bool shortCut,
                     float smallSendPct, bool smallCut,
                     float bigSendPct,   bool bigCut,
                     float lushSendPct,  bool lushCut,
                     const VocalAutoDucker& ducker,
                     int& sleepingModulesCounter) noexcept
        {
            const int numSamples  = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();
            float* mainL = buffer.getWritePointer(0);
            float* mainR = numChannels > 1 ? buffer.getWritePointer(1) : mainL;

            const float inPeak = std::max(buffer.getMagnitude(0, 0, numSamples),
                                          numChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : 0.0f);

            const std::array<float, 4> targetSends {{
                std::clamp(shortSendPct * 0.01f, 0.0f, 1.0f) * 0.55f,
                std::clamp(smallSendPct * 0.01f, 0.0f, 1.0f) * 0.50f,
                std::clamp(bigSendPct   * 0.01f, 0.0f, 1.0f) * 0.55f,
                std::clamp(lushSendPct  * 0.01f, 0.0f, 1.0f) * 0.60f
            }};

            const std::array<bool, 4> indivCuts {{ shortCut, smallCut, bigCut, lushCut }};

            std::array<bool, 4> runEngine {{ false, false, false, false }};
            for (size_t m = 0; m < 4; ++m)
            {
                sendSmoothed[m].setTargetValue(targetSends[m]);
                cutSmoothed[m].setTargetValue((globalRevCut || indivCuts[m]) ? 0.0f : 1.0f);

                const bool sendActive = (targetSends[m] > 0.0005f);
                if (sleepTrackers[m].checkShouldSkipBlock(sendActive, inPeak))
                {
                    ++sleepingModulesCounter;
                    runEngine[m] = false;
                }
                else
                {
                    runEngine[m] = true;
                    if (scratchBuffers[m].getNumSamples() < numSamples)
                        scratchBuffers[m].setSize(2, numSamples, false, false, true);

                    float* sL = scratchBuffers[m].getWritePointer(0);
                    float* sR = scratchBuffers[m].getWritePointer(1);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float sg = sendSmoothed[m].getNextValue();
                        sL[i] = mainL[i] * sg;
                        sR[i] = mainR[i] * sg;
                    }
                }
            }

            const float fSr = static_cast<float>(sr);

            if (runEngine[0])
            {
                float* sL = scratchBuffers[0].getWritePointer(0);
                float* sR = scratchBuffers[0].getWritePointer(1);

                static constexpr std::array<float, 6> tapsMsL  {{ 4.3f, 9.7f, 15.2f, 23.8f, 34.1f, 47.5f }};
                static constexpr std::array<float, 6> tapsMsR  {{ 5.1f, 11.3f, 17.9f, 26.4f, 37.8f, 52.2f }};
                static constexpr std::array<float, 6> tapGains {{ 0.50f, -0.38f, 0.30f, 0.24f, -0.18f, 0.13f }};

                for (int i = 0; i < numSamples; ++i)
                {
                    float accL = 0.0f;
                    float accR = 0.0f;
                    for (size_t t = 0; t < 6; ++t)
                    {
                        accL += readDelayHermite4Pt(shortLineL, shortWritePos, tapsMsL[t] * 0.001f * fSr, shortBufMask) * tapGains[t];
                        accR += readDelayHermite4Pt(shortLineR, shortWritePos, tapsMsR[t] * 0.001f * fSr, shortBufMask) * tapGains[t];
                    }

                    shortLineL[static_cast<size_t>(shortWritePos)] = shortDampLpf.processLowPass(0, sL[i] + accR * 0.22f);
                    shortLineR[static_cast<size_t>(shortWritePos)] = shortDampLpf.processLowPass(1, sR[i] + accL * 0.22f);
                    shortWritePos = (shortWritePos + 1) & shortBufMask;

                    sL[i] = accL;
                    sR[i] = accR;
                }
            }

            if (runEngine[1])
            {
                juce::dsp::AudioBlock<float> blk(scratchBuffers[1].getArrayOfWritePointers(), 2, static_cast<size_t>(numSamples));
                juce::dsp::ProcessContextReplacing<float> ctx(blk);
                smallReverb.process(ctx);
            }

            if (runEngine[2])
            {
                juce::dsp::AudioBlock<float> blk(scratchBuffers[2].getArrayOfWritePointers(), 2, static_cast<size_t>(numSamples));
                juce::dsp::ProcessContextReplacing<float> ctx(blk);
                bigReverb.process(ctx);
            }

            if (runEngine[3])
            {
                juce::dsp::AudioBlock<float> blk(scratchBuffers[3].getArrayOfWritePointers(), 2, static_cast<size_t>(numSamples));
                juce::dsp::ProcessContextReplacing<float> ctx(blk);
                lushReverb.process(ctx);

                float* sL = scratchBuffers[3].getWritePointer(0);
                float* sR = scratchBuffers[3].getWritePointer(1);
                const float baseModDly = 0.012f * fSr;
                const float modDepth   = 0.0022f * fSr;

                for (int i = 0; i < numSamples; ++i)
                {
                    lushModL[static_cast<size_t>(lushModWritePos)] = sL[i];
                    lushModR[static_cast<size_t>(lushModWritePos)] = sR[i];

                    const float dL = baseModDly + modDepth * std::sin(lushLfoPhase);
                    const float dR = baseModDly + modDepth * std::cos(lushLfoPhase);

                    sL[i] = 0.55f * sL[i] + 0.45f * readDelayHermite4Pt(lushModL, lushModWritePos, dL, lushModBufMask);
                    sR[i] = 0.55f * sR[i] + 0.45f * readDelayHermite4Pt(lushModR, lushModWritePos, dR, lushModBufMask);

                    lushLfoPhase += lushLfoInc;
                    if (lushLfoPhase >= juce::MathConstants<float>::twoPi)
                        lushLfoPhase -= juce::MathConstants<float>::twoPi;

                    lushModWritePos = (lushModWritePos + 1) & lushModBufMask;
                }
            }

            for (size_t m = 0; m < 4; ++m)
            {
                if (!runEngine[m])
                    continue;

                const float* revL = scratchBuffers[m].getReadPointer(0);
                const float* revR = scratchBuffers[m].getReadPointer(1);
                float maxTail = 0.0f;

                for (int i = 0; i < numSamples; ++i)
                {
                    const float cleanL = postHpf[m].processHighPass(0, revL[i]);
                    const float cleanR = postHpf[m].processHighPass(1, revR[i]);

                    const float absVal = std::max(std::abs(cleanL), std::abs(cleanR));
                    if (absVal > maxTail)
                        maxTail = absVal;

                    const float cutGain  = cutSmoothed[m].getNextValue();
                    const float duckGain = ducker.getDuckingLinearAt(i);
                    const float outGain  = cutGain * duckGain;

                    mainL[i] += cleanL * outGain;
                    mainR[i] += cleanR * outGain;
                }

                const bool sendActive = (targetSends[m] > 0.0005f);
                const bool isCut      = globalRevCut || indivCuts[m];
                sleepTrackers[m].updateAfterBlock(sendActive, isCut ? 0.0f : maxTail, numSamples);
            }
        }

    private:
        double sr = 44100.0;

        int shortBufSize  = 16384;
        int shortBufMask  = 16383;
        int shortWritePos = 0;
        std::vector<float> shortLineL, shortLineR;
        ZeroLatFirstOrderFilter shortDampLpf;

        juce::dsp::Reverb smallReverb;
        juce::dsp::Reverb bigReverb;
        juce::dsp::Reverb lushReverb;

        int lushModBufSize  = 8192;
        int lushModBufMask  = 8191;
        int lushModWritePos = 0;
        float lushLfoPhase  = 0.0f;
        float lushLfoInc    = 0.0001f;
        std::vector<float> lushModL, lushModR;

        std::array<juce::AudioBuffer<float>, 4> scratchBuffers;
        std::array<ZeroLatFirstOrderFilter, 4>  postHpf;
        std::array<AutoSleepTracker, 4>         sleepTrackers;
        std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, 4> sendSmoothed;
        std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, 4> cutSmoothed;
    };

    class OutputStageModule
    {
    public:
        void prepare(double sampleRate) noexcept
        {
            sr = sampleRate;
            outGainSmoothed.reset(sr, 0.025);
            outGainSmoothed.setCurrentAndTargetValue(1.0f);

            hpf100Order1.setCutoff(100.0f, sr);
            hpf100Order2.setHighPass(100.0f, 1.0f, sr);

            rmsBallisticCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.250f));
            limiterRelCoeff   = std::exp(-1.0f / (static_cast<float>(sr) * 0.040f));
            reset();
        }

        void reset() noexcept
        {
            hpf100Order1.reset();
            hpf100Order2.reset();
            rmsSquaredState = 0.0f;
            limiterGainLin  = 1.0f;
        }

        template <typename TelemetryType>
        void process(juce::AudioBuffer<float>& buffer,
                     float outGainDb,
                     bool lowCutEnabled,
                     TelemetryType& telemetry) noexcept
        {
            outGainSmoothed.setTargetValue(dbToGain(outGainDb));

            const int numSamples  = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();
            float* chL = buffer.getWritePointer(0);
            float* chR = numChannels > 1 ? buffer.getWritePointer(1) : chL;

            constexpr float kSafetyCeiling = 0.98855f; // -0.1 dBFS
            float blockPeak = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                float sL = chL[i];
                float sR = chR[i];

                if (lowCutEnabled)
                {
                    sL = hpf100Order2.processSample(0, hpf100Order1.processHighPass(0, sL));
                    sR = hpf100Order2.processSample(1, hpf100Order1.processHighPass(1, sR));
                }

                const float g = outGainSmoothed.getNextValue();
                sL *= g;
                sR *= g;

                const float rawAbs = std::max(std::abs(sL), std::abs(sR));
                if (rawAbs >= 0.999f)
                    telemetry.outputClipped.store(true, std::memory_order_relaxed);

                const float targetLim = (rawAbs > kSafetyCeiling) ? (kSafetyCeiling / rawAbs) : 1.0f;
                if (targetLim < limiterGainLin)
                    limiterGainLin = targetLim;
                else
                    limiterGainLin = limiterRelCoeff * limiterGainLin + (1.0f - limiterRelCoeff) * targetLim;

                sL *= limiterGainLin;
                sR *= limiterGainLin;

                const float outAbs = std::max(std::abs(sL), std::abs(sR));
                if (outAbs > blockPeak)
                    blockPeak = outAbs;

                const float power = 0.5f * (sL * sL + sR * sR);
                rmsSquaredState = rmsBallisticCoeff * rmsSquaredState + (1.0f - rmsBallisticCoeff) * power;

                chL[i] = sL;
                chR[i] = sR;
            }

            telemetry.outputPeakDb.store(gainToDb(blockPeak), std::memory_order_relaxed);
            telemetry.outputRmsDb.store(10.0f * std::log10(std::max(rmsSquaredState, 1.0e-10f)), std::memory_order_relaxed);
        }

    private:
        double sr = 44100.0;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGainSmoothed;

        ZeroLatFirstOrderFilter hpf100Order1;
        ZeroLatBiquad           hpf100Order2;

        float rmsBallisticCoeff = 0.999f;
        float rmsSquaredState   = 0.0f;
        float limiterRelCoeff   = 0.999f;
        float limiterGainLin    = 1.0f;
    };

} // namespace BigVoixDSP
