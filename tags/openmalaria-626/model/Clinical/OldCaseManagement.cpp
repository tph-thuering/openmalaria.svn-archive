/* This file is part of OpenMalaria.
 *
 * Copyright (C) 2005,2006,2007,2008 Swiss Tropical Institute and Liverpool School Of Tropical Medicine
 *
 * OpenMalaria is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include "Clinical/OldCaseManagement.h"
#include "Clinical/ClinicalModel.h"

#include "WithinHost/WithinHostModel.h"
#include "inputData.h"
#include "Global.h"
#include "Surveys.h"
#include "util/gsl.h"
#include "util/ModelOptions.hpp"
#include "util/errors.hpp"

#include <limits>
#include <cmath>

namespace OM { namespace Clinical {

int OldCaseManagement::healthSystemSource;
const int OldCaseManagement::SEQUELAE_AGE_BOUND[NUM_SEQUELAE_AGE_GROUPS] = { 1, 10 };
double OldCaseManagement::_oddsRatioThreshold;
bool OldCaseManagement::_noMortality;
std::vector<double> OldCaseManagement::_inputAge;
std::vector<double> OldCaseManagement::_caseFatalityRate;
double OldCaseManagement::probGetsTreatment[3];
double OldCaseManagement::probParasitesCleared[3];
double OldCaseManagement::cureRate[3];
double OldCaseManagement::probSequelaeTreated[2];
double OldCaseManagement::probSequelaeUntreated[2];


// -----  utility  -----

const scnXml::HealthSystem& getHealthSystem (int healthSystemSource) {
    if (healthSystemSource == -1) {
	return InputData.getHealthSystem();
    } else {
	const scnXml::Intervention* interv = InputData.getInterventionByTime (healthSystemSource);
	if (interv == NULL || !interv->getChangeHS().present())
	    throw runtime_error ("healthSystemSource invalid");
	return interv->getChangeHS().get();
    }
    assert(false);	// unreachable
}

// -----  static init  -----

void OldCaseManagement::init ()
{
    if (util::ModelOptions::option (util::INCLUDES_PK_PD))
	throw util::xml_scenario_error ("OldCaseManagement is not compatible with INCLUDES_PK_PD");

    _oddsRatioThreshold = exp (InputData.getParameter (Params::LOG_ODDS_RATIO_CF_COMMUNITY));
    
    setHealthSystem(-1);
}

void OldCaseManagement::setHealthSystem (int source) {
    healthSystemSource = source;
    const scnXml::HealthSystem& healthSystem = getHealthSystem(healthSystemSource);
    
    if (source == -1)
	Episode::healthSystemMemory = healthSystem.getHealthSystemMemory();
    else if (Episode::healthSystemMemory != healthSystem.getHealthSystemMemory())
	throw util::xml_scenario_error ("Change of health system had a different healthSystemMemory");
    
    setParasiteCaseParameters (healthSystem);
    
    const scnXml::ByAgeItems::ItemSequence& items = healthSystem.getPSequelaeInpatient().getItem();
    for (int agegrp = 0; agegrp < NUM_SEQUELAE_AGE_GROUPS; agegrp++) {
	for (size_t i = 0; i < items.size(); i++) {
	    if (items[i].getMaxAgeYrs() > SEQUELAE_AGE_BOUND[agegrp]) {
		probSequelaeTreated[agegrp] =
		probSequelaeUntreated[agegrp] = items[i].getValue();
		goto gotItem;
	    }
	}
	throw util::xml_scenario_error ("In scenario.xml: healthSystem: pSequelaeInpatient: expected item with maxAgeYrs > 10");
	gotItem:; // alternative to for ... break ... else
    }
    
    readCaseFatalityRatio (healthSystem);
}


// -----  non-static construction/desctruction  -----

OldCaseManagement::OldCaseManagement (double tSF) :
    _latestRegimen (0), _tLastTreatment (Global::TIMESTEP_NEVER), _treatmentSeekingFactor (tSF)
{
}
OldCaseManagement::~OldCaseManagement()
{
}

void OldCaseManagement::staticCheckpoint (ostream& stream) {
    healthSystemSource & stream;
}
void OldCaseManagement::staticCheckpoint (istream& stream) {
    healthSystemSource & stream;
    if (healthSystemSource != -1)
	setHealthSystem(healthSystemSource);
}

// -----  other public  -----

void OldCaseManagement::doCaseManagement (Pathogenesis::State pgState, WithinHost::WithinHostModel& withinHostModel, Episode& latestReport, double ageYears, int& doomed)
{
  bool effectiveTreatment = false;

  if (pgState & Pathogenesis::MALARIA) {
    if (pgState & Pathogenesis::COMPLICATED)
      effectiveTreatment = severeMalaria (latestReport, ageYears, doomed);
    else if (pgState == Pathogenesis::STATE_MALARIA) {
      // NOTE: if condition means this doesn't happen if INDIRECT_MORTALITY is
      // included. Validity is debatable, but there's no point changing now.
      // (This does affect tests.)
      effectiveTreatment = uncomplicatedEvent (latestReport, true, ageYears);
    }

    if ((pgState & Pathogenesis::INDIRECT_MORTALITY) && doomed == 0)
      doomed = -Global::interval;

    if (util::ModelOptions::option (util::PENALISATION_EPISODES)) {
      withinHostModel.immunityPenalisation();
    }
  } else if (pgState & Pathogenesis::SICK) { // sick but not from malaria
    effectiveTreatment = uncomplicatedEvent (latestReport, false, ageYears);
  }

  if (effectiveTreatment) {
    if (! (util::ModelOptions::option (util::INCLUDES_PK_PD)))
      withinHostModel.clearInfections (latestReport.getState() == Pathogenesis::STATE_SEVERE);
  }
}


// -----  private  -----

bool OldCaseManagement::uncomplicatedEvent (Episode& latestReport, bool isMalaria, double ageYears)
{
  SurveyAgeGroup ageGroup(ageYears);
  Pathogenesis::State entrypoint = isMalaria ? Pathogenesis::STATE_MALARIA
                                   : Pathogenesis::SICK;
  int nextRegimen = getNextRegimen (Global::simulationTime, entrypoint, _tLastTreatment);
  bool successfulTreatment = false;
  
  if (probGetsTreatment[nextRegimen-1]*_treatmentSeekingFactor > (gsl::rngUniform())) {
    _latestRegimen = nextRegimen;
    _tLastTreatment = Global::simulationTime;
      Surveys.current->reportTreatment(ageGroup, _latestRegimen);

    if (probParasitesCleared[nextRegimen-1] > gsl::rngUniform()) {
      successfulTreatment = true;	// Parasites are cleared
      // We don't report out-of-hospital recoveries, so this wouldn't do anything extra:
      //entrypoint = Pathogenesis::State (entrypoint | Pathogenesis::RECOVERY);
    } else {
      // No change in parasitological status: treated outside of hospital
    }
  } else {
    // No change in parasitological status: non-treated
  }
  latestReport.update (Global::simulationTime, ageGroup, entrypoint);
  return successfulTreatment;
}

bool OldCaseManagement::severeMalaria (Episode& latestReport, double ageYears, int& doomed)
{
  SurveyAgeGroup ageGroup(ageYears);
  int isAdultIndex = 1;
  if (ageYears >= 5.0) {
    isAdultIndex = 0;
  }
  
  int nextRegimen = getNextRegimen (Global::simulationTime, Pathogenesis::STATE_SEVERE, _tLastTreatment);

  double p2, p3, p4, p5, p6, p7;
  // Probability of getting treatment (only part which is case managment):
  p2 = probGetsTreatment[nextRegimen-1] * _treatmentSeekingFactor;
  // Probability of getting cured after getting treatment:
  p3 = cureRate[nextRegimen-1];
  // p4 is the hospital case-fatality rate from Tanzania
  p4 = caseFatality (ageYears);
  // p5 here is the community threshold case-fatality rate
  p5 = getCommunityCaseFatalityRate (p4);
  p6 = probSequelaeTreated[isAdultIndex];
  p7 = probSequelaeUntreated[isAdultIndex];

  double q[9];
  // Community deaths
  q[0] = (1 - p2) * p5;
  // Community sequelae
  q[1] = q[0] + (1 - p2) * (1 - p5) * p7;
  // Community survival
  q[2] = q[1] + (1 - p2) * (1 - p5) * (1 - p7);
  // Parasitological failure deaths
  q[3] = q[2] + p2 * p5 * (1 - p3);
  // Parasitological failure sequelae
  q[4] = q[3] + p2 * (1 - p3) * (1 - p5) * p7;
  // Parasitological failure survivors
  q[5] = q[4] + p2 * (1 - p3) * (1 - p5) * (1 - p7);
  // Parasitological success deaths
  q[6] = q[5] + p2 * p3 * p4;
  // Parasitological success sequelae
  q[7] = q[6] + p2 * p3 * (1 - p4) * p6;
  // Parasitological success survival
  q[8] = q[7] + p2 * p3 * (1 - p4) * (1 - p6);
  /*
  if (q(5).lt.1) stop
  NOT TREATED
  */

  double prandom = (gsl::rngUniform());

  if (q[2] <= prandom) { // Patient gets in-hospital treatment
    _tLastTreatment = Global::simulationTime;
    _latestRegimen = nextRegimen;
    Surveys.current->reportTreatment(ageGroup,_latestRegimen);

    Pathogenesis::State sevTreated = Pathogenesis::State (Pathogenesis::STATE_SEVERE | Pathogenesis::EVENT_IN_HOSPITAL);
    if (q[5] <= prandom) { // Parasites cleared (treated, in hospital)
      if (q[6] > prandom) {
        latestReport.update (Global::simulationTime, ageGroup, Pathogenesis::State (sevTreated | Pathogenesis::DIRECT_DEATH));
        doomed  = 4;
      } else if (q[7] > prandom) { // Patient recovers, but with sequelae (don't report full recovery)
        latestReport.update (Global::simulationTime, ageGroup, Pathogenesis::State (sevTreated | Pathogenesis::SEQUELAE));
      } else { /*if (q[8] > prandom)*/
        latestReport.update (Global::simulationTime, ageGroup, Pathogenesis::State (sevTreated | Pathogenesis::RECOVERY));
      }
      return true;
    } else { // Treated but parasites not cleared (in hospital)
      if (q[3] > prandom) {
        latestReport.update (Global::simulationTime, ageGroup, Pathogenesis::State (sevTreated | Pathogenesis::DIRECT_DEATH));
        doomed  = 4;
      } else if (q[4] > prandom) { // sequelae without parasite clearance
        latestReport.update (Global::simulationTime, ageGroup, Pathogenesis::State (sevTreated | Pathogenesis::SEQUELAE));
      } else { /*if (q[5] > prandom)*/
        // No change in parasitological status: in-hospital patients
        latestReport.update (Global::simulationTime, ageGroup, Pathogenesis::STATE_SEVERE); // no event
      }
      return false;
    }
  } else { // Not treated
    if (q[0] > prandom) {
      latestReport.update (Global::simulationTime, ageGroup, Pathogenesis::State (Pathogenesis::STATE_SEVERE | Pathogenesis::DIRECT_DEATH));
      doomed  = 4;
    } else if (q[1] > prandom) {
      latestReport.update (Global::simulationTime, ageGroup, Pathogenesis::State (Pathogenesis::STATE_SEVERE | Pathogenesis::SEQUELAE));
    } else { /*if (q[2] > prandom)*/
      // No change in parasitological status: non-treated
      latestReport.update (Global::simulationTime, ageGroup, Pathogenesis::STATE_SEVERE);
    }
    return false;
  }
}

