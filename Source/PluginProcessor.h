#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

class RiseProcessor : public juce::AudioProcessor
{
public:
    RiseProcessor();
    ~RiseProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Rise Machine"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

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
    double getHostBpm();
    float computeDelaySamples();
    float readDelay (int ch, float delaySamples) const;

    std::atomic<float>* pBuild = nullptr;
    std::atomic<float>* pFilter = nullptr;
    std::atomic<float>* pReso = nullptr;
    std::atomic<float>* pDelay = nullptr;
    std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pDivision = nullptr;
    std::atomic<float>* pOutput = nullptr;

    double sr = 44100.0;

    juce::dsp::StateVariableTPTFilter<float> hpf;
    juce::Reverb reverb;

    std::array<std::vector<float>, 2> delayBuf;
    std::array<float, 2> fbState { 0.0f, 0.0f };
    int delaySize = 1;
    int writePos = 0;
    float lpCoef = 0.3f;

    juce::SmoothedValue<float> buildSm, outGain, delaySm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiseProcessor)
};
