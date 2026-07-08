#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <sstream>

// =============================================================
// Parameter helpers
// =============================================================

float ArtifactRemoverAudioProcessor::getParameterSafe(const char* name,
                                                       float defaultValue)
{
    auto* param = apvts.getRawParameterValue(name);
    if (param == nullptr)
    {
        DebugLogger::getInstance().log(std::string("Parameter null: ") + name);
        return defaultValue;
    }
    return param->load();
}

int ArtifactRemoverAudioProcessor::getParameterIntSafe(const char* name,
                                                        int defaultValue)
{
    return static_cast<int>(getParameterSafe(name, static_cast<float>(defaultValue)));
}

// =============================================================
// Parameter layout
// =============================================================

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

// =============================================================
// Constructor / Destructor
// =============================================================

ArtifactRemoverAudioProcessor::ArtifactRemoverAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::mono(), true)
          .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    latencySamples    = 0;
    currentWindowSize = 500;

    DebugLogger::getInstance().initialize();
    DebugLogger::getInstance().log("ArtifactRemoverAudioProcessor initialized");
}

ArtifactRemoverAudioProcessor::~ArtifactRemoverAudioProcessor()
{
    DebugLogger::getInstance().log("Destructor - stopping background thread");
    stopBackgroundThread();
    DebugLogger::getInstance().log("Destructor - done");
}

// =============================================================
// Background thread
// =============================================================

void ArtifactRemoverAudioProcessor::startBackgroundThread()
{
    shouldStop = false;
    bgThread   = std::thread(&ArtifactRemoverAudioProcessor::backgroundThreadFunc, this);
    DebugLogger::getInstance().log("Background thread started");
}

void ArtifactRemoverAudioProcessor::stopBackgroundThread()
{
    shouldStop = true;
    bgCv.notify_all();
    if (bgThread.joinable())
        bgThread.join();
    DebugLogger::getInstance().log("Background thread stopped");
}

void ArtifactRemoverAudioProcessor::backgroundThreadFunc()
{
    while (!shouldStop.load())
    {
        // --- Attente d'un nouveau bloc d'input ---
        {
            std::unique_lock<std::mutex> lock(bgMutex);
            bgCv.wait(lock, [this] {
                return newInputReady.load() || shouldStop.load();
            });

            if (shouldStop.load())
                break;

            newInputReady = false;
        }

        // --- Copie locale hors lock (ne bloque pas le thread RT) ---
        std::vector<double> localInput;
        {
            std::lock_guard<std::mutex> lock(bgMutex);
            localInput = pendingInput;
        }

        const int   windowSize   = bgWindowSize.load();
        const float lowerFreq    = bgLowerFreq.load();
        const float upperFreq    = bgUpperFreq.load();
        const float factor       = bgFactor.load();
        const float svdThreshold = bgSvdThreshold.load() / 1000.0f;
        const int   hankelSize   = static_cast<int>(bgHankelSize.load());

        if (static_cast<int>(localInput.size()) < windowSize)
            continue;

        // --- Construit le vecteur Eigen ---
        Vector signal(windowSize);
        for (int i = 0; i < windowSize; ++i)
            signal(i) = localInput[i];

        // --- SVD + reconstruction (peut prendre 10-50 ms, hors RT) ---
        try
        {
            RemovalResult result = remover.remove_artifact(
                signal,
                hankelSize,
                1,
                PROCESSING_SAMPLE_RATE,
                static_cast<double>(lowerFreq),
                static_cast<double>(upperFreq),
                static_cast<double>(factor),
                static_cast<double>(svdThreshold)
            );

            // --- Dépose le résultat pour le thread RT ---
            {
                std::lock_guard<std::mutex> lock(bgMutex);
                pendingOutput.assign(
                    result.cleaned_signal.data(),
                    result.cleaned_signal.data() + result.cleaned_signal.size()
                );
                newOutputReady = true;
            }
        }
        catch (const std::exception& e)
        {
            DebugLogger::getInstance().log(
                std::string("BG thread exception: ") + e.what());
        }
        catch (...)
        {
            DebugLogger::getInstance().log("BG thread unknown exception");
        }
    }
}

// =============================================================
// JUCE lifecycle
// =============================================================