void OldCaseManagement::readCaseFatalityRatio (const scnXml::HealthSystem& healthSystem)
{
    const scnXml::AgeGroups::GroupSequence& xmlGroupSeq = healthSystem.getCFR().getGroup();

  int numOfGroups = xmlGroupSeq.size();
  _inputAge.resize (numOfGroups + 1);
  _caseFatalityRate.resize (numOfGroups + 1);

  for (int i = 0;i < numOfGroups; i++) {
    const scnXml::Group& xmlGroup = xmlGroupSeq[i];
    _inputAge[i] = xmlGroup.getLowerbound();
    _caseFatalityRate[i] = xmlGroup.getCfr();
  }

  //NOTE: need to make sure _inputAge[0] == 0 (or less)
  _inputAge[0] = 0;

  //Use a high number for the upper bound.
  _inputAge[numOfGroups] = numeric_limits<double>::infinity();
  //CFR is constant for everyone above the highest upperbound
  _caseFatalityRate[numOfGroups] = _caseFatalityRate[numOfGroups-1];
  _noMortality = (numOfGroups == 1) && (_caseFatalityRate[0] == 0);
}

double OldCaseManagement::getCommunityCaseFatalityRate (double caseFatalityRatio)
{
  double x = caseFatalityRatio * _oddsRatioThreshold;
  return x / (1 - caseFatalityRatio + x);
}

