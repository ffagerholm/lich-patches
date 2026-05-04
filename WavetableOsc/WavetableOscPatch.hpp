#ifndef __WavetableOscPatch_hpp__
#define __WavetableOscPatch_hpp__

/**

AUTHOR:
    Copyright (c) 2025 Andrew Ionascu
    Copyright (c) 2026 Fredrik Fagerholm

    Derived from wavetable_synth.cpp by Andrew Ionascu
    https://github.com/adion12/DaisyWavetable
LICENSE:
    This program is distributed under the MIT licence,
    the same as the original source code.
DESCRIPTION:
    This is a port of the Wavetable oscillator to the Befaco Lich (OWL platform).

    The oscillator is a wavetable oscillator, interpolating between two
    wavetables that are generated randomly. An LFO controls the "position"
    between the two wavetables. A set of random inharmonic frequencies
    can be added.

    The port includes only the wavetable generation and lfo for scanning
    between the tables. The "drum" mode is not included because of the more
    limited controls on the Lich.

    Parameters on the Befaco Lich module:
        PARAMETER_A controls the frequency/tuning of the oscillator.
        PARAMETER_B contorls the gain.
        PARAMETER_C controls the volume of the inharmonic frequencies.
        PARAMETER_D controls the LFO frequency.
        Button A and GATE 1 regenerates wavetable 1.
        Button B and GATE 2 regenerates wavetable 2.
*/

#include "Patch.h"
#include "SineOscillator.h"
#include "VoltsPerOctave.h"
#include "custom_dsp.h"

constexpr uint16_t N = 2048;
constexpr uint16_t H_T = 16;

struct Inharmonic
{
    float freq_factor;
    float amplitude;
    float phase;
    float theta;
};

class WavetableOscPatch : public Patch
{
private:
    const uint16_t H_0 = 16;
    const uint16_t H_1 = 2;
    const int FS = getSampleRate();
    float phase_wt = 0.0f;
    float amp[H_T];
    float phase[H_T];
    float wavetable_0[H_T][N];
    float wavetable_1[H_T][N];
    Inharmonic inharmonics_0[H_T - 1];
    Inharmonic inharmonics_1[H_T - 1];
    uint32_t xorshiftState = 1; // Cannot be initialized to 0
    bool buttonstateA = false;
    bool buttonstateB = false;
    SineOscillator *lfo;
    VoltsPerOctave hz;

public:
    WavetableOscPatch()
    {
        registerParameter(PARAMETER_A, "Frequency");
        registerParameter(PARAMETER_B, "Gain");
        registerParameter(PARAMETER_C, "Inharmonic");
        registerParameter(PARAMETER_D, "LFO Freq");
        generateWavetable(wavetable_0, inharmonics_0, H_0);
        generateWavetable(wavetable_1, inharmonics_1, H_1);
        lfo = SineOscillator::create(FS);
    }

    ~WavetableOscPatch()
    {
        SineOscillator::destroy(lfo);
    }

    float randomFloat01()
    {
        xorshiftState ^= xorshiftState << 13;
        xorshiftState ^= xorshiftState >> 17;
        xorshiftState ^= xorshiftState << 5;
        return (xorshiftState >> 8) * (1.0f / 16777216.0f);
    }

    void generateWavetable(float wavetable[][N], Inharmonic *inharmonics, uint16_t H) // Generate waves up to the H-th harmonic
    {
        uint16_t h, n;
        float theta, theta_inc;
        float x_rms = 0.0f;

        // Generate random amplitudes and phases for H harmonics
        for (h = 0; h < H; h++)
        {
            amp[h] = randomFloat01();
            phase[h] = TWOPI_F * randomFloat01();
            x_rms += (amp[h] * amp[h]);
        }
        x_rms /= 2.0f;
        x_rms = sqrtf(x_rms);

        // Scale all amplitudes by 1/x_rms
        for (h = 0; h < H; h++)
        {
            amp[h] /= x_rms;
        }

        // First harmonic (fundamental)
        theta = phase[0];
        theta_inc = TWOPI_F / N;
        for (n = 0; n < N; n++)
        {
            wavetable[0][n] = amp[0] * sinf(theta);
            theta += theta_inc;
        }

        // Rest of harmonics
        for (h = 1; h < H; h++)
        {
            theta = phase[h];
            theta_inc = TWOPI_F * (h + 1) / N;
            for (n = 0; n < N; n++)
            {
                wavetable[h][n] = (amp[h] * sinf(theta)) + wavetable[h - 1][n];
                theta += theta_inc;
                if (theta > TWOPI_F)
                {
                    theta -= TWOPI_F;
                }
            }
        }

        // Generate random factors, amplitudes, and phases for H-1 inharmonics
        for (h = 0; h < H - 1; h++)
        {
            inharmonics[h].freq_factor = randomFloat01() + h + 1;
            inharmonics[h].amplitude = randomFloat01() / x_rms;
            inharmonics[h].theta = TWOPI_F * randomFloat01();
            inharmonics[h].phase = inharmonics[h].theta;
        }
    }

    void processAudio(AudioBuffer &buffer) override
    {
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);
        int size = buffer.getSize();

        if (isButtonPressed(PUSHBUTTON) != buttonstateA)
        {
            buttonstateA = isButtonPressed(PUSHBUTTON);
            if (buttonstateA)
            {
                generateWavetable(wavetable_0, inharmonics_0, H_0);
            }
        }
        if (isButtonPressed(BUTTON_B) != buttonstateB)
        {
            buttonstateB = isButtonPressed(BUTTON_B);
            if (buttonstateB)
            {
                generateWavetable(wavetable_1, inharmonics_1, H_1);
            }
        }

        float tune = getParameterValue(PARAMETER_A) * 8.0 - 4.0;
        float out_gain = 0.25f * getParameterValue(PARAMETER_B);
        float inharmonic_gain = getParameterValue(PARAMETER_C);
        float lfo_freq = (6.95f * getParameterValue(PARAMETER_D)) + 0.05f;

        hz.setTune(tune);
        float fund_freq = hz.getFrequency(left[0]);
        lfo->setFrequency(lfo_freq);

        float frac;
        float lfo_val;
        uint16_t idx0, idx1;
        uint16_t level, level_0, level_1, ih;
        float s0, s1, wave_0, wave_1, wave_out;

        for (int i = 0; i < size; i++)
        {
            // Compute wavetable phase and indices
            idx0 = (uint16_t)phase_wt;
            idx1 = (idx0 + 1) % N;
            frac = phase_wt - idx0;
            phase_wt += fund_freq * N / FS; // Increment wavetable phase
            if (phase_wt >= N)
            {
                phase_wt -= N;
            }

            // Choose best table based on fundamental
            if (fund_freq >= 1600.0f)
            {
                level = (FS / (2 * fund_freq)) + 1;
                level_0 = DSY_MIN(level, H_0);
                level_1 = DSY_MIN(level, H_1);
            }
            else
            {
                level_0 = H_0;
                level_1 = H_1;
            }

            // -- Wave 0 Process --
            // Harmonics (Wavetable Synthesis)
            s0 = wavetable_0[level_0 - 1][idx0];
            s1 = wavetable_0[level_0 - 1][idx1];
            wave_0 = s0 + (s1 - s0) * frac; // linearly interpolate

            // Inharmonics (Additive Synthesis)
            for (ih = 0; ih < level_0 - 1; ih++)
            {
                inharmonics_0[ih].phase += fund_freq * inharmonics_0[ih].freq_factor / FS; // Increment phase
                if (inharmonics_0[ih].phase >= 1)
                {
                    inharmonics_0[ih].phase -= 1;
                } // Wrap phase
                wave_0 += inharmonic_gain * inharmonics_0[ih].amplitude * sinf(TWOPI_F * inharmonics_0[ih].phase);
            }

            wave_0 *= out_gain;

            // -- Wave 1 Process --
            // Harmonics (Wavetable Synthesis)
            s0 = wavetable_1[level_1 - 1][idx0];
            s1 = wavetable_1[level_1 - 1][idx1];
            wave_1 = s0 + (s1 - s0) * frac; // linearly interpolate

            // Inharmonics (Additive Synthesis)
            for (ih = 0; ih < level_1 - 1; ih++)
            {
                inharmonics_1[ih].phase += fund_freq * inharmonics_1[ih].freq_factor / FS; // Increment phase
                if (inharmonics_1[ih].phase >= 1)
                {
                    inharmonics_1[ih].phase -= 1;
                } // Wrap phase
                wave_1 += inharmonic_gain * inharmonics_1[ih].amplitude * sinf(TWOPI_F * inharmonics_1[ih].phase);
            }

            wave_1 *= out_gain;
            lfo_val = lfo->generate() * 0.5 + 0.5;
            // Morph between waves
            wave_out = (lfo_val * wave_0) + ((1.f - lfo_val) * wave_1);

            left[i] = wave_out;
            right[i] = wave_out;
        }
    }
};

#endif // __WavetableOscPatch_hpp__
