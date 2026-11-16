#include "PluginEditor.h"
#include <cmath>

//==============================================================================
XYPad::XYPad (juce::RangedAudioParameter& freq,
              juce::RangedAudioParameter& amount,
              juce::RangedAudioParameter& width,
              juce::RangedAudioParameter& mode)
    : freqParam (freq),
      amountParam (amount),
      freqAttach   (freq,   [this] (float v) { freqNorm     = freqParam.convertTo0to1 (v);   repaint(); }),
      amountAttach (amount, [this] (float v) { amountNorm   = amountParam.convertTo0to1 (v); repaint(); }),
      widthAttach  (width,  [this] (float v) { widthOctaves = v;                             repaint(); }),
      modeAttach   (mode,   [this] (float v) { modeIndex    = (int) std::round (v);          repaint(); })
{
    freqAttach.sendInitialUpdate();
    amountAttach.sendInitialUpdate();
    widthAttach.sendInitialUpdate();
    modeAttach.sendInitialUpdate();
}

void XYPad::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff17191c));
    g.fillRoundedRectangle (b, 4.0f);

    // Frequency grid: log spaced like an EQ display, brighter at 1 kHz and 10 kHz.
    for (float decade : { 100.0f, 1000.0f, 10000.0f })
    {
        for (int m = 1; m < 10; ++m)
        {
            const float hz = decade * (float) m;
            if (hz <= 300.0f || hz >= 18000.0f)
                continue;

            const float x = b.getX() + b.getWidth() * freqParam.convertTo0to1 (hz);
            g.setColour (juce::Colour (m == 1 ? 0xff3a3f46 : 0xff262a2f));
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
        }
    }

    // Amount grid
    g.setColour (juce::Colour (0xff2a2e33));
    for (int i = 1; i < 4; ++i)
    {
        const float y = b.getY() + b.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
    }

    // Noise band, drawn with the same band-pass shape the DSP uses.
    if (modeIndex != 2 && amountNorm > 0.0f)
    {
        // Grow the curve in over the first 5% of the pad's height so it doesn't pop in.
        const float fade = juce::jmin (1.0f, amountNorm / 0.05f);

        const float f0 = freqParam.convertFrom0to1 (freqNorm);
        const float bw = std::pow (2.0f, juce::jmax (widthOctaves, 0.01f));
        const float q  = std::sqrt (bw) / (bw - 1.0f);

        juce::Path curve;
        const int steps = 120;

        for (int i = 0; i <= steps; ++i)
        {
            const float nx  = (float) i / (float) steps;
            const float f   = freqParam.convertFrom0to1 (nx);
            const float r   = f / f0;
            const float d   = r - 1.0f / r;
            const float mag = 1.0f / std::sqrt (1.0f + q * q * d * d);

            const float x = b.getX() + b.getWidth() * nx;
            const float y = b.getBottom() - b.getHeight() * 0.7f * fade * mag;

            if (i == 0)
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }

        g.setColour (juce::Colour (0xff3d8fe0).withAlpha (fade));
        g.strokePath (curve, juce::PathStrokeType (1.5f));
    }

    // Handle
    const float dotX = b.getX() + b.getWidth()  * freqNorm;
    const float dotY = b.getBottom() - b.getHeight() * amountNorm;

    g.setColour (juce::Colour (0xff6fd14a));
    if (dragging)
        g.drawEllipse (dotX - 7.0f, dotY - 7.0f, 14.0f, 14.0f, 2.0f);
    else
        g.fillEllipse (dotX - 7.0f, dotY - 7.0f, 14.0f, 14.0f);
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    freqAttach.beginGesture();
    amountAttach.beginGesture();
    setFromMouse (e);
}

void XYPad::mouseDrag (const juce::MouseEvent& e)
{
    setFromMouse (e);
}

void XYPad::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    freqAttach.endGesture();
    amountAttach.endGesture();
    repaint();
}

void XYPad::setFromMouse (const juce::MouseEvent& e)
{
    auto b = getLocalBounds().toFloat();

    const float nx = juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
    const float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - b.getY()) / b.getHeight());

    freqAttach.setValueAsPartOfGesture   (freqParam.convertFrom0to1 (nx));
    amountAttach.setValueAsPartOfGesture (amountParam.convertFrom0to1 (ny));
}

//==============================================================================
CorrodeEditor::CorrodeEditor (CorrodeProcessor& p)
    : AudioProcessorEditor (&p),
      pad (*p.apvts.getParameter ("freq"),
           *p.apvts.getParameter ("amount"),
           *p.apvts.getParameter ("width"),
           *p.apvts.getParameter ("mode")),
      modeAttach (*p.apvts.getParameter ("mode"), [this] (float v)
      {
          const int index = (int) std::round (v);
          for (size_t i = 0; i < modeButtons.size(); ++i)
              modeButtons[i].setToggleState ((int) i == index, juce::dontSendNotification);
      })
{
    addAndMakeVisible (pad);

    const char* modeNames[] = { "Noise", "Wide Noise", "Sine" };
    for (size_t i = 0; i < modeButtons.size(); ++i)
    {
        auto& button = modeButtons[i];
        button.setButtonText (modeNames[i]);
        button.onClick = [this, i] { modeAttach.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (button);
    }
    modeAttach.sendInitialUpdate();

    auto setupKnob = [this] (juce::Slider& slider, juce::Label& label, const juce::String& text)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
        addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    };

    setupKnob (freqSlider,   freqLabel,   "Freq");
    setupKnob (widthSlider,  widthLabel,  "Width");
    setupKnob (amountSlider, amountLabel, "Amount");

    auto attach = [&p] (const char* id, juce::Slider& slider)
    {
        auto* param = p.apvts.getParameter (id);
        auto attachment = std::make_unique<juce::SliderParameterAttachment> (*param, slider);
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        return attachment;
    };

    freqSliderAttach   = attach ("freq",   freqSlider);
    widthSliderAttach  = attach ("width",  widthSlider);
    amountSliderAttach = attach ("amount", amountSlider);

    setSize (340, 364);
}

void CorrodeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff202225));
}

void CorrodeEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    pad.setBounds (area.removeFromTop (180));
    area.removeFromTop (8);

    auto buttonRow = area.removeFromTop (28);
    const int buttonWidth = buttonRow.getWidth() / 3;
    for (auto& button : modeButtons)
        button.setBounds (buttonRow.removeFromLeft (buttonWidth).reduced (2, 0));

    area.removeFromTop (8);

    const int columnWidth = area.getWidth() / 3;

    auto layoutColumn = [&] (juce::Label& label, juce::Slider& slider)
    {
        auto column = area.removeFromLeft (columnWidth);
        label.setBounds (column.removeFromTop (18));
        slider.setBounds (column);
    };

    layoutColumn (freqLabel,   freqSlider);
    layoutColumn (widthLabel,  widthSlider);
    layoutColumn (amountLabel, amountSlider);
}