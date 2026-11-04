// reference · Master Silk
// Small, allocation-free DSP building blocks shared by the engine and the analyser.
// Nothing in this folder depends on JUCE so it can be unit-tested on any platform.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace ref
{
constexpr double kPi = 3.14159265358979323846;

inline double dbToGain (double db) noexcept { return std::pow (10.0, db * 0.05); }
inline double gainToDb (double g, double floorDb = -150.0) noexcept
{
    return g > 0.0 ? std::max (floorDb, 20.0 * std::log10 (g)) : floorDb;
}
inline double powerToDb (double p, double floorDb = -150.0) noexcept
{
    return p > 0.0 ? std::max (floorDb, 10.0 * std::log10 (p)) : floorDb;
}
template <typename T> inline T clampv (T v, T lo, T hi) noexcept { return std::min (hi, std::max (lo, v)); }
template <typename T> inline T lerp (T a, T b, T t) noexcept { return a + (b - a) * t; }

/** One-pole coefficient for a time constant in milliseconds at a given rate. */
inline double timeCoeff (double ms, double rate) noexcept
{
    return ms <= 0.0 ? 0.0 : std::exp (-1.0 / (ms * 0.001 * rate));
}

// ---------------------------------------------------------------------------------------------
struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

/** RBJ audio-EQ-cookbook designs. Frequencies are clamped safely below Nyquist. */
namespace design
{
    inline double safeFreq (double fs, double f) { return clampv (f, 5.0, fs * 0.49); }

    inline BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    inline BiquadCoeffs lowpass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * safeFreq (fs, f) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
    }
    inline BiquadCoeffs highpass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * safeFreq (fs, f) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
    }
    /** Band-pass with 0 dB peak gain. */
    inline BiquadCoeffs bandpass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * safeFreq (fs, f) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise (al, 0, -al, 1 + al, -2 * cw, 1 - al);
    }
    inline BiquadCoeffs peak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * safeFreq (fs, f) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
    }
    inline BiquadCoeffs lowShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * safeFreq (fs, f) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double sa = 2.0 * std::sqrt (A) * al;
        return normalise (A * ((A + 1) - (A - 1) * cw + sa), 2 * A * ((A - 1) - (A + 1) * cw),
                          A * ((A + 1) - (A - 1) * cw - sa), (A + 1) + (A - 1) * cw + sa,
                          -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sa);
    }
    inline BiquadCoeffs highShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * safeFreq (fs, f) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double sa = 2.0 * std::sqrt (A) * al;
        return normalise (A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw),
                          A * ((A + 1) + (A - 1) * cw - sa), (A + 1) - (A - 1) * cw + sa,
                          2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa);
    }
} // namespace design

/** Transposed direct form II biquad, double precision state. */
struct Biquad
{
    BiquadCoeffs c;
    double z1 = 0, z2 = 0;

    inline double process (double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

/** Magnitude response of a biquad at frequency f (for tests and UI). */
inline double biquadMagnitude (const BiquadCoeffs& c, double fs, double f)
{
    const double w = 2.0 * kPi * f / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    return std::abs ((c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2));
}

/** Attack / release envelope follower on a non-negative input. */
struct Envelope
{
    double att = 0, rel = 0, y = 0;
    void setup (double rate, double attackMs, double releaseMs)
    {
        att = timeCoeff (attackMs, rate);
        rel = timeCoeff (releaseMs, rate);
    }
    inline double process (double x) noexcept
    {
        const double c = x > y ? att : rel;
        y = x + c * (y - x);
        return y;
    }
    void reset (double v = 0) noexcept { y = v; }
};

/** DC blocker (one-zero / one-pole high-pass around 8 Hz). */
struct DcBlocker
{
    double r = 0.999, x1 = 0, y1 = 0;
    void setup (double fs) { r = std::exp (-2.0 * kPi * 8.0 / fs); }
    inline double process (double x) noexcept
    {
        const double y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0; }
};

// ---------------------------------------------------------------------------------------------
/** Iterative radix-2 complex FFT. All memory is allocated in prepare(). */
class FFT
{
public:
    void prepare (int size)
    {
        n = size;
        logN = 0;
        while ((1 << logN) < n)
            ++logN;
        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < logN; ++b)
                if (i & (1 << b))
                    r |= 1 << (logN - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
            tw[(size_t) i] = std::polar (1.0f, (float) (-2.0 * kPi * i / n));
    }

    int size() const noexcept { return n; }

    /** In-place transform. Inverse is unscaled. */
    void perform (std::complex<float>* d, bool inverse) const noexcept
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i])
                std::swap (d[i], d[rev[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    std::complex<float> w = tw[(size_t) (j * step)];
                    if (inverse)
                        w = std::conj (w);
                    const std::complex<float> u = d[i + j], v = d[i + j + half] * w;
                    d[i + j] = u + v;
                    d[i + j + half] = u - v;
                }
        }
    }

private:
    int n = 0, logN = 0;
    std::vector<int> rev;
    std::vector<std::complex<float>> tw;
};

