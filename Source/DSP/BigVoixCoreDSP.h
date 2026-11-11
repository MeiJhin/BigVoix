#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>
#include <array>
#include <vector>
#include <atomic>

namespace BigVoixDSP
{
    inline float dbToGain(float db) noexcept
    {
        return db > -100.0f ? std::pow(10.0f, db * 0.05f) : 0.0f;
    }

    inline float gainToDb(float gain) noexcept
    {
        return gain > 1.0e-5f ? 20.0f * std::log10(gain) : -100.0f;
    }

    inline float computeSoftKneeGrDb(float inDb, float threshDb, float ratio, float kneeDb) noexcept
    {
        const float slope     = 1.0f - (1.0f / std::max(1.0f, ratio));
        const float halfKnee  = kneeDb * 0.5f;
        const float overshoot = inDb - threshDb;

        if (overshoot <= -halfKnee)
            return 0.0f;

        if (overshoot >= halfKnee || kneeDb <= 0.001f)
            return overshoot * slope;

        const float x = overshoot + halfKnee;
        return slope * (x * x) / (2.0f * kneeDb);
    }

    inline float readDelayHermite4Pt(const std::vector<float>& buf, int writePos, float delaySamples, int mask) noexcept
    {
        const float readPos = static_cast<float>(writePos) - delaySamples;
        const int idx0      = static_cast<int>(std::floor(readPos));
        const float frac    = readPos - static_cast<float>(idx0);

        const float y_1 = buf[static_cast<size_t>((idx0 - 1) & mask)];
        const float y0  = buf[static_cast<size_t>((idx0)     & mask)];
        const float y1  = buf[static_cast<size_t>((idx0 + 1) & mask)];
        const float y2  = buf[static_cast<size_t>((idx0 + 2) & mask)];

        const float c0 = y0;
        const float c1 = 0.5f * (y1 - y_1);
        const float c2 = y_1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - y_1) + 1.5f * (y0 - y1);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    struct AutoSleepTracker
    {
        static constexpr float kSilenceLinearThreshold = 0.0001f; // -80 dBFS
        int silentSamplesRequired = 8820;
        int currentSilentSamples  = 8820;
        bool sleeping             = true;

        void prepare(double sampleRate) noexcept
        {
            silentSamplesRequired = std::max(256, static_cast<int>(sampleRate * 0.200));
            currentSilentSamples  = silentSamplesRequired;
            sleeping              = true;
        }

        void reset() noexcept
        {
            currentSilentSamples = silentSamplesRequired;
            sleeping             = true;
        }

        bool checkShouldSkipBlock(bool knobActive, float inputBlockPeak) noexcept
        {
            if (knobActive && inputBlockPeak > kSilenceLinearThreshold)
            {
                sleeping = false;
                currentSilentSamples = 0;
                return false;
            }
            return sleeping;
        }

        void updateAfterBlock(bool knobActive, float outputBlockPeak, int numSamples) noexcept
        {
            if (!knobActive && outputBlockPeak < kSilenceLinearThreshold)
            {
                currentSilentSamples += numSamples;
                if (currentSilentSamples >= silentSamplesRequired)
                    sleeping = true;
            }
            else if (outputBlockPeak >= kSilenceLinearThreshold)
            {
                currentSilentSamples = 0;
                sleeping = false;
            }
        }
    };

    struct ZeroLatFirstOrderFilter
    {
        float g = 0.0f;
        std::array<float, 2> z1 { 0.0f, 0.0f };

        void reset() noexcept { z1.fill(0.0f); }

        void setCutoff(float freqHz, double sampleRate) noexcept
        {
            const float f = std::clamp(freqHz, 10.0f, static_cast<float>(sampleRate * 0.48));
            const float wa = 2.0f * static_cast<float>(sampleRate)
                           * std::tan(juce::MathConstants<float>::pi * f / static_cast<float>(sampleRate));
            const float gVal = wa / (2.0f * static_cast<float>(sampleRate));
            g = gVal / (1.0f + gVal);
        }

        inline float processHighPass(int ch, float x) noexcept
        {
            const size_t c = static_cast<size_t>(ch);
            const float v  = (x - z1[c]) * g;
            const float lp = v + z1[c];
            z1[c] = lp + v;
            return x - lp;
        }

        inline float processLowPass(int ch, float x) noexcept
        {
            const size_t c = static_cast<size_t>(ch);
            const float v  = (x - z1[c]) * g;
            const float lp = v + z1[c];
            z1[c] = lp + v;
            return lp;
        }
    };

    struct ZeroLatBiquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        std::array<float, 2> s1 { 0.0f, 0.0f };
        std::array<float, 2> s2 { 0.0f, 0.0f };

        void reset() noexcept
        {
            s1.fill(0.0f);
            s2.fill(0.0f);
        }

