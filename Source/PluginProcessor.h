#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

// Lagostom Glue
// Cadeia de sinal: Drive -> Saturacao (2x oversampling) -> Tone -> Compressor -> Output -> Mix
class LagostomGlueProcessor : public juce::AudioProcessor
{
public:
    LagostomGlueProcessor();
    ~LagostomGlueProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Lagostom Glue"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

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

    // Parametros (ponteiros atomicos, seguros para a thread de audio)
    std::atomic<float>* driveParam     = nullptr;
    std::atomic<float>* warmthParam    = nullptr;
    std::atomic<float>* toneParam      = nullptr;
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* ratioParam     = nullptr;
    std::atomic<float>* attackParam    = nullptr;
    std::atomic<float>* releaseParam   = nullptr;
    std::atomic<float>* outputParam    = nullptr;
    std::atomic<float>* mixParam       = nullptr;

    // Blocos de DSP
    juce::dsp::Gain<float> driveGain;
    juce::dsp::Gain<float> outputGain;
    juce::dsp::Oversampling<float> oversampling { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::dsp::StateVariableTPTFilter<float> toneFilter;   // passa-baixa (Tone)
    juce::dsp::StateVariableTPTFilter<float> dcBlocker;    // passa-alta 20 Hz
    juce::dsp::Compressor<float> compressor;
    juce::dsp::DryWetMixer<float> mixer;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LagostomGlueProcessor)
};
