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
#include "Host/Human.h"

#include "Host/intervention.h"
#include "Host/InfectionIncidenceModel.h"
#include "Clinical/ClinicalModel.h"
#include "WithinHost/DescriptiveIPT.h"	// only for summarizing

#include "inputData.h"
#include "util/gsl.h"
#include "Transmission/TransmissionModel.h"
#include "Surveys.h"

#include <string>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <stdexcept>


/*
  Constants common to all humans
  Decay in anti-toxic immunity
*/

int Human::_ylagLen;


// -----  Static functions  -----

void Human::initHumanParameters () {	// static
  // Init models used by humans:
  PerHostTransmission::initParameters(getInterventions());
  InfectionIncidenceModel::init();
  WithinHostModel::init();
  ClinicalModel::init();
  Vaccine::initParameters();
  _ylagLen = Global::intervalsPer5Days * 4;
}

void Human::clear() {	// static clear
  WithinHostModel::clear();
  Vaccine::clearParameters();
}


// -----  Non-static functions: creation/destruction, checkpointing  -----

// Create new human
Human::Human(TransmissionModel& tm, int ID, int dateOfBirth, int simulationTime) :
    perHostTransmission(),
    infIncidence(InfectionIncidenceModel::createModel()),
    withinHostModel(WithinHostModel::createWithinHostModel()),
    _dateOfBirth(dateOfBirth), /*_ID(ID),*/
    _lastVaccineDose(0),
    _BSVEfficacy(0.0), _PEVEfficacy(0.0), _TBVEfficacy(0.0),
    _probTransmissionToMosquito(0.0)
{
  if (_dateOfBirth != simulationTime && (Simulation::timeStep >= 0 || _dateOfBirth > simulationTime))
    throw out_of_range ("Invalid date of birth!");
  
  _ylag.assign (_ylagLen, 0.0);
  
  
  /* Human heterogeneity; affects:
   * _comorbidityFactor (stored in PathogenesisModel)
   * _treatmentSeekingFactor (stored in CaseManagementModel)
   * availabilityFactor (stored in PerHostTransmission)
   */
  double _comorbidityFactor = 1.0;
  double _treatmentSeekingFactor = 1.0;
  double availabilityFactor = 1.0;
  
  if (Global::modelVersion & TRANS_HET) {
    availabilityFactor=0.2;
    if (gsl::rngUniform() < 0.5) {
      availabilityFactor=1.8;
    }
  }
  if (Global::modelVersion & COMORB_HET) {
    _comorbidityFactor=0.2;
    if (gsl::rngUniform() < 0.5) {
      _comorbidityFactor=1.8;
    }	
  }
  if (Global::modelVersion & TREAT_HET) {
    _treatmentSeekingFactor=0.2;
    if (gsl::rngUniform() < 0.5) {            
      _treatmentSeekingFactor=1.8;
    }	
  }
  if (Global::modelVersion & TRANS_TREAT_HET) {
    _treatmentSeekingFactor=0.2;
    availabilityFactor=1.8;
    if (gsl::rngUniform()<0.5) {
      _treatmentSeekingFactor=1.8;
      availabilityFactor=0.2;
    }
  } else if (Global::modelVersion & COMORB_TRANS_HET) {
    if (gsl::rngUniform()<0.5) {
      _treatmentSeekingFactor=0.2;
    } else {
      _treatmentSeekingFactor=1.8;
    }
    availabilityFactor=1.8;
    _comorbidityFactor=1.8;
    if (gsl::rngUniform()<0.5) {
      availabilityFactor=0.2;
      _comorbidityFactor=0.2;
    }
  } else if (Global::modelVersion & TRIPLE_HET) {
    availabilityFactor=1.8;
    _comorbidityFactor=1.8;
    _treatmentSeekingFactor=0.2;
    if (gsl::rngUniform()<0.5) {
      availabilityFactor=0.2;
      _comorbidityFactor=0.2;
      _treatmentSeekingFactor=1.8;
    }
  }
  perHostTransmission.initialise (tm, availabilityFactor * infIncidence->getAvailabilityFactor(1.0));
  clinicalModel=ClinicalModel::createClinicalModel (_comorbidityFactor, _treatmentSeekingFactor);
}

