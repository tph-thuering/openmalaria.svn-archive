/*

  This file is part of OpenMalaria.
 
  Copyright (C) 2005,2006,2007,2008 Swiss Tropical Institute and Liverpool School Of Tropical Medicine
 
  OpenMalaria is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or (at
  your option) any later version.
 
  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#include "PkPd/Drug/Drug.h"

#include <assert.h>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <sstream>

using namespace std;

namespace OM { namespace PkPd {
    
/*
 * Static variables and functions
 */

double Drug::minutesPerTimeStep;


void Drug::init () {
  minutesPerTimeStep = Global::interval * 24*60;
}

Drug::Drug(const DrugType* type) :
  typeData (type),
  _concentration (0),
  _nextConcentration (0)
{}

void Drug::checkpoint (istream& stream) {
    _concentration & stream;
    _nextConcentration & stream;
}
void Drug::checkpoint (ostream& stream) {
  _concentration & stream;
  _nextConcentration & stream;
}


void Drug::addDose (double concentration, int delay) {
    assert (delay == 0);
    _concentration += concentration;
    _nextConcentration = _concentration * decayFactor (minutesPerTimeStep);
}

bool Drug::decay() {
  _concentration = _nextConcentration;
  _nextConcentration = _concentration * decayFactor (minutesPerTimeStep);
  //TODO: if concentration is negligible, return true to clean up this object
  return false;
}

} }