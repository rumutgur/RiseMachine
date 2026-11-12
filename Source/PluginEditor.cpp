#include "PluginEditor.h"

RiseEditor::RiseEditor (RiseProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    const auto accent = juce::Colour (0xffff5a36);
    const auto soft   = juce::Colour (0xff6ec6ff);

    setupKnob (build,  "build",  "BUILD",     accent);
    setupKnob (filter, "filter", "Filter",    soft);
    setupKnob (reso,   "reso",   "Resonance", soft);
    setupKnob (delay,  "delay",  "Delay",     soft);
    setupKnob (reverb, "reverb", "Reverb",    soft);
    setupKnob (output, "output", "Output",    juce::Colours::lightgrey);

    divisionLabel.setText ("Delay Time", juce::dontSendNotification);
    divisionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (divisionLabel);

    divisionBox.addItem ("1/4", 1);
    divisionBox.addItem ("Dotted 1/8", 2);
    divisionBox.addItem ("1/8", 3);
    divisionBox.addItem ("1/16", 4);
    addAndMakeVisible (divisionBox);
    divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "division", divisionBox);

    setSize (600, 420);
}

void RiseEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name, juce::Colour colour)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    k.slider.setNumDecimalPlacesToDisplay (2);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramId, k.slider);
}

void RiseEditor::layoutKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (18));
    k.slider.setBounds (area);
}

void RiseEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15161a));
    g.setColour (juce::Colours::white);
    g.setFont (22.0f);
    g.drawText ("RISE MACHINE", getLocalBounds().removeFromTop (50), juce::Justification::centred);
}

void RiseEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (36);

    auto top = r.removeFromTop (190);
    layoutKnob (build, top.withSizeKeepingCentre (200, 190));

    auto row = r.removeFromTop (120);
    const int w = row.getWidth() / 5;
    layoutKnob (filter, row.removeFromLeft (w));
    layoutKnob (reso,   row.removeFromLeft (w));
    layoutKnob (delay,  row.removeFromLeft (w));
    layoutKnob (reverb, row.removeFromLeft (w));
    layoutKnob (output, row);

    r.removeFromTop (8);
    auto combo = r.removeFromTop (26).withSizeKeepingCentre (260, 26);
    divisionLabel.setBounds (combo.removeFromLeft (95));
    divisionBox.setBounds (combo);
}
