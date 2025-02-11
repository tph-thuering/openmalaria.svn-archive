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

#include "Pathogenesis/Pyrogen.h"
#include "inputData.h"
#include <cmath>

using namespace std;

namespace OM { namespace Pathogenesis {
    
double PyrogenPathogenesis::initPyroThres;
double PyrogenPathogenesis::smuY;
double PyrogenPathogenesis::Ystar2_13;
double PyrogenPathogenesis::alpha14;
double PyrogenPathogenesis::Ystar1_26;

void PyrogenPathogenesis::init(){
  initPyroThres=InputData.getParameter(Params::Y_STAR_0);
  smuY=-log(0.5)/(Global::DAYS_IN_YEAR/Global::interval*InputData.getParameter(Params::Y_STAR_HALF_LIFE));
  Ystar2_13=InputData.getParameter(Params::Y_STAR_SQ);
  alpha14=InputData.getParameter(Params::ALPHA);
  Ystar1_26=InputData.getParameter(Params::Y_STAR_1);
}

PyrogenPathogenesis::PyrogenPathogenesis(double cF) :
     PathogenesisModel (cF), _pyrogenThres (initPyroThres)
{}


double PyrogenPathogenesis::getPEpisode(double timeStepMaxDensity, double totalDensity) {
  updatePyrogenThres(totalDensity);
  return 1-1/(1+(timeStepMaxDensity/_pyrogenThres));;
}

void PyrogenPathogenesis::summarize (Monitoring::Survey& survey, Monitoring::AgeGroup ageGroup) {
  survey.addToPyrogenicThreshold(ageGroup, _pyrogenThres);
  survey.addToLogPyrogenicThreshold(ageGroup, log(_pyrogenThres+1.0));
}

void PyrogenPathogenesis::updatePyrogenThres(double totalDensity){
    // Note: this calculation is slow (something like 5% of runtime)
    
  //Number of categories in the numerical approx. below
  const int n= 11;
  const double delt= 1.0/n;
  //Numerical approximation to equation 2, AJTMH p.57
  for (int i=1;i<=n; i++) {
    _pyrogenThres += totalDensity * alpha14 * Global::interval * delt /
	( (Ystar1_26 + totalDensity) * (Ystar2_13 + _pyrogenThres) )
	- smuY * _pyrogenThres * delt;
  }
}


void PyrogenPathogenesis::checkpoint (istream& stream) {
    PathogenesisModel::checkpoint (stream);
    _pyrogenThres & stream;
}
void PyrogenPathogenesis::checkpoint (ostream& stream) {
    PathogenesisModel::checkpoint (stream);
    _pyrogenThres & stream;
}

} }