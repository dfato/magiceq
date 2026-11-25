#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace magiceq;

MagicEQProcessor::MagicEQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "MagicEQ", params::createLayout())
{
    for (int b = 0; b < EqEngine::numBands; ++b)
    {
        auto& p = bandParams[(size_t) b];
        p.on    = apvts.getRawParameterValue (params::on (b));
        p.type  = apvts.getRawParameterValue (params::type (b));
        p.freq  = apvts.getRawParameterValue (params::freq (b));
        p.gain  = apvts.getRawParameterValue (params::gain (b));
        p.q     = apvts.getRawParameterValue (params::q (b));
        p.slope = apvts.getRawParameterValue (params::slope (b));
    }
    outputParam = apvts.getRawParameterValue (params::output);

    compParams.on        = apvts.getRawParameterValue (params::compOn);
    compParams.threshold = apvts.getRawParameterValue (params::compThresh);
    compParams.ratio     = apvts.getRawParameterValue (params::compRatio);
    compParams.attack    = apvts.getRawParameterValue (params::compAttack);
    compParams.release   = apvts.getRawParameterValue (params::compRelease);
    compParams.knee      = apvts.getRawParameterValue (params::compKnee);
    compParams.makeup    = apvts.getRawParameterValue (params::compMakeup);
    compParams.mix       = apvts.getRawParameterValue (params::compMix);

    limOnParam       = apvts.getRawParameterValue (params::limOn);
    limCeilingParam  = apvts.getRawParameterValue (params::limCeiling);
    limReleaseParam  = apvts.getRawParameterValue (params::limRelease);
    limTruePeakParam = apvts.getRawParameterValue (params::limTruePeak);

    startTimerHz (30);
}

MagicEQProcessor::~MagicEQProcessor()
{
    stopTimer();
}

bool MagicEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void MagicEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (int b = 0; b < EqEngine::numBands; ++b)
        eq.setTarget (b, getBand (b));
    eq.prepare (sampleRate);

    compressor.setSettings (getComp());
    compressor.prepare (sampleRate);

    limiter.setSettings (getLimiter());
    limiter.prepare (sampleRate);
    setLatencySamples (limiter.getLatencySamples()); // ~1.2 ms of look-ahead, reported so the DAW compensates

    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    monoScratch.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);

    if (detectState == DetectState::listening)
        accumulator.prepare (sampleRate);
}

BandSettings MagicEQProcessor::getBand (int band) const
{
    const auto& p = bandParams[(size_t) band];
    BandSettings s;
    s.enabled = p.on->load() > 0.5f;
    s.type = (BandType) juce::jlimit (0, (int) BandType::numTypes - 1, (int) p.type->load());
    s.frequency = p.freq->load();
    s.gainDb = p.gain->load();
    s.q = p.q->load();
    s.slopeIndex = juce::jlimit (0, numCutSlopes - 1, (int) p.slope->load());
    return s;
}

CompSettings MagicEQProcessor::getComp() const
{
    CompSettings c;
    c.enabled     = compParams.on->load() > 0.5f;
    c.thresholdDb = compParams.threshold->load();
    c.ratio       = juce::jmax (1.0f, compParams.ratio->load());
    c.attackMs    = compParams.attack->load();
    c.releaseMs   = compParams.release->load();
    c.kneeDb      = compParams.knee->load();
    c.makeupDb    = compParams.makeup->load();
    c.mix         = compParams.mix->load() / 100.0f;
    return c;
}

LimiterSettings MagicEQProcessor::getLimiter() const
{
    LimiterSettings l;
    l.enabled   = limOnParam->load() > 0.5f;
    l.ceilingDb = limCeilingParam->load();
    l.releaseMs = limReleaseParam->load();
    l.truePeak  = limTruePeakParam->load() > 0.5f;
    return l;
}

namespace
{
    void storeMax (std::atomic<float>& a, float v) noexcept
    {
        float prev = a.load (std::memory_order_relaxed);
        while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
    }

    void measure (const juce::AudioBuffer<float>& b, int numChannels, std::atomic<float>& peak, std::atomic<float>& rms) noexcept
    {
        float p = 0.0f, r = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            p = juce::jmax (p, b.getMagnitude (ch, 0, b.getNumSamples()));
            r = juce::jmax (r, b.getRMSLevel (ch, 0, b.getNumSamples()));
        }
        storeMax (peak, p);
        storeMax (rms, r);
    }
}

