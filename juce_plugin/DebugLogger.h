#pragma once

#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <string>
#include <vector>

// Simple debug logger - lazy init to avoid hanging during build
class DebugLogger
{
public:
    static DebugLogger& getInstance()
    {
        static DebugLogger instance;
        return instance;
    }

    void initialize(const std::string& filePath = "")
    {
        if (initialized_)
            return;
        
        std::string path = filePath;
        if (path.empty())
        {
            const char* tempDir = std::getenv("TEMP");
            if (tempDir)
                path = std::string(tempDir) + "\\ArtifactRemover_Debug.log";
            else
                path = "C:\\Temp\\ArtifactRemover_Debug.log";
        }
        // remove file if it exists to start fresh
        std::remove(path.c_str());
        logFile_.open(path, std::ios::app);
        initialized_ = logFile_.is_open();
    }

    void log(const std::string& message)
    {
        if (!initialized_ || !logFile_.is_open())
            return;

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << "[" << std::put_time(std::localtime(&time), "%H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
            << message;

        logFile_ << oss.str() << std::endl;
        logFile_.flush();
    }

    void logBufferInfo(int numSamples, int numChannels, double sampleRate)
    {
        std::ostringstream oss;
        oss << "Buffer Info - Samples: " << numSamples 
            << ", Channels: " << numChannels 
            << ", SR: " << sampleRate << " Hz";
        log(oss.str());
    }

    void logProcessingStart(int windowSize, int downsampledBufferSize, bool isBufferFull)
    {
        std::ostringstream oss;
        oss << ">>> PROCESS BLOCK START - Window: " << windowSize 
            << ", Downsampled Buffer: " << downsampledBufferSize 
            << ", Buffer Full: " << (isBufferFull ? "YES" : "NO");
        log(oss.str());
    }

    void logParameters(float lowerFreq, float upperFreq, float factor, 
                       float hankelSize, float svdThreshold)
    {
        std::ostringstream oss;
        oss << "Parameters - Lower: " << lowerFreq << "Hz, Upper: " << upperFreq 
            << "Hz, Factor: " << factor << ", Hankel: " << hankelSize 
            << ", SVD Thresh: " << svdThreshold;
        log(oss.str());
    }

    void logRemovalResult(int signalSize, int rejectedCount, 
                         const std::vector<int>& rejectedIdx,
                         double cleanedSignalMin, double cleanedSignalMax)
    {
        std::ostringstream oss;
        oss << "Removal Result - Signal Size: " << signalSize 
            << ", Rejected Components: " << rejectedCount;
        log(oss.str());

        if (rejectedCount > 0)
        {
            oss.str("");
            oss.clear();
            oss << "  Rejected Indices: [";
            for (size_t i = 0; i < rejectedIdx.size() && i < 10; ++i)
            {
                if (i > 0) oss << ", ";
                oss << rejectedIdx[i];
            }
            if (rejectedIdx.size() > 10)
                oss << ", ... (" << rejectedIdx.size() << " total)";
            oss << "]";
            log(oss.str());
        }

        oss.str("");
        oss.clear();
        oss << "  Cleaned Signal Range: [" << cleanedSignalMin 
            << ", " << cleanedSignalMax << "]";
        log(oss.str());
    }

    void logProcessingEnd(int outputSamples)
    {
        std::ostringstream oss;
        oss << "<<< PROCESS BLOCK END - Output Samples: " << outputSamples;
        log(oss.str());
    }

private:
    DebugLogger() : initialized_(false) {}
    ~DebugLogger()
    {
        if (logFile_.is_open())
            logFile_.close();
    }

    std::ofstream logFile_;
    bool initialized_;
};
