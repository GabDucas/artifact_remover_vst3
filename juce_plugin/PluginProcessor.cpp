#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <sstream>

float ArtifactRemoverAudioProcessor::getParameterSafe(const char* name, float defaultValue)
{
    try {
        auto* param = apvts.getRawParameterValue(name);
        if (param == nullptr) {
            DebugLogger::getInstance().log(std::string("Parameter null: ") + name);
            return defaultValue;
        }
        return *param;
    }
    catch (const std::exception& e) {
        DebugLogger::getInstance().log(std::string("Exception getting param ") + name + ": " + e.what());
        return defaultValue;
    }
}

int ArtifactRemoverAudioProcessor::getParameterIntSafe(const char* name, int defaultValue)
{
    return (int)getParameterSafe(name, (float)defaultValue);
}

juce::AudioProcessorValueTreeState::ParameterLayout 
ArtifactRemoverAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterInt>(
        "window_size", "Window Size", MIN_WINDOW_SIZE, MAX_WINDOW_SIZE, 500));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "lower_freq", "Lower Frequency", 0.0f, 100.0f, 10.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "upper_freq", "Upper Frequency", 100.0f, 600.0f, 300.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "factor", "Factor", 0.0f, 1.0f, 0.4f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "hankel_size", "Hankel Size", 50.0f, 300.0f, 100.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "svd_threshold", "SVD Threshold", 0.0f, 1.0f, 0.0f));

    return layout;
}

ArtifactRemoverAudioProcessor::ArtifactRemoverAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::mono(), true)
        .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    latencySamples = 0;
    currentWindowSize = 500;
    
    DebugLogger::getInstance().initialize();
    DebugLogger::getInstance().log("ArtifactRemoverAudioProcessor initialized");
}

ArtifactRemoverAudioProcessor::~ArtifactRemoverAudioProcessor()
{
    DebugLogger::getInstance().log("ArtifactRemoverAudioProcessor destructor - cleaning up");
}

void ArtifactRemoverAudioProcessor::prepareToPlay(double inSampleRate, int samplesPerBlock)
{
    try {
        DebugLogger::getInstance().log("prepareToPlay starting...");
        
        sampleRate = inSampleRate;
        currentWindowSize = getParameterIntSafe("window_size", 500);
        
        downsampledBuffer.clear();
        bufferInitialized = false;
        
        std::ostringstream oss;
        oss << "prepareToPlay - SR: " << inSampleRate << " Hz, Samples per block: " << samplesPerBlock 
            << ", Window: " << currentWindowSize;
        DebugLogger::getInstance().log(oss.str());
        
        DebugLogger::getInstance().log("prepareToPlay complete");
    }
    catch (const std::exception& e) {
        DebugLogger::getInstance().log(std::string("prepareToPlay exception: ") + e.what());
    }
}

void ArtifactRemoverAudioProcessor::releaseResources()
{
    DebugLogger::getInstance().log("releaseResources() called - clearing buffers");
    downsampledBuffer.clear();
    DebugLogger::getInstance().log("releaseResources() complete");
}

std::vector<double> ArtifactRemoverAudioProcessor::linearUpsample(const std::vector<double>& input, int factor) {
    std::vector<double> output;
    
    if (input.empty())
        return output;
    
    if (input.size() == 1)
    {
        output.push_back(input[0]);
        return output;
    }
    
    for (size_t i = 0; i < input.size() - 1; ++i) {
        for (int j = 0; j < factor; ++j) {
            double alpha = (double)j / factor;
            output.push_back(input[i] * (1.0 - alpha) + input[i+1] * alpha);
        }
    }
    output.push_back(input.back());
    return output;
}

// upsample by adding zerros between samples - this is a simpler method but can introduce more artifacts compared to linear interpolation
std::vector<double> ArtifactRemoverAudioProcessor::zeroPadUpsample(const std::vector<double>& input, int factor) {
    std::vector<double> output(input.size() * factor, 0.0);
    for (size_t i = 0; i < input.size(); ++i) {
        output[i * factor] = input[i];
    }
    return output;
}

void ArtifactRemoverAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    try {
        juce::ScopedNoDenormals noDenormals;
        
        auto* inputChannelData = buffer.getReadPointer(0);
        auto* outputChannelData = buffer.getWritePointer(0);
        auto numSamples = buffer.getNumSamples();
        bool isSilent = buffer.getMagnitude(0, numSamples) < 1e-10f;
        if (isSilent)
        {
            // If input is silent, pass through and skip processing
            for (int i = 0; i < numSamples; ++i)
            {
                outputChannelData[i] = inputChannelData[i];
            }
            return;
        }
        // Get parameters safely with defaults
        int windowSize = getParameterIntSafe("window_size", 500);
        float lowerFreq = getParameterSafe("lower_freq", 10.0f);
        float upperFreq = getParameterSafe("upper_freq", 300.0f);
        float factor = getParameterSafe("factor", 0.4f);
        float svdThreshold = getParameterSafe("svd_threshold", 0.0f) / 1000.0f;
        float hankelSize = getParameterSafe("hankel_size", 100.0f);
        
        // Downsample: take every 8th sample (48kHz -> 6kHz)
        for (int i = 0; i < numSamples; i += DOWNSAMPLE_FACTOR)
        {
            downsampledBuffer.push_back(static_cast<double>(inputChannelData[i]));
        }
        
        // Keep buffer at max size
        int maxBufferSize = MAX_DOWNSAMPLED_BUFFER;
        while (static_cast<int>(downsampledBuffer.size()) > maxBufferSize)
        {
            downsampledBuffer.pop_front();
        }
        
        // Log buffer status periodically
        static int blockCounter = 0;
        blockCounter++;
        if (blockCounter % 20 == 0)
        {
            std::ostringstream oss;
            oss << "Buffer: " << downsampledBuffer.size() << "/" << windowSize 
                << ", Queue: " << outputQueue.size();
            DebugLogger::getInstance().log(oss.str());
        }
        int bufferSize = static_cast<int>(downsampledBuffer.size());
        // Process when we have enough data
        if (bufferSize >= windowSize)
        {
            DebugLogger::getInstance().log(">>> PROCESSING TRIGGERED");
                        
            Vector signal(windowSize);
            double signalMin = 1e9, signalMax = -1e9, signalMean = 0;
            for (int i = 0; i < windowSize; ++i)
            {
                int bufIdx = i;
                if (bufIdx < bufferSize)
                {
                    signal(i) = downsampledBuffer[bufIdx];
                }
                else
                {
                    signal(i) = 0.0;
                }
                signalMin = std::min(signalMin, signal(i));
                signalMax = std::max(signalMax, signal(i));
            }
            std::ostringstream oss;
            oss << "Signal stats - Min: " << signalMin << ", Max: " << signalMax;
            DebugLogger::getInstance().log(oss.str());
            // Run artifact removal
            RemovalResult result = remover.remove_artifact(
                signal,
                static_cast<int>(hankelSize),
                1,
                PROCESSING_SAMPLE_RATE,
                static_cast<double>(lowerFreq),
                static_cast<double>(upperFreq),
                static_cast<double>(factor),
                static_cast<double>(svdThreshold)
            );
            
            // Get last half of cleaned signal (256 samples @ 6kHz)
            int halfWindow = windowSize / 2;
            std::vector<double> lastHalf(result.cleaned_signal.data() + halfWindow,
                                        result.cleaned_signal.data() + windowSize);
            
            // Upsample back to 48kHz
            std::vector<double> upsampled = linearUpsample(lastHalf, DOWNSAMPLE_FACTOR);
            
            // Extract last 256 samples
            int extractSize = std::min(numSamples, static_cast<int>(upsampled.size()));
            int extractStart = upsampled.size() - extractSize;
            
            for (int i = extractStart; i < static_cast<int>(upsampled.size()); ++i)
            {
                outputChannelData[i - extractStart] = upsampled[i];
                std::ostringstream oss;
                oss << "Output sample " << (i - extractStart) << ": " << upsampled[i];
                DebugLogger::getInstance().log(oss.str());
            }
        }
        else
        {
            // Buffer not full yet - pass through input directly
            for (int i = 0; i < numSamples; ++i)
            {
                outputChannelData[i] = inputChannelData[i];
                // outputQueue.push_back(inputChannelData[i]);
            }
        }
        
        // // Fill output buffer from queue
        // for (int i = 0; i < numSamples; ++i)
        // {
        //     if (!outputQueue.empty())
        //     {
        //         outputChannelData[i] = outputQueue.front();
        //         outputQueue.pop_front();
        //     }
        //     else
        //     {
        //         outputChannelData[i] = inputChannelData[i];
        //     }
        // }
    }
    catch (const std::exception& e) {
        DebugLogger::getInstance().log(std::string("processBlock EXCEPTION: ") + e.what());
    }
    catch (...) {
        DebugLogger::getInstance().log("processBlock UNKNOWN EXCEPTION");
    }
}


juce::AudioProcessorEditor* ArtifactRemoverAudioProcessor::createEditor()
{
    return new ArtifactRemoverEditor(*this);
}

bool ArtifactRemoverAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String ArtifactRemoverAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ArtifactRemoverAudioProcessor::acceptsMidi() const
{
    return false;
}

bool ArtifactRemoverAudioProcessor::producesMidi() const
{
    return false;
}

bool ArtifactRemoverAudioProcessor::isMidiEffect() const
{
    return false;
}

double ArtifactRemoverAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ArtifactRemoverAudioProcessor::getNumPrograms()
{
    return 1;
}

int ArtifactRemoverAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ArtifactRemoverAudioProcessor::setCurrentProgram(int)
{
}

const juce::String ArtifactRemoverAudioProcessor::getProgramName(int)
{
    return {};
}

void ArtifactRemoverAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void ArtifactRemoverAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ArtifactRemoverAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

// Create the plugin instance
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArtifactRemoverAudioProcessor();
}
