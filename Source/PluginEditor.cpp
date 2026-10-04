#include "PluginEditor.h"

void NoteView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 8.0f);

    const auto prog = proc.getProgression();
    if (prog.notes.empty()) return;

    int lo = 127, hi = 0;
    for (auto& n : prog.notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
    const float rowH = r.getHeight() / (float) (hi - lo + 3);

    for (int bar = 0; bar < 8; ++bar)
    {
        const float x = r.getX() + r.getWidth() * (float) bar / 8.0f;
        g.setColour (juce::Colours::white.withAlpha (bar == 0 ? 0.0f : 0.08f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    }
    for (auto& n : prog.notes)
    {
        const float x = r.getX() + r.getWidth() * (float) (n.start / 32.0);
        const float w = std::max (2.0f, r.getWidth() * (float) (n.length / 32.0) - 1.0f);
        const float y = r.getBottom() - rowH * (float) (n.pitch - lo + 2);
        g.setColour ((n.isBass ? ui::accent2 : ui::accent).withAlpha (0.35f + 0.65f * (float) n.velocity / 127.0f));
        g.fillRoundedRectangle (x, y, w, std::max (2.0f, rowH - 1.0f), 2.0f);
    }
    if (proc.isPlaying())
    {
        const float x = r.getX() + r.getWidth() * (float) (proc.getPlayBeat() / 32.0);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    }
}

void DragPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setGradientFill (juce::ColourGradient (ui::accent, 0, 0, ui::accent2, r.getWidth(), 0, false));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText ("DRAG THIS MIDI INTO FL STUDIO  (piano roll / playlist / channel)", getLocalBounds(),
                juce::Justification::centred);
}

void DragPad::mouseDown (const juce::MouseEvent&)
{
    file = proc.exportMidi();
    dragging = false;
}

void DragPad::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 6 && file.existsAsFile())
    {
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    }
}

ChordForgeEditor::ChordForgeEditor (ChordForgeProcessor& p)
    : AudioProcessorEditor (&p), proc (p), noteView (p), dragPad (p)
{
    setSize (780, 500);

    keyBox.addItemList (cf::keyNames(), 1);
    modeBox.addItemList ({ "Major", "Minor" }, 1);
    richBox.addItemList ({ "Simple", "Jazzy (7ths)", "Lush (9ths)" }, 1);

    for (auto* s : { &strumSlider, &bpmSlider })
    {
        s->setSliderStyle (juce::Slider::LinearBar);
        s->setColour (juce::Slider::trackColourId, ui::accent.withAlpha (0.6f));
        s->setColour (juce::Slider::textBoxTextColourId, ui::text);
    }
    bpmSlider.setTextValueSuffix (" bpm");

    for (auto* c : std::initializer_list<juce::Component*> { &keyBox, &modeBox, &richBox, &strumSlider, &bpmSlider })
    {
        c->setColour (juce::ComboBox::backgroundColourId, ui::panel);
        c->setColour (juce::ComboBox::textColourId, ui::text);
        c->setColour (juce::ComboBox::outlineColourId, ui::accent.withAlpha (0.4f));
        addAndMakeVisible (*c);
    }
    addLabeled (lKey, "KEY", keyBox);
    addLabeled (lMode, "MODE", modeBox);
    addLabeled (lRich, "RICHNESS", richBox);
    addLabeled (lStrum, "STRUM", strumSlider);
    addLabeled (lBpm, "TEMPO", bpmSlider);

    for (auto* t : { &bassToggle, &soundToggle })
    {
        t->setColour (juce::ToggleButton::textColourId, ui::text);
        t->setColour (juce::ToggleButton::tickColourId, ui::accent2);
        addAndMakeVisible (*t);
    }
    for (auto* b : { &newBtn, &playBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, ui::panel);
        b->setColour (juce::TextButton::textColourOffId, ui::text);
        addAndMakeVisible (*b);
    }
    newBtn.onClick = [this] { proc.newIdea(); };
    playBtn.onClick = [this] { proc.setPlaying (! proc.isPlaying()); };

    addAndMakeVisible (noteView);
    addAndMakeVisible (dragPad);

    keyA = std::make_unique<CA> (proc.apvts, "key", keyBox);
    modeA = std::make_unique<CA> (proc.apvts, "mode", modeBox);
    richA = std::make_unique<CA> (proc.apvts, "richness", richBox);
    strumA = std::make_unique<SA> (proc.apvts, "strum", strumSlider);
    bpmA = std::make_unique<SA> (proc.apvts, "bpm", bpmSlider);
    bassA = std::make_unique<BA> (proc.apvts, "bass", bassToggle);
    soundA = std::make_unique<BA> (proc.apvts, "sound", soundToggle);

    startTimerHz (25);
}

ChordForgeEditor::~ChordForgeEditor() { stopTimer(); }

void ChordForgeEditor::addLabeled (juce::Label& l, const juce::String& t, juce::Component& c)
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, ui::text.withAlpha (0.6f));
    l.attachToComponent (&c, false);
    addAndMakeVisible (l);
}

void ChordForgeEditor::timerCallback()
{
    playBtn.setButtonText (proc.isPlaying() ? "STOP" : "PLAY");
    noteView.repaint();
    if (proc.getVersion() != lastVersion || proc.isPlaying())
    {
        lastVersion = proc.getVersion();
        repaint (0, 120, getWidth(), 56);
    }
}

void ChordForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("CHORDFORGE", 20, 10, 300, 32, juce::Justification::centredLeft);
    g.setColour (ui::text.withAlpha (0.5f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("expressive 8-bar progressions  |  seed " + juce::String (proc.getSeed()),
                20, 38, 400, 16, juce::Justification::centredLeft);

    const auto prog = proc.getProgression();
    const int cur = proc.isPlaying() ? (int) (proc.getPlayBeat() / 4.0) : -1;
    const float w = (getWidth() - 40) / 8.0f;
    for (int i = 0; i < prog.chordNames.size() && i < 8; ++i)
    {
        juce::Rectangle<float> r (20 + w * (float) i + 2, 122.0f, w - 4, 48.0f);
        g.setColour (i == cur ? ui::accent : ui::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (ui::text);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText (prog.chordNames[i], r, juce::Justification::centred);
    }
}

void ChordForgeEditor::resized()
{
    auto r = getLocalBounds().reduced (20);
    r.removeFromTop (60);
    auto row = r.removeFromTop (30);
    const int w = (row.getWidth() - 4 * 12) / 5;
    for (auto* c : std::initializer_list<juce::Component*> { &keyBox, &modeBox, &richBox, &strumSlider, &bpmSlider })
    {
        c->setBounds (row.removeFromLeft (w));
        row.removeFromLeft (12);
    }
    r.removeFromTop (14 + 56);               // chord strip drawn in paint()
    noteView.setBounds (r.removeFromTop (180));
    r.removeFromTop (12);
    auto ctl = r.removeFromTop (34);
    newBtn.setBounds (ctl.removeFromLeft (150));
    ctl.removeFromLeft (10);
    playBtn.setBounds (ctl.removeFromLeft (110));
    ctl.removeFromLeft (20);
    bassToggle.setBounds (ctl.removeFromLeft (120));
    soundToggle.setBounds (ctl.removeFromLeft (140));
    r.removeFromTop (12);
    dragPad.setBounds (r.removeFromTop (56));
}
