// reference · Master Silk — a synthetic stereo "song" for the tests: four-on-the-floor kick,
// a finger bass, Latin / Gulf style hand percussion that pushes 200 Hz - 1 kHz (doum ~250 Hz,
// tek ~700 Hz with a ringing body), a sung-like vocal (harmonics through formants) in the centre,
// wide shakers / hats, and one narrow resonance at 1.8 kHz that keeps ringing.
#pragma once

#include "SynthBass.h"
#include "SynthKick.h"

namespace reftest
{
struct MixOptions
{
    double fs = 48000;
    double seconds = 40;
    double percDb = -14;        // hand percussion level (the 200 Hz - 1 kHz pusher)
    double vocalDb = -20;
    double hatsDb = -30;
    double resoDb = -34;        // the 1.8 kHz ring (0 = off is -200)
    double kickDb = -8;
    double bassDb = -16;
    double width = 0.6;         // how far percussion / hats are panned
    unsigned seed = 11;
};

struct StereoBuffer
{
    std::vector<float> l, r;
    size_t size() const { return l.size(); }
};

/** Percussion only (for the BODY test). Returns hit start positions. */
inline StereoBuffer synthPercussion (const MixOptions& o, std::vector<size_t>* hits = nullptr)
{
    using namespace ref;
    const double fs = o.fs;
    const size_t total = (size_t) (o.seconds * fs);
    StereoBuffer b { std::vector<float> (total, 0.0f), std::vector<float> (total, 0.0f) };
    std::mt19937 rng (o.seed + 1);
    std::uniform_real_distribution<double> uni (-1.0, 1.0);
    const double beat = 60.0 / 120.0 * fs;
    // a 16th pattern: doum on 1 and 2.5, tek elsewhere (darbuka / conga feel)
    const int pattern[16] = { 2, 0, 1, 1, 0, 1, 2, 0, 1, 0, 1, 1, 2, 1, 0, 1 };
    size_t step = 0;
    for (double t = 0.02 * fs; t + 16 < total; t += beat / 4.0, ++step)
    {
        const int kind = pattern[step % 16];
        if (kind == 0) continue;
        const size_t start = (size_t) t;
        if (hits) hits->push_back (start);
        const bool doum = kind == 2;
        const double f = doum ? 250.0 : 700.0, dec = doum ? 120.0 : 70.0;
        const double g = dbToGain (o.percDb + (doum ? 0.0 : -2.0) + uni (rng) * 1.5);
        const double pan = doum ? 0.0 : (step % 2 ? o.width : -o.width) * 0.6;
        const double gl = std::sqrt (0.5 * (1 - pan)), gr = std::sqrt (0.5 * (1 + pan));
        Biquad body, slap;
        body.c = design::bandpass (fs, f, doum ? 4.0 : 6.0);
        slap.c = design::bandpass (fs, doum ? 900.0 : 2400.0, 1.2);
        const size_t len = std::min (total - start, (size_t) (0.4 * fs));
        double ph = 0;
        for (size_t i = 0; i < len; ++i)
        {
            const double tm = i * 1000.0 / fs;
            ph += 2 * kPi * f * (1.0 + 0.04 * std::exp (-tm / 10.0)) / fs;
            const double exc = uni (rng) * std::exp (-tm / 2.5);
            double v = 0.7 * std::sin (ph) * std::exp (-tm / dec) * std::min (1.0, tm / 0.5) + 2.5 * body.process (exc) + 0.8 * slap.process (exc);
            v *= g;
            b.l[start + i] += (float) (v * gl * 1.41);
            b.r[start + i] += (float) (v * gr * 1.41);
        }
    }
    return b;
}

inline StereoBuffer synthMix (const MixOptions& o)
{
    using namespace ref;
    const double fs = o.fs;
    const size_t total = (size_t) (o.seconds * fs);

    KickOptions ko;
    ko.fs = fs;
    ko.seconds = o.seconds;
    ko.levelDb = o.kickDb;
    const auto kick = synthKick (ko);
    SynthOptions bo;
    bo.fs = fs;
    bo.seconds = o.seconds;
    bo.levelDb = o.bassDb;
    const auto bass = synthBass (bo);
    StereoBuffer mix = synthPercussion (o);

    std::mt19937 rng (o.seed);
    std::uniform_real_distribution<double> uni (-1.0, 1.0);

    // vocal: a sawtooth-ish voice with vibrato through three formants, phrases with gaps
    Biquad f1, f2, f3, hat;
    f1.c = design::bandpass (fs, 650.0, 3.0);
    f2.c = design::bandpass (fs, 1500.0, 4.0);
    f3.c = design::bandpass (fs, 2800.0, 5.0);
    hat.c = design::highpass (fs, 7000.0, 0.7071);
    const double notes[] = { 220.0, 246.9, 261.6, 293.7, 329.6, 293.7, 261.6, 246.9 };
    double vph = 0, rph = 0;
    const double vg = dbToGain (o.vocalDb), hg = dbToGain (o.hatsDb), rg = dbToGain (o.resoDb);
    const double eighth = 60.0 / 120.0 * fs / 2.0;
    Envelope hatEnv;
    hatEnv.setup (fs, 0.1, 35.0);
    for (size_t i = 0; i < total; ++i)
    {
        const double t = (double) i / fs;
        const int phrase = (int) (t / 2.0);
        const bool singing = std::fmod (t, 4.0) < 3.2;
        const double f0 = notes[phrase % 8] * (1.0 + 0.006 * std::sin (2 * kPi * 5.5 * t));
        vph += f0 / fs;
        vph -= std::floor (vph);
        double saw = 0;
        for (int h = 1; h <= 24; ++h)
            saw += std::sin (2 * kPi * h * vph) / h;
        const double env = singing ? std::min (1.0, std::fmod (t, 4.0) / 0.05) * std::min (1.0, (3.2 - std::fmod (t, 4.0)) / 0.08) : 0.0;
        const double voc = vg * env * (1.6 * f1.process (saw) + 1.1 * f2.process (saw) + 0.7 * f3.process (saw) + 0.08 * saw);

        // hats / shakers on eighths, panned wide
        const bool trig = std::fmod ((double) i, eighth) < 1.0;
        const double he = hatEnv.process (trig ? 1.0 : 0.0);
        const double hn = hg * he * hat.process (uni (rng));
        const int side = ((int) (i / (size_t) eighth)) % 2 ? 1 : -1;

        // a narrow 1.8 kHz resonance that rings all the time (a room / instrument problem)
        rph += 2 * kPi * 1800.0 / fs;
        const double res = rg * std::sin (rph) * (0.7 + 0.3 * std::sin (2 * kPi * 0.3 * t));

        const double c = kick[i] + bass[i] + voc + res;
        mix.l[i] += (float) (c + hn * (1.0 + o.width * side) * 0.7);
        mix.r[i] += (float) (c + hn * (1.0 - o.width * side) * 0.7);
    }
    return mix;
}
} // namespace reftest