// ---------------------------------------------------------------------------------------------
/** 2x oversampler with a linear-phase windowed-sinc half-band low-pass (latency kLatency). */
class Oversampler2x
{
public:
    static constexpr int kTaps = 47;
    static constexpr int kLatency = (kTaps - 1) / 2; // in base-rate samples (up + down)

    Oversampler2x()
    {
        // Kaiser-windowed sinc, cutoff 0.23 of the oversampled rate (≈ 0.46 fs base).
        const double fc = 0.23, beta = 8.0;
        auto bessel0 = [] (double x) {
            double s = 1, t = 1;
            for (int k = 1; k < 30; ++k)
            {
                t *= (x / (2.0 * k)) * (x / (2.0 * k));
                s += t;
            }
            return s;
        };
        double sum = 0;
        for (int i = 0; i < kTaps; ++i)
        {
            const double m = i - (kTaps - 1) / 2.0;
            const double sinc = std::abs (m) < 1e-9 ? 2 * fc : std::sin (2 * kPi * fc * m) / (kPi * m);
            const double r = 2.0 * i / (kTaps - 1) - 1.0;
            const double win = bessel0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / bessel0 (beta);
            h[i] = sinc * win;
            sum += h[i];
        }
        for (double& v : h)
            v /= sum;
        reset();
    }

    void reset()
    {
        std::fill (std::begin (upLine), std::end (upLine), 0.0);
        std::fill (std::begin (downLine), std::end (downLine), 0.0);
        upPos = downPos = 0;
    }

    /** Produces two oversampled samples from one input sample. */
    inline void upsample (double x, double& o0, double& o1) noexcept
    {
        o0 = upStep (2.0 * x);
        o1 = upStep (0.0);
    }

    /** Consumes two oversampled samples, returns one base-rate sample. */
    inline double downsample (double i0, double i1) noexcept
    {
        pushDown (i0);
        double acc = 0;
        int p = downPos;
        for (int k = 0; k < kTaps; ++k)
        {
            p = (p == 0 ? kTaps - 1 : p - 1);
            acc += h[k] * downLine[p];
        }
        pushDown (i1);
        return acc;
    }

private:
    inline double upStep (double x) noexcept
    {
        upLine[upPos] = x;
        upPos = (upPos + 1) % kTaps;
        double acc = 0;
        int p = upPos;
        for (int k = 0; k < kTaps; ++k)
        {
            p = (p == 0 ? kTaps - 1 : p - 1);
            acc += h[k] * upLine[p];
        }
        return acc;
    }
    inline void pushDown (double x) noexcept
    {
        downLine[downPos] = x;
        downPos = (downPos + 1) % kTaps;
    }

    double h[kTaps] {};
    double upLine[kTaps] {}, downLine[kTaps] {};
    int upPos = 0, downPos = 0;
};

/** Fast, smooth tanh approximation (accurate to ~1e-4, monotonic, saturates at ±1). */
inline double fastTanh (double x) noexcept
{
    if (x > 4.97) return 1.0;
    if (x < -4.97) return -1.0;
    const double x2 = x * x;
    const double a = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)));
    const double b = 135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0));
    return clampv (a / b, -1.0, 1.0);
}

/** Simple fixed-size histogram over dB values (0.5 dB resolution, -120..+20 dB). */
struct DbHistogram
{
    static constexpr int kBins = 280;
    static constexpr double kMin = -120.0, kStep = 0.5;
    uint32_t c[kBins] {};
    uint64_t total = 0;

    void reset() { std::fill (std::begin (c), std::end (c), 0u); total = 0; }
    void add (double db) noexcept
    {
        const int i = clampv ((int) ((db - kMin) / kStep), 0, kBins - 1);
        ++c[i];
        ++total;
    }
    double percentile (double p) const noexcept
    {
        if (total == 0) return kMin;
        const double target = p * (double) total;
        double acc = 0;
        for (int i = 0; i < kBins; ++i)
        {
            acc += c[i];
            if (acc >= target) return kMin + (i + 0.5) * kStep;
        }
        return kMin + kBins * kStep;
    }
};
} // namespace ref
