// =========================================================
// JuceAudioProcessingTest.cpp
// Test the artifact remover with audio file processing
// mimics the VST3 plugin processing loop
// =========================================================

#include <deque>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <algorithm>

#include "artifact_remover/remover.h"

using artifact_remover::Remover;
using artifact_remover::Vector;
using artifact_remover::Matrix;
using artifact_remover::RemovalResult;

// Processing constants
constexpr double PROCESSING_SAMPLE_RATE = 6000.0;   // Downsampled rate
constexpr int DOWNSAMPLE_FACTOR = 8;                // 48000 / 6000 = 8
constexpr int PROCESSING_WINDOW_SIZE = 450;         // @ 6kHz
constexpr int INPUT_BLOCK_SIZE = 256;               // Samples per block from audio file

std::vector<double> linearUpsample(const std::vector<double>& input, int factor)
{
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

std::vector<double> zeroPadUpsample(const std::vector<double>& input, int factor)
{
    std::vector<double> output(input.size() * factor, 0.0);
    for (size_t i = 0; i < input.size(); ++i) {
        output[i * factor] = input[i];
    }
    return output;
}

int main(int argc, char* argv[])
{
    // Read raw audio data from file
    std::vector<float> audioData;
    double sampleRate = 48000.0;
    
    // Accept optional path as argument, otherwise look in current directory
    std::string inputPath = (argc > 1) ? argv[1] : "test_48khz.txt";
    std::ifstream inFile(inputPath);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open test_48khz.txt\n";
        std::cerr << "Make sure to run from demo directory or provide full path\n";
        return 1;
    }
    
    std::string line;
    while (std::getline(inFile, line)) {
        if (!line.empty()) {
            try {
                audioData.push_back(std::stof(line));
            }
            catch (...) {
                // Skip invalid lines
            }
        }
    }
    inFile.close();
    
    std::cout << "=== Audio Processing Test (mimics VST3 plugin) ===\n\n";
    std::cout << "Input file: " << inputPath << "\n";
    std::cout << "Audio data loaded: " << audioData.size() << " samples @ " << sampleRate << " Hz\n";
    std::cout << "Duration: " << (audioData.size() / sampleRate) << " seconds\n";
    std::cout << "Processing window: " << PROCESSING_WINDOW_SIZE << " samples @ 6kHz\n";
    std::cout << "Block size: " << INPUT_BLOCK_SIZE << " samples @ 48kHz\n\n";
    
    // Initialize processing
    Remover remover;
    std::deque<double> downsampledBuffer;
    std::deque<float> outputQueue;
    
    // Processing parameters
    int windowSize = PROCESSING_WINDOW_SIZE;
    float lowerFreq = 10.0f;
    float upperFreq = 300.0f;
    float factor = 0.3f;
    float hankelSize = 100.0f;
    float svdThreshold = 0.0f;
    
    // Process in blocks
    std::vector<float> processedOutput;
    int blockCount = 0;
    int processCount = 0;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t sampleIdx = 0; sampleIdx < audioData.size(); sampleIdx += INPUT_BLOCK_SIZE)
    {
        // Get block of 256 samples
        int blockSize = std::min((size_t)INPUT_BLOCK_SIZE, audioData.size() - sampleIdx);
        const float* blockData = audioData.data() + sampleIdx;
        
        blockCount++;
        
        // Downsample: take every 8th sample (48kHz -> 6kHz)
        for (int i = 0; i < blockSize; i += DOWNSAMPLE_FACTOR)
        {
            downsampledBuffer.push_back(static_cast<double>(blockData[i]));
        }
        
        // Keep buffer at reasonable size (must be large enough for window size!)
        // 512 samples @ 6kHz = max window size. Add headroom for safety.
        int maxBufferSize = PROCESSING_WINDOW_SIZE * 2;
        while (static_cast<int>(downsampledBuffer.size()) > maxBufferSize)
        {
            downsampledBuffer.pop_front();
        }
        
        // Process when we have enough data
        if (static_cast<int>(downsampledBuffer.size()) >= windowSize)
        {
            processCount++;
            
            // Extract last windowSize samples
            int bufferSize = static_cast<int>(downsampledBuffer.size());
            int startIdx = std::max(0, bufferSize - windowSize);
            
            Vector signal(windowSize);
            double signalMin = 1e9, signalMax = -1e9, signalMean = 0;
            
            for (int i = 0; i < windowSize; ++i)
            {
                int bufIdx = startIdx + i;
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
                signalMean += signal(i);
            }
            signalMean /= windowSize;
            
            // std::cout << "Process " << processCount << " - Input Signal: Min=" << signalMin 
            //           << ", Max=" << signalMax << ", Mean=" << signalMean << "\n";
            
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
            int extractSize = std::min(INPUT_BLOCK_SIZE, static_cast<int>(upsampled.size()));
            int extractStart = upsampled.size() - extractSize;
            
            for (int i = extractStart; i < static_cast<int>(upsampled.size()); ++i)
            {
                processedOutput.push_back(static_cast<float>(upsampled[i]));
            }
        }
        else
        {
            // Buffer not full yet - pass through input directly (skip queue for speed)
            for (int i = 0; i < blockSize; ++i)
            {
                processedOutput.push_back(blockData[i]);
            }
        }
        
        // if (blockCount % 10 == 0)
        // {
        //     std::cout << "  Processed " << blockCount << " blocks, " << processedOutput.size() 
        //               << " output samples\n";
        // }
    }
    
    // Trim output to match input size exactly
    if (processedOutput.size() > audioData.size())
    {
        processedOutput.resize(audioData.size());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "\n=== Processing Complete ===\n";
    std::cout << "Total processing time: " << duration.count() << " ms\n";
    std::cout << "Average time per block: " << (duration.count() / blockCount) << " ms\n";
    std::cout << "Blocks processed: " << blockCount << "\n";
    std::cout << "Processing operations: " << processCount << "\n";
    std::cout << "Output samples: " << processedOutput.size() << " / " << audioData.size() << "\n\n";
    
    // Save output to file
    // Derive output path from input path
    std::string outputPath = inputPath;
    size_t dotPos = outputPath.rfind('.');
    if (dotPos != std::string::npos)
        outputPath = outputPath.substr(0, dotPos) + "_processed" + outputPath.substr(dotPos);
    else
        outputPath += "_processed.txt";
    std::ofstream outFile(outputPath);
    if (outFile.is_open())
    {
        for (float sample : processedOutput)
        {
            outFile << sample << "\n";
        }
        outFile.close();
        std::cout << "Output saved to: " << outputPath << "\n";
    }
    else
    {
        std::cerr << "Error: Could not write output file\n";
    }
    
    return 0;
}