int OldCaseManagement::getNextRegimen (int simulationTime, int diagnosis, int tLastTreated)
{
  if (diagnosis == Pathogenesis::STATE_SEVERE)
    return 3;

  if (tLastTreated + Episode::healthSystemMemory > simulationTime)
    return 2;

  return 1;
}

double OldCaseManagement::caseFatality (double ageYears)
{
  // NOTE: assume ageYears >= 0 and _inputAge[0] <= 0
  if (_noMortality)
    return 0.0;

  size_t i = 0;
  while (_inputAge[i] <= ageYears)
    ++i;

  // _inputAge[i-1] <= ageYears < _inputAge[i]
  double a0 = _inputAge[i-1];
  double f0 = _caseFatalityRate[i-1];
  return (ageYears - a0) / (_inputAge[i] - a0) * (_caseFatalityRate[i] - f0) + f0;
}


double getHealthSystemACRByName (const scnXml::TreatmentDetails& td, string firstLineDrug)
{
  if (firstLineDrug == "CQ")
    return td.getCQ().present() ? td.getCQ().get().getValue() : 0;
  else if (firstLineDrug == "SP")
    return td.getSP().present() ? td.getSP().get().getValue() : 0;
  else if (firstLineDrug == "AQ")
    return td.getAQ().present() ? td.getAQ().get().getValue() : 0;
  else if (firstLineDrug == "SPAQ")
    return td.getSPAQ().present() ? td.getSPAQ().get().getValue() : 0;
  else if (firstLineDrug == "ACT")
    return td.getACT().present() ? td.getACT().get().getValue() : 0;
  else if (firstLineDrug == "QN")
    return td.getQN().present() ? td.getQN().get().getValue() : 0;
  else if (firstLineDrug == "selfTreatment")
    return td.getSelfTreatment().getValue();
  else {
    throw util::xml_scenario_error ("healthSystem.drugRegimen->firstLine has bad value");
  }
}

