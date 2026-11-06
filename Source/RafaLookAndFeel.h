#pragma once

#include <JuceHeader.h>

/** Farby a vzhlad RAFA MUSIC STUDIO (tmava tema). */
namespace RafaColours
{
    const juce::Colour background   { 0xff15171c };
    const juce::Colour panel        { 0xff1f232b };
    const juce::Colour panelLight   { 0xff2a2f3a };
    const juce::Colour text         { 0xffe8eaf0 };
    const juce::Colour textDim      { 0xff9aa1b2 };
    const juce::Colour accent       { 0xfff2a33a };   // teple zlato-oranzova
    const juce::Colour record       { 0xffe5484d };
    const juce::Colour play         { 0xff3fb950 };
    const juce::Colour meterLow     { 0xff3fb950 };
    const juce::Colour meterMid     { 0xfff2c94c };
    const juce::Colour meterHigh    { 0xffe5484d };
    const juce::Colour waveform     { 0xff7aa2f7 };
}

class RafaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RafaLookAndFeel()
    {
        using namespace RafaColours;

        setColourScheme ({ background, panel, panelLight,
                           textDim, text, accent,
                           juce::Colours::black, accent, text });

        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::TextButton::buttonColourId, panelLight);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, text);
        setColour (juce::ComboBox::backgroundColourId, panelLight);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, panelLight.brighter (0.2f));
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.35f));
        setColour (juce::Label::textColourId, text);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::ScrollBar::thumbColourId, panelLight.brighter (0.3f));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::FontOptions (juce::jmin (17.0f, (float) buttonHeight * 0.45f), juce::Font::bold);
    }
};
