#include "PluginEditor.h"

ArtifactRemoverEditor::ArtifactRemoverEditor(ArtifactRemoverAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setResizable(true, true);
    setSize(700, 480);
    
    // Setup sliders
    setupSlider(windowSizeSlider, 128.0f, 2048.0f, 512.0f, 1.0);
    setupSlider(lowerFreqSlider, 0.0f, 100.0f, 10.0f, 1.0);
    setupSlider(upperFreqSlider, 100.0f, 600.0f, 300.0f, 1.0);
    setupSlider(factorSlider, 0.0f, 1.0f, 0.4f, 0.01);
    setupSlider(svdThresholdSlider, 0.0f, 1.0f, 0.0f, 0.1);
    setupSlider(hankelSizeSlider, 50.0f, 300.0f, 100.0f, 1.0);
    
    // Add sliders
    addAndMakeVisible(windowSizeSlider);
    addAndMakeVisible(lowerFreqSlider);
    addAndMakeVisible(upperFreqSlider);
    addAndMakeVisible(factorSlider);
    addAndMakeVisible(svdThresholdSlider);
    addAndMakeVisible(hankelSizeSlider);
    
    // Setup labels (attach to left of slider)
    windowSizeLabel.attachToComponent(&windowSizeSlider, true);
    lowerFreqLabel.attachToComponent(&lowerFreqSlider, true);
    upperFreqLabel.attachToComponent(&upperFreqSlider, true);
    factorLabel.attachToComponent(&factorSlider, true);
    svdThresholdLabel.attachToComponent(&svdThresholdSlider, true);
    hankelSizeLabel.attachToComponent(&hankelSizeSlider, true);
    
    addAndMakeVisible(windowSizeLabel);
    addAndMakeVisible(lowerFreqLabel);
    addAndMakeVisible(upperFreqLabel);
    addAndMakeVisible(factorLabel);
    addAndMakeVisible(svdThresholdLabel);
    addAndMakeVisible(hankelSizeLabel);
    
    // Setup value labels
    windowSizeValue.setJustificationType(juce::Justification::centredRight);
    lowerFreqValue.setJustificationType(juce::Justification::centredRight);
    upperFreqValue.setJustificationType(juce::Justification::centredRight);
    factorValue.setJustificationType(juce::Justification::centredRight);
    svdThresholdValue.setJustificationType(juce::Justification::centredRight);
    hankelSizeValue.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(windowSizeValue);
    addAndMakeVisible(lowerFreqValue);
    addAndMakeVisible(upperFreqValue);
    addAndMakeVisible(factorValue);
    addAndMakeVisible(svdThresholdValue);
    addAndMakeVisible(hankelSizeValue);
    
    // Create attachments
    windowSizeAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *audioProcessor.apvts.getParameter("window_size"), windowSizeSlider);
    
    lowerFreqAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *audioProcessor.apvts.getParameter("lower_freq"), lowerFreqSlider);
    
    upperFreqAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *audioProcessor.apvts.getParameter("upper_freq"), upperFreqSlider);
    
    factorAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *audioProcessor.apvts.getParameter("factor"), factorSlider);
    
    svdThresholdAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *audioProcessor.apvts.getParameter("svd_threshold"), svdThresholdSlider);
    
    hankelSizeAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *audioProcessor.apvts.getParameter("hankel_size"), hankelSizeSlider);

    // Add listeners
    windowSizeSlider.addListener(this);
    lowerFreqSlider.addListener(this);
    upperFreqSlider.addListener(this);
    factorSlider.addListener(this);
    svdThresholdSlider.addListener(this);
    hankelSizeSlider.addListener(this);
}

ArtifactRemoverEditor::~ArtifactRemoverEditor() = default;

void ArtifactRemoverEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawFittedText("Artifact Remover VST3", getLocalBounds().removeFromTop(30), juce::Justification::centredTop, 1);
}

void ArtifactRemoverEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    int sliderHeight = 40;
    int labelWidth = 140;
    int valueWidth = 80;
    int spacing = 10;
    
    bounds.removeFromTop(40);
    
    // Helper lambda for consistent layout
    auto layoutSlider = [&](auto& slider, auto& valueLabel) {
        auto sliderBounds = bounds.removeFromTop(sliderHeight + spacing);
        
        // Label already positioned by attachToComponent
        int sliderLeft = labelWidth + 10;
        int sliderRight = sliderBounds.getWidth() - valueWidth - spacing;
        int sliderWidth = sliderRight - sliderLeft;
        
        slider.setBounds(sliderLeft, sliderBounds.getY(), sliderWidth, sliderHeight);
        valueLabel.setBounds(sliderRight, sliderBounds.getY(), valueWidth, sliderHeight);
    };
    
    layoutSlider(windowSizeSlider, windowSizeValue);
    layoutSlider(lowerFreqSlider, lowerFreqValue);
    layoutSlider(upperFreqSlider, upperFreqValue);
    layoutSlider(factorSlider, factorValue);
    layoutSlider(svdThresholdSlider, svdThresholdValue);
    layoutSlider(hankelSizeSlider, hankelSizeValue);
}

void ArtifactRemoverEditor::setupSlider(juce::Slider& slider, float min, float max, float defaultVal, float step)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::lightblue);
    slider.setColour(juce::Slider::trackColourId, juce::Colours::darkgrey);
    
    // Clamp default to range
    if (defaultVal < min)
        defaultVal = min;
    else if (defaultVal > max)
        defaultVal = max;

    slider.setRange(min, max, step);
    slider.setValue(defaultVal);
}

void ArtifactRemoverEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &windowSizeSlider)
    {
        windowSizeValue.setText(juce::String(static_cast<int>(windowSizeSlider.getValue())), juce::dontSendNotification);
    }
    else if (slider == &lowerFreqSlider)
    {
        lowerFreqValue.setText(juce::String(lowerFreqSlider.getValue(), 1), juce::dontSendNotification);
    }
    else if (slider == &upperFreqSlider)
    {
        upperFreqValue.setText(juce::String(upperFreqSlider.getValue(), 1), juce::dontSendNotification);
    }
    else if (slider == &factorSlider)
    {
        factorValue.setText(juce::String(factorSlider.getValue(), 2), juce::dontSendNotification);
    }
    else if (slider == &svdThresholdSlider)
    {
        svdThresholdValue.setText(juce::String(svdThresholdSlider.getValue(), 3), juce::dontSendNotification);
    }
    else if (slider == &hankelSizeSlider)
    {
        hankelSizeValue.setText(juce::String(static_cast<int>(hankelSizeSlider.getValue())), juce::dontSendNotification);
    }
}