MagicEQProcessor::MeterReadings MagicEQProcessor::takeMeterReadings() noexcept
{
    MeterReadings m;
    m.inPeak = inPeak.exchange (0.0f);
    m.inRms = inRms.exchange (0.0f);
    m.outPeak = outPeak.exchange (0.0f);
    m.outRms = outRms.exchange (0.0f);
    m.limiterGrDb = limiterGr.exchange (0.0f);
    return m;
}

void MagicEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    if (numChannels == 0 || numSamples == 0)
        return;

    if ((int) monoScratch.size() < numSamples) // hosts occasionally exceed the announced block size
        monoScratch.resize ((size_t) numSamples);

    auto mixToMono = [&]
    {
        const float scale = 1.0f / (float) numChannels;
        for (int i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                sum += buffer.getSample (ch, i);
            monoScratch[(size_t) i] = sum * scale;
        }
    };

    measure (buffer, numChannels, inPeak, inRms);

    // Detect listens to the vocal before MagicEQ changes it.
    if (captureForDetect.load (std::memory_order_relaxed))
    {
        mixToMono();
        detectFifo.push (monoScratch.data(), numSamples);
    }

    for (int b = 0; b < EqEngine::numBands; ++b)
        eq.setTarget (b, getBand (b));

    eq.process (buffer.getArrayOfWritePointers(), numChannels, numSamples);

    // Compressor after the EQ, so the EQ's tone shaping is what gets compressed (the usual vocal chain order).
    compressor.setSettings (getComp());
    const float gr = compressor.process (buffer.getArrayOfWritePointers(), numChannels, numSamples);
    storeMax (gainReductionDb, gr);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    if (outputGain.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outputGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else if (std::abs (outputGain.getTargetValue() - 1.0f) > 1.0e-6f)
    {
        buffer.applyGain (outputGain.getTargetValue());
    }

    // Limiter last, after the output gain, so its ceiling really is the final level.
    limiter.setSettings (getLimiter());
    storeMax (limiterGr, limiter.process (buffer.getArrayOfWritePointers(), numChannels, numSamples));

    measure (buffer, numChannels, outPeak, outRms);

    mixToMono();
    analyzerFifo.push (monoScratch.data(), numSamples);
}

// ============================================================================
// Detect
// ============================================================================

double MagicEQProcessor::getDetectTargetSeconds() const
{
    const int idx = juce::jlimit (0, 2, (int) apvts.getRawParameterValue (params::detectLen)->load());
    return params::detectLengthSeconds[idx];
}

float MagicEQProcessor::getDetectProgress() const noexcept
{
    if (detectState != DetectState::listening)
        return 0.0f;
    return (float) juce::jlimit (0.0, 1.0, accumulator.getActiveSeconds() / getDetectTargetSeconds());
}

bool MagicEQProcessor::isHearingVoice() const noexcept
{
    return detectState == DetectState::listening
        && accumulator.getLastFrameLevelDb() > SpectrumAccumulator::absoluteGateDb;
}

void MagicEQProcessor::startDetect()
{
    if (detectState == DetectState::applying)
        return;

    // Throw away anything left in the queue from a previous run.
    captureForDetect = false;
    while (detectFifo.pull (detectPullBuffer.data(), (int) detectPullBuffer.size()) > 0) {}

    accumulator.prepare (currentSampleRate);
    listenStartedMs = juce::Time::getMillisecondCounterHiRes();
    detectState = DetectState::listening;
    captureForDetect = true;
}

void MagicEQProcessor::cancelDetect()
{
    if (detectState == DetectState::listening)
    {
        captureForDetect = false;
        detectState = DetectState::idle;
    }
}

void MagicEQProcessor::timerCallback()
{
    if (detectState == DetectState::listening)
    {
        for (;;)
        {
            const int n = detectFifo.pull (detectPullBuffer.data(), (int) detectPullBuffer.size());
            if (n <= 0)
                break;
            accumulator.push (detectPullBuffer.data(), n);
        }

        if (accumulator.getActiveSeconds() >= getDetectTargetSeconds())
            finishDetect();
    }
    else if (detectState == DetectState::applying)
    {
        updateGlide();
    }
}

