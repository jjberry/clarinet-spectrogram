#include "PluginEditor.h"

ClariSynthEditor::ClariSynthEditor (ClariSynthProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (600, 500);

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

    // Pitch-tracking rotary controls
    auto setupRotary = [this] (juce::Slider& s, juce::Label& label, const juce::String& text,
                               const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        s.setTextValueSuffix (suffix);
        addAndMakeVisible (s);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    };

    setupRotary (pitchSmoothSlider, pitchSmoothLabel, "Smooth",  {});
    setupRotary (pitchSlewSlider,   pitchSlewLabel,   "Slew Dn", " st/s");
    setupRotary (pitchMinSlider,    pitchMinLabel,    "Min",     " Hz");
    setupRotary (pitchMaxSlider,    pitchMaxLabel,    "Max",     " Hz");

    pitchSmoothAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "pitchSmoothing", pitchSmoothSlider);
    pitchSlewAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "pitchSlewDown", pitchSlewSlider);
    pitchMinAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "pitchMinHz", pitchMinSlider);
    pitchMaxAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "pitchMaxHz", pitchMaxSlider);
}

void ClariSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("ClariSynth", getLocalBounds().removeFromTop (30),
                      juce::Justification::centred, 1);

    // "Pitch Tracking" section heading above the rotary row
    g.setColour (juce::Colours::lightgrey);
    g.setFont (13.0f);
    g.drawFittedText ("Pitch Tracking", pitchSectionHeader,
                      juce::Justification::centredLeft, 1);
}

void ClariSynthEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (30); // title

    const int labelH = 20;

    // Reserve the bottom for the pitch-tracking row (header + knobs)
    auto pitchArea = area.removeFromBottom (150);
    pitchSectionHeader = pitchArea.removeFromTop (20);
    area.removeFromBottom (10); // gap between sections

    // Top section: 8 harmonic sliders + odd/even + tilt + mix = 11 columns
    const int numCols = 11;
    const int colW    = area.getWidth() / numCols;

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

    // Bottom section: 4 pitch rotary knobs
    juce::Slider* pitchSliders[] = { &pitchSmoothSlider, &pitchSlewSlider, &pitchMinSlider, &pitchMaxSlider };
    juce::Label*  pitchLabels[]  = { &pitchSmoothLabel,  &pitchSlewLabel,  &pitchMinLabel,  &pitchMaxLabel  };

    const int pitchColW = pitchArea.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto pcol = pitchArea.removeFromLeft (pitchColW);
        pitchLabels[i]->setBounds (pcol.removeFromTop (labelH));
        pitchSliders[i]->setBounds (pcol);
    }
}
