#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class ClariSynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit ClariSynthEditor (ClariSynthProcessor& p);
    ~ClariSynthEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ClariSynthProcessor& processor;

    // One slider + label per parameter
    juce::Slider oddEvenSlider, centroidSlider, mixSlider;
    juce::Label  oddEvenLabel, centroidLabel, mixLabel;

    std::array<juce::Slider, 8> harmonicSliders;
    std::array<juce::Label,  8> harmonicLabels;

    // Pitch-tracking controls (rotary)
    juce::Slider pitchSmoothSlider, pitchSlewSlider, pitchMinSlider, pitchMaxSlider;
    juce::Label  pitchSmoothLabel, pitchSlewLabel, pitchMinLabel, pitchMaxLabel;
    juce::Rectangle<int> pitchSectionHeader;  // bounds of the "Pitch Tracking" heading, set in resized()

    // Attachments keep sliders in sync with APVTS
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> oddEvenAttach, centroidAttach, mixAttach;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> harmonicAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchSmoothAttach, pitchSlewAttach, pitchMinAttach, pitchMaxAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClariSynthEditor)
};
