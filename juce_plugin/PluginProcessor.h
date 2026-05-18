#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <deque>
#include <vector>

#include "artifact_remover/remover.h"
#include "DebugLogger.h"

using artifact_remover::Remover;
using artifact_remover::Vector;
using artifact_remover::Matrix;
using artifact_remover::RemovalResult;

class ArtifactRemoverAudioProcessor : public juce::AudioProcessor
{
public:
    ArtifactRemoverAudioProcessor();
    ~ArtifactRemoverAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter management
    juce::AudioProcessorValueTreeState apvts;

private:
    Remover remover;
    double sampleRate = 48000.0;
    int latencySamples = 0;
    
    // Parameters (declare constants first)
    static constexpr int TIME_DELAY = 1;
    static constexpr int MIN_WINDOW_SIZE = 128;
    static constexpr int MAX_WINDOW_SIZE = 2048;
    
    // Resampling constants
    static constexpr double PROCESSING_SAMPLE_RATE = 6000.0;  // Downsampled rate
    static constexpr int DOWNSAMPLE_FACTOR = 8;  // 48000 / 6000 = 8
    static constexpr int PROCESSING_WINDOW_SIZE = 512;  // @ 6kHz - LEGACY, use max window size now
    
    // Single buffer for downsampled samples - fixed at MAX window size to avoid reshaping crashes
    std::deque<double> downsampledBuffer;
    static constexpr int MAX_DOWNSAMPLED_BUFFER = MAX_WINDOW_SIZE;  // Must hold max window size
    
    // Output queue for samples waiting to be output
    std::deque<float> outputQueue;
    
    // Flag to track if we have enough data for first processing
    bool bufferInitialized = false;
    
    // Moving window framework
    int currentWindowSize = 512;
    
    // Safe parameter access with defaults
    float getParameterSafe(const char* name, float defaultValue);
    int getParameterIntSafe(const char* name, int defaultValue);
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Resampling pipeline methods
    std::vector<double> linearUpsample(const std::vector<double>& input, int factor);
    std::vector<double> zeroPadUpsample(const std::vector<double>& input, int factor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArtifactRemoverAudioProcessor)
};