// Load human from checkpoint
Human::Human(istream& in, TransmissionModel& tm) :
    perHostTransmission(in, tm),
    infIncidence(InfectionIncidenceModel::createModel(in)),
    withinHostModel(WithinHostModel::createWithinHostModel(in)),
    clinicalModel(ClinicalModel::createClinicalModel(in))
{
  in >> _dateOfBirth; 
  //in >> _ID; 
  in >> _lastVaccineDose; 
  in >> _BSVEfficacy; 
  in >> _PEVEfficacy; 
  in >> _TBVEfficacy; 
  _ylag.resize (_ylagLen);
  for (int i=0;i<_ylagLen; i++) {
    in >> _ylag[i];
  }
  in >> _probTransmissionToMosquito;
}

void Human::destroy() {
  delete infIncidence;
  delete withinHostModel;
  delete clinicalModel;
}

void Human::write (ostream& out) const{
  perHostTransmission.write (out);
  infIncidence->write (out);
  withinHostModel->write (out);
  clinicalModel->write (out);
  out << _dateOfBirth << endl; 
  //out << _ID << endl ; 
  out << _lastVaccineDose << endl;
  out << _BSVEfficacy << endl; 
  out << _PEVEfficacy << endl; 
  out << _TBVEfficacy << endl; 
  for (int i=0;i<_ylagLen; i++) {
    out << _ylag[i] << endl;
  }
  out << _probTransmissionToMosquito << endl;
}


// -----  Non-static functions: per-timestep update  -----

bool Human::update(int simulationTime, TransmissionModel* transmissionModel) {
  int ageTimeSteps = simulationTime-_dateOfBirth;
  if (clinicalModel->isDead(ageTimeSteps))
    return true;
  
  // Comments here refer to which WHM functions get called, and which parameters
  // read/set, by the next call. Temporary for some WHM refactoring (TODO).
  
  //withinHostModel->IPTSetLastSPDose	in(iptiEffect) out(_lastIptiOrPlacebo,_lastSPDose,reportIPTDose)
  updateInterventionStatus();
  //withinHostModel->newInfection	inout(_MOI,_cumulativeInfections,infections)
  //withinHostModel->getTotalDensity	in (totalDensity)
  //withinHostModel->calculateDensities	inout(infections,_MOI)...
  updateInfection(transmissionModel);
  //withinHostModel.getTimeStepMaxDensity	in(timeStepMaxDensity)
  //withinHostModel.getTotalDensity	in (totalDensity)
  //old?withinHostModel.immunityPenalisation	inout(_cumulativeY) in(_cumulativeYlag)
  //old?withinHostModel.clearInfections	in(_lastIptiOrPlacebo,iptiEffect) inout(_lastSPDose,infections) out(_MOI)
  //new?withinHostModel.parasiteDensityDetectible	in(totalDensity)
  //new?withinHostModel.medicate	inout(drugProxy)
  clinicalModel->update (*withinHostModel, getAgeInYears(), Simulation::simulationTime-_dateOfBirth);
  clinicalModel->updateInfantDeaths (ageTimeSteps);
  _probTransmissionToMosquito = calcProbTransmissionToMosquito ();
  return false;
}

void Human::updateInfection(TransmissionModel* transmissionModel){
  int numInf = infIncidence->numNewInfections(transmissionModel->getEIR(Simulation::simulationTime, perHostTransmission, getAgeInYears()),
					      _PEVEfficacy, perHostTransmission);
  for (int i=1;i<=numInf; i++) {
    withinHostModel->newInfection();
  }
  
  // Cache total density for infectiousness calculations
  _ylag[Simulation::simulationTime%_ylagLen]=withinHostModel->getTotalDensity();
  
  withinHostModel->calculateDensities(getAgeInYears(), _BSVEfficacy);
}

