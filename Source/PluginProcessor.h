#pragma once

#include <JuceHeader.h>

#include "Parameters.h"
#include "core/MatchFitter.h"
#include "core/PresetManager.h"
#include "core/SpectrumAccumulator.h"
#include "core/Dynamics.h"
#include "dsp/Compressor.h"
#include "dsp/EqEngine.h"
#include "dsp/Limiter.h"

/** A lock-free single-producer/single-consumer sample queue (audio thread -> UI/analysis). */
class SampleFifo
{
public:
    explicit SampleFifo (int capacity) : fifo (capacity), buffer ((size_t) capacity) {}

    void push (const float* data, int num) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (num, s1, n1, s2, n2); // drops samples if the reader falls behind
        if (n1 > 0) std::copy (data, data + n1, buffer.begin() + s1);
        if (n2 > 0) std::copy (data + n1, data + n1 + n2, buffer.begin() + s2);
        fifo.finishedWrite (n1 + n2);
    }

    int pull (float* dest, int maxNum) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (maxNum, s1, n1, s2, n2);
        if (n1 > 0) std::copy (buffer.begin() + s1, buffer.begin() + s1 + n1, dest);
        if (n2 > 0) std::copy (buffer.begin() + s2, buffer.begin() + s2 + n2, dest + n1);
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }

    void clear() noexcept { fifo.reset(); }

private:
    juce::AbstractFifo fifo;
    std::vector<float> buffer;
};

class MagicEQProcessor : public juce::AudioProcessor,
                         private juce::Timer
{
public:
    enum class DetectState { idle, listening, applying };

    MagicEQProcessor();
    ~MagicEQProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ----- Detect (message thread only) -----
    void startDetect();
    void cancelDetect();
    DetectState getDetectState() const noexcept { return detectState; }
    float getDetectProgress() const noexcept;       // 0..1 while listening
    double getDetectSecondsHeard() const noexcept { return accumulator.getActiveSeconds(); }
    double getDetectTargetSeconds() const;
    bool isHearingVoice() const noexcept;           // the last analysed frame passed the gate
    const magiceq::MatchResult& getLastMatch() const noexcept { return lastMatch; }
    const magiceq::DynamicsFit& getLastDynamics() const noexcept { return lastDynamics; }

    /** Current compressor settings, read from the parameters. */
    magiceq::CompSettings getComp() const;

    /** Largest compressor gain reduction since the last call, in dB (UI meter; message thread). */
    float takeGainReductionDb() noexcept { return gainReductionDb.exchange (0.0f); }

    /** Current limiter settings, read from the parameters. */
    magiceq::LimiterSettings getLimiter() const;

    /** Meter readings since the last call (message thread). Peaks are linear, limiter GR in dB. */
    struct MeterReadings { float inPeak = 0, inRms = 0, outPeak = 0, outRms = 0, limiterGrDb = 0; };
    MeterReadings takeMeterReadings() noexcept;

    // ----- A/B compare (message thread only) -----
    void switchAB();

    // ----- Presets (message thread only) -----
    magiceq::PresetManager& getPresets() noexcept { return presets; }
    magiceq::ParamValues captureValues() const;
    bool loadPreset (const juce::String& name);        // one Undo step
    bool saveCurrentAs (const juce::String& name);
    juce::String getCurrentPresetName() const { return currentPreset; }
    bool isPresetModified() const;                    // changed since the preset was loaded or saved
    void stepPreset (int delta);                      // previous / next
    bool isOnB() const noexcept { return onB; }

    /** Current settings of a band, read from the parameters. */
    magiceq::BandSettings getBand (int band) const;

    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    /** Post-EQ mono signal for the editor's spectrum analyzer. */
    SampleFifo analyzerFifo { 1 << 15 };

private:
    void timerCallback() override;
    void finishDetect();
    void updateGlide();

    struct BandParams
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* slope = nullptr;
    };
    std::array<BandParams, magiceq::EqEngine::numBands> bandParams;
    std::atomic<float>* outputParam = nullptr;

    struct CompParams
    {
        std::atomic<float>* on = nullptr, *threshold = nullptr, *ratio = nullptr, *attack = nullptr,
                          *release = nullptr, *knee = nullptr, *makeup = nullptr, *mix = nullptr;
    } compParams;

    magiceq::Compressor compressor;
    magiceq::Limiter limiter;
    std::atomic<float>* limOnParam = nullptr, *limCeilingParam = nullptr, *limReleaseParam = nullptr, *limTruePeakParam = nullptr;
    std::atomic<float> inPeak { 0.0f }, inRms { 0.0f }, outPeak { 0.0f }, outRms { 0.0f }, limiterGr { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    magiceq::EqEngine eq;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGain;
    std::vector<float> monoScratch;
    double currentSampleRate = 48000.0;

    // Detect
    std::atomic<bool> captureForDetect { false };
    SampleFifo detectFifo { 1 << 17 };
    magiceq::SpectrumAccumulator accumulator;
    std::vector<float> detectPullBuffer = std::vector<float> (1 << 14);
    DetectState detectState = DetectState::idle;
    magiceq::MatchResult lastMatch;
    magiceq::DynamicsFit lastDynamics;
    double listenStartedMs = 0.0;

    struct GlideItem
    {
        juce::RangedAudioParameter* param;
        float from, to; // normalised
    };
    std::vector<GlideItem> glide;
    std::vector<std::pair<juce::RangedAudioParameter*, float>> afterGlide; // switched at the end (bands turning off)
    double glideStartMs = 0.0;
    static constexpr double glideDurationMs = 900.0;

    // A/B
    bool onB = false;
    juce::ValueTree otherSlot;

    // Presets
    magiceq::PresetManager presets { magiceq::PresetManager::defaultFolder() };
    juce::String currentPreset { "Init (Flat)" };
    magiceq::ParamValues presetSnapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagicEQProcessor)
};
