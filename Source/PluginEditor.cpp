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

juce::Rectangle<float> XYPad::plotArea() const
{
    // Inset so the handle stays fully visible at the edges.
    return getLocalBounds().toFloat().reduced (10.0f);
}

void XYPad::paint (juce::Graphics& g)
{
    auto b    = getLocalBounds().toFloat();
    auto plot = plotArea();
    const auto accent = accentForMode (modeIndex);

    // Background
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff14171b), 0.0f, b.getY(),
                                             juce::Colour (0xff0c0d10), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 6.0f);

    juce::Path clip;
    clip.addRoundedRectangle (b, 6.0f);
    g.reduceClipRegion (clip);

    // Frequency grid: log spaced like an EQ display, brighter at 1 kHz and 10 kHz.
    for (float decade : { 100.0f, 1000.0f, 10000.0f })
    {
        for (int m = 1; m < 10; ++m)
        {
            const float hz = decade * (float) m;
            if (hz <= 300.0f || hz >= 18000.0f)
                continue;

            const float x = plot.getX() + plot.getWidth() * freqParam.convertTo0to1 (hz);
            g.setColour (juce::Colours::white.withAlpha (m == 1 ? 0.10f : 0.04f));
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
        }
    }

    // Amount grid
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 1; i < 4; ++i)
    {
        const float y = plot.getY() + plot.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
    }

    // Frequency labels
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    for (float hz : { 1000.0f, 10000.0f })
    {
        const float x = plot.getX() + plot.getWidth() * freqParam.convertTo0to1 (hz);
        g.drawText (hz < 5000.0f ? "1k" : "10k",
                    juce::Rectangle<float> (x - 14.0f, b.getBottom() - 14.0f, 28.0f, 12.0f),
                    juce::Justification::centred);
    }

    const float dotX = plot.getX() + plot.getWidth()  * freqNorm;
    const float dotY = plot.getBottom() - plot.getHeight() * amountNorm;

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

            const float x = plot.getX() + plot.getWidth() * nx;
            const float y = plot.getBottom() - plot.getHeight() * 0.7f * fade * mag;

            if (i == 0)
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }

        // Soft fill under the curve
        juce::Path fill (curve);
        fill.lineTo (plot.getRight(), b.getBottom());
        fill.lineTo (plot.getX(), b.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.20f * fade), 0.0f, plot.getY() + plot.getHeight() * 0.3f,
                                                 accent.withAlpha (0.0f),         0.0f, b.getBottom(), false));
        g.fillPath (fill);

        // Glow, then the line itself
        g.setColour (accent.withAlpha (0.22f * fade));
        g.strokePath (curve, juce::PathStrokeType (5.0f));
        g.setColour (accent.withAlpha (fade));
        g.strokePath (curve, juce::PathStrokeType (1.6f));
    }

    // Handle: halo, body, bright centre
    juce::ColourGradient halo (accent.withAlpha (0.45f), dotX, dotY,
                               accent.withAlpha (0.0f),  dotX + 18.0f, dotY, true);
    g.setGradientFill (halo);
    g.fillEllipse (dotX - 18.0f, dotY - 18.0f, 36.0f, 36.0f);

    g.setColour (accent);
    g.fillEllipse (dotX - 6.0f, dotY - 6.0f, 12.0f, 12.0f);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillEllipse (dotX - 2.0f, dotY - 2.0f, 4.0f, 4.0f);

    if (dragging)
    {
        g.setColour (accent.withAlpha (0.6f));
        g.drawEllipse (dotX - 10.0f, dotY - 10.0f, 20.0f, 20.0f, 1.5f);

        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (freqParam.getCurrentValueAsText() + "   " + amountParam.getCurrentValueAsText(),
                    b.reduced (8.0f, 6.0f).removeFromTop (14.0f),
                    juce::Justification::topLeft);
    }

    // Border
    g.setColour (juce::Colour (0xff23272e));
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
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
    auto plot = plotArea();

    const float nx = juce::jlimit (0.0f, 1.0f, (e.position.x - plot.getX()) / plot.getWidth());
    const float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - plot.getY()) / plot.getHeight());

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

          laf.accent = accentForMode (index);
          repaint();
      })
{
    setLookAndFeel (&laf);

    addAndMakeVisible (pad);

    const char* modeNames[] = { "Noise", "Wide Noise", "Sine" };
    for (size_t i = 0; i < modeButtons.size(); ++i)
    {
        auto& button = modeButtons[i];
        button.setButtonText (modeNames[i]);
        button.getProperties().set ("segment", (int) i);
        button.onClick = [this, i] { modeAttach.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (button);
    }
    modeAttach.sendInitialUpdate();

    auto setupKnob = [this] (juce::Slider& slider, juce::Label& label, const juce::String& text)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        addAndMakeVisible (slider);

        label.setText (text.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.12f));
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

    setSize (360, 410);
}

void CorrodeEditor::paint (juce::Graphics& g)
{
    // Edge to edge: the host's own window frame is the border.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1e24), 0.0f, 0.0f,
                                             juce::Colour (0xff121418), 0.0f, (float) getHeight(), false));
    g.fillAll();

    auto header = getLocalBounds().reduced (12).removeFromTop (26);

    g.setColour (juce::Colour (0xff8a919b));
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)).withExtraKerningFactor (0.25f));
    g.drawText ("CORRODE", header, juce::Justification::centredLeft);

    // Small status dot in the current mode's colour
    g.setColour (laf.accent);
    g.fillEllipse ((float) header.getRight() - 8.0f, (float) header.getCentreY() - 3.0f, 6.0f, 6.0f);
}

void CorrodeEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    area.removeFromTop (26);   // header, drawn in paint()

    pad.setBounds (area.removeFromTop (180));
    area.removeFromTop (10);

    // Mode buttons join up into one segmented control.
    auto buttonRow = area.removeFromTop (30);
    const int buttonWidth = buttonRow.getWidth() / 3;
    modeButtons[0].setBounds (buttonRow.removeFromLeft (buttonWidth));
    modeButtons[1].setBounds (buttonRow.removeFromLeft (buttonWidth));
    modeButtons[2].setBounds (buttonRow);

    area.removeFromTop (10);

    const int columnWidth = area.getWidth() / 3;

    auto layoutColumn = [&] (juce::Label& label, juce::Slider& slider)
    {
        auto column = area.removeFromLeft (columnWidth);
        label.setBounds (column.removeFromBottom (18));
        slider.setBounds (column.reduced (6, 0));
    };

    layoutColumn (freqLabel,   freqSlider);
    layoutColumn (widthLabel,  widthSlider);
    layoutColumn (amountLabel, amountSlider);
}