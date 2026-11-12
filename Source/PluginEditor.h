#pragma once
#include "PluginProcessor.h"

class RiseEditor : public juce::AudioProcessorEditor
{
public:
    explicit RiseEditor (RiseProcessor&);
    ~RiseEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, juce::Colour colour);
    static void layoutKnob (Knob&, juce::Rectangle<int> area);

    RiseProcessor& proc;
    Knob build, filter, reso, delay, reverb, output;
    juce::Label divisionLabel;
    juce::ComboBox divisionBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiseEditor)
};
