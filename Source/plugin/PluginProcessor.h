#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include <ethernauta/DroneVoice.h>

#include <array>
#include <atomic>

#include "ParameterLayout.h"

class EthernautaAudioProcessor : public juce::AudioProcessor {
public:
    EthernautaAudioProcessor();

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.3; } // gate release time

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // The single parameter tree. The editor attaches to it; it never calls processor methods.
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts_; }

private:
    static constexpr int kNumVoices = ethernauta::plugin::kNumVoices;

    struct VoiceParamPointers {
        std::atomic<float>* wave = nullptr;
        std::atomic<float>* latch = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* pitch = nullptr;
    };

    juce::AudioProcessorValueTreeState apvts_;
    std::array<VoiceParamPointers, kNumVoices> params_{};
    std::array<ethernauta::DroneVoice, kNumVoices> voices_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EthernautaAudioProcessor)
};
