#ifndef __StereoVCAPatch_hpp__
#define __StereoVCAPatch_hpp__

/**
AUTHOR:
    (c) 2025 Fredrik Fagerholm

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
    Naive stereo VCA Patch for testing the build system.
*/

#include "Patch.h"

class StereoVCAPatch : public Patch
{
private:
public:
    StereoVCAPatch()
    {
        registerParameter(PARAMETER_A, "Gain A");
        registerParameter(PARAMETER_B, "Gain B");
    }

    ~StereoVCAPatch()
    {
    }

    void processAudio(AudioBuffer &buffer)
    {
        // Get knob (or CV) values
        float gainA = getParameterValue(PARAMETER_A);
        float gainB = getParameterValue(PARAMETER_B);
        // Process buffer
        int size = buffer.getSize();
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);
        // Scale each sample in the buffer by the
        // value set for the A and B knobs
        for (int n = 0; n < size; n++)
        {
            left[n] = gainA * left[n];
            right[n] = gainB * right[n];
        }
    }
};

#endif // __StereoVCAPatch_hpp__
