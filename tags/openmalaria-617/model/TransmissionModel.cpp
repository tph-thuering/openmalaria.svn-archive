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
//This is needed to get symbols like M_PI with MSVC:
#define _USE_MATH_DEFINES

#include "TransmissionModel.h"
#include <math.h> 
#include "global.h" 
#include <gsl/gsl_vector.h> 
#include "intervention.h" 
#include <iostream>
#include "inputData.h"
#include "TransmissionModel/NonVector.h"
#include "TransmissionModel/Vector.h"
#include "TransmissionModel/PerHost.h"
#include "simulation.h"
#include "summary.h"

//static (class) variables
const double TransmissionModel::agemin[nwtgrps] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 20, 25, 30, 40, 50, 60 };
const double TransmissionModel::agemax[nwtgrps] = { 0.99, 1.99, 2.99, 3.99, 4.99, 5.99, 6.99, 7.99, 8.99, 9.99, 10.99, 11.99, 12.99, 13.99, 14.99, 19.99, 24.99, 29.99, 39.99, 49.99, 59.99, 60.99 };
const double TransmissionModel::bsa_prop[nwtgrps] = { 0.1843, 0.2225, 0.252, 0.2706, 0.2873, 0.3068, 0.3215, 0.3389, 0.3527, 0.3677, 0.3866, 0.3987, 0.4126, 0.4235, 0.441, 0.4564, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 };


TransmissionModel* TransmissionModel::createTransmissionModel () {
  // EntoData contains either a list of at least one anopheles or a list of at
  // least one EIRDaily.
  if (getEntoData().getAnopheles().size())
    return new VectorTransmission();
  else
    return new NonVectorTransmission();
}

TransmissionModel::TransmissionModel(){
  EIPDuration = get_EipDuration();
  PerHostTransmission::BaselineAvailabilityShapeParam=getParameter(Params::BASELINE_AVAILABILITY_SHAPE);	// also set in Human
  
  kappa.resize (Global::intervalsPerYear);
  initialKappa.resize (Global::intervalsPerYear);
  initialisationEIR.resize (Global::intervalsPerYear);
  
  initAgeExposureConversion();
}

TransmissionModel::~TransmissionModel () {
}

double TransmissionModel::getEIR (int simulationTime, PerHostTransmission& host) {
  if (Global::simulationMode == equilibriumMode)
    return initialisationEIR[simulationTime % Global::intervalsPerYear];
  else
    return calculateEIR (simulationTime, host);
}

void TransmissionModel::updateKappa (double sumWeight, double sumWt_kappa) {
  size_t tmod = Simulation::simulationTime % Global::intervalsPerYear;
  //Prevent NaNs
  if (sumWeight == 0.0) {
    kappa[tmod] = 0.0;
    cerr << "sW.eq.0" << endl;
  } else {
    kappa[tmod] = sumWt_kappa / sumWeight;
  }
  
  //Calculate time-weighted average of kappa
  if (tmod == 1) {
    _sumAnnualKappa = 0.0;
  }
  _sumAnnualKappa += kappa[tmod] * Global::interval * initialisationEIR[tmod];
  if (tmod == 0) {
    if (annualEIR == 0) {
      _annualAverageKappa=0;
      cerr << "aE.eq.0" << endl;
    }
    else {
      _annualAverageKappa = _sumAnnualKappa / annualEIR;
    }
  }
}

void TransmissionModel::summarize (Summary& summary) {
  summary.setNumTransmittingHosts(kappa[Simulation::simulationTime % Global::intervalsPerYear]);
  summary.setAnnualAverageKappa(_annualAverageKappa);
}

double TransmissionModel::getRelativeAvailability(double ageyrs) {
  return ageSpecificRelativeAvailability[getAgeGroup(ageyrs)];
}

void TransmissionModel::initAgeExposureConversion(){
  for (size_t i=0; i<nages; i++) {
    ageSpecificRelativeAvailability[i] = bsa_prop[i] / (1-bsa_prop[i]);
  }

  /* Not used.
  // The following code calculates average bites for children <6 years old
  // relative to adults. This is for analysis of Saradidi (Beier et al) data.

  // avbites_6 is the exposure of children < 6 years old relative to adults
  double avbites_6=0.0;
  
  for (int i = 0; agemin[i] <  6; ++i) {
    if (agemax[i] <= 0.5)	// NOTE: never true − is it obsolete?
      continue;
    
    //agemin0 is the lower bound of the youngest age group used in this calculation
    double agemin0=agemin[i];
    if (agemin0 < 0.5)
      agemin0=0.5;
    
    avbites_6 += ageSpecificRelativeAvailability[i] * (agemax[i]-agemin0)/5.5;
  }
  biteratio_6 = avbites_6 / (1.0-avbites_6);
  */
}

size_t TransmissionModel::getAgeGroup (double age) {
  for (size_t i = 0; i < nages; ++i) {
    if (agemax[i] > age)
      return i;
  }
  return nages-1;	// final category
}


// -----  checkpointing  -----

void TransmissionModel::write(ostream& out) const {
  out << annualEIR << endl;
  for (size_t i = 0; i < Global::intervalsPerYear; ++i)
    out << kappa[i] << endl;
  out << _annualAverageKappa << endl;
  out << _sumAnnualKappa << endl;
}
void TransmissionModel::read(istream& in) {
  in >> annualEIR;
  for (size_t i = 0; i < Global::intervalsPerYear; ++i)
    in >> kappa[i];
  in >> _annualAverageKappa;
  in >> _sumAnnualKappa;
}