void Human::updateInterventionStatus() {
  int ageTimeSteps = Simulation::simulationTime-_dateOfBirth;
  if (Vaccine::anyVaccine) {
    /*
      Update the effect of the vaccine
      We should assume the effect is maximal 25 days after vaccination
      TODO: consider the sensitivity of the predictions to the introduction 
      of a delay until the vaccine has reached max. efficacy.
    */
    if ( _lastVaccineDose >  0) {
      _PEVEfficacy *= Vaccine::PEV.decay;
      _TBVEfficacy *= Vaccine::TBV.decay;
      _BSVEfficacy *= Vaccine::BSV.decay;
    }
    /*
      Determine eligibility for vaccination
      check for number of vaccine doses in the vaccinate subroutine
      TODO: The tstep conditional is appropriate if we assume there is no intervention during warmup
      It won't work if we introduce interventions into a scenario with a pre-existing intervention.
    */
    //_ctsIntervs.deploy(ageTimeSteps);
    if (Simulation::timeStep >= 0) {
      if (_lastVaccineDose < (int)Vaccine::_numberOfEpiDoses){
	  if (Vaccine::targetAgeTStep[_lastVaccineDose] == ageTimeSteps &&
	      gsl::rngUniform() <  Vaccine::vaccineCoverage[_lastVaccineDose] ) {
          vaccinate();
          Surveys.current->reportEPIVaccinations (ageGroup(), 1);
        }
      }
    }
  }
  withinHostModel->IPTSetLastSPDose(ageTimeSteps, ageGroup());
  perHostTransmission.continousItnDistribution (ageTimeSteps);
}


void Human::massVaccinate () {
  vaccinate();
  Surveys.current->reportMassVaccinations (ageGroup(), 1);
}
void Human::vaccinate(){
  //Index to look up initial efficacy relevant for this dose.
  if (Vaccine::PEV.active)
    _PEVEfficacy = Vaccine::PEV.getEfficacy(_lastVaccineDose);
  
  if (Vaccine::BSV.active)
    _BSVEfficacy = Vaccine::BSV.getEfficacy(_lastVaccineDose);
  
  if (Vaccine::TBV.active)
    _TBVEfficacy = Vaccine::TBV.getEfficacy(_lastVaccineDose);
  
  ++_lastVaccineDose;
}

void Human::IPTiTreatment () {
  withinHostModel->IPTiTreatment (ageGroup());
}


void Human::clearInfections () {
  //NOTE: if Population::massTreatment is incompatible with IPT, we can just pass false:
  withinHostModel->clearInfections(clinicalModel->latestDiagnosisIsSevereMalaria());
}

SurveyAgeGroup Human::ageGroup() const{
  return SurveyAgeGroup(getAgeInYears());
}

double Human::getAgeInYears() const{
  return double((Simulation::simulationTime-_dateOfBirth)*Global::interval) / daysInYear;
}


void Human::summarize(Survey& survey) {
  if (DescriptiveIPTWithinHost::iptActive && clinicalModel->recentTreatment())
    return;	//NOTE: do we need this?
  
  SurveyAgeGroup ageGrp = ageGroup();
  survey.reportHosts (ageGrp, 1);
  withinHostModel->summarize (survey, ageGrp);
  infIncidence->summarize (survey, ageGrp);
  clinicalModel->summarize (survey, ageGrp);
}


double Human::calcProbTransmissionToMosquito() const {
  /* This model was designed for 5-day timesteps. We use the same model
  (sampling 10, 15 and 20 days ago) for 1-day timesteps to avoid having to
  (design and analyse a new model. Description: AJTMH pp.32-33 */
  int ageTimeSteps=Simulation::simulationTime-_dateOfBirth;
  if (ageTimeSteps*Global::interval <= 20 || Simulation::simulationTime*Global::interval <= 20)
    return 0.0;
  
  //Infectiousness parameters: see AJTMH p.33, tau=1/sigmag**2 
  static const double beta1=1.0;
  static const double beta2=0.46;
  static const double beta3=0.17;
  static const double tau= 0.066;
  static const double mu= -8.1;
  
  // Take weighted sum of total asexual blood stage density 10, 15 and 20 days before.
  // These values are one timestep more recent than that, however the calculated
  // value is not used until the next timestep when then ages would be correct.
  double x = beta1 * _ylag[(Simulation::simulationTime-2*Global::intervalsPer5Days+1) % _ylagLen]
	   + beta2 * _ylag[(Simulation::simulationTime-3*Global::intervalsPer5Days+1) % _ylagLen]
	   + beta3 * _ylag[(Simulation::simulationTime-4*Global::intervalsPer5Days+1) % _ylagLen];
  if (x < 0.001)
    return 0.0;
  
  double zval=(log(x)+mu)/sqrt(1.0/tau);
  double pone = gsl::cdfUGaussianP (zval);
  double transmit=(pone*pone);
  //transmit has to be between 0 and 1
  transmit=std::max(transmit, 0.0);
  transmit=std::min(transmit, 1.0);
  
  //	Include here the effect of transmission-blocking vaccination
  return transmit*(1.0-_TBVEfficacy);
}
