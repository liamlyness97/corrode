#pragma once

#include "PluginProcessor.h"
#include <array>
#include <memory>

// Draggable X/Y control: X is Frequency, Y is Amount. In the noise modes it also
// sketches the noise band as a bell curve (a first guess at Erosion's display).
class XYPad : public juce::Component
{
public:
    XYPad (juce::RangedAudioParameter& freq,
           juce::RangedAudioParameter& amount,
           juce::RangedAudioParameter& width,
           juce::RangedAudioParameter& mode);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void setFromMouse (const juce::MouseEvent&);

    juce::RangedAudioParameter& freqParam;
    juce::RangedAudioParameter& amountParam;

    float freqNorm     = 0.0f;
    float amountNorm   = 0.0f;
    float widthOctaves = 1.0f;
    int   modeIndex    = 0;
    bool  dragging     = false;

    juce::ParameterAttachment freqAttach, amountAttach, widthAttach, modeAttach;
};

class CorrodeEditor : public juce::AudioProcessorEditor
{
public:
    explicit CorrodeEditor (CorrodeProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    XYPad pad;

    std::array<juce::TextButton, 3> modeButtons;
    juce::ParameterAttachment modeAttach;

    juce::Slider freqSlider, widthSlider, amountSlider;
    juce::Label  freqLabel,  widthLabel,  amountLabel;

    std::unique_ptr<juce::SliderParameterAttachment> freqSliderAttach,
                                                     widthSliderAttach,
                                                     amountSliderAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorrodeEditor)
};