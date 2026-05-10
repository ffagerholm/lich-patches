#ifndef __KickallPatch_hpp__
#define __KickallPatch_hpp__

/**
AUTHOR:
    (c) 2024 Befaco
    (c) 2025 Fredrik Fagerholm
    Derived from Kickall.cpp by the Befaco, licensed under GPL v3.
    https://github.com/VCVRack/Befaco/blob/v2/src/Kickall.cpp
LICENSE:
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
DESCRIPTION:
    A port of Befaco Kickall module functionality, based on the
    VCV rack version of the module.

    The patch consists of a sine wave generator with a controllable waveshape,
    a "decay" envelope controlling the volume and pitch.

    Parameters on the Befaco Lich module:
        PARAMETER_A "Tuning" controls the frequency/tuning of the oscillator.
        PARAMETER_B "Shape" controls the shape of the oscillator waveform.
            The base shape of the waveform is a sine wave. Turning the shape
            know clockwise changes the wave into a more square-shaped waveform,
            adding more harmonics and a harsher sound.
        PARAMETER_C "Decay" controls the length of the decay envelope for either the
            pitch or the volume based on the state of button B.
        PARAMETER_D "Bend" controls how much the envelope affects the pitch.
        Button A and GATE 1 Trigger the envelope.
        Button B and GATE 2 Selects whether the "Decay" control affects the
            length of the volume or pitch envelope. If the button is off,
            the "Decay" knob controls the pitch envelope length. If the button
            is on, the "Decay" knob controls the volume envelope length.
*/

#include "Patch.h"
#include "ExponentialDecayEnvelope.h"
#include "VoltsPerOctave.h"

float clamp(float x, float lower, float upper)
{
    return max(lower, min(x, upper));
}

float rescale(float x, float minA, float maxA, float minB, float maxB)
{
    return (maxB - minB) * (x - minA) / (maxA - minA) + minB;
}

class SoftTakeoverKnob
{
private:
    float threshold;
    bool engaged;
    float lastInput;
    float targetValue;

    static bool hasCrossed(float target,
                           float previous,
                           float current)
    {
        return (previous <= target && current >= target) ||
               (previous >= target && current <= target);
    }
public:
    explicit SoftTakeoverKnob(float threshold = 0.02f)
        : threshold(threshold),
          engaged(false),
          lastInput(0.0f),
          targetValue(0.0f)
    {
    }

    void setValue(float value)
    {
        // Set the current value.
        targetValue = value;
    }

    float getValue() const
    {
        return targetValue;
    }

    void disengage()
    {
        // Force the knob to re-pickup before it can
        // control the target again.
        engaged = false;
    }

    // Updates targetValue only after takeover engages.
    void process(float input)
    {
        if (!engaged)
        {
            // Engage if input is close enough
            if (abs(input - targetValue) <= threshold)
            {
                engaged = true;
            }
            // Or if the input crossed the target
            // else if (hasCrossed(targetValue, lastInput, input))
            // {
            //     engaged = true;
            // }
        }
        lastInput = input;
        // Only update after engagement
        if (engaged) {
            targetValue = input;
        }
    }
};

class KickallPatch : public Patch
{
private:
    static constexpr float FREQ_A0 = 10.f; // 27.5f;
    static constexpr float FREQ_B2 = 123.471f;
    static constexpr float minVolumeDecay = 0.075f;
    static constexpr float maxVolumeDecay = 10.f;
    static constexpr float minPitchDecay = 0.0075f;
    static constexpr float maxPitchDecay = 2.f;
    static constexpr float bendRange = 10000;
    bool buttonstateA = false;
    bool buttonstateB = false;
    bool configstate = false;
    float sr;
    ExponentialDecayEnvelope *pitch;
    ExponentialDecayEnvelope *volume;
    VoltsPerOctave hz;
    float cvFreq;
    float vcaGain;
    float kickFrequency;
    float phase, phaseInc;
    float pitchEnv;
    float volEnv;
    SoftTakeoverKnob pitchDecayKnob;
    SoftTakeoverKnob volumeDecayKnob;

public:
    KickallPatch()
    {
        registerParameter(PARAMETER_A, "Tuning"); // scale to range [FREQ_A0, FREQ_B2]
        registerParameter(PARAMETER_B, "Shape");  // Wave shape
        registerParameter(PARAMETER_C, "Decay");  // VCA Envelope decay time
        registerParameter(PARAMETER_D, "Bend");   // Pitch envelope attenuator
        // Outputs
        registerParameter(PARAMETER_F, "Pitch Envelope>");  // Pitch Envelope output (CV Out 1)
        registerParameter(PARAMETER_G, "Volume Envelope>"); // Volume Envelope output (CV Out 1)
        // Set sample rate
        sr = getSampleRate();
        // Create pitch envelope
        pitch = ExponentialDecayEnvelope::create(sr);
        // Create volume envelope
        volume = ExponentialDecayEnvelope::create(sr);
        // Set tuning down by 2 octaves
        hz.setTune(-4);
        // Set default pitch and volume decay
        pitchDecayKnob.setValue(0.25f);
        volumeDecayKnob.setValue(0.5f);
    }

