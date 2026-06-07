#include "PluginEditor.h"

ClariSynthEditor::ClariSynthEditor (ClariSynthProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (600, 400);

    auto& apvts = p.apvts;

    // Harmonic sliders H1–H8
    for (int i = 0; i < 8; ++i)
    {
        auto& s = harmonicSliders[i];
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 18);
        addAndMakeVisible (s);

        harmonicLabels[i].setText ("H" + juce::String(i + 1), juce::dontSendNotification);
        harmonicLabels[i].setJustificationType (juce::Justification::centred);
        addAndMakeVisible (harmonicLabels[i]);

        harmonicAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, "harmonic" + juce::String(i + 1), s);
    }

    // Odd/even slider
    oddEvenSlider.setSliderStyle (juce::Slider::LinearVertical);
    oddEvenSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible (oddEvenSlider);
    oddEvenLabel.setText ("Odd/Even", juce::dontSendNotification);
    oddEvenLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (oddEvenLabel);
    oddEvenAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "oddEvenBoost", oddEvenSlider);

    // Centroid tilt slider
    centroidSlider.setSliderStyle (juce::Slider::LinearVertical);
    centroidSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible (centroidSlider);
    centroidLabel.setText ("Tilt", juce::dontSendNotification);
    centroidLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (centroidLabel);
    centroidAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "centroidTilt", centroidSlider);

    // Mix slider
    mixSlider.setSliderStyle (juce::Slider::LinearVertical);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible (mixSlider);
    mixLabel.setText ("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (mixLabel);
    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "mix", mixSlider);
}

void ClariSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("ClariSynth", getLocalBounds().removeFromTop (30),
                      juce::Justification::centred, 1);
}

void ClariSynthEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (30); // title

    const int labelH   = 20;
    const int sliderW  = 50;
    const int totalW   = getWidth() - 20;

    // 8 harmonic sliders + 3 control sliders = 11 columns
    const int numCols  = 11;
    const int colW     = totalW / numCols;

    for (int i = 0; i < 8; ++i)
    {
        auto col = area.removeFromLeft (colW);
        harmonicLabels[i].setBounds (col.removeFromBottom (labelH));
        harmonicSliders[i].setBounds (col);
    }

    auto col = area.removeFromLeft (colW);
    oddEvenLabel.setBounds (col.removeFromBottom (labelH));
    oddEvenSlider.setBounds (col);

    col = area.removeFromLeft (colW);
    centroidLabel.setBounds (col.removeFromBottom (labelH));
    centroidSlider.setBounds (col);

    col = area.removeFromLeft (colW);
    mixLabel.setBounds (col.removeFromBottom (labelH));
    mixSlider.setBounds (col);
}
