// Developer tool: renders the MagicEQ window to a PNG after running Detect on a synthetic
// "boxy, dull" vocal. Used to produce docs/screenshot.png. Not part of the plugin.
//   MagicEQSnapshot output.png [styleIndex]

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <random>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "screenshot.png"));

    MagicEQProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    if (argc > 2)
        if (auto* p = proc.apvts.getParameter (magiceq::params::style))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::String (argv[2]).getIntValue()));
    if (auto* p = proc.apvts.getParameter (magiceq::params::detectLen))
        p->setValueNotifyingHost (0.0f); // 10 s

    // Shape white noise into a boxy, dull, voice-ish signal with syllable-like bursts.
    using namespace magiceq;
    std::vector<BandSettings> shape;
    auto add = [&] (BandType t, float f, float g, float q) { BandSettings b; b.enabled = true; b.type = t; b.frequency = f; b.gainDb = g; b.q = q; shape.push_back (b); };
    add (BandType::LowCut, 90, 0, 0.707f);
    add (BandType::Bell, 400, 7, 1.0f);
    add (BandType::HighShelf, 1500, -10, 0.6f);
    add (BandType::HighShelf, 7000, -8, 0.7f);
    add (BandType::Bell, 4000, -5, 1.2f);
    std::vector<BandDesign> designs;
    for (auto& b : shape) designs.push_back (designBand (b, 48000.0));
    std::vector<std::array<BiquadState, maxSectionsPerBand>> states (designs.size());

    std::mt19937 rng (3);
    std::normal_distribution<float> noise (0.0f, 0.15f);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    long sampleIndex = 0;
    float pk[7] {};

    auto renderBlock = [&]
    {
        for (int i = 0; i < 512; ++i, ++sampleIndex)
        {
            // Paul Kellet's pink-noise filter: equal energy per octave, like real sources.
            const float w = noise (rng);
            pk[0] = 0.99886f * pk[0] + w * 0.0555179f; pk[1] = 0.99332f * pk[1] + w * 0.0750759f;
            pk[2] = 0.96900f * pk[2] + w * 0.1538520f; pk[3] = 0.86650f * pk[3] + w * 0.3104856f;
            pk[4] = 0.55000f * pk[4] + w * 0.5329522f; pk[5] = -0.7616f * pk[5] - w * 0.0168980f;
            float x = (pk[0] + pk[1] + pk[2] + pk[3] + pk[4] + pk[5] + pk[6] + w * 0.5362f) * 0.25f;
            pk[6] = w * 0.115926f;
            for (size_t d = 0; d < designs.size(); ++d)
                for (int s = 0; s < designs[d].numSections; ++s)
                    x = states[d][(size_t) s].process (designs[d].sections[(size_t) s], x, 0);
            const double t = (double) sampleIndex / 48000.0;
            const float env = (float) std::max (0.0, std::sin (2.0 * juce::MathConstants<double>::pi * 2.1 * t)) * 0.8f + 0.05f;
            buffer.setSample (0, i, x * env);
            buffer.setSample (1, i, x * env);
        }
        proc.processBlock (buffer, midi);
    };

    proc.startDetect();
    // Feed audio "in real time" so the processor's timers see it arrive.
    for (int step = 0; step < 3000 && proc.getDetectState() == MagicEQProcessor::DetectState::listening; ++step)
    {
        for (int k = 0; k < 10; ++k) renderBlock();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }
    for (int step = 0; step < 100 && proc.getDetectState() != MagicEQProcessor::DetectState::idle; ++step)
    {
        renderBlock();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (1240, 740);
    if (auto* e = dynamic_cast<MagicEQEditor*> (editor.get()))
        e->selectBand (2);

    for (int step = 0; step < 60; ++step)
    {
        for (int k = 0; k < 2; ++k) renderBlock();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (33);
    }

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (image, stream);

    for (const auto& b : proc.getLastMatch().bands)
        std::printf ("band: type %d  %.0f Hz  %+.1f dB  Q %.2f\n", (int) b.type, b.frequency, b.gainDb, b.q);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
