// reference · Kick Sculpt — synthetic "problem kicks" for the tests: a pitch-dropping sine body,
// a 100-200 Hz partial that rings on (piles up with a finger bass), a boxy low-mid ring, a click.
#pragma once

#include "../Source/core/DspUtil.h"

#include <random>
#include <vector>

namespace reftest
{
struct KickOptions
{
    double fs = 48000;
    double seconds = 30;
    double bpm = 120;          // four on the floor
    double f0 = 52, fStart = 170, pitchMs = 28;
    double decayMs = 170;      // body decay (time constant)
    double pileHz = 150, pileDb = -9, pileDecayMs = 220;   // ringing 100-200 Hz partial (0 Hz = none)
    double boxHz = 330, boxDb = -17, boxDecayMs = 110;
    double clickDb = -14;      // click burst level
    double levelDb = -6;       // peak-ish level
    double jitterDb = 2;       // hit-to-hit level variation
    unsigned seed = 3;
};

inline std::vector<float> synthKick (const KickOptions& o, std::vector<size_t>* hitsOut = nullptr)
{
    using namespace ref;
    const double fs = o.fs;
    const size_t total = (size_t) (o.seconds * fs);
    std::vector<float> out (total, 0.0f);
    std::mt19937 rng (o.seed);
    std::uniform_real_distribution<double> uni (-1.0, 1.0);
    const size_t step = (size_t) (60.0 / o.bpm * fs);
    for (size_t start = (size_t) (0.05 * fs); start + 16 < total; start += step)
    {
        if (hitsOut) hitsOut->push_back (start);
        const double g = dbToGain (o.levelDb + uni (rng) * o.jitterDb * 0.5);
        const size_t len = std::min (total - start, (size_t) (0.9 * fs));
        double ph = 0, php = 0, phb = 0;
        Biquad hp, bp;
        hp.c = design::highpass (fs, 1500.0, 0.7071);
        bp.c = design::peak (fs, 4000.0, 1.0, 6.0);
        for (size_t i = 0; i < len; ++i)
        {
            const double t = (double) i / fs, tm = t * 1000.0;
            const double f = o.f0 + (o.fStart - o.f0) * std::exp (-tm / o.pitchMs);
            ph += 2 * kPi * f / fs;
            const double atk = std::min (1.0, tm / 0.6);
            double v = atk * std::sin (ph) * std::exp (-tm / o.decayMs);
            if (o.pileHz > 0)
            {
                php += 2 * kPi * o.pileHz / fs;
                v += atk * dbToGain (o.pileDb) * std::sin (php) * std::exp (-tm / o.pileDecayMs);
            }
            if (o.boxHz > 0)
            {
                phb += 2 * kPi * o.boxHz / fs;
                v += atk * dbToGain (o.boxDb) * std::sin (phb) * std::exp (-tm / o.boxDecayMs);
            }
            if (tm < 4.0)
                v += dbToGain (o.clickDb) * 3.0 * bp.process (hp.process (uni (rng))) * std::exp (-tm / 1.2);
            out[start + i] += (float) (g * v);
        }
    }
    return out;
}
} // namespace reftest
