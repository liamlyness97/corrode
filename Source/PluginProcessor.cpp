#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    // Placeholder defaults: replace with Ableton's once known.
    constexpr int   defaultMode   = 0;        // 0 Noise, 1 Wide Noise, 2 Sine
    constexpr float defaultFreq   = 4000.0f;
    constexpr float defaultWidth  = 1.0f;
    constexpr float defaultAmount = 0.0f;

    // Three significant figures, as Ableton shows them (4.74, 18.0, 2.50, 13.1, 0.00).
    juce::String sig3 (float v)
    {
        return juce::String::toDecimalStringWithSignificantFigures ((double) v, 3);
    }
}

CorrodeProcessor::CorrodeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    modeParam   = apvts.getRawParameterValue ("mode");
    freqParam   = apvts.getRawParameterValue ("freq");
    widthParam  = apvts.getRawParameterValue ("width");
    amountParam = apvts.getRawParameterValue ("amount");
}

juce::AudioProcessorValueTreeState::ParameterLayout CorrodeProcessor::createLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode",
        StringArray { "Noise", "Wide Noise", "Sine" }, defaultMode));

    // Exact log scale between 300 Hz and 18 kHz.
    NormalisableRange<float> freqRange (
        300.0f, 18000.0f,
        [] (float start, float end, float v) { return start * std::pow (end / start, v); },
        [] (float start, float end, float v) { return std::log (jmax (v, start) / start) / std::log (end / start); });

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "freq", 1 }, "Frequency",
        freqRange, defaultFreq,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
            {
                return v >= 1000.0f ? sig3 (v / 1000.0f) + " kHz"
                                    : sig3 (v) + " Hz";
            })
            .withValueFromStringFunction ([] (const String& t)
            {
                const float v = t.getFloatValue();
                return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
            })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "width", 1 }, "Width",
        NormalisableRange<float> (0.1f, 2.5f, 0.01f), defaultWidth,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return sig3 (v); })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "amount", 1 }, "Amount",
        NormalisableRange<float> (0.0f, 200.0f, 0.01f), defaultAmount,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return sig3 (v); })));

    return layout;
}

void CorrodeProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    const int size = (int) std::ceil (0.010 * sampleRate) + 8;
    for (auto& line : delayLine)
        line.assign ((size_t) size, 0.0f);

    writePos = 0;
    sinePhase = 0.0;

    for (auto& s : svf)
        s = Svf {};

    noise[0].setSeed (0x1234);
    noise[1].setSeed (0x5678);

    amountSmoothed.reset (sampleRate, 0.02);
    amountSmoothed.setCurrentAndTargetValue (amountParam->load() / 200.0f);
}

bool CorrodeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CorrodeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples  = buffer.getNumSamples();

    if (numChannels == 0 || delayLine[0].empty())
        return;

    const float fs = (float) sampleRate;
    const float pi = juce::MathConstants<float>::pi;

    const int   mode = (int) modeParam->load();            // 0 Noise, 1 Wide Noise, 2 Sine
    const float freq = juce::jlimit (20.0f, 0.45f * fs, freqParam->load());

    // Provisional: Width is read as bandwidth in octaves (0.1 to 2.5).
    const float widthOctaves = juce::jmax (widthParam->load(), 0.01f);

    // Amount 0..200 maps onto 0..1 depth. Provisional until measured against Erosion.
    amountSmoothed.setTargetValue (amountParam->load() / 200.0f);

    // Band-pass filter (TPT state variable). Bandwidth in octaves to Q.
    const float bw = std::pow (2.0f, widthOctaves);
    const float q  = std::sqrt (bw) / (bw - 1.0f);
    const float g  = std::tan (pi * freq / fs);
    k  = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;

    // Rough estimate of the filtered noise level, used to keep the control signal
    // at a similar size whatever the frequency and width. Tune by ear.
    const float estStd    = std::sqrt (1.0f / 3.0f) * std::sqrt (pi * freq / (q * fs));
    const float noiseGain = 0.5f / juce::jmax (estStd, 0.01f);

    const int    size            = (int) delayLine[0].size();
    const float  maxDelaySamples = maxModulationMs * 0.001f * fs;
    const double sinePhaseInc    = (double) freq / sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const float amount = amountSmoothed.getNextValue();

        float mod[2] = { 0.0f, 0.0f };

        if (mode == 2)
        {
            mod[0] = mod[1] = (float) std::sin (juce::MathConstants<double>::twoPi * sinePhase);
            sinePhase += sinePhaseInc;
            if (sinePhase >= 1.0)
                sinePhase -= 1.0;
        }
        else
        {
            const int noiseChannels = (mode == 1) ? 2 : 1;

            for (int ch = 0; ch < noiseChannels; ++ch)
            {
                const float white = noise[(size_t) ch].nextFloat() * 2.0f - 1.0f;

                auto& s = svf[(size_t) ch];
                const float v3 = white - s.ic2;
                const float v1 = a1 * s.ic1 + a2 * v3;
                const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
                s.ic1 = 2.0f * v1 - s.ic1;
                s.ic2 = 2.0f * v2 - s.ic2;

                mod[ch] = std::tanh (k * v1 * noiseGain);
            }

            if (noiseChannels == 1)
                mod[1] = mod[0];
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto& line = delayLine[(size_t) ch];

            line[(size_t) writePos] = data[i];

            const float delay = amount * maxDelaySamples * (0.5f + 0.5f * mod[ch]);
            const int   d     = (int) std::lround (delay);   // no interpolation, on purpose

            int readPos = writePos - d;
            if (readPos < 0)
                readPos += size;

            data[i] = line[(size_t) readPos];
        }

        if (++writePos >= size)
            writePos = 0;
    }
}

juce::AudioProcessorEditor* CorrodeProcessor::createEditor()
{
    return new CorrodeEditor (*this);
}

void CorrodeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CorrodeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// JUCE's plugin wrappers call this to create the plugin. It must be at global scope.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CorrodeProcessor();
}