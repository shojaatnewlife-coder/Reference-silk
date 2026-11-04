// reference · Bass Sculpt — synthetic "problem bass" lines for the tests.
// Finger bass: plucked-string partials with uneven note levels, sharp finger/fret clicks,
// a boxy low-mid bump and a narrow body resonance. 808: pitched sine kick-bass with a click.
#pragma once

#include "../Source/core/DspUtil.h"

#include <random>
#include <vector>

namespace reftest
{
struct NoteInfo
{
    size_t start = 0, length = 0;
    double freq = 0, gain = 1;
};

struct SynthOptions
{
    double fs = 48000;
    double seconds = 40;
    bool eight08 = false;
    double unevenDb = 9.0;      // spread of the note levels (sub you can't trust)
    double clickDb = 0.0;       // extra level of the attack clicks
    double resonanceHz = 520;   // narrow resonance (0 = none)
    double resonanceDb = 8.0;
    double mudDb = 5.0;         // boxy low-mid bump around 300 Hz
    double levelDb = -14.0;     // average level
    unsigned seed = 7;
};

inline std::vector<float> synthBass (const SynthOptions& o, std::vector<NoteInfo>* notesOut = nullptr)
{
    using namespace ref;
    const double fs = o.fs;
    const size_t total = (size_t) (o.seconds * fs);
    std::vector<float> out (total, 0.0f);
    std::mt19937 rng (o.seed);
    std::uniform_real_distribution<double> uni (0.0, 1.0);
    std::normal_distribution<double> gauss (0.0, 1.0);

    // a simple groove over E1 .. A2
    const double scale[] = { 41.2, 46.25, 49.0, 55.0, 61.74, 65.41, 73.42, 82.41, 98.0, 110.0 };
    const double noteSec = o.eight08 ? 0.75 : 0.375;
    size_t pos = 0;
    int idx = 0;
    while (pos + (size_t) (0.1 * fs) < total)
    {
        NoteInfo ni;
        ni.start = pos;
        ni.length = std::min ((size_t) (noteSec * fs * (uni (rng) < 0.25 ? 2.0 : 1.0)), total - pos);
        ni.freq = scale[(idx * 3 + (int) (uni (rng) * 4)) % 10];
        if (o.eight08) ni.freq = scale[(idx * 2 + (int) (uni (rng) * 3)) % 6];
        ni.gain = dbToGain ((uni (rng) - 0.5) * o.unevenDb);
        ++idx;

        const size_t len = ni.length;
        if (! o.eight08)
        {
            // plucked string: partials with frequency-dependent decay, slight inharmonicity
            for (int h = 1; h <= 60; ++h)
            {
                const double f = ni.freq * h * std::sqrt (1.0 + 0.00008 * h * h);
                if (f > fs * 0.45) break;
                const double amp = 0.9 / std::pow ((double) h, 1.05) * (h == 1 ? 1.2 : 1.0);
                const double decay = 1.5 + 0.9 * h; // per second
                const double ph = uni (rng) * 2 * kPi;
                for (size_t i = 0; i < len; ++i)
                {
                    const double t = (double) i / fs;
                    const double rel = i > len - (size_t) (0.01 * fs) ? (double) (len - i) / (0.01 * fs) : 1.0;
                    out[pos + i] += (float) (ni.gain * amp * std::exp (-decay * t) * rel * std::sin (2 * kPi * f * t + ph));
                }
            }
            // finger / fret click: short, bright noise burst
            Biquad hp; hp.c = design::highpass (fs, 1800.0, 0.7);
            const size_t clickLen = (size_t) (0.004 * fs);
            const double cg = ni.gain * 0.35 * dbToGain (o.clickDb);
            for (size_t i = 0; i < clickLen && pos + i < total; ++i)
                out[pos + i] += (float) (cg * hp.process (gauss (rng)) * std::exp (-(double) i / (0.0012 * fs)));
        }
        else
        {
            // 808: sine with a fast pitch drop, long decay, soft saturation and a click
            double ph = 0;
            for (size_t i = 0; i < len; ++i)
            {
                const double t = (double) i / fs;
                const double f = ni.freq * (1.0 + 1.5 * std::exp (-t / 0.02));
                ph += 2 * kPi * f / fs;
                const double rel = i > len - (size_t) (0.01 * fs) ? (double) (len - i) / (0.01 * fs) : 1.0;
                const double env = std::exp (-t / 0.9) * rel;
                out[pos + i] += (float) (ni.gain * std::tanh (1.6 * std::sin (ph)) * env * 0.8);
            }
            Biquad hp; hp.c = design::highpass (fs, 2500.0, 0.7);
            const size_t clickLen = (size_t) (0.003 * fs);
            for (size_t i = 0; i < clickLen && pos + i < total; ++i)
                out[pos + i] += (float) (ni.gain * 0.3 * dbToGain (o.clickDb) * hp.process (gauss (rng)) * std::exp (-(double) i / (0.0008 * fs)));
        }
        if (notesOut) notesOut->push_back (ni);
        pos += ni.length;
    }

    // the room / instrument problems
    Biquad mud, res;
    mud.c = design::peak (fs, 300.0, 1.2, o.mudDb);
    res.c = o.resonanceHz > 0 ? design::peak (fs, o.resonanceHz, 9.0, o.resonanceDb) : BiquadCoeffs {};
    double sq = 0;
    for (auto& v : out)
    {
        v = (float) res.process (mud.process (v));
        sq += (double) v * v;
    }
    const double g = dbToGain (o.levelDb - powerToDb (sq / (double) out.size()));
    for (auto& v : out) v = (float) (v * g);
    return out;
}
} // namespace reftest