        void setHighPass(float freqHz, float q, double sampleRate) noexcept
        {
            const float f = std::clamp(freqHz, 15.0f, static_cast<float>(sampleRate * 0.48));
            const float w0 = juce::MathConstants<float>::twoPi * f / static_cast<float>(sampleRate);
            const float cosW0 = std::cos(w0);
            const float alpha = std::sin(w0) / (2.0f * q);
            const float a0Inv = 1.0f / (1.0f + alpha);

            b0 = ((1.0f + cosW0) * 0.5f) * a0Inv;
            b1 = -(1.0f + cosW0) * a0Inv;
            b2 = b0;
            a1 = (-2.0f * cosW0) * a0Inv;
            a2 = (1.0f - alpha) * a0Inv;
        }

        void setLowPass(float freqHz, float q, double sampleRate) noexcept
        {
            const float f = std::clamp(freqHz, 40.0f, static_cast<float>(sampleRate * 0.48));
            const float w0 = juce::MathConstants<float>::twoPi * f / static_cast<float>(sampleRate);
            const float cosW0 = std::cos(w0);
            const float alpha = std::sin(w0) / (2.0f * q);
            const float a0Inv = 1.0f / (1.0f + alpha);

            b1 = (1.0f - cosW0) * a0Inv;
            b0 = b1 * 0.5f;
            b2 = b0;
            a1 = (-2.0f * cosW0) * a0Inv;
            a2 = (1.0f - alpha) * a0Inv;
        }

        void setBandPass0dB(float freqHz, float q, double sampleRate) noexcept
        {
            const float f = std::clamp(freqHz, 40.0f, static_cast<float>(sampleRate * 0.48));
            const float w0 = juce::MathConstants<float>::twoPi * f / static_cast<float>(sampleRate);
            const float cosW0 = std::cos(w0);
            const float alpha = std::sin(w0) / (2.0f * q);
            const float a0Inv = 1.0f / (1.0f + alpha);

            b0 = alpha * a0Inv;
            b1 = 0.0f;
            b2 = -alpha * a0Inv;
            a1 = (-2.0f * cosW0) * a0Inv;
            a2 = (1.0f - alpha) * a0Inv;
        }

        void setPeakingBell(float freqHz, float q, float gainDb, double sampleRate) noexcept
        {
            const float f = std::clamp(freqHz, 20.0f, static_cast<float>(sampleRate * 0.48));
            const float A = std::pow(10.0f, gainDb * 0.025f);
            const float w0 = juce::MathConstants<float>::twoPi * f / static_cast<float>(sampleRate);
            const float cosW0 = std::cos(w0);
            const float alpha = std::sin(w0) / (2.0f * q);
            const float a0Inv = 1.0f / (1.0f + alpha / A);

            b0 = (1.0f + alpha * A) * a0Inv;
            b1 = (-2.0f * cosW0) * a0Inv;
            b2 = (1.0f - alpha * A) * a0Inv;
            a1 = b1;
            a2 = (1.0f - alpha / A) * a0Inv;
        }

