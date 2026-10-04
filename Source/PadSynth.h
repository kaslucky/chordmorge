#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Tiny built-in pad so you can audition the progression without loading a synth.
struct PadSound : juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

struct PadVoice : juce::SynthesiserVoice
{
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<PadSound*> (s) != nullptr; }

    void startNote (int note, float vel, juce::SynthesiserSound*, int) override
    {
        freq = juce::MidiMessage::getMidiNoteInHertz (note);
        level = vel * 0.12f;
        adsr.setSampleRate (getSampleRate());
        adsr.setParameters ({ 0.04f, 0.5f, 0.7f, 0.6f });
        adsr.noteOn();
        p1 = 0.0; p2 = 0.3; lp = 0.0f;
    }
    void stopNote (float, bool allowTail) override
    {
        if (allowTail) adsr.noteOff();
        else { adsr.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;
        const double sr = getSampleRate();
        for (int i = 0; i < num; ++i)
        {
            const float env = adsr.getNextSample();
            if (! adsr.isActive()) { clearCurrentNote(); break; }
            const float s = (float) ((2.0 * p1 - 1.0) + (2.0 * p2 - 1.0)) * 0.5f;
            p1 += freq * 1.004 / sr; if (p1 >= 1.0) p1 -= 1.0;
            p2 += freq * 0.996 / sr; if (p2 >= 1.0) p2 -= 1.0;
            lp += 0.12f * (s - lp);
            const float o = lp * env * level;
            for (int c = 0; c < out.getNumChannels(); ++c)
                out.addSample (c, start + i, o);
        }
    }

    juce::ADSR adsr;
    double freq = 440.0, p1 = 0.0, p2 = 0.0;
    float level = 0.1f, lp = 0.0f;
};
