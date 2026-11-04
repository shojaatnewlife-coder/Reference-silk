// reference · Silk — tests of the RED / BLUE engine (no JUCE)
#include "../Source/core/SilkCore.h"
#include "SynthMix.h"
#include <chrono>
#include <cstdio>

using namespace ref;
static int failures = 0;
#define CHECK(cond, ...) do { const bool ok_ = (cond); std::printf ("  %s  ", ok_ ? "ok  " : "FAIL"); std::printf (__VA_ARGS__); std::printf ("\n"); if (! ok_) ++failures; } while (0)

static std::vector<float> runMono (SilkEngine& e, const std::vector<float>& in)
{
    std::vector<float> l = in, r = in;
    for (size_t p = 0; p < l.size(); p += 512)
    {
        const int n = (int) std::min ((size_t) 512, l.size() - p);
        float* c[2] = { l.data() + p, r.data() + p };
        e.process (c, 2, n);
    }
    return l;
}

static double toneDb (const std::vector<float>& x, size_t from, double f, double fs)
{
    double re = 0, im = 0;
    const size_t n = x.size() - from;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * kPi * (double) i / (double) n);
        re += w * x[from + i] * std::cos (2 * kPi * f * (double) i / fs);
        im += w * x[from + i] * std::sin (2 * kPi * f * (double) i / fs);
    }
    return 20.0 * std::log10 (std::sqrt (re * re + im * im) / ((double) n * 0.25) + 1e-15);
}

static std::vector<float> sine (double f, double db, double fs, double sec)
{
    std::vector<float> v ((size_t) (fs * sec));
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (dbToGain (db) * std::sin (2 * kPi * f * (double) i / fs));
    return v;
}

struct Thd { double h2, h3, thd; };
static Thd measure (double f, double db, float red, float blue, double fs)
{
    SilkEngine e;
    e.prepare (fs);
    SilkEngine::Settings s;
    s.red = red; s.blue = blue;
    e.set (s);
    e.reset();
    const auto o = runMono (e, sine (f, db, fs, 2.0));
    const size_t from = (size_t) fs;
    const double h1 = toneDb (o, from, f, fs);
    double sum = 0;
    for (int k = 2; k <= 9; ++k)
        if (f * k < fs * 0.45) sum += std::pow (10.0, (toneDb (o, from, f * k, fs) - h1) / 10.0);
    return { toneDb (o, from, 2 * f, fs) - h1, toneDb (o, from, 3 * f, fs) - h1, 100.0 * std::sqrt (sum) };
}

static double rmsDb (const std::vector<float>& x, size_t from)
{
    double a = 0;
    for (size_t i = from; i < x.size(); ++i) a += (double) x[i] * x[i];
    return 10.0 * std::log10 (a / (double) (x.size() - from) + 1e-20);
}

int main (int argc, char** argv)
{
    const double fs = 48000;
    const bool table = argc > 1;
    (void) argv;

    std::printf ("[1] At 0 %% (or OFF) the sound is untouched\n");
    {
        SilkEngine e;
        e.prepare (fs);
        SilkEngine::Settings s;
        s.red = 0; s.blue = 0;
        e.set (s);
        e.reset();
        const auto in = sine (1000.0, -6.0, fs, 1.0);
        const auto o = runMono (e, in);
        double err = 0;
        for (size_t i = 2000; i < in.size(); ++i) err = std::max (err, std::abs ((double) o[i] - in[i - (size_t) SilkEngine::latency()]));
        CHECK (err < 2e-4, "RED 0 %% + BLUE 0 %%: output = input (delayed %d samples), max error %.1e", SilkEngine::latency(), err);
        SilkEngine::Settings off;
        off.red = 100; off.blue = 100; off.redOn = false; off.blueOn = false;
        e.set (off);
        e.reset();
        const auto o2 = runMono (e, in);
        double err2 = 0;
        for (size_t i = 2000; i < in.size(); ++i) err2 = std::max (err2, std::abs ((double) o2[i] - in[i - (size_t) SilkEngine::latency()]));
        CHECK (err2 < 2e-4, "both OFF at 100 %%: untouched (max error %.1e)", err2);
        SilkEngine::Settings pw;
        pw.red = 100; pw.blue = 100; pw.power = false;
        e.set (pw);
        e.reset();
        const auto o3 = runMono (e, in);
        double err3 = 0;
        for (size_t i = 100; i < in.size(); ++i) err3 = std::max (err3, std::abs ((double) o3[i] - in[i - (size_t) SilkEngine::latency()]));
        CHECK (! (err3 > 0.0), "POWER off: bit-exact, same latency (max error %.1e)", err3);
    }

    std::printf ("[2] Harmonics at 100 %% (sine at -12 dBFS, a loud instrument)\n");
    for (float a : { 10.0f, 50.0f, 100.0f })
    {
        const Thd b = measure (100.0, -12.0, 0, a, fs);
        std::printf ("        BLUE %3.0f %%  100 Hz: THD %.2f %%  H2 %.1f dB  H3 %.1f dB\n", a, b.thd, b.h2, b.h3);
        const Thd r = measure (8000.0, -12.0, a, 0, fs);
        std::printf ("        RED  %3.0f %%  8 kHz : THD %.2f %%  H2 %.1f dB  H3 %.1f dB\n", a, r.thd, r.h2, r.h3);
    }
    {
        const Thd b10 = measure (100.0, -12.0, 0, 10, fs), b100 = measure (100.0, -12.0, 0, 100, fs);
        const Thd r10 = measure (8000.0, -12.0, 10, 0, fs), r100 = measure (8000.0, -12.0, 100, 0, fs);
        CHECK (b10.h3 > b10.h2 && b100.h2 > b100.h3, "BLUE: low setting mostly 3rd harmonic, high setting mostly 2nd (like the hardware)");
        CHECK (r100.h2 > r100.h3 + 20.0 && r10.thd < 0.6 * r100.thd, "RED: even (2nd) sheen that grows steadily with the knob (10 %% THD %.2f %%, 100 %% THD %.2f %%)", r10.thd, r100.thd);
        CHECK (b100.thd > 0.5 && b100.thd < 3.0, "BLUE 100 %%: hardware-like amount, about 1 %% (THD %.2f %%)", b100.thd);
        CHECK (r100.thd > 2.0 && r100.thd < 8.0, "RED 100 %%: clear sheen, mostly 2nd harmonic, like the hardware driven (THD %.2f %%)", r100.thd);
        const Thd rQuiet = measure (8000.0, -24.0, 100, 0, fs);
        CHECK (rQuiet.thd > 0.2 && rQuiet.thd < r100.thd, "RED follows the level, still there on quiet highs (8 kHz at -24 dBFS: THD %.2f %%)", rQuiet.thd);
        const Thd rMid = measure (1000.0, -12.0, 100, 0, fs), rLow = measure (100.0, -12.0, 100, 0, fs);
        CHECK (r100.thd > 50.0 * rMid.thd && r100.thd > 50.0 * rLow.thd,
               "RED leans to the highs: 8 kHz %.2f %% > 1 kHz %.2f %% >= 100 Hz %.2f %%", r100.thd, rMid.thd, rLow.thd);
        const Thd bHi = measure (3000.0, -12.0, 0, 100, fs), b40 = measure (40.0, -12.0, 0, 100, fs);
        CHECK (b100.thd > 5.0 * bHi.thd && b40.thd >= b100.thd,
               "BLUE leans to the lows: 40 Hz %.2f %% >= 100 Hz %.2f %% > 3 kHz %.2f %%", b40.thd, b100.thd, bHi.thd);
        const Thd hot = measure (1000.0, -1.0, 100, 100, fs);
        CHECK (hot.thd < 12.0, "both at 100 %% on a -1 dBFS signal: still musical (THD %.2f %%)", hot.thd);
    }
    std::printf ("[2a] No harshness, no mud\n");
    {
        // RED: two loud tones in the bite zone must not make a mid-range difference tone (the harsh part of saturation)
        SilkEngine e;
        e.prepare (fs);
        SilkEngine::Settings s;
        s.red = 100; s.blue = 0;
        e.set (s);
        e.reset();
        auto t = sine (3000.0, -9.0, fs, 1.5);
        const auto t2 = sine (4000.0, -9.0, fs, 1.5);
        for (size_t i = 0; i < t.size(); ++i) t[i] += t2[i];
        const auto o = runMono (e, t);
        const double f1 = toneDb (o, (size_t) fs / 2, 3000.0, fs);
        const double d1k = toneDb (o, (size_t) fs / 2, 1000.0, fs) - f1, s7k = toneDb (o, (size_t) fs / 2, 7000.0, fs) - f1;
        const double h8k = toneDb (o, (size_t) fs / 2, 8000.0, fs) - f1, h6k = toneDb (o, (size_t) fs / 2, 6000.0, fs) - f1;
        CHECK (d1k < -75.0 && std::max ({ s7k, h6k, h8k }) < -60.0, "RED: 3 kHz + 4 kHz add nothing at 1 kHz (%.0f dB) and nothing in the 6 - 8 kHz bite zone (%.0f dB)", d1k, std::max ({ s7k, h6k, h8k }));
        // BLUE: a loud 60 Hz note keeps its harmonics out of the 400 Hz - 1 kHz mud zone
        SilkEngine b;
        b.prepare (fs);
        SilkEngine::Settings sb;
        sb.red = 0; sb.blue = 100;
        b.set (sb);
        b.reset();
        const auto ob = runMono (b, sine (60.0, -6.0, fs, 1.5));
        const double h1 = toneDb (ob, (size_t) fs / 2, 60.0, fs);
        double warm = -200, mud = -200;
        for (int k = 2; k <= 4; ++k) warm = std::max (warm, toneDb (ob, (size_t) fs / 2, 60.0 * k, fs) - h1);
        for (int k = 7; k <= 16; ++k) mud = std::max (mud, toneDb (ob, (size_t) fs / 2, 60.0 * k, fs) - h1);
        CHECK (warm > -45.0 && mud < -70.0, "BLUE: warmth in 120 - 240 Hz (%.0f dB), nothing in the mud zone 420 Hz - 1 kHz (%.0f dB)", warm, mud);
        // RED rounds off quick spikes in the bite zone, but leaves steady sound alone
        SilkEngine r2;
        r2.prepare (fs);
        r2.set (s);
        r2.reset();
        std::vector<float> clicks ((size_t) fs * 2, 0.0f);
        for (size_t p = (size_t) fs / 4; p + 2000 < clicks.size(); p += (size_t) fs / 4)
            for (size_t i = 0; i < 400; ++i) clicks[p + i] = (float) (0.5 * std::exp (-(double) i / 60.0) * std::sin (2 * kPi * 7200.0 * (double) i / fs));
        const auto oc = runMono (r2, clicks);
        double pin = 0, pout = 0; // energy of the spikes
        for (size_t i = 0; i + 40 < clicks.size(); ++i) { pin += (double) clicks[i] * clicks[i]; pout += (double) oc[i + 23] * oc[i + 23]; }
        pin = std::sqrt (pin); pout = std::sqrt (pout);
        const double steady = toneDb (runMono (r2, sine (7200.0, -20.0, fs, 1.0)), (size_t) fs / 2, 7200.0, fs) + 20.0;
        CHECK (gainToDb (pout / pin) - steady < -1.5 && std::abs (steady) < 3.5, "RED: a sharp 7.2 kHz spike is rounded off by %.1f dB more than a steady 7.2 kHz tone (%+.1f dB)", steady - gainToDb (pout / pin), steady);
    }
    std::printf ("[2b] The transformer's response change (small signal, -40 dBFS)\n");
    {
        auto gainAt = [&] (double f, float red, float blue) {
            SilkEngine e;
            e.prepare (fs);
            SilkEngine::Settings s;
            s.red = red; s.blue = blue;
            e.set (s);
            e.reset();
            const auto o = runMono (e, sine (f, -40.0, fs, 1.0));
            return toneDb (o, (size_t) fs / 2, f, fs) + 40.0;
        };
        const double r16 = gainAt (12000, 100, 0), r1k = gainAt (1000, 100, 0), r100 = gainAt (100, 100, 0);
        const double b50 = gainAt (50, 0, 100), b1k = gainAt (1000, 0, 100), b10k = gainAt (10000, 0, 100);
        std::printf ("        RED 100 %%:  100 Hz %+.2f  1 kHz %+.2f  12 kHz %+.2f dB\n", r100, r1k, r16);
        std::printf ("        BLUE 100 %%:  50 Hz %+.2f  1 kHz %+.2f  10 kHz %+.2f dB\n", b50, b1k, b10k);
        CHECK (r16 - r1k > 3.0 && r16 - r1k < 6.5 && std::abs (r100) < 0.3, "RED: open, smooth top (%.2f dB at 12 kHz), lows unchanged", r16 - r1k);
        CHECK (b50 - b1k > 2.0 && b50 - b1k < 5.0 && std::abs (b10k - b1k) < 0.3, "BLUE: warm, full bottom (%.2f dB at 50 Hz), highs unchanged", b50 - b1k);
    }
    std::printf ("[2c] Clean top end: no aliasing\n");
    {
        SilkEngine e;
        e.prepare (fs);
        SilkEngine::Settings s;
        s.red = 100; s.blue = 100;
        e.set (s);
        e.reset();
        const double f = 9000.0;
        const auto o = runMono (e, sine (f, -6.0, fs, 1.0));
        const double h1 = toneDb (o, (size_t) fs / 2, f, fs);
        // the 3rd harmonic (27 kHz) folds to 21 kHz, the 4th (36 kHz) to 12 kHz, the 5th to 3 kHz
        const double a3 = toneDb (o, (size_t) fs / 2, fs - 3 * f, fs) - h1, a4 = toneDb (o, (size_t) fs / 2, 4 * f - fs, fs) - h1,
                     a5 = toneDb (o, (size_t) fs / 2, 5 * f - fs, fs) - h1;
        CHECK (std::max ({ a3, a4, a5 }) < -60.0, "9 kHz at -6 dBFS: folded tones %.0f / %.0f / %.0f dB (oversampled)", a3, a4, a5);
    }

    std::printf ("[3] Level on a full song (only a gentle trim: like the hardware, Silk may sound a touch fuller)\n");
    {
        reftest::MixOptions mo;
        mo.seconds = 20;
        const auto song = reftest::synthMix (mo);
        auto runSong = [&] (float red, float blue, bool redOn = true, bool blueOn = true) {
            SilkEngine e;
            e.prepare (fs);
            SilkEngine::Settings s;
            s.red = red; s.blue = blue; s.redOn = redOn; s.blueOn = blueOn;
            e.set (s);
            e.reset();
            std::vector<float> l = song.l, r = song.r;
            for (size_t p = 0; p < l.size(); p += 512)
            {
                const int n = (int) std::min ((size_t) 512, l.size() - p);
                float* c[2] = { l.data() + p, r.data() + p };
                e.process (c, 2, n);
            }
            return std::make_pair (l, r);
        };
        const auto ref0 = runSong (0, 0);
        const double r0 = rmsDb (ref0.first, (size_t) fs) + rmsDb (ref0.second, (size_t) fs);
        double worst = 0;
        for (float a : { 25.0f, 50.0f, 75.0f, 100.0f })
        {
            const auto ob = runSong (0, a), orr = runSong (a, 0), both = runSong (a, a);
            const double db = 0.5 * (rmsDb (ob.first, (size_t) fs) + rmsDb (ob.second, (size_t) fs) - r0);
            const double dr = 0.5 * (rmsDb (orr.first, (size_t) fs) + rmsDb (orr.second, (size_t) fs) - r0);
            const double dbo = 0.5 * (rmsDb (both.first, (size_t) fs) + rmsDb (both.second, (size_t) fs) - r0);
            std::printf ("        %3.0f %%: BLUE %+.2f dB  RED %+.2f dB  both %+.2f dB\n", a, db, dr, dbo);
            worst = std::max ({ worst, std::abs (db), std::abs (dr), std::abs (dbo) });
        }
        if (! table)
            CHECK (worst < 1.6, "level within 1.6 dB at every setting (worst %.2f dB)", worst);
        // how much Silk actually adds to the song (the difference to the dry song)
        auto addedDb = [&] (float red, float blue) {
            const auto o = runSong (red, blue);
            const int L = SilkEngine::latency();
            double es = 0, ed = 0;
            for (size_t i = (size_t) fs; i < o.first.size(); ++i)
            {
                const double d = song.l[i - (size_t) L];
                es += d * d;
                ed += (o.first[i] - d) * (o.first[i] - d);
            }
            return 10.0 * std::log10 (ed / es);
        };
        const double rA = addedDb (100, 0), rH = addedDb (50, 0), bA = addedDb (0, 100);
        std::printf ("        added to the song: RED 50 %% %.1f dB, RED 100 %% %.1f dB, BLUE 100 %% %.1f dB\n", rH, rA, bA);
        CHECK (rA > -34.0, "RED 100 %% is clearly audible on a song (changes it by %.1f dB)", rA);
    }

    std::printf ("[4] Robust and light\n");
    {
        bool fin = true;
        for (double r : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            SilkEngine e;
            e.prepare (r);
            SilkEngine::Settings s;
            s.red = 100; s.blue = 100;
            e.set (s);
            auto x = sine (60.0, 12.0, r, 0.5);
            x[100] = NAN; x[200] = INFINITY;
            const auto o = runMono (e, x);
            for (float v : o) fin = fin && std::isfinite (v) && std::abs (v) < 10.0f;
        }
        CHECK (fin, "44.1 - 192 kHz, +12 dB hot, NaN / inf in: finite output");
        SilkEngine e;
        e.prepare (fs);
        SilkEngine::Settings s;
        s.red = 70; s.blue = 70;
        e.set (s);
        const auto x = sine (220.0, -10.0, fs, 10.0);
        std::vector<float> l = x, r = x;
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t p = 0; p < l.size(); p += 256)
        {
            float* c[2] = { l.data() + p, r.data() + p };
            e.process (c, 2, (int) std::min ((size_t) 256, l.size() - p));
        }
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        CHECK (ms < 10000.0 * 0.05, "CPU: 10 s of stereo audio in %.0f ms (%.2f %% of one core)", ms, ms / 100.0);
        // switching ON / OFF while playing: no click
        SilkEngine e2;
        e2.prepare (fs);
        auto t = sine (100.0, -10.0, fs, 2.0);
        double prev = 0, step = 0;
        for (size_t p = 0; p < t.size(); p += 512)
        {
            SilkEngine::Settings st;
            st.red = 100; st.blue = 100;
            st.blueOn = (p / 512) % 20 < 10;
            st.power = (p / 512) % 34 < 30;
            e2.set (st);
            std::vector<float> a (t.begin() + (long) p, t.begin() + (long) std::min (t.size(), p + 512)), b = a;
            float* c[2] = { a.data(), b.data() };
            e2.process (c, 2, (int) a.size());
            for (float v : a) { step = std::max (step, std::abs (v - prev)); prev = v; }
        }
        CHECK (step < 0.006, "switching ON / OFF and POWER while playing: no click (largest step %.4f, the sine itself steps 0.004)", step);
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
