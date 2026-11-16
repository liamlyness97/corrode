#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

class CorrodeProcessor : public juce::AudioProcessor
{
public:
    CorrodeProcessor();
    ~CorrodeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Corrode"; }
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

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Tuning: modulation depth at Amount = 200. Delay buffer is sized for 10 ms.
    static constexpr float maxModulationMs = 0.5f;

    struct Svf
    {
        float ic1 = 0.0f;
        float ic2 = 0.0f;
    };

    std::atomic<float>* modeParam   = nullptr;
    std::atomic<float>* freqParam   = nullptr;
    std::atomic<float>* widthParam  = nullptr;
    std::atomic<float>* amountParam = nullptr;

    double sampleRate = 44100.0;

    std::array<std::vector<float>, 2> delayLine;
    int writePos = 0;

    std::array<Svf, 2> svf;
    std::array<juce::Random, 2> noise;
    double sinePhase = 0.0;

    // Band-pass filter coefficients, updated once per block.
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;

    juce::SmoothedValue<float> amountSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorrodeProcessor)
};