        void setLowShelf(float freqHz, float q, float gainDb, double sampleRate) noexcept
        {
            const float f = std::clamp(freqHz, 20.0f, static_cast<float>(sampleRate * 0.47));
            const float A = std::pow(10.0f, gainDb * 0.025f);
            const float w0 = juce::MathConstants<float>::twoPi * f / static_cast<float>(sampleRate);
            const float cosW0 = std::cos(w0);
            const float alpha = std::sin(w0) / (2.0f * q);
            const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
            const float a0Inv = 1.0f / ((A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha);

            b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha)) * a0Inv;
            b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0)) * a0Inv;
            b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha)) * a0Inv;
            a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0)) * a0Inv;
            a2 = ((A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha) * a0Inv;
        }

        void setHighShelf(float freqHz, float q, float gainDb, double sampleRate) noexcept
        {
            const float f = std::clamp(freqHz, 1000.0f, static_cast<float>(sampleRate * 0.47));
            const float A = std::pow(10.0f, gainDb * 0.025f);
            const float w0 = juce::MathConstants<float>::twoPi * f / static_cast<float>(sampleRate);
            const float cosW0 = std::cos(w0);
            const float alpha = std::sin(w0) / (2.0f * q);
            const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
            const float a0Inv = 1.0f / ((A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha);

            b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha)) * a0Inv;
            b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0)) * a0Inv;
            b2 = (A * ((A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha)) * a0Inv;
            a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0)) * a0Inv;
            a2 = ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha) * a0Inv;
        }

        inline float processSample(int ch, float x) noexcept
        {
            const size_t c = static_cast<size_t>(ch);
            const float y = b0 * x + s1[c];
            s1[c] = b1 * x - a1 * y + s2[c];
            s2[c] = b2 * x - a2 * y;
            return y;
        }
    };

    class InputStageModule
    {
    public:
        void prepare(double sampleRate) noexcept
        {
            sr = sampleRate;
            inGainSmoothed.reset(sr, 0.03);
            inGainSmoothed.setCurrentAndTargetValue(1.0f);

            rmsBallisticCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.300f));
            rmsSquaredState   = 0.0f;

            hpf90Order1.setCutoff(90.0f, sr);
            hpf90Order2.setHighPass(90.0f, 1.0f, sr);

            targetAnalysisSamples = static_cast<int64_t>(sr * 10.0);
            reset();
        }

        void reset() noexcept
        {
            hpf90Order1.reset();
            hpf90Order2.reset();
            rmsSquaredState = 0.0f;
            cancelAutoInput();
        }

        void startAutoInput() noexcept
        {
            accumulatedEnergy   = 0.0;
            accumulatedSamples  = 0;
            computedNewInGainDb = 0.0f;
            resultReadyToApply  = false;
            analyzingActive     = true;
        }

        void cancelAutoInput() noexcept
        {
            analyzingActive    = false;
            accumulatedEnergy  = 0.0;
            accumulatedSamples = 0;
            resultReadyToApply = false;
        }

        bool popCompletedAutoGainDb(float& outNewInGainDb) noexcept
        {
            if (resultReadyToApply)
            {
                outNewInGainDb     = computedNewInGainDb;
                resultReadyToApply = false;
                return true;
            }
            return false;
        }

        template <typename TelemetryType>
        void process(juce::AudioBuffer<float>& buffer,
                     float currentInGainDbParam,
                     bool lowCutEnabled,
                     TelemetryType& telemetry) noexcept
        {
            inGainSmoothed.setTargetValue(dbToGain(currentInGainDbParam));

            const int numSamples  = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();
            float* chL = buffer.getWritePointer(0);
            float* chR = numChannels > 1 ? buffer.getWritePointer(1) : chL;

            float blockPeak      = 0.0f;
            double rawBlockPower = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                const float rawL = chL[i];
                const float rawR = chR[i];
                rawBlockPower += 0.5 * static_cast<double>(rawL * rawL + rawR * rawR);

                const float g = inGainSmoothed.getNextValue();
                float sL = rawL * g;
                float sR = rawR * g;

                const float samplePeak = std::max(std::abs(sL), std::abs(sR));
                if (samplePeak > blockPeak)
                    blockPeak = samplePeak;

                const float samplePower = 0.5f * (sL * sL + sR * sR);
                rmsSquaredState = rmsBallisticCoeff * rmsSquaredState + (1.0f - rmsBallisticCoeff) * samplePower;

                if (lowCutEnabled)
                {
                    sL = hpf90Order2.processSample(0, hpf90Order1.processHighPass(0, sL));
                    sR = hpf90Order2.processSample(1, hpf90Order1.processHighPass(1, sR));
                }

                chL[i] = sL;
                chR[i] = sR;
            }

            if (blockPeak >= 0.999f)
                telemetry.inputClipped.store(true, std::memory_order_relaxed);

            telemetry.inputPeakDb.store(gainToDb(blockPeak), std::memory_order_relaxed);
            telemetry.inputRmsDb.store(10.0f * std::log10(std::max(rmsSquaredState, 1.0e-10f)), std::memory_order_relaxed);

            if (analyzingActive && numSamples > 0)
            {
                const double meanRawPower = rawBlockPower / static_cast<double>(numSamples);
                const float postInRmsEst  = static_cast<float>(std::sqrt(meanRawPower)) * dbToGain(currentInGainDbParam);

                constexpr float kVoiceActivityThreshold = 0.00794f; // -42 dBFS
                const bool isSinging = (postInRmsEst >= kVoiceActivityThreshold);
                telemetry.autoInputVoiceActive.store(isSinging, std::memory_order_relaxed);

                if (isSinging)
                {
                    accumulatedEnergy  += rawBlockPower;
                    accumulatedSamples += numSamples;

                    const float progress = std::clamp(
                        static_cast<float>(accumulatedSamples) / static_cast<float>(targetAnalysisSamples),
                        0.0f, 1.0f);
                    telemetry.autoInputProgress.store(progress, std::memory_order_relaxed);

                    if (accumulatedSamples >= targetAnalysisSamples)
                    {
                        const double avgRawRms = std::sqrt(accumulatedEnergy / static_cast<double>(accumulatedSamples));
                        const float avgRawDb   = gainToDb(static_cast<float>(avgRawRms));

                        computedNewInGainDb = std::clamp(-18.0f - avgRawDb, -24.0f, 24.0f);
                        resultReadyToApply  = true;
                        analyzingActive     = false;
                        telemetry.autoInputListening.store(false, std::memory_order_relaxed);
                        telemetry.autoInputProgress.store(1.0f, std::memory_order_relaxed);
                    }
                }
            }
            else
            {
                telemetry.autoInputVoiceActive.store(false, std::memory_order_relaxed);
            }
        }

    private:
        double sr = 44100.0;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inGainSmoothed;

        float rmsBallisticCoeff = 0.999f;
        float rmsSquaredState   = 0.0f;

        ZeroLatFirstOrderFilter hpf90Order1;
        ZeroLatBiquad           hpf90Order2;

        bool    analyzingActive       = false;
        bool    resultReadyToApply    = false;
        double  accumulatedEnergy     = 0.0;
        int64_t accumulatedSamples    = 0;
        int64_t targetAnalysisSamples = 441000;
        float   computedNewInGainDb   = 0.0f;
    };

    class ThreeStageVocalComp
    {
    public:
        void prepare(double sampleRate) noexcept
        {
            sr = sampleRate;
            compSmoothed.reset(sr, 0.025);
            compSmoothed.setCurrentAndTargetValue(0.70f);

            sidechainHpf.setCutoff(95.0f, sr);

            stage1AttCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.018f));
            stage1RelCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.180f));

            stage2AttCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.0015f));
            stage2RelCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.065f));

            stage3AttCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.0002f));
            stage3RelCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.035f));

            reset();
        }

        void reset() noexcept
        {
            sidechainHpf.reset();
            envStage1 = 0.0f;
            envStage2 = 0.0f;
            envStage3 = 0.0f;
        }

        template <typename TelemetryType>
        void process(juce::AudioBuffer<float>& buffer, float compPct, TelemetryType& telemetry) noexcept
        {
            compSmoothed.setTargetValue(std::clamp(compPct * 0.01f, 0.0f, 1.0f));

            const int numSamples  = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();
            float* chL = buffer.getWritePointer(0);
            float* chR = numChannels > 1 ? buffer.getWritePointer(1) : chL;

            float maxTotalGrDb = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float c = compSmoothed.getNextValue();
                if (c <= 0.001f)
                    continue;

                const float inL = chL[i];
                const float inR = chR[i];

                const float scL = sidechainHpf.processHighPass(0, inL);
                const float scR = sidechainHpf.processHighPass(1, inR);
                const float scPeak = std::max(std::abs(scL), std::abs(scR));

                // Étage 1 : Compression douce parallèle (0..50%)
                const float stage1Drive = std::clamp(c * 2.0f, 0.0f, 1.0f);
                envStage1 = (scPeak > envStage1)
                          ? (stage1AttCoeff * envStage1 + (1.0f - stage1AttCoeff) * scPeak)
                          : (stage1RelCoeff * envStage1 + (1.0f - stage1RelCoeff) * scPeak);

                const float s1ThreshDb = -10.0f - 18.0f * stage1Drive;
                const float s1GrDb     = computeSoftKneeGrDb(gainToDb(envStage1), s1ThreshDb, 2.6f, 8.0f);
                const float s1GainLin  = dbToGain(-s1GrDb);
                const float s1WetMix   = 0.70f * std::pow(stage1Drive, 0.75f);

                float sigL = inL * (1.0f - s1WetMix) + (inL * s1GainLin) * s1WetMix;
                float sigR = inR * (1.0f - s1WetMix) + (inR * s1GainLin) * s1WetMix;

                // Étage 2 : Compression agressive en série (50..100%)
                const float stage2Drive = std::clamp((c - 0.20f) / 0.80f, 0.0f, 1.0f);
                float s2GrDb = 0.0f;
                if (stage2Drive > 0.001f)
                {
                    const float s1Peak = std::max(std::abs(sigL), std::abs(sigR));
                    envStage2 = (s1Peak > envStage2)
                              ? (stage2AttCoeff * envStage2 + (1.0f - stage2AttCoeff) * s1Peak)
                              : (stage2RelCoeff * envStage2 + (1.0f - stage2RelCoeff) * s1Peak);

                    const float s2ThreshDb = -8.0f - 18.0f * stage2Drive;
                    const float s2Ratio    = 3.2f + 4.8f * stage2Drive;
                    s2GrDb = computeSoftKneeGrDb(gainToDb(envStage2), s2ThreshDb, s2Ratio, 3.5f);
                    const float s2GainLin = dbToGain(-s2GrDb);

                    sigL *= s2GainLin;
                    sigR *= s2GainLin;
                }

                // Étage 3 : Limiteur de crêtes (> 75%)
                const float stage3Drive = std::clamp((c - 0.70f) / 0.30f, 0.0f, 1.0f);
                float s3GrDb = 0.0f;
                if (stage3Drive > 0.001f)
                {
                    const float s2Peak = std::max(std::abs(sigL), std::abs(sigR));
                    envStage3 = (s2Peak > envStage3)
                              ? (stage3AttCoeff * envStage3 + (1.0f - stage3AttCoeff) * s2Peak)
                              : (stage3RelCoeff * envStage3 + (1.0f - stage3RelCoeff) * s2Peak);

                    const float s3CeilingDb = -6.0f - 8.0f * stage3Drive;
                    s3GrDb = computeSoftKneeGrDb(gainToDb(envStage3), s3CeilingDb, 20.0f, 1.5f);
                    const float s3GainLin = dbToGain(-s3GrDb);

                    sigL *= s3GainLin;
                    sigR *= s3GainLin;
                }

                const float makeupDb = (3.2f * stage1Drive * s1WetMix)
                                     + (5.8f * stage2Drive)
                                     + (2.4f * stage3Drive);
                const float makeupLin = dbToGain(makeupDb);

                chL[i] = sigL * makeupLin;
                chR[i] = sigR * makeupLin;

                const float sampleTotalGr = (s1GrDb * s1WetMix) + s2GrDb + s3GrDb;
                if (sampleTotalGr > maxTotalGrDb)
                    maxTotalGrDb = sampleTotalGr;
            }

            telemetry.compGainReductionDb.store(maxTotalGrDb, std::memory_order_relaxed);
        }

    private:
        double sr = 44100.0;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compSmoothed;
        ZeroLatFirstOrderFilter sidechainHpf;

        float stage1AttCoeff = 0.99f, stage1RelCoeff = 0.999f;
        float stage2AttCoeff = 0.99f, stage2RelCoeff = 0.999f;
        float stage3AttCoeff = 0.99f, stage3RelCoeff = 0.999f;

        float envStage1 = 0.0f;
        float envStage2 = 0.0f;
        float envStage3 = 0.0f;
    };

    class VocalEQ4Band
    {
    public:
        void prepare(double sampleRate) noexcept
        {
            sr = sampleRate;

            lowDbSmoothed.reset(sr, 0.02);
            midDbSmoothed.reset(sr, 0.02);
            hiDbSmoothed.reset(sr, 0.02);
            airDbSmoothed.reset(sr, 0.02);

            lowDbSmoothed.setCurrentAndTargetValue(0.0f);
            midDbSmoothed.setCurrentAndTargetValue(0.0f);
            hiDbSmoothed.setCurrentAndTargetValue(0.0f);
            airDbSmoothed.setCurrentAndTargetValue(0.0f);

            secretMudCut.setPeakingBell(260.0f, 1.15f, -1.6f, sr);
            secretPresenceLift.setPeakingBell(2400.0f, 0.90f, 1.2f, sr);

            airHighBandExtractor.setHighPass(9800.0f, 0.707f, sr);
            airDynAttCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.001f));
            airDynRelCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.050f));

            updateControllableBands(0.0f, 0.0f, 0.0f, 0.0f);
            reset();
        }

        void reset() noexcept
        {
            secretMudCut.reset();
            secretPresenceLift.reset();
            lowShelf.reset();
            midBell.reset();
            hiBell.reset();
            airShelf.reset();
            airHighBandExtractor.reset();
            airEnvLinear = 0.0f;
        }

        template <typename TelemetryType>
        void process(juce::AudioBuffer<float>& buffer,
                     bool sectionBypassed,
                     float lowDb, float midDb, float hiDb, float airDb,
                     TelemetryType& telemetry) noexcept
        {
            if (sectionBypassed)
            {
                telemetry.airDynGainReductionDb.store(0.0f, std::memory_order_relaxed);
                return;
            }

            lowDbSmoothed.setTargetValue(std::clamp(lowDb, -15.0f, 15.0f));
            midDbSmoothed.setTargetValue(std::clamp(midDb, -15.0f, 15.0f));
            hiDbSmoothed.setTargetValue(std::clamp(hiDb, -15.0f, 15.0f));
            airDbSmoothed.setTargetValue(std::clamp(airDb, -15.0f, 15.0f));

            const int numSamples  = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();
            float* chL = buffer.getWritePointer(0);
            float* chR = numChannels > 1 ? buffer.getWritePointer(1) : chL;

            const float curLow = lowDbSmoothed.skip(numSamples);
            const float curMid = midDbSmoothed.skip(numSamples);
            const float curHi  = hiDbSmoothed.skip(numSamples);
            const float curAir = airDbSmoothed.skip(numSamples);

            updateControllableBands(curLow, curMid, curHi, curAir);

            float maxAirDynGrDb = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                float sL = chL[i];
                float sR = chR[i];

                sL = secretPresenceLift.processSample(0, secretMudCut.processSample(0, sL));
                sR = secretPresenceLift.processSample(1, secretMudCut.processSample(1, sR));

                sL = airShelf.processSample(0, hiBell.processSample(0, midBell.processSample(0, lowShelf.processSample(0, sL))));
                sR = airShelf.processSample(1, hiBell.processSample(1, midBell.processSample(1, lowShelf.processSample(1, sR))));

                if (curAir > 0.5f)
                {
                    const float hfL = airHighBandExtractor.processSample(0, sL);
                    const float hfR = airHighBandExtractor.processSample(1, sR);
                    const float hfPeak = std::max(std::abs(hfL), std::abs(hfR));

                    airEnvLinear = (hfPeak > airEnvLinear)
                                 ? (airDynAttCoeff * airEnvLinear + (1.0f - airDynAttCoeff) * hfPeak)
                                 : (airDynRelCoeff * airEnvLinear + (1.0f - airDynRelCoeff) * hfPeak);

                    const float dynThreshDb = -16.0f - (curAir * 0.45f);
                    const float maxClampDb  = curAir * 0.70f;
                    const float dynGrDb     = std::min(maxClampDb,
                                                       computeSoftKneeGrDb(gainToDb(airEnvLinear), dynThreshDb, 4.0f, 4.0f));
                    if (dynGrDb > maxAirDynGrDb)
                        maxAirDynGrDb = dynGrDb;

                    const float cancelGain = 1.0f - dbToGain(-dynGrDb);
                    sL -= cancelGain * hfL;
                    sR -= cancelGain * hfR;
                }

                chL[i] = sL;
                chR[i] = sR;
            }

            telemetry.airDynGainReductionDb.store(maxAirDynGrDb, std::memory_order_relaxed);
        }

    private:
        void updateControllableBands(float lowDb, float midDb, float hiDb, float airDb) noexcept
        {
            lowShelf.setLowShelf(140.0f, 0.707f, lowDb, sr);
            midBell.setPeakingBell(1350.0f, 0.95f, midDb, sr);
            hiBell.setPeakingBell(3500.0f, 1.05f, hiDb, sr);
            airShelf.setHighShelf(10500.0f, 0.707f, airDb, sr);
        }

        double sr = 44100.0;

        ZeroLatBiquad secretMudCut;
        ZeroLatBiquad secretPresenceLift;

        ZeroLatBiquad lowShelf;
        ZeroLatBiquad midBell;
        ZeroLatBiquad hiBell;
        ZeroLatBiquad airShelf;
        ZeroLatBiquad airHighBandExtractor;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowDbSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> midDbSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> hiDbSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> airDbSmoothed;

        float airDynAttCoeff = 0.99f;
        float airDynRelCoeff = 0.999f;
        float airEnvLinear   = 0.0f;
    };

    class VocalFXRack
    {
    public:
        void prepare(double sampleRate) noexcept
        {
            sr = sampleRate;

            deEssSmoothed.reset(sr, 0.02);
            driveSmoothed.reset(sr, 0.02);
            phoneSmoothed.reset(sr, 0.02);
            doublerSmoothed.reset(sr, 0.025);
            chorusSmoothed.reset(sr, 0.025);

            sibilanceFilter.setBandPass0dB(6400.0f, 1.9f, sr);
            deEssAttCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.0004f));
            deEssRelCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.040f));

            drivePreFilter.setBandPass0dB(1400.0f, 0.7f, sr);
            drivePostLowPass.setCutoff(9500.0f, sr);
            driveDcBlocker.setCutoff(20.0f, sr);

            phoneHpf.setHighPass(80.0f, 0.85f, sr);
            phoneLpf.setLowPass(18000.0f, 0.85f, sr);

            modBufSize = 16384;
            modBufMask = modBufSize - 1;
            doublerBufL.assign(static_cast<size_t>(modBufSize), 0.0f);
            doublerBufR.assign(static_cast<size_t>(modBufSize), 0.0f);
            chorusBufL.assign(static_cast<size_t>(modBufSize), 0.0f);
            chorusBufR.assign(static_cast<size_t>(modBufSize), 0.0f);

            doublerBandHpf.setCutoff(200.0f, sr);
            doublerBandLpf.setCutoff(7000.0f, sr);
            chorusBandHpf.setCutoff(160.0f, sr);

            deEssSleep.prepare(sr);
            doublerSleep.prepare(sr);
            chorusSleep.prepare(sr);

            reset();
        }

        void reset() noexcept
        {
            sibilanceFilter.reset();
            drivePreFilter.reset();
            drivePostLowPass.reset();
            driveDcBlocker.reset();
            phoneHpf.reset();
            phoneLpf.reset();
            doublerBandHpf.reset();
            doublerBandLpf.reset();
            chorusBandHpf.reset();

            std::fill(doublerBufL.begin(), doublerBufL.end(), 0.0f);
            std::fill(doublerBufR.begin(), doublerBufR.end(), 0.0f);
            std::fill(chorusBufL.begin(), chorusBufL.end(), 0.0f);
            std::fill(chorusBufR.begin(), chorusBufR.end(), 0.0f);

            deEssEnv  = 0.0f;
            writePos  = 0;
            dblPhase1 = 0.0f;
            dblPhase2 = 1.5707963f;
            choPhaseL = 0.0f;
            choPhaseR = 3.1415926f;

            deEssSleep.reset();
            doublerSleep.reset();
            chorusSleep.reset();
        }

        template <typename TelemetryType>
        void process(juce::AudioBuffer<float>& buffer,
                     bool fxSectionBypassed,
                     float deEssPct,
                     float drivePct,
                     float phonePct,
                     float doublerPct,
                     float chorusPct,
                     TelemetryType& telemetry,
                     int& sleepingModulesCounter) noexcept
        {
            if (fxSectionBypassed)
            {
                telemetry.deEssGainReduction.store(0.0f, std::memory_order_relaxed);
                return;
            }

            const int numSamples  = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();
            float* chL = buffer.getWritePointer(0);
            float* chR = numChannels > 1 ? buffer.getWritePointer(1) : chL;

            const float inPeak = std::max(buffer.getMagnitude(0, 0, numSamples),
                                          numChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : 0.0f);

            const float targetDeEss   = std::clamp(deEssPct   * 0.01f, 0.0f, 1.0f);
            const float targetDrive   = std::clamp(drivePct   * 0.01f, 0.0f, 1.0f);
            const float targetPhone   = std::clamp(phonePct   * 0.01f, 0.0f, 1.0f);
            const float targetDoubler = std::clamp(doublerPct * 0.01f, 0.0f, 1.0f);
            const float targetChorus  = std::clamp(chorusPct  * 0.01f, 0.0f, 1.0f);

            deEssSmoothed.setTargetValue(targetDeEss);
            driveSmoothed.setTargetValue(targetDrive);
            phoneSmoothed.setTargetValue(targetPhone);
            doublerSmoothed.setTargetValue(targetDoubler);
            chorusSmoothed.setTargetValue(targetChorus);

            if (targetPhone > 0.001f)
            {
                const float pCurve = std::pow(targetPhone, 0.85f);
                const float hpFreq = 80.0f + 500.0f * pCurve;
                const float lpFreq = 18000.0f * std::pow(2400.0f / 18000.0f, pCurve);
                phoneHpf.setHighPass(hpFreq, 0.90f + 0.6f * pCurve, sr);
                phoneLpf.setLowPass(lpFreq, 0.90f + 0.6f * pCurve, sr);
            }

            const bool runDeEss   = !deEssSleep.checkShouldSkipBlock(targetDeEss > 0.001f, inPeak);
            const bool runDoubler = !doublerSleep.checkShouldSkipBlock(targetDoubler > 0.001f, inPeak);
            const bool runChorus  = !chorusSleep.checkShouldSkipBlock(targetChorus > 0.001f, inPeak);

            if (!runDeEss)   ++sleepingModulesCounter;
            if (!runDoubler) ++sleepingModulesCounter;
            if (!runChorus)  ++sleepingModulesCounter;

            const float fSr     = static_cast<float>(sr);
            const float dblInc1 = juce::MathConstants<float>::twoPi * 0.72f / fSr;
            const float dblInc2 = juce::MathConstants<float>::twoPi * 0.91f / fSr;
            const float choInc  = juce::MathConstants<float>::twoPi * 1.15f / fSr;

            float maxDeEssGrDb  = 0.0f;
            float maxDoublerOut = 0.0f;
            float maxChorusOut  = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float dEssAmt = deEssSmoothed.getNextValue();
                const float drvAmt  = driveSmoothed.getNextValue();
                const float phnAmt  = phoneSmoothed.getNextValue();
                const float dblAmt  = doublerSmoothed.getNextValue();
                const float choAmt  = chorusSmoothed.getNextValue();

                float sL = chL[i];
                float sR = chR[i];

                if (runDeEss && dEssAmt > 0.001f)
                {
                    const float sibL = sibilanceFilter.processSample(0, sL);
                    const float sibR = sibilanceFilter.processSample(1, sR);
                    const float sibPeak = std::max(std::abs(sibL), std::abs(sibR));

                    deEssEnv = (sibPeak > deEssEnv)
                             ? (deEssAttCoeff * deEssEnv + (1.0f - deEssAttCoeff) * sibPeak)
                             : (deEssRelCoeff * deEssEnv + (1.0f - deEssRelCoeff) * sibPeak);

                    const float threshDb = -10.0f - 28.0f * dEssAmt;
                    const float maxRedDb = 16.0f * dEssAmt;
                    const float grDb     = std::min(maxRedDb,
                                                    computeSoftKneeGrDb(gainToDb(deEssEnv), threshDb, 6.0f, 3.0f));
                    if (grDb > maxDeEssGrDb)
                        maxDeEssGrDb = grDb;

                    const float cancel = 1.0f - dbToGain(-grDb);
                    sL -= cancel * sibL;
                    sR -= cancel * sibR;
                }

                if (drvAmt > 0.001f)
                {
                    const float preL = drivePreFilter.processSample(0, sL);
                    const float preR = drivePreFilter.processSample(1, sR);

                    const float pregain  = 1.5f + 7.5f * drvAmt;
                    constexpr float bias = 0.14f;
                    const float norm     = 1.0f / std::tanh(pregain * 0.8f);

                    float satL = (std::tanh(preL * pregain + bias) - std::tanh(bias)) * norm;
                    float satR = (std::tanh(preR * pregain + bias) - std::tanh(bias)) * norm;

                    satL = drivePostLowPass.processLowPass(0, driveDcBlocker.processHighPass(0, satL));
                    satR = drivePostLowPass.processLowPass(1, driveDcBlocker.processHighPass(1, satR));

                    const float parallelGain = 0.72f * std::pow(drvAmt, 0.8f);
                    sL = (sL + satL * parallelGain) / (1.0f + 0.30f * parallelGain);
                    sR = (sR + satR * parallelGain) / (1.0f + 0.30f * parallelGain);
                }

                if (phnAmt > 0.001f)
                {
                    const float phL = phoneLpf.processSample(0, phoneHpf.processSample(0, sL)) * 1.35f;
                    const float phR = phoneLpf.processSample(1, phoneHpf.processSample(1, sR)) * 1.35f;

                    sL = sL * (1.0f - phnAmt) + phL * phnAmt;
                    sR = sR * (1.0f - phnAmt) + phR * phnAmt;
                }

                if (runDoubler)
                {
                    const float monoMid = 0.5f * (sL + sR);
                    const float cleanIn = doublerBandLpf.processLowPass(0, doublerBandHpf.processHighPass(0, monoMid));

                    doublerBufL[static_cast<size_t>(writePos)] = cleanIn;
                    doublerBufR[static_cast<size_t>(writePos)] = cleanIn;

                    const float baseDly1 = 0.0135f * fSr;
                    const float baseDly2 = 0.0215f * fSr;
                    const float modDepth = 0.0011f * fSr;

                    const float tap1 = readDelayHermite4Pt(doublerBufL, writePos, baseDly1 + modDepth * std::sin(dblPhase1), modBufMask);
                    const float tap2 = readDelayHermite4Pt(doublerBufR, writePos, baseDly2 + modDepth * std::cos(dblPhase2), modBufMask);

                    dblPhase1 += dblInc1;
                    if (dblPhase1 >= juce::MathConstants<float>::twoPi) dblPhase1 -= juce::MathConstants<float>::twoPi;
                    dblPhase2 += dblInc2;
                    if (dblPhase2 >= juce::MathConstants<float>::twoPi) dblPhase2 -= juce::MathConstants<float>::twoPi;

                    const float sideSignal = 0.5f * (tap1 - tap2) * (dblAmt * 0.95f);
                    sL += sideSignal;
                    sR -= sideSignal;

                    maxDoublerOut = std::max(maxDoublerOut, std::abs(sideSignal));
                }

                if (runChorus)
                {
                    const float choInL = chorusBandHpf.processHighPass(0, sL);
                    const float choInR = chorusBandHpf.processHighPass(1, sR);

                    chorusBufL[static_cast<size_t>(writePos)] = choInL;
                    chorusBufR[static_cast<size_t>(writePos)] = choInR;

                    const float centerDly = 0.016f * fSr;
                    const float depthDly  = (0.0012f + 0.0022f * choAmt) * fSr;

                    const float cTapL = readDelayHermite4Pt(chorusBufL, writePos, centerDly + depthDly * std::sin(choPhaseL), modBufMask);
                    const float cTapR = readDelayHermite4Pt(chorusBufR, writePos, centerDly + depthDly * std::sin(choPhaseR), modBufMask);

                    choPhaseL += choInc;
                    if (choPhaseL >= juce::MathConstants<float>::twoPi) choPhaseL -= juce::MathConstants<float>::twoPi;
                    choPhaseR += choInc;
                    if (choPhaseR >= juce::MathConstants<float>::twoPi) choPhaseR -= juce::MathConstants<float>::twoPi;

                    const float wetCho = choAmt * 0.55f;
                    sL += cTapL * wetCho;
                    sR += cTapR * wetCho;

                    maxChorusOut = std::max(maxChorusOut, std::max(std::abs(cTapL), std::abs(cTapR)) * wetCho);
                }

                chL[i] = sL;
                chR[i] = sR;
                writePos = (writePos + 1) & modBufMask;
            }

            deEssSleep.updateAfterBlock(targetDeEss > 0.001f, maxDeEssGrDb > 0.05f ? 1.0f : 0.0f, numSamples);
            doublerSleep.updateAfterBlock(targetDoubler > 0.001f, maxDoublerOut, numSamples);
            chorusSleep.updateAfterBlock(targetChorus > 0.001f, maxChorusOut, numSamples);

            telemetry.deEssGainReduction.store(std::clamp(maxDeEssGrDb / 12.0f, 0.0f, 1.0f), std::memory_order_relaxed);
        }

    private:
        double sr = 44100.0;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> deEssSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> phoneSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> doublerSmoothed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> chorusSmoothed;

        ZeroLatBiquad           sibilanceFilter;
        ZeroLatBiquad           drivePreFilter;
        ZeroLatFirstOrderFilter drivePostLowPass;
        ZeroLatFirstOrderFilter driveDcBlocker;
        ZeroLatBiquad           phoneHpf;
        ZeroLatBiquad           phoneLpf;
        ZeroLatFirstOrderFilter doublerBandHpf;
        ZeroLatFirstOrderFilter doublerBandLpf;
        ZeroLatFirstOrderFilter chorusBandHpf;

        float deEssAttCoeff = 0.99f;
        float deEssRelCoeff = 0.999f;
        float deEssEnv      = 0.0f;

        int modBufSize = 16384;
        int modBufMask = 16383;
        int writePos   = 0;
        std::vector<float> doublerBufL, doublerBufR;
        std::vector<float> chorusBufL,  chorusBufR;

        float dblPhase1 = 0.0f, dblPhase2 = 1.5707963f;
        float choPhaseL = 0.0f, choPhaseR = 3.1415926f;

        AutoSleepTracker deEssSleep;
        AutoSleepTracker doublerSleep;
        AutoSleepTracker chorusSleep;
    };

} // namespace BigVoixDSP
