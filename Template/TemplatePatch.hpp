#ifndef __TemplatePatch_hpp__
#define __TemplatePatch_hpp__

/**
AUTHOR:
    (c) 2025 Fredrik Fagerholm
    Based on TemplatePatch.hpp by mars
    https://github.com/pingdynasty/OwlPatches/blob/master/TemplatePatch.hpp
LICENSE:
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2, as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

DESCRIPTION:
    Template for oscillator patches.
*/

#include "Patch.h"

class TemplatePatch : public Patch
{
private:
    float paramA;
    float paramB;
    float paramC;
    float paramD;

public:
    TemplatePatch()
    {
        registerParameter(PARAMETER_A, "A");
        registerParameter(PARAMETER_B, "B");
        registerParameter(PARAMETER_C, "C");
        registerParameter(PARAMETER_D, "D");
        registerParameter(PARAMETER_F, "F>");
        registerParameter(PARAMETER_G, "G>");
    }

    ~TemplatePatch()
    {
    }

    void processAudio(AudioBuffer &buffer)
    {
        // Add code here
    }
};

#endif // __TemplatePatch_hpp__
