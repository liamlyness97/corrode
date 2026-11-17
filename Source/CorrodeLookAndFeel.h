#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Neon accent for each mode: 0 Noise, 1 Wide Noise, 2 Sine.
inline juce::Colour accentForMode (int mode)
{
    switch (mode)
    {
        case 1:  return juce::Colour (0xffff4df0);   // Wide Noise: magenta
        case 2:  return juce::Colour (0xff8dff3d);   // Sine: lime
        default: return juce::Colour (0xff2fe6ff);   // Noise: cyan
    }
}

class CorrodeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CorrodeLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffd5dae0));
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,  juce::Colour (0xff2fe6ff).withAlpha (0.35f));
        setColour (juce::Label::textColourId,               juce::Colour (0xff7d848e));
    }

    // Set by the editor whenever the mode changes.
    juce::Colour accent { 0xff2fe6ff };

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (12.0f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
        const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto  centre   = bounds.getCentre();
        const float radius   = diameter * 0.5f;
        const float angle    = startAngle + sliderPos * (endAngle - startAngle);

        const float arcThickness = 3.0f;
        const float arcRadius    = radius - arcThickness * 0.5f;
        const float bodyRadius   = radius - arcThickness - 5.0f;

        // Track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2a2e35));
        g.strokePath (track, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // Value arc with a soft glow underneath
        if (sliderPos > 0.0f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);

            g.setColour (accent.withAlpha (0.22f));
            g.strokePath (value, juce::PathStrokeType (arcThickness + 5.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (value, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        // Knob body
        juce::Path body;
        body.addEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 8, { 0, 3 }).drawForPath (g, body);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff30343c), centre.x, centre.y - bodyRadius,
                                                 juce::Colour (0xff121417), centre.x, centre.y + bodyRadius,
                                                 false));
        g.fillPath (body);

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.strokePath (body, juce::PathStrokeType (1.0f));

        // Pointer
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -bodyRadius + 4.0f, 3.0f, bodyRadius * 0.38f, 1.5f);

        g.setColour (juce::Colour (0xffe6eaf0));
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    // Mode buttons draw as one segmented control. The editor sets each button's
    // "segment" property: 0 first, 1 middle, 2 last.
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool highlighted, bool) override
    {
        auto b = button.getLocalBounds().toFloat().reduced (0.5f);

        const int  segment = (int) button.getProperties().getWithDefault ("segment", 1);
        const bool first   = segment == 0;
        const bool last    = segment == 2;

        juce::Path p;
        p.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(),
                               6.0f, 6.0f, first, last, first, last);

        if (button.getToggleState())
            g.setColour (accent);
        else
            g.setColour (juce::Colour (highlighted ? 0xff22262c : 0xff1a1d22));

        g.fillPath (p);

        g.setColour (juce::Colour (0xff2c3138));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.1f));
        g.setColour (button.getToggleState() ? juce::Colour (0xff0d0f12) : juce::Colour (0xff8a919b));
        g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds(),
                    juce::Justification::centred);
    }
};