#include "PluginProcessor.h"
#include "PluginEditor.h"

RiseProcessor::RiseProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pBuild    = apvts.getRawParameterValue ("build");
    pFilter   = apvts.getRawParameterValue ("filter");
    pReso     = apvts.getRawParameterValue ("reso");
    pDelay    = apvts.getRawParameterValue ("delay");
    pReverb   = apvts.getRawParameterValue ("reverb");
    pDivision = apvts.getRawParameterValue ("division");
    pOutput   = apvts.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout RiseProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "build", 1 }, "Build",
                 NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "filter", 1 }, "Filter",
                 NormalisableRange<float> (0.0f, 1.0f), 0.8f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reso", 1 }, "Resonance",
                 NormalisableRange<float> (0.0f, 1.0f), 0.2f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "delay", 1 }, "Delay",
                 NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reverb", 1 }, "Reverb",
                 NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Delay Time",
                 StringArray { "1/4", "Dotted 1/8", "1/8", "1/16" }, 1));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                 NormalisableRange<float> (-12.0f, 12.0f), 0.0f));

    return { p.begin(), p.end() };
}

double RiseProcessor::getHostBpm()
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0)
                    return *bpm;
    return 120.0;
}

float RiseProcessor::computeDelaySamples()
{
    static const float beats[] = { 1.0f, 0.75f, 0.5f, 0.25f };
    const int idx = juce::jlimit (0, 3, (int) pDivision->load());
    const double seconds = beats[idx] * 60.0 / getHostBpm();
    return juce::jlimit (1.0f, (float) (delaySize - 4), (float) (seconds * sr));
}

float RiseProcessor::readDelay (int ch, float delaySamples) const
{
    float rp = (float) writePos - delaySamples;
    while (rp < 0.0f) rp += (float) delaySize;
    const int i0 = (int) rp;
    const int i1 = (i0 + 1) % delaySize;
    const float frac = rp - (float) i0;
    const auto& buf = delayBuf[(size_t) ch];
    return buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;
}

void RiseProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    hpf.prepare (spec);
    hpf.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpf.reset();

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    delaySize = (int) (sampleRate * 4.0) + 4;
    for (auto& d : delayBuf)
        d.assign ((size_t) delaySize, 0.0f);
    writePos = 0;
    fbState = { 0.0f, 0.0f };
    lpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 4000.0f / (float) sampleRate);

    buildSm.reset (sampleRate, 0.03);
    buildSm.setCurrentAndTargetValue (pBuild->load());
    outGain.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    delaySm.reset (sampleRate, 0.08);
    delaySm.setCurrentAndTargetValue (computeDelaySamples());
}

bool RiseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void RiseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const float fDepth = pFilter->load();
    const float dDepth = pDelay->load();
    const float rDepth = pReverb->load();
    const float reso   = pReso->load();

    buildSm.setTargetValue (pBuild->load());
    outGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    delaySm.setTargetValue (computeDelaySamples());
    hpf.setResonance (0.707f + reso * 5.0f);

    float feedback = 0.0f, delayWet = 0.0f;
    int counter = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float b = buildSm.getNextValue();

        if (counter == 0)
        {
            // High-pass sweeps up exponentially: 20 Hz -> 6 kHz at full depth
            hpf.setCutoffFrequency (20.0f * std::pow (300.0f, b * fDepth));
            feedback = 0.25f + b * dDepth * 0.6f;
            delayWet = b * dDepth * 0.6f;
        }
        counter = (counter + 1) & 31;

        const float dTime = delaySm.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const float hp = hpf.processSample (ch, data[i]);
            const float echo = readDelay (ch, dTime);

            fbState[(size_t) ch] += lpCoef * (echo - fbState[(size_t) ch]);
            delayBuf[(size_t) ch][(size_t) writePos] = hp + fbState[(size_t) ch] * feedback;

            data[i] = hp + echo * delayWet;
        }

        if (++writePos >= delaySize)
            writePos = 0;
    }

    // Reverb opens up with Build
    const float bEnd = buildSm.getCurrentValue();
    juce::Reverb::Parameters rp;
    rp.roomSize   = 0.4f + bEnd * rDepth * 0.55f;
    rp.damping    = 0.5f;
    rp.wetLevel   = bEnd * rDepth * 0.25f;
    rp.dryLevel   = 0.5f;   // JUCE reverb doubles this, so 0.5 = unity dry
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    if (numCh == 2)
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    else if (numCh == 1)
        reverb.processMono (buffer.getWritePointer (0), numSamples);

    // Output trim
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

juce::AudioProcessorEditor* RiseProcessor::createEditor()
{
    return new RiseEditor (*this);
}

void RiseProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RiseProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RiseProcessor();
}
