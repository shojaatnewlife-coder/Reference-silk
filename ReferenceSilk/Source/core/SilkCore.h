// reference · Silk v2 — the RED / BLUE Silk + Texture of the Portico II, and nothing else.
//
// What the hardware does (RND manuals, Sound On Sound, Tape Op):
//   Silk reduces the negative feedback around the output transformer, so the transformer's own
//   character comes through. RED lets it through in the upper mids and highs, BLUE in the lows and
//   low mids. Texture sets how much: from almost nothing (mostly 3rd harmonic) up to a clear colour
//   (mostly 2nd harmonic). The harder the transformer is driven, the more you hear. Reviewers: RED
//   "opens the top end without any harshness or bite", BLUE "adds weight and density to the bottom".
//
// Why v1.1 did not sound like it: far too much distortion (up to 6 % even at normal levels) placed in
// the 2 - 5 kHz harsh zone, mid-range intermodulation, only ~1.5 dB of response change, and a full
// loudness compensation that hid most of what was left.
//
// The v2 model, per channel, at 2x the sample rate:
//   RED  1. less feedback at the top = a wide, smooth lift of the air band (shelf ~9 kHz, Q 0.55)
//        2. transformer harmonics from the highs only (> 2.5 kHz), mostly 2nd at high Texture; the
//           mid-range difference tones (what makes saturation sound harsh) are filtered out, the
//           harmonics land above ~5 kHz as sheen
//        3. the transformer's "silk": quick 2.5 - 5 kHz spikes are rounded off (a few dB, only on the
//           spikes, relative to the music's own level), so more Texture = clearer, never harsher
//   BLUE 1. less feedback at the bottom = a resonant low shelf (~80 Hz) whose natural dip, plus a
//           small wide cut at ~300 Hz, keeps the low mids clean (no mud)
//        2. transformer core saturation: driven by the magnetic flux (the signal's integral), so the
//           deepest notes saturate most, exactly like a real core; it rounds the sub peaks (tight,
//           dense low end) and adds low-order warmth, filtered so it never fills the 300 Hz - 1 kHz mud
// At 0 % nothing is added, the sound is the input exactly.
#pragma once

#include "DspUtil.h"

namespace ref
{
/** Transformer-style curve: unity slope at rest, soft saturation, bias for even harmonics. */
struct SilkCurve
{
    double d = 1.0, bias = 0.0, off = 0.0, norm = 1.0;

    void set (double drive, double biasAmount) noexcept
    {
        d = drive;
        bias = biasAmount;
        off = fastTanh (d * bias);
        const double sech = 1.0 - off * off; // tanh' at the bias point
        norm = 1.0 / (d * std::max (0.2, sech));
    }
    inline double operator() (double u) const noexcept { return (fastTanh (d * (u + bias)) - off) * norm; }
};

/** Fast one-pole low-pass (used for envelopes and the flux integrator). */
struct OnePole
{
    double a = 0, y = 0;
    void setHz (double fs, double hz) { a = std::exp (-2.0 * kPi * hz / fs); }
    inline double process (double x) noexcept { y = x + a * (y - x); return y; }
    void reset() noexcept { y = 0; }
};

// =============================================================================================
/** RED Silk: the upper mids and highs. Runs at the oversampled rate. */
class RedSilk
{
public:
    // ---- voicing (all at Texture 100 %) ----
    // v2.2: one smooth, continuous rise from ~1.5 kHz to the top: presence around 2 kHz is lifted (never
    // cut), 6 - 8 kHz only gently, most of the lift is the air above 10 kHz. The sheen harmonics are made
    // from the band above 5 kHz only, so they land above ~10 kHz (silk), never on the 6 - 8 kHz bite.
    static constexpr double kPresHz = 1800.0, kPresQ = 0.5, kPresDb = 2.2;    // presence and clarity
    static constexpr double kAirHz = 12000.0, kAirQ = 0.6, kAirDb = 3.8;     // air: silk and shine
    static constexpr double kHarmHpHz = 5500.0;                               // only the top makes harmonics
    static constexpr double kHarmClean = 9000.0;                              // harmonics land above this
    static constexpr double kHarmTop = 19000.0;                               // round off the very top of the harmonics
    static constexpr double kSheen = 0.5;                                     // sheen drive (level following)
    static constexpr double kHarmGain = 1.0;
    static constexpr double kSpikeHz = 7200.0, kSpikeQ = 0.9;                  // the "bite" zone (s, cymbals)
    static constexpr double kSpikeMaxDb = 4.0;                                // most a spike is rounded off