    ~KickallPatch()
    {
        ExponentialDecayEnvelope::destroy(pitch);
        ExponentialDecayEnvelope::destroy(volume);
    }

    void processAudio(AudioBuffer &buffer)
    {

        if (isButtonPressed(BUTTON_B) != buttonstateB)
        {
            buttonstateB = isButtonPressed(BUTTON_B);
            if (buttonstateB)
            {
                configstate = !configstate;
            }
        }

        float envAmt = getParameterValue(PARAMETER_C);
        // Change envelope amount for either pitch or volume based
        // on the config state. Use soft-takeover to avoid jumping
        // values when the state is changed.
        if (configstate)
        {
            volumeDecayKnob.disengage();
            pitchDecayKnob.process(envAmt);
            setButton(BUTTON_B, false);
        }
        else
        {
            pitchDecayKnob.disengage();
            volumeDecayKnob.process(envAmt);
            setButton(BUTTON_B, true);
        }

        // Pitch envelope
        const float bend = bendRange * pow(getParameterValue(PARAMETER_D), 3.0);
        float pitchDecay = (maxPitchDecay - minPitchDecay) * pitchDecayKnob.getValue() + minPitchDecay;
        // Set pitch decay rate
        pitch->setDecay(pitchDecay);

        // Volume envelope
        const float volumeDecay = minVolumeDecay * pow(
                                                       2.f,
                                                       volumeDecayKnob.getValue() * log2(maxVolumeDecay / minVolumeDecay));
        volume->setDecay(
            clamp(
                volumeDecay,
                0.01,
                maxVolumeDecay));

        // Calculate oscillator waveform shape
        const float shape = getParameterValue(PARAMETER_B) * 0.99f;
        const float shapeB = (1.0f - shape) / (1.0f + shape);
        const float shapeA = (4.0f * shape) / ((1.0f - shape) * (1.0f + shape));

        // Fill the output buffers
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);
        int size = buffer.getSize();

        // Set oscillator base frequency
        // Scale parameter to range [FREQ_A0, FREQ_B2)
        float tuningParam = getParameterValue(PARAMETER_A);
        float freq = (FREQ_B2 - FREQ_A0) * tuningParam + FREQ_A0;
        // freq *=

        float x = 0.0f;

        if (isButtonPressed(PUSHBUTTON) != buttonstateA)
        {
            buttonstateA = isButtonPressed(PUSHBUTTON);
            if (buttonstateA)
            {
                volume->trigger();
                pitch->trigger();
                phase = 0.0f;
            }
        }

        for (int n = 0; n < size; n++)
        {
            // V/Oct input
            // Take as CV V/Oct input from IN A
            cvFreq = hz.getFrequency(left[n]);
            // "VCA" Gain
            // Take as CV input from IN B ("Accent" input)
            vcaGain = hz.sampleToVolts(right[n]);

            pitchEnv = pitch->getNextSample();
            // Calculate phase increment based on base pitch + CV input level (V/Oct) + pitch envelope
            kickFrequency = max(10.0f, freq + cvFreq + bend * pitchEnv);
            phaseInc = kickFrequency * 2 * M_PI / sr;

            // Calculate phase
            phase += phaseInc;
            // phase -= std::floorf(phase);
            phase = fmod(phase, 2 * M_PI);

            // Get Oscillator value
            x = sinf(phase);
            // Waveshaping of oscillator
            x = x * (shapeA + shapeB) / ((abs(x) * shapeA) + shapeB);
            // Apply volume envelope and gain
            volEnv = volume->getNextSample();
            left[n] = (volEnv * x * 5.0f) * (1.f + vcaGain);

            setParameterValue(PARAMETER_F, pitchEnv);
            setParameterValue(PARAMETER_G, volEnv);
        }
        left.tanh();
        right.copyFrom(left);
    }
};

#endif // __KickallPatch_hpp__