void OldCaseManagement::setParasiteCaseParameters (const scnXml::HealthSystem& healthSystem)
{
  // --- calculate cureRate ---

  //We get the ACR depending on the name of firstLineDrug.
  cureRate[0] = getHealthSystemACRByName (healthSystem.getInitialACR(),
                                          healthSystem.getDrugRegimen().getFirstLine());

  //Calculate curerate 0
  double pSeekOfficialCareUncomplicated1 = healthSystem.getPSeekOfficialCareUncomplicated1().getValue();
  double pSelfTreatment = healthSystem.getPSelfTreatUncomplicated().getValue();
  if (pSeekOfficialCareUncomplicated1 + pSelfTreatment > 0) {
    double cureRateSelfTreatment = healthSystem.getInitialACR().getSelfTreatment().getValue();

    cureRate[0] = (cureRate[0] * pSeekOfficialCareUncomplicated1
                   + cureRateSelfTreatment * pSelfTreatment)
                  / (pSeekOfficialCareUncomplicated1 + pSelfTreatment);
  }

  cureRate[1] = getHealthSystemACRByName (healthSystem.getInitialACR(),
                                          healthSystem.getDrugRegimen().getSecondLine());

  cureRate[2] = getHealthSystemACRByName (healthSystem.getInitialACR(),
                                          healthSystem.getDrugRegimen().getInpatient());


  // --- calculate probGetsTreatment ---

  probGetsTreatment[0] = healthSystem.getPSeekOfficialCareUncomplicated1().getValue() + healthSystem.getPSelfTreatUncomplicated().getValue();
  probGetsTreatment[1] = healthSystem.getPSeekOfficialCareUncomplicated2().getValue();
  probGetsTreatment[2] = healthSystem.getPSeekOfficialCareSevere().getValue();


  // --- calculate probParasitesCleared ---

  string firstLineDrug = healthSystem.getDrugRegimen().getFirstLine();
  string secondLineDrug = healthSystem.getDrugRegimen().getSecondLine();
  pSeekOfficialCareUncomplicated1 = healthSystem.getPSeekOfficialCareUncomplicated1().getValue();

  double complianceFirstLine = getHealthSystemACRByName (healthSystem.getCompliance(), firstLineDrug);
  double complianceSecondLine = getHealthSystemACRByName (healthSystem.getCompliance(), secondLineDrug);

  double cureRateFirstLine = getHealthSystemACRByName (healthSystem.getInitialACR(), firstLineDrug);
  double cureRateSecondLine = getHealthSystemACRByName (healthSystem.getInitialACR(), secondLineDrug);

  double nonCompliersEffectiveFirstLine = getHealthSystemACRByName (healthSystem.getNonCompliersEffective(), firstLineDrug);
  double nonCompliersEffectiveSecondLine = getHealthSystemACRByName (healthSystem.getNonCompliersEffective(), secondLineDrug);

  pSelfTreatment = healthSystem.getPSelfTreatUncomplicated().getValue();
  double complianceSelfTreatment = healthSystem.getCompliance().getSelfTreatment().getValue();
  double cureRateSelfTreatment = healthSystem.getInitialACR().getSelfTreatment().getValue();

  //calculate probParasitesCleared 0
  if ( (pSeekOfficialCareUncomplicated1 + pSelfTreatment) > 0) {
    probParasitesCleared[0] = (pSeekOfficialCareUncomplicated1
                               * (complianceFirstLine * cureRateFirstLine
                                  + (1 - complianceFirstLine) * nonCompliersEffectiveFirstLine)
                               + pSelfTreatment
                               * (complianceSelfTreatment * cureRateSelfTreatment
                                  + (1 - complianceSelfTreatment) * nonCompliersEffectiveFirstLine))
                              / (pSeekOfficialCareUncomplicated1 + pSelfTreatment);
  } else {
    probParasitesCleared[0] = 0;
  }

  //calculate probParasitesCleared 1
  probParasitesCleared[1] = complianceSecondLine * cureRateSecondLine
                            + (1 - complianceSecondLine)
                            * nonCompliersEffectiveSecondLine;

  //calculate probParasitesCleared 2 : cool :)
  probParasitesCleared[2] = 0;
}

} }