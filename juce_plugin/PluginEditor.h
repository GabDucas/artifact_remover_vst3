#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class ArtifactRemoverEditor : public juce::AudioProcessorEditor, 
                               private juce::Slider::Listener
{
public:
    explicit ArtifactRemoverEditor(ArtifactRemoverAudioProcessor&);
    ~ArtifactRemoverEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ArtifactRemoverAudioProcessor& audioProcessor;

    // Sliders for parameters
    juce::Slider windowSizeSlider;
    juce::Slider lowerFreqSlider;
    juce::Slider upperFreqSlider;
    juce::Slider factorSlider;
    juce::Slider svdThresholdSlider;
    juce::Slider hankelSizeSlider;

    // Labels
    juce::Label windowSizeLabel{"Window Size", "Window Size:"};
    juce::Label lowerFreqLabel{"Lower Freq", "Lower Freq:"};
    juce::Label upperFreqLabel{"Upper Freq", "Upper Freq:"};
    juce::Label factorLabel{"Factor", "Factor:"};
    juce::Label svdThresholdLabel{"SVD Threshold", "SVD Threshold:"};
    juce::Label hankelSizeLabel{"Hankel Size", "Hankel Size:"};

    // Value labels
    juce::Label windowSizeValue;
    juce::Label lowerFreqValue;
    juce::Label upperFreqValue;
    juce::Label factorValue;
    juce::Label svdThresholdValue;
    juce::Label hankelSizeValue;

    // Attachments for two-way binding
    std::unique_ptr<juce::SliderParameterAttachment> windowSizeAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> lowerFreqAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> upperFreqAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> factorAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> svdThresholdAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> hankelSizeAttachment;

    void sliderValueChanged(juce::Slider*) override;
    void setupSlider(juce::Slider& slider, float min, float max, float defaultVal, float step = 1.0);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArtifactRemoverEditor)
};
