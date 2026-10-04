#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg      { 0xff13111c };
    const juce::Colour panel   { 0xff1e1a2e };
    const juce::Colour accent  { 0xff8a6bff };
    const juce::Colour accent2 { 0xffff6b9a };
    const juce::Colour text    { 0xffe9e6f5 };
}

class NoteView : public juce::Component
{
public:
    explicit NoteView (ChordForgeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    ChordForgeProcessor& proc;
};

class DragPad : public juce::Component
{
public:
    explicit DragPad (ChordForgeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    ChordForgeProcessor& proc;
    juce::File file;
    bool dragging = false;
};

class ChordForgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChordForgeEditor (ChordForgeProcessor&);
    ~ChordForgeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void addLabeled (juce::Label& l, const juce::String& t, juce::Component& c);

    ChordForgeProcessor& proc;
    juce::ComboBox keyBox, modeBox, richBox;
    juce::Slider strumSlider, bpmSlider;
    juce::ToggleButton bassToggle { "Bass note" }, soundToggle { "Preview sound" };
    juce::TextButton newBtn { "NEW IDEA" }, playBtn { "PLAY" };
    juce::Label lKey, lMode, lRich, lStrum, lBpm;
    NoteView noteView;
    DragPad dragPad;

    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<CA> keyA, modeA, richA;
    std::unique_ptr<SA> strumA, bpmA;
    std::unique_ptr<BA> bassA, soundA;
    int lastVersion = -1;
};