void ArtifactRemoverAudioProcessor::prepareToPlay(double inSampleRate,
                                                   int    samplesPerBlock)
{
    try
    {
        DebugLogger::getInstance().log("prepareToPlay starting...");

        sampleRate        = inSampleRate;
        currentWindowSize = getParameterIntSafe("window_size", 500);

        // --- Ring buffer input (downsamplé) ---
        downsampledRing.assign(MAX_DOWNSAMPLED_BUFFER, 0.0);
        ringWritePos = 0;
        ringFull     = false;

        // --- Ring buffer output (downsamplé, résultats du thread BG) ---
        outputRing.assign(MAX_DOWNSAMPLED_BUFFER, 0.0);
        outputRingWritePos = 0;
        outputRingReadPos  = 0;

        // --- Buffers de communication ---
        pendingInput .resize(currentWindowSize, 0.0);
        pendingOutput.resize(currentWindowSize, 0.0);

        // --- Latence déclarée à l'hôte ---
        // Une fenêtre complète de traitement (en samples natifs)
        latencySamples = currentWindowSize * DOWNSAMPLE_FACTOR;
        setLatencySamples(latencySamples);

        // --- Filtre de reconstruction anti-image (cutoff = Nyquist 6 kHz = 3 kHz) ---
        reconstructionFilter = Biquad::makeLowPass(inSampleRate, PROCESSING_SAMPLE_RATE / 2.0);
        reconstructionFilter.reset();

        // --- Redémarre le thread background ---
        stopBackgroundThread();
        startBackgroundThread();

        std::ostringstream oss;
        oss << "prepareToPlay OK - SR=" << inSampleRate
            << " block=" << samplesPerBlock
            << " window=" << currentWindowSize
            << " latency=" << latencySamples << " samples";
        DebugLogger::getInstance().log(oss.str());
    }
    catch (const std::exception& e)
    {
        DebugLogger::getInstance().log(
            std::string("prepareToPlay exception: ") + e.what());
    }
}

void ArtifactRemoverAudioProcessor::releaseResources()
{
    DebugLogger::getInstance().log("releaseResources() called");
    stopBackgroundThread();
    downsampledRing.clear();
    outputRing.clear();
    DebugLogger::getInstance().log("releaseResources() done");
}

// =============================================================
// Upsample helpers
// =============================================================

std::vector<double>
ArtifactRemoverAudioProcessor::linearUpsample(const std::vector<double>& input,
                                               int factor)
{
    std::vector<double> output;

    if (input.empty())
        return output;

    if (input.size() == 1)
    {
        output.push_back(input[0]);
        return output;
    }

    output.reserve((input.size() - 1) * factor + 1);

    for (size_t i = 0; i < input.size() - 1; ++i)
    {
        for (int j = 0; j < factor; ++j)
        {
            const double alpha = static_cast<double>(j) / factor;
            output.push_back(input[i] * (1.0 - alpha) + input[i + 1] * alpha);
        }
    }
    output.push_back(input.back());
    return output;
}

std::vector<double>
ArtifactRemoverAudioProcessor::zeroPadUpsample(const std::vector<double>& input,
                                                int factor)
{
    std::vector<double> output(input.size() * factor, 0.0);
    for (size_t i = 0; i < input.size(); ++i)
        output[i * factor] = input[i];
    return output;
}

// =============================================================
// processBlock  — RT thread, aucune allocation lourde
// =============================================================

void ArtifactRemoverAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer&)
{
    try
    {
        juce::ScopedNoDenormals noDenormals;

        auto* in         = buffer.getReadPointer(0);
        auto* out        = buffer.getWritePointer(0);
        const int numSamples = buffer.getNumSamples();

        // Etat initial : pass-through propre
        juce::FloatVectorOperations::copy(out, in, numSamples);

        // Silence → bypass immédiat
        if (buffer.getMagnitude(0, numSamples) < 1e-10f)
            return;

        // --- Mise à jour des paramètres pour le thread BG (atomic, RT-safe) ---
        bgWindowSize  .store(getParameterIntSafe("window_size", 500));
        bgLowerFreq   .store(getParameterSafe("lower_freq",    10.0f));
        bgUpperFreq   .store(getParameterSafe("upper_freq",   300.0f));
        bgFactor      .store(getParameterSafe("factor",         0.4f));
        bgSvdThreshold.store(getParameterSafe("svd_threshold",  0.0f));
        bgHankelSize  .store(getParameterSafe("hankel_size",  100.0f));

        const int windowSize = bgWindowSize.load();

        // -------------------------------------------------------
        // 1. Alimentation du ring buffer input (downsamplé)
        //    Aucune allocation, aucun déplacement mémoire
        // -------------------------------------------------------
        for (int i = 0; i < numSamples; i += DOWNSAMPLE_FACTOR)
        {
            downsampledRing[ringWritePos] = static_cast<double>(in[i]);
            ringWritePos = (ringWritePos + 1) % MAX_DOWNSAMPLED_BUFFER;
            if (ringWritePos == 0)
                ringFull = true;
        }

        const int available = ringFull ? MAX_DOWNSAMPLED_BUFFER : ringWritePos;
        if (available < windowSize)
            return; // buffer pas encore plein → pass-through

        // -------------------------------------------------------
        // 2. Envoi d'un snapshot au thread BG (non-bloquant)
        //    try_lock : si le mutex est pris, on skip ce bloc
        // -------------------------------------------------------
        if (!newInputReady.load())
        {
            if (bgMutex.try_lock())
            {
                pendingInput.resize(windowSize);
                const int startIdx = (ringWritePos - windowSize
                                      + MAX_DOWNSAMPLED_BUFFER)
                                      % MAX_DOWNSAMPLED_BUFFER;
                for (int i = 0; i < windowSize; ++i)
                    pendingInput[i] = downsampledRing[
                        (startIdx + i) % MAX_DOWNSAMPLED_BUFFER];

                newInputReady = true;
                bgMutex.unlock();
                bgCv.notify_one();
            }
        }

        // -------------------------------------------------------
        // 3. Récupération du résultat du thread BG (non-bloquant)
        //    Écrit dans le ring buffer output
        // -------------------------------------------------------
        if (newOutputReady.load())
        {
            if (bgMutex.try_lock())
            {
                const int sz = static_cast<int>(pendingOutput.size());
                for (int i = 0; i < sz; ++i)
                {
                    outputRing[outputRingWritePos] = pendingOutput[i];
                    outputRingWritePos = (outputRingWritePos + 1)
                                         % MAX_DOWNSAMPLED_BUFFER;
                }
                newOutputReady = false;
                bgMutex.unlock();
            }
        }

        // -------------------------------------------------------
        // 4. Lecture du ring buffer output + upsample + écriture
        // -------------------------------------------------------
        const int samplesNeeded = (numSamples + DOWNSAMPLE_FACTOR - 1)
                                   / DOWNSAMPLE_FACTOR;

        // Consume sequentially from the output ring using a dedicated read cursor.
        // outputRingReadPos advances every block; outputRingWritePos advances only
        // when the BG thread delivers a new result.  Reading the same write-relative
        // position on every block (the old approach) produced a zero-order hold that
        // repeated a 32-sample window ~16 times, generating a harmonic comb at
        // multiples of 6000/32 ≈ 187 Hz.  With independent cursors, each block
        // reads the next fresh slice; underrun (BG not yet ready) outputs silence.
        std::vector<double> toUpsample(samplesNeeded, 0.0);
        int avail = (outputRingWritePos - outputRingReadPos
                     + MAX_DOWNSAMPLED_BUFFER) % MAX_DOWNSAMPLED_BUFFER;
        for (int i = 0; i < samplesNeeded; ++i)
        {
            if (avail > 0)
            {
                toUpsample[i]    = outputRing[outputRingReadPos];
                outputRingReadPos = (outputRingReadPos + 1) % MAX_DOWNSAMPLED_BUFFER;
                --avail;
            }
            // else: leave 0.0 — BG thread hasn't produced output yet (underrun)
        }

        std::vector<double> upsampled =
            linearUpsample(toUpsample, DOWNSAMPLE_FACTOR);

        // Reconstruction filter: removes spectral images introduced by upsampling.
        // Linear interpolation alone leaves images above ~1 kHz; this Butterworth
        // LP at 3 kHz (Nyquist of the 6 kHz processing rate) eliminates them.
        for (auto& s : upsampled)
            s = reconstructionFilter.processSample(s);

        const int upsampledSize = static_cast<int>(upsampled.size());

        for (int i = 0; i < numSamples; ++i)
        {
            const int srcIdx = upsampledSize - numSamples + i;
            if (srcIdx >= 0 && srcIdx < upsampledSize)
                out[i] = static_cast<float>(upsampled[srcIdx]);
            // sinon : in[i] déjà copié en début de fonction
        }
    }
    catch (const std::exception& e)
    {
        DebugLogger::getInstance().log(
            std::string("processBlock EXCEPTION: ") + e.what());
    }
    catch (...)
    {
        DebugLogger::getInstance().log("processBlock UNKNOWN EXCEPTION");
    }
}

// =============================================================
// JUCE boilerplate
// =============================================================

juce::AudioProcessorEditor* ArtifactRemoverAudioProcessor::createEditor()
{
    return new ArtifactRemoverEditor(*this);
}

bool ArtifactRemoverAudioProcessor::hasEditor() const { return true; }

const juce::String ArtifactRemoverAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ArtifactRemoverAudioProcessor::acceptsMidi() const { return false; }
bool ArtifactRemoverAudioProcessor::producesMidi() const { return false; }
bool ArtifactRemoverAudioProcessor::isMidiEffect() const { return false; }

double ArtifactRemoverAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ArtifactRemoverAudioProcessor::getNumPrograms()          { return 1; }
int ArtifactRemoverAudioProcessor::getCurrentProgram()       { return 0; }
void ArtifactRemoverAudioProcessor::setCurrentProgram(int)   {}

const juce::String ArtifactRemoverAudioProcessor::getProgramName(int)
{
    return {};
}

void ArtifactRemoverAudioProcessor::changeProgramName(int, const juce::String&) {}

void ArtifactRemoverAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ArtifactRemoverAudioProcessor::setStateInformation(const void* data,
                                                         int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArtifactRemoverAudioProcessor();
}