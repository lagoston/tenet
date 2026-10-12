#include "PluginProcessor.h"

namespace
{
    using Range = juce::NormalisableRange<float>;
    using ParamID = juce::ParameterID;
}

juce::AudioProcessorValueTreeState::ParameterLayout LagostomGlueProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID { "drive", 1 }, "Drive", Range (0.0f, 24.0f, 0.1f), 6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID { "warmth", 1 }, "Warmth", Range (0.0f, 100.0f, 0.1f), 25.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID { "tone", 1 }, "Tone",
        Range (1000.0f, 20000.0f, 1.0f, 0.3f), 16000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID { "threshold", 1 }, "Threshold", Range (-40.0f, 0.0f, 0.1f), -12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID { "ratio", 1 }, "Ratio", Range (1.0f, 10.0f, 0.1f, 0.5f), 2.0f,
        juce::AudioParameterFloatAttributes().withLabel (":1")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID { "attack", 1 }, "Attack", Range (0.1f, 100.0f, 0.1f, 0.4f), 20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID { "release", 1 }, "Release", Range (20.0f, 1000.0f, 1.0f, 0.4f), 200.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID { "output", 1 }, "Output", Range (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID { "mix", 1 }, "Mix", Range (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    return { params.begin(), params.end() };
}

LagostomGlueProcessor::LagostomGlueProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    driveParam     = apvts.getRawParameterValue ("drive");
    warmthParam    = apvts.getRawParameterValue ("warmth");
    toneParam      = apvts.getRawParameterValue ("tone");
    thresholdParam = apvts.getRawParameterValue ("threshold");
    ratioParam     = apvts.getRawParameterValue ("ratio");
    attackParam    = apvts.getRawParameterValue ("attack");
    releaseParam   = apvts.getRawParameterValue ("release");
    outputParam    = apvts.getRawParameterValue ("output");
    mixParam       = apvts.getRawParameterValue ("mix");
}

bool LagostomGlueProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void LagostomGlueProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    driveGain.prepare (spec);
    driveGain.setRampDurationSeconds (0.02);

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);

    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    dcBlocker.prepare (spec);
    dcBlocker.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    dcBlocker.setCutoffFrequency (20.0f);

    compressor.prepare (spec);

    const auto latency = oversampling.getLatencyInSamples();
    setLatencySamples (juce::roundToInt (latency));

    mixer.prepare (spec);
    mixer.setMixingRule (juce::dsp::DryWetMixingRule::balanced);
    mixer.setWetLatency (latency);   // alinha o sinal "dry" com o "wet"
}

void LagostomGlueProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Le os parametros uma vez por bloco
    const float driveDb   = driveParam->load();
    const float warmth    = warmthParam->load() * 0.01f;
    const float toneHz    = juce::jmin (toneParam->load(), (float) (currentSampleRate * 0.45));
    const float thresh    = thresholdParam->load();
    const float ratio     = ratioParam->load();
    const float attackMs  = attackParam->load();
    const float releaseMs = releaseParam->load();
    const float outDb     = outputParam->load();
    const float mix       = mixParam->load() * 0.01f;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // 1) guarda o sinal original para o Mix
    mixer.setWetMixProportion (mix);
    mixer.pushDrySamples (block);

    // 2) Drive
    driveGain.setGainDecibels (driveDb);
    driveGain.process (context);

    // 3) Saturacao em 2x oversampling (reduz aliasing)
    //    tanh(x + bias) - tanh(bias): o bias cria assimetria = harmonicos pares ("calor")
    const float bias = warmth * 0.4f;
    const float biasOffset = std::tanh (bias);

    auto osBlock = oversampling.processSamplesUp (block);
    for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
    {
        auto* data = osBlock.getChannelPointer (ch);
        for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
            data[i] = std::tanh (data[i] + bias) - biasOffset;
    }
    oversampling.processSamplesDown (block);

    // 4) remove DC gerado pela assimetria, depois filtro Tone
    dcBlocker.process (context);
    toneFilter.setCutoffFrequency (toneHz);
    toneFilter.process (context);

    // 5) Compressor "glue"
    compressor.setThreshold (thresh);
    compressor.setRatio (ratio);
    compressor.setAttack (attackMs);
    compressor.setRelease (releaseMs);
    compressor.process (context);

    // 6) Output e mistura final
    outputGain.setGainDecibels (outDb);
    outputGain.process (context);

    mixer.mixWetSamples (block);
}

juce::AudioProcessorEditor* LagostomGlueProcessor::createEditor()
{
    // Interface generica: um slider por parametro. Depois trocamos por uma UI propria.
    return new juce::GenericAudioProcessorEditor (*this);
}

void LagostomGlueProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LagostomGlueProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LagostomGlueProcessor();
}