void MagicEQProcessor::finishDetect()
{
    captureForDetect = false;

    GridCurve spectrum {};
    if (! accumulator.getAverageSpectrum (spectrum))
    {
        detectState = DetectState::idle;
        return;
    }

    MatchFitter::Options options;
    options.strength = apvts.getRawParameterValue (params::strength)->load() / 100.0f;
    const int styleIndex = (int) apvts.getRawParameterValue (params::style)->load();
    lastMatch = MatchFitter::fit (spectrum, styleIndex, options);
    lastDynamics = DynamicsFitter::fit (accumulator.getActiveFrameLevels(), styleIndex, options.strength);

    // Turn the result into parameter moves. One undo step covers the whole Detect.
    undoManager.beginNewTransaction ("Detect");
    glide.clear();
    afterGlide.clear();

    auto param = [this] (const juce::String& id) { return apvts.getParameter (id); };
    auto setNow = [] (juce::RangedAudioParameter* p, float value)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    };
    auto addGlide = [this] (juce::RangedAudioParameter* p, float to)
    {
        p->beginChangeGesture();
        glide.push_back ({ p, p->getValue(), p->convertTo0to1 (to) });
    };

    for (int b = 0; b < EqEngine::numBands; ++b)
    {
        const auto current = getBand (b);
        const bool used = b < (int) lastMatch.bands.size();

        if (used)
        {
            const auto& t = lastMatch.bands[(size_t) b];
            if (! current.enabled)
                setNow (param (params::gain (b)), 0.0f); // fade in from flat

            setNow (param (params::type (b)), (float) (int) t.type);
            setNow (param (params::slope (b)), (float) t.slopeIndex);
            setNow (param (params::on (b)), 1.0f);

            addGlide (param (params::freq (b)), t.frequency);
            addGlide (param (params::gain (b)), t.gainDb);
            addGlide (param (params::q (b)), t.q);
        }
        else if (current.enabled)
        {
            // Fade out, then switch off once the glide is done.
            addGlide (param (params::gain (b)), 0.0f);
            afterGlide.push_back ({ param (params::on (b)), 0.0f });
        }
    }

    // Compressor: switch on (or off) at once, and glide the knobs into place.
    if (lastDynamics.profile.valid)
    {
        const auto& c = lastDynamics.comp;
        setNow (param (params::compOn), c.enabled ? 1.0f : 0.0f);
        if (c.enabled)
        {
            addGlide (param (params::compThresh), c.thresholdDb);
            addGlide (param (params::compRatio), c.ratio);
            addGlide (param (params::compAttack), c.attackMs);
            addGlide (param (params::compRelease), c.releaseMs);
            addGlide (param (params::compKnee), c.kneeDb);
            addGlide (param (params::compMakeup), c.makeupDb);
            addGlide (param (params::compMix), c.mix * 100.0f);
        }
    }

    glideStartMs = juce::Time::getMillisecondCounterHiRes();
    detectState = DetectState::applying;
}

void MagicEQProcessor::updateGlide()
{
    const double t = juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - glideStartMs) / glideDurationMs);
    const float eased = (float) (t * t * (3.0 - 2.0 * t)); // smoothstep: ease in and out

    for (auto& g : glide)
        g.param->setValueNotifyingHost (g.from + (g.to - g.from) * eased);

    if (t >= 1.0)
    {
        for (auto& g : glide)
            g.param->endChangeGesture();

        for (auto& [p, value] : afterGlide)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }

        glide.clear();
        afterGlide.clear();
        detectState = DetectState::idle;
    }
}

// ============================================================================
// A/B compare and state
// ============================================================================

void MagicEQProcessor::switchAB()
{
    auto current = apvts.copyState();
    if (! otherSlot.isValid())
        otherSlot = current.createCopy();

    apvts.replaceState (otherSlot);
    otherSlot = current;
    onB = ! onB;
}

void MagicEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MagicEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MagicEQProcessor::createEditor()
{
    return new MagicEQEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MagicEQProcessor();
}
