#pragma once

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

// 2nd-order direct-form-II Butterworth biquad, used as reconstruction filter
// after upsampling. State persists across processBlock calls.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    double processSample(double x)
    {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0.0; }

    static Biquad makeLowPass(double sampleRate, double frequency)
    {
        const double pi    = 3.14159265358979323846;
        const double w0    = 2.0 * pi * frequency / sampleRate;
        const double cosw0 = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * std::sqrt(2.0)); // Q = 1/sqrt(2)
        const double a0    = 1.0 + alpha;
        Biquad b;
        b.b0 = (1.0 - cosw0) / (2.0 * a0);
        b.b1 = (1.0 - cosw0) / a0;
        b.b2 = (1.0 - cosw0) / (2.0 * a0);
        b.a1 = (-2.0 * cosw0) / a0;
        b.a2 = (1.0 - alpha)  / a0;
        return b;
    }
};

#include "DebugLogger.h"
#include "artifact_remover/remover.h"

using artifact_remover::Matrix;
using artifact_remover::RemovalResult;
using artifact_remover::Remover;
using artifact_remover::Vector;

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

    juce::AudioProcessorValueTreeState apvts;

private:

    // ==========================================================
    // Constantes
    // ==========================================================

    static constexpr int    MIN_WINDOW_SIZE        = 128;
    static constexpr int    MAX_WINDOW_SIZE         = 2048;
    static constexpr int    TIME_DELAY              = 1;
    static constexpr double PROCESSING_SAMPLE_RATE  = 6000.0;
    static constexpr int    DOWNSAMPLE_FACTOR       = 8;       // 48000 / 6000
    static constexpr int    MAX_DOWNSAMPLED_BUFFER  = MAX_WINDOW_SIZE;

    // ==========================================================
    // État interne
    // ==========================================================

    Remover remover;
    Biquad  reconstructionFilter;
    double  sampleRate        = 48000.0;
    int     latencySamples    = 0;
    int     currentWindowSize = 500;
    bool    bufferInitialized = false;

    // ==========================================================
    // Ring buffer input  (thread RT → thread BG)
    // Contient les samples downsamplés en attente de traitement
    // ==========================================================

    std::vector<double> downsampledRing;
    int  ringWritePos = 0;
    bool ringFull     = false;

    // ==========================================================
    // Ring buffer output (thread BG → thread RT)
    // Contient les samples nettoyés prêts à être upsamplés
    // ==========================================================

    std::vector<double> outputRing;
    int outputRingWritePos = 0;
    int outputRingReadPos  = 0;

    // ==========================================================
    // Communication RT <-> thread background
    //
    // Protocole :
    //   RT écrit pendingInput  + newInputReady  = true  → signal bgCv
    //   BG lit  pendingInput   + newInputReady  = false
    //   BG écrit pendingOutput + newOutputReady = true
    //   RT lit  pendingOutput  + newOutputReady = false
    //
    // Tous les accès à pendingInput / pendingOutput sont sous bgMutex.
    // Les flags newInputReady / newOutputReady sont atomiques pour
    // permettre une lecture sans lock depuis le thread RT (try_lock).
    // ==========================================================

    std::vector<double>     pendingInput;
    std::vector<double>     pendingOutput;

    std::mutex              bgMutex;
    std::condition_variable bgCv;
    std::atomic<bool>       newInputReady  { false };
    std::atomic<bool>       newOutputReady { false };
    std::atomic<bool>       shouldStop     { false };
    std::thread             bgThread;

    // ==========================================================
    // Paramètres miroir pour le thread BG (atomic, RT-safe)
    // Écrits par le thread RT dans processBlock,
    // lus par le thread BG dans backgroundThreadFunc.
    // ==========================================================

    std::atomic<int>   bgWindowSize   { 500 };
    std::atomic<float> bgLowerFreq    { 10.0f };
    std::atomic<float> bgUpperFreq    { 300.0f };
    std::atomic<float> bgFactor       { 0.4f };
    std::atomic<float> bgSvdThreshold { 0.0f };
    std::atomic<float> bgHankelSize   { 100.0f };

    // ==========================================================
    // Méthodes privées
    // ==========================================================

    void startBackgroundThread();
    void stopBackgroundThread();
    void backgroundThreadFunc();

    float getParameterSafe   (const char* name, float defaultValue);
    int   getParameterIntSafe(const char* name, int   defaultValue);

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::vector<double> linearUpsample (const std::vector<double>& input, int factor);
    std::vector<double> zeroPadUpsample(const std::vector<double>& input, int factor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArtifactRemoverAudioProcessor)
};