    void prepare (double rate)
    {
        fs = rate;
        pres.c = design::highShelf (fs, kPresHz, kPresQ, kPresDb);
        air.c = design::highShelf (fs, kAirHz, kAirQ, kAirDb);
        hp1.c = design::highpass (fs, kHarmHpHz, 0.707);
        hp2.c = design::highpass (fs, kHarmHpHz, 0.707);
        hp3.c = design::highpass (fs, kHarmHpHz, 0.707);
        clean1.c = design::highpass (fs, kHarmClean, 0.707);
        clean2.c = design::highpass (fs, kHarmClean, 0.707);
        clean3.c = design::highpass (fs, kHarmClean, 0.707);
        top.c = design::lowpass (fs, kHarmTop, 0.707);
        spike.c = design::bandpass (fs, kSpikeHz, kSpikeQ);
        fast.setup (fs, 0.2, 25.0);
        slow.setup (fs, 40.0, 350.0);
        bandEnv.setup (fs, 15.0, 250.0);
        dc.setup (fs);
        reset();
    }
    void reset()
    {
        for (auto* b : { &pres, &air, &hp1, &hp2, &hp3, &clean1, &clean2, &clean3, &top, &spike })
            b->reset();
        fast.reset();
        slow.reset();
        bandEnv.reset();
        dc.reset();
        lastAmount = -1.0;
    }

    /** amount 0..1 (Texture, smoothed by the caller). Returns the processed sample. */
    inline double process (double x, double amount) noexcept
    {
        if (std::abs (amount - lastAmount) > 1e-6)
        {
            lastAmount = amount;
            // low Texture: symmetric (mostly 3rd), high Texture: biased (mostly 2nd)
            curve.set (1.0 + 0.4 * amount, 0.3 * (0.5 + 0.5 * amount)); // even, progressive sheen all along the knob
            sheen = kSheen * (0.6 + 0.4 * amount);
        }
        // 1. the open high mids and top (parallel blend keeps it smooth while the knob moves)
        const double toneDelta = air.process (pres.process (x)) - x;

        // 2. harmonics of the highs, with the harsh in-between tones removed. The drive follows the
        //    band's level at half the rate (2:1), so the sheen is there on normal mix levels too and still
        //    grows when the transformer is pushed harder.
        const double hb = hp3.process (hp2.process (hp1.process (x)));
        const double env = bandEnv.process (std::abs (hb));
        const double g = sheen / std::sqrt (std::max (env, 1.0e-4));
        const double e = hb * g;
        double h = (curve (e) - e) / g;
        h = top.process (clean3.process (clean2.process (clean1.process (dc.process (h)))));

        // 3. round off only the quick spikes at 6 - 9 kHz (s, cymbal hits), relative to the music's own level
        const double b = spike.process (x);
        const double fe = fast.process (std::abs (b)), se = slow.process (std::abs (b));
        double cut = 0.0;
        if (fe > 1e-5 && fe > se * 1.8)
        {
            const double overDb = gainToDb (fe / (se * 1.8));
            const double grDb = std::min (kSpikeMaxDb, overDb * 0.7);
            cut = 1.0 - dbToGain (-grDb);
        }

        return x + amount * (toneDelta + kHarmGain * h - cut * b);
    }

private:
    double fs = 96000, lastAmount = -1.0, sheen = kSheen;
    Biquad pres, air, hp1, hp2, hp3, clean1, clean2, clean3, top, spike;
    Envelope fast, slow, bandEnv;
    DcBlocker dc;
    SilkCurve curve;
};

// =============================================================================================
/** BLUE Silk: the lows and low mids. Runs at the oversampled rate. */
class BlueSilk
{
public:
    static constexpr double kLowHz = 95.0, kLowQ = 0.7, kLowDb = 3.6;        // the warm, full bottom
    static constexpr double kMudHz = 380.0, kMudQ = 0.9, kMudDb = -1.0;       // keeps it clean
    static constexpr double kCoreLpHz = 220.0;                                // what reaches the core
    static constexpr double kFluxHz = 45.0;                                   // below this the flux stops rising
    static constexpr double kDrive = 0.7;                                     // core drive for 0 dBFS at kFluxHz
    static constexpr double kHarmTop = 700.0;                                 // warmth stays below the mud zone
    static constexpr double kHarmGain = 1.0;

    void prepare (double rate)
    {
        fs = rate;
        low.c = design::lowShelf (fs, kLowHz, kLowQ, kLowDb);
        mud.c = design::peak (fs, kMudHz, kMudQ, kMudDb);
        lp1.c = design::lowpass (fs, kCoreLpHz, 0.707);
        lp2.c = design::lowpass (fs, kCoreLpHz, 0.707);
        top1.c = design::lowpass (fs, kHarmTop, 0.707);
        top2.c = design::lowpass (fs, kHarmTop, 0.707);
        flux.setHz (fs, kFluxHz);
        // the one-pole "integrator" has gain ~ kFluxHz / f above its corner: scale it to 1 at kFluxHz
        fluxNorm = std::sqrt (2.0);
        dc.setup (fs);
        reset();
    }
    void reset()
    {
        for (auto* b : { &low, &mud, &lp1, &lp2, &top1, &top2 })
            b->reset();
        flux.reset();
        dc.reset();
        lastAmount = -1.0;
    }

