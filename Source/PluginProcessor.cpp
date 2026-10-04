#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout ChordForgeProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", cf::keyNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Major", "Minor" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "richness", 1 }, "Richness",
                                                   StringArray { "Simple", "Jazzy (7ths)", "Lush (9ths)" }, 1));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "strum", 1 }, "Strum", 0, 30, 6));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "bass", 1 }, "Bass", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "sound", 1 }, "Preview Sound", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "BPM",
                                                  NormalisableRange<float> (60.f, 180.f, 1.f), 100.f));
    return l;
}

ChordForgeProcessor::ChordForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    seed = juce::Random::getSystemRandom().nextInt (100000) + 1;
    for (auto id : { "key", "mode", "richness", "strum", "bass" })
        apvts.addParameterListener (id, this);

    for (int i = 0; i < 16; ++i) synth.addVoice (new PadVoice());
    synth.addSound (new PadSound());
    regenerate();
}

ChordForgeProcessor::~ChordForgeProcessor()
{
    cancelPendingUpdate();
    for (auto id : { "key", "mode", "richness", "strum", "bass" })
        apvts.removeParameterListener (id, this);
}

void ChordForgeProcessor::prepareToPlay (double sr, int)
{
    synth.setCurrentPlaybackSampleRate (sr);
}

void ChordForgeProcessor::regenerate()
{
    auto prog = cf::generate ((int) *apvts.getRawParameterValue ("key"),
                              (int) *apvts.getRawParameterValue ("mode") == 1,
                              (int) *apvts.getRawParameterValue ("richness"),
                              seed.load(),
                              (int) *apvts.getRawParameterValue ("strum"),
                              *apvts.getRawParameterValue ("bass") > 0.5f);

    std::vector<Ev> ev;
    for (auto& n : prog.notes)
    {
        ev.push_back ({ n.start, true, n.pitch, n.velocity });
        ev.push_back ({ std::min (n.start + n.length, kLoop - 0.01), false, n.pitch, 0 });
    }
    std::stable_sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b)
                      { return a.beat != b.beat ? a.beat < b.beat : (! a.on && b.on); });
    {
        juce::SpinLock::ScopedLockType sl (lock);
        progression = std::move (prog);
        events = std::move (ev);
    }
    ++version;
}

cf::Progression ChordForgeProcessor::getProgression() const
{
    juce::SpinLock::ScopedLockType sl (lock);
    return progression;
}

void ChordForgeProcessor::newIdea()
{
    seed = juce::Random::getSystemRandom().nextInt (100000) + 1;
    regenerate();
}

void ChordForgeProcessor::setPlaying (bool shouldPlay)
{
    if (shouldPlay) restart = true;
    playing = shouldPlay;
}

void ChordForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear();
    const int n = buffer.getNumSamples();
    const bool nowPlaying = playing.load();

    if (wasPlaying && ! nowPlaying)
    {
        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        synth.allNotesOff (1, true);
    }
    wasPlaying = nowPlaying;

    if (nowPlaying)
    {
        if (restart.exchange (false))
        {
            playBeat = 0.0;
            midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            synth.allNotesOff (1, false);
        }
        const double bps = (double) *apvts.getRawParameterValue ("bpm") / 60.0 / getSampleRate();
        const double b0 = playBeat, b1 = b0 + n * bps;

        juce::SpinLock::ScopedTryLockType tl (lock);
        if (tl.isLocked())
        {
            auto emit = [&] (double from, double to, double baseSample)
            {
                for (auto& e : events)
                    if (e.beat >= from && e.beat < to)
                    {
                        const int pos = juce::jlimit (0, n - 1, (int) ((e.beat - from) / bps + baseSample));
                        midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.pitch, (juce::uint8) e.vel)
                                            : juce::MidiMessage::noteOff (1, e.pitch), pos);
                    }
            };
            if (b1 <= kLoop) emit (b0, b1, 0.0);
            else { emit (b0, kLoop, 0.0); emit (0.0, b1 - kLoop, (kLoop - b0) / bps); }
        }
        playBeat = std::fmod (b1, kLoop);
        uiBeat = playBeat;
    }

    if (*apvts.getRawParameterValue ("sound") > 0.5f)
        synth.renderNextBlock (buffer, midi, 0, n);
}

juce::File ChordForgeProcessor::exportMidi()
{
    const auto prog = getProgression();
    const int ppq = 480;
    const int bpm = (int) *apvts.getRawParameterValue ("bpm");

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (60000000 / juce::jmax (1, bpm)), 0.0);
    for (auto& n : prog.notes)
    {
        seq.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) n.velocity), n.start * ppq);
        seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch), (n.start + n.length) * ppq);
    }
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ChordForge");
    dir.createDirectory();
    const auto key = cf::keyNames()[(int) *apvts.getRawParameterValue ("key")];
    const auto mode = (int) *apvts.getRawParameterValue ("mode") == 1 ? "minor" : "major";
    auto file = dir.getChildFile ("ChordForge " + key + " " + mode + " #" + juce::String (seed.load()) + ".mid");
    file.deleteFile();
    if (auto os = file.createOutputStream())
        mf.writeTo (*os);
    return file;
}

void ChordForgeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("seed", seed.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ChordForgeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            seed = (int) state.getProperty ("seed", seed.load());
            apvts.replaceState (state);
            regenerate();
        }
    }
}

juce::AudioProcessorEditor* ChordForgeProcessor::createEditor() { return new ChordForgeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChordForgeProcessor(); }
