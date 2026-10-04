#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace cf
{
struct NoteEvent { int pitch; double start; double length; int velocity; bool isBass; };
struct Progression { std::vector<NoteEvent> notes; juce::StringArray chordNames; };

enum Type { Maj, BMaj, Min, Dom, Dim, Sus };
struct Step { int off; Type type; };

inline const juce::StringArray& keyNames()
{
    static const juce::StringArray k { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return k;
}

// Hand-written 8-bar skeletons: semitones above the tonic + chord type.
// BMaj = borrowed / non-diatonic major (add9 colour, never maj7).
inline const std::vector<std::vector<Step>>& bank (bool minor)
{
    static const std::vector<std::vector<Step>> major = {
        {{0,Maj},{7,Dom},{9,Min},{5,Maj},{0,Maj},{4,Min},{5,Maj},{7,Sus}},
        {{0,Maj},{4,Min},{9,Min},{5,Maj},{2,Min},{7,Dom},{0,Maj},{7,Sus}},
        {{5,Maj},{4,Min},{2,Min},{0,Maj},{9,Min},{2,Min},{7,Dom},{0,Maj}},
        {{0,Maj},{5,Maj},{5,Min},{0,Maj},{9,Min},{2,Dom},{7,Dom},{0,Maj}},
        {{0,Maj},{10,BMaj},{5,Maj},{0,Maj},{9,Min},{8,BMaj},{5,Maj},{7,Dom}},
        {{2,Min},{7,Dom},{0,Maj},{9,Min},{2,Min},{7,Dom},{4,Min},{9,Dom}},
        {{9,Min},{5,Maj},{0,Maj},{7,Dom},{9,Min},{5,Maj},{2,Min},{7,Sus}} };
    static const std::vector<std::vector<Step>> minor_ = {
        {{0,Min},{8,Maj},{3,Maj},{10,BMaj},{0,Min},{5,Min},{10,BMaj},{7,Dom}},
        {{0,Min},{10,BMaj},{8,Maj},{10,BMaj},{0,Min},{5,Min},{8,Maj},{7,Dom}},
        {{0,Min},{5,Min},{10,BMaj},{3,Maj},{8,Maj},{2,Dim},{7,Dom},{0,Min}},
        {{0,Min},{3,Maj},{5,Min},{8,Maj},{0,Min},{10,BMaj},{8,Maj},{7,Dom}},
        {{0,Min},{5,Min},{8,Maj},{1,BMaj},{0,Min},{3,Maj},{10,BMaj},{7,Dom}},
        {{8,Maj},{10,BMaj},{0,Min},{0,Min},{8,Maj},{10,BMaj},{3,Maj},{7,Dom}},
        {{0,Min},{7,Min},{8,Maj},{5,Min},{0,Min},{7,Dom},{8,Maj},{7,Dom}} };
    return minor ? minor_ : major;
}

// richness 0 = triads, 1 = 7ths, 2 = 9ths
inline const std::vector<int>& intervals (Type t, int r)
{
    static const std::vector<int> table[6][3] = {
        { {0,4,7}, {0,4,7,11},  {0,4,7,11,14} },
        { {0,4,7}, {0,4,7,14},  {0,4,7,14}    },
        { {0,3,7}, {0,3,7,10},  {0,3,7,10,14} },
        { {0,4,7}, {0,4,7,10},  {0,4,7,10,14} },
        { {0,3,6}, {0,3,6,10},  {0,3,6,10}    },
        { {0,5,7}, {0,5,7,10},  {0,5,7,10,14} } };
    return table[t][r];
}

inline juce::String suffix (Type t, int r)
{
    static const char* s[6][3] = { {"","maj7","maj9"}, {"","add9","add9"}, {"m","m7","m9"},
                                   {"","7","9"}, {"dim","m7b5","m7b5"}, {"sus4","7sus4","9sus4"} };
    return s[t][r];
}

// Pick the inversion/octave closest to the previous chord (smooth voice leading).
inline std::vector<int> pickVoicing (const std::vector<int>& pcs, const std::vector<int>& prev,
                                     juce::Random& rng, bool allowDrop)
{
    std::vector<std::vector<int>> cands;
    const int n = (int) pcs.size();
    for (int k = 0; k < n; ++k)
    {
        std::vector<int> rot;
        for (int j = 0; j < n; ++j) rot.push_back (pcs[(size_t) ((k + j) % n)]);
        for (int start = 50; start < 68; ++start)
        {
            if (start % 12 != rot[0]) continue;
            std::vector<int> notes { start };
            for (int j = 1; j < n; ++j)
            {
                int m = notes.back() + 1;
                while (m % 12 != rot[(size_t) j]) ++m;
                notes.push_back (m);
            }
            cands.push_back (notes);
            if (allowDrop && n >= 4)
            {
                auto d = notes;
                d[(size_t) n - 2] -= 12;
                std::sort (d.begin(), d.end());
                cands.push_back (d);
            }
        }
    }

    std::vector<int> pv = prev;
    std::sort (pv.begin(), pv.end());

    auto score = [&] (const std::vector<int>& v)
    {
        double sum = 0; for (int x : v) sum += x;
        const double mean = sum / (double) v.size();
        const int mn = *std::min_element (v.begin(), v.end());
        const int mx = *std::max_element (v.begin(), v.end());
        double s = std::abs (mean - 64) * 0.6;
        s += std::max (0, (mx - mn) - 19) * 3;
        s += std::max (0, 52 - mn) * 3 + std::max (0, mx - 77) * 3;
        if (! pv.empty())
        {
            auto a = v; std::sort (a.begin(), a.end());
            if (a.size() == pv.size())
                for (size_t i = 0; i < a.size(); ++i) s += std::abs (a[i] - pv[i]);
            else
            {
                double ps = 0; for (int x : pv) ps += x;
                s += std::abs (mean - ps / (double) pv.size()) * (double) a.size();
            }
        }
        return s + rng.nextFloat() * 2.0;
    };

    size_t best = 0; double bestScore = 1e9;
    for (size_t i = 0; i < cands.size(); ++i)
    {
        const double sc = score (cands[i]);
        if (sc < bestScore) { bestScore = sc; best = i; }
    }
    return cands[best];
}

inline Progression generate (int key, bool minor, int rich, int seed, int strumAmt, bool bass)
{
    static const std::vector<std::pair<double,double>> pats[4] = {
        { {0,4} }, { {0,2},{2,2} }, { {0,2.5},{2.5,1.5} }, { {0,1.5},{1.5,1},{2.5,1.5} } };

    juce::Random rng (seed);
    const auto& b = bank (minor);
    const auto& prog = b[(size_t) rng.nextInt ((int) b.size())];
    const double strum = strumAmt / 96.0;

    Progression out;
    std::vector<int> prev;

    for (int bar = 0; bar < 8; ++bar)
    {
        const Step st = prog[(size_t) bar];
        const int root = (key + st.off) % 12;
        std::vector<int> pcs;
        for (int i : intervals (st.type, rich)) pcs.push_back ((root + i) % 12);

        auto voicing = pickVoicing (pcs, prev, rng, rich >= 1);
        prev = voicing;
        out.chordNames.add (keyNames()[root] + suffix (st.type, rich));

        int pi = 0;
        if (bar == 7)       pi = 0;
        else if (rich == 0) pi = rng.nextInt (2);
        else if (rich == 1) pi = rng.nextInt (3);
        else                pi = rng.nextInt (4);

        for (auto [s, l] : pats[pi])
        {
            const bool offBeat = std::fmod (s, 1.0) != 0.0;
            for (size_t i = 0; i < voicing.size(); ++i)
            {
                NoteEvent n;
                n.pitch = voicing[i];
                n.start = bar * 4.0 + s + (double) i * strum + rng.nextFloat() * strum * 0.5;
                n.length = std::max (0.25, l * 0.97 - (double) i * strum);
                int vel = 80 + rng.nextInt (15) - 7 - (offBeat ? 7 : 0);
                if (i == voicing.size() - 1) vel += 7;
                n.velocity = juce::jlimit (20, 127, vel);
                n.isBass = false;
                out.notes.push_back (n);
            }
        }

        if (bass)
        {
            NoteEvent n { 36 + root, bar * 4.0, 3.92, 88 + rng.nextInt (9) - 4, true };
            out.notes.push_back (n);
        }
    }
    return out;
}
} // namespace cf
