#ifndef __DualFMPatch_hpp__
#define __DualFMPatch_hpp__

/**
AUTHOR:
    (c) 2017 Martin Klang
    (c) 2025 Fredrik Fagerholm
    Derived from QuadFMPatch by Martin Klang
    https://github.com/pingdynasty/MyPatches/blob/master/QuadFMPatch.hpp
LICENSE:
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2, as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.

DESCRIPTION:
    Two operator FM patch.

    Parameters:
    A : Modulation Amount (FM Amount/modulator index)
    B : Semitones (Tuning ratio for operator 2)
    C : Feedback (FM Feedback for operator 1)
    D : Mix (Mix between operator 1 and 2)
*/

#include "Patch.h"
#include "VoltsPerOctave.h"
#include "SineOscillator.h"
#include "BiquadFilter.h"
#include "SmoothValue.h"

float roundToNearestHalf(float x)
{
    return std::round(x * 2.0f) / 2.0f;
}

class Operator : public Oscillator
{
private:
    float lastOutput = 0.0f;

public:
    SineOscillator osc;

public:
    float index = 1.0;
    float ratio = 1.0;
    float offset = 0.0;
    float phase = 0.0;
    float frequency = 0.0;
    SmoothFloat level = index;
    Operator() : osc(48000)
    {
        level.lambda = 0.99;
    }
    ~Operator()
    {
    }
    void setSampleRate(float value)
    {
        osc.setSampleRate(value);
    }
    void setFrequency(float freq)
    {
        frequency = freq;
        osc.setFrequency(freq * ratio + offset);
    }
    float getFrequency()
    {
        return frequency;
    }
    void setPhase(float phase)
    {
        phase = phase;
    }
    float getPhase()
    {
        return phase;
    }
    void reset()
    {
        phase = 0.0f;
        lastOutput = 0.0f;
    }
    float getNextSample()
    {
        return osc.generate();
    }

    float getNextSample(float fm)
    {
        return osc.generate(fm);
    }

    float getNextSample(float fm, float feedback = 0.0f)
    {
        // feedback is just lastOutput scaled
        float sample = osc.generate(fm + feedback * lastOutput);
        lastOutput = sample; // store for next iteration
        return sample;
    }

    float generate(float fm)
    {
        return getNextSample(fm, 0.0f);
    }
    void generate(FloatArray output, FloatArray fm)
    {
        for (size_t i = 0; i < output.getSize(); ++i)
            output[i] = generate(fm[i]);
    }
};

class DualFM : public Oscillator
{
public:
    Operator ops[2];
    float carrier_ratio;
    float carrier_offset;
    float carrier_index;
    float modulator_ratio;
    float modulator_offset;
    float modulator_index;
    float feedback;
    float mix;
    const float RATIO_DEFAULT = 1;
    const float RATIO_MIN = 0.5;
    const float RATIO_RANGE = 7.5;
    const float OFFSET_DEFAULT = 0.0;
    const float OFFSET_RANGE = 1000;
    const float INDEX_DEFAULT = 0.1;

public:
    DualFM()
    {
        modulator_ratio = RATIO_DEFAULT * 3;
        modulator_offset = OFFSET_DEFAULT;
        modulator_index = INDEX_DEFAULT;
        carrier_ratio = RATIO_DEFAULT * 2;
        carrier_offset = OFFSET_DEFAULT;
        carrier_index = INDEX_DEFAULT;
        feedback = 0.0;
        mix = 1.0;
    }
    void setSampleRate(float value)
    {
        ops[0].setSampleRate(value);
        ops[1].setSampleRate(value);
    }
    void setFrequency(float freq)
    {
        ops[0].setFrequency(freq);
        ops[0].index = modulator_index;
        ops[0].ratio = roundToNearestHalf(RATIO_MIN + modulator_ratio * RATIO_RANGE);
        ops[0].offset = modulator_offset * OFFSET_RANGE;

        ops[1].setFrequency(freq);
        ops[1].index = 1.0;
        ops[1].ratio = RATIO_MIN + carrier_ratio * RATIO_RANGE;
        ops[1].offset = carrier_offset * OFFSET_RANGE;
    }
    float getFrequency()
    {
        return ops[0].getFrequency();
    }

    void setPhase(float phase)
    {
        return ops[0].setPhase(phase);
    }

    float getPhase()
    {
        return ops[0].getPhase();
    }
    void reset()
    {
        ops[0].reset();
        ops[1].reset();
    }

    float getNextSample()
    {
        return generate();
    }

    void getSamples(FloatArray samples)
    {
        for (int i = 0; i < samples.getSize(); ++i)
            samples[i] = generate();
    }

    float generate()
    {
        // 0>1
        float modulator_sample = ops[0].getNextSample();
        float carrier_sample = ops[1].getNextSample(modulator_index * modulator_sample, feedback);
        float w0 = sin((M_PI / 2.0f) * mix);
        float w1 = cos((M_PI / 2.0f) * mix);
        return w0 * modulator_sample + w1 * carrier_sample;
    }

    float generate(float fm)
    {
        return generate();
    }
};

class DualFMPatch : public Patch
{
private:
    DualFM osc;
    VoltsPerOctave hz;
    BiquadFilter *lp;

public:
    DualFMPatch()
    {
        osc.setSampleRate(getSampleRate());
        registerParameter(PARAMETER_A, "Modulation Amount");
        registerParameter(PARAMETER_B, "Semitones");
        registerParameter(PARAMETER_C, "Feedback");
        registerParameter(PARAMETER_D, "Mix");
        lp = StereoBiquadFilter::create(2);
        lp->setLowPass(0.8, FilterStage::BUTTERWORTH_Q);
    }
    ~DualFMPatch()
    {
    }
    void processAudio(AudioBuffer &buffer)
    {
        float mod = getParameterValue(PARAMETER_A);
        float ratio = getParameterValue(PARAMETER_B);
        float fb = 0.5f * getParameterValue(PARAMETER_C);
        float mix = getParameterValue(PARAMETER_D);

        osc.modulator_index = mod;
        osc.modulator_ratio = ratio;
        osc.feedback = fb;
        osc.mix = mix;

        float fundamental = -2.0;
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);
        hz.setTune(fundamental);

        float freq = hz.getFrequency(left[0]);
        osc.setFrequency(freq);

        osc.getSamples(left);
        lp->process(left);
        right.copyFrom(left);
    }
};

#endif // __DualFMPatch_hpp__