    inline double process (double x, double amount) noexcept
    {
        if (std::abs (amount - lastAmount) > 1e-6)
        {
            lastAmount = amount;
            curve.set (1.0 + 0.5 * amount, 0.27 * amount * amount * amount);
            drive = kDrive * (0.35 + 0.65 * amount);
        }
        // 1. resonant low shelf + small mud cut
        const double toneDelta = mud.process (low.process (x)) - x;

        // 2. core saturation driven by the flux: deepest notes hit hardest
        const double lb = lp2.process (lp1.process (x));
        const double phi = flux.process (lb) * fluxNorm * drive;
        const double sat = (curve (phi) - phi) / (drive * fluxNorm); // harmonics + sub peak rounding, in flux units
        // back to signal level: the flux-domain error at low notes is scaled by how much that note was integrated
        double h = top2.process (top1.process (dc.process (sat)));

        return x + amount * (toneDelta + kHarmGain * h);
    }

private:
    double fs = 96000, lastAmount = -1.0, drive = kDrive, fluxNorm = 1.0;
    Biquad low, mud, lp1, lp2, top1, top2;
    OnePole flux;
    DcBlocker dc;
    SilkCurve curve;
};

// =============================================================================================
/** The whole plugin's sound: BLUE then RED at 2x, with ramped ON / OFF and power. */
class SilkEngine
{
public:
    struct Settings
    {
        float red = 40, blue = 30; // Texture 0..100 %
        bool redOn = true, blueOn = true, power = true;
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            blue[c].prepare (fs * 2.0);
            red[c].prepare (fs * 2.0);
        }
        smoothCoef = timeCoeff (30.0, fs);
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            blue[c].reset();
            red[c].reset();
            os[c].reset();
        }
        for (auto& d : dry) std::fill (std::begin (d), std::end (d), 0.0);
        dryPos = 0;
        redA = target (settings.red, settings.redOn);
        blueA = target (settings.blue, settings.blueOn);
        wet = settings.power ? 1.0 : 0.0;
    }
    void set (const Settings& s) noexcept { settings = s; }
    static constexpr int latency() { return Oversampler2x::kLatency; }

    /** Partial loudness compensation (like the hardware, Silk may get a touch fuller, never a lot louder). */
    static double blueTrimDb (double a) noexcept { return -0.7 * a; }
    static double redTrimDb (double a) noexcept { return 0.0 * a; }

    void process (float* const* ch, int nc, int n) noexcept
    {
        const double rT = target (settings.red, settings.redOn), bT = target (settings.blue, settings.blueOn);
        const double wT = settings.power ? 1.0 : 0.0;
        nc = std::min (nc, 2);
        for (int i = 0; i < n; ++i)
        {
            redA = rT + smoothCoef * (redA - rT);
            blueA = bT + smoothCoef * (blueA - bT);
            wet = wT + smoothCoef * (wet - wT);
            if (std::abs (redA - rT) < 1e-6) redA = rT;
            if (std::abs (blueA - bT) < 1e-6) blueA = bT;
            if (std::abs (wet - wT) < 1e-6) wet = wT;
            const double g = dbToGain (blueTrimDb (blueA) + redTrimDb (redA));
            for (int c = 0; c < nc; ++c)
            {
                double x = ch[c][i];
                if (! std::isfinite (x)) x = 0.0;
                x = clampv (x, -8.0, 8.0);
                dry[c][dryPos] = x;
                const double dd = dry[c][(dryPos - latency() + 64) & 63];
                // only what Silk changes goes through the oversampler; the dry signal itself stays bit-true
                double a, b;
                os[c].upsample (x, a, b);
                const double da = red[c].process (blue[c].process (a, blueA), redA) - a;
                const double db = red[c].process (blue[c].process (b, blueA), redA) - b;
                double y = (dd + os[c].downsample (da, db)) * g;
                if (! std::isfinite (y)) y = dd;
                ch[c][i] = (float) (dd + wet * (y - dd));
            }
            dryPos = (dryPos + 1) & 63;
        }
    }

private:
    /** Texture taper: the first half of the knob already gives a clear colour. */
    static double target (float pct, bool on) noexcept
    {
        if (! on || ! std::isfinite (pct)) return 0.0;
        return std::pow (clampv ((double) pct, 0.0, 100.0) / 100.0, 0.7);
    }

    double fs = 48000, smoothCoef = 0.99;
    Settings settings;
    BlueSilk blue[2];
    RedSilk red[2];
    Oversampler2x os[2];
    double redA = 0, blueA = 0, wet = 1;
    double dry[2][64] {};
    int dryPos = 0;
};
} // namespace ref
