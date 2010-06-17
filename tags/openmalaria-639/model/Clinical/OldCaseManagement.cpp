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
#include "Monitoring/Surveys.h"
#include "util/random.h"
#include "util/ModelOptions.hpp"
#include "util/errors.hpp"

#include <cmath>

namespace OM { namespace Clinical {
    using namespace OM::util;

const int OldCaseManagement::SEQUELAE_AGE_BOUND[NUM_SEQUELAE_AGE_GROUPS] = { 5, 99 };
double OldCaseManagement::probGetsTreatment[3];
double OldCaseManagement::probParasitesCleared[3];
double OldCaseManagement::cureRate[3];
double OldCaseManagement::probSequelaeTreated[NUM_SEQUELAE_AGE_GROUPS];
double OldCaseManagement::probSequelaeUntreated[NUM_SEQUELAE_AGE_GROUPS];


// -----  static init  -----

void OldCaseManagement::init ()
{
    if (util::ModelOptions::option (util::INCLUDES_PK_PD))
	throw util::xml_scenario_error ("OldCaseManagement is not compatible with INCLUDES_PK_PD");
}

void OldCaseManagement::setHealthSystem (const scnXml::HealthSystem& healthSystem) {
    if( !healthSystem.getImmediateOutcomes().present() )
	throw util::xml_scenario_error ("Expected ImmediateOutcomes section in healthSystem data (initial or intervention)");
    const scnXml::HSImmediateOutcomes& immediateOutcomes = healthSystem.getImmediateOutcomes().get();
    
    setParasiteCaseParameters (immediateOutcomes);
    
    const scnXml::ByAgeItems::ItemSequence& pSeqGroups = immediateOutcomes.getPSequelaeInpatient().getItem();
    /* Note: Previously age groups specified in the XML were remapped; this was misleading
    (age groups could be ignored or have different bounds). To avoid letting non-corresponding
    entries now have a different effect, we check the bounds correspond _exactly_ to what we expect
    (and appears to have always been the case). */
    if (pSeqGroups.size() != NUM_SEQUELAE_AGE_GROUPS)
	throw util::xml_scenario_error ("Expected: 2 pSequelaeInpatient age groups with maxAgeYrs 5 and 99");
    for (size_t agegrp = 0; agegrp < NUM_SEQUELAE_AGE_GROUPS; agegrp++) {
	if (pSeqGroups[agegrp].getMaxAgeYrs() != SEQUELAE_AGE_BOUND[agegrp])
	    throw util::xml_scenario_error ("Expected: 2 pSequelaeInpatient age groups with maxAgeYrs 5 and 99");
	
	probSequelaeTreated[agegrp] = probSequelaeUntreated[agegrp] = pSeqGroups[agegrp].getValue();
    }
}


// -----  non-static construction/desctruction  -----

OldCaseManagement::OldCaseManagement (double tSF) :
    _tLastTreatment (Global::TIMESTEP_NEVER), _treatmentSeekingFactor (tSF)
{
}
OldCaseManagement::~OldCaseManagement()
{
}

// -----  other public  -----

void OldCaseManagement::doCaseManagement (Pathogenesis::State pgState, WithinHost::WithinHostModel& withinHostModel, Episode& latestReport, double ageYears, Monitoring::AgeGroup ageGroup, int& doomed)
{
  bool effectiveTreatment = false;

  if (pgState & Pathogenesis::MALARIA) {
    if (pgState & Pathogenesis::COMPLICATED)
      effectiveTreatment = severeMalaria (latestReport, ageYears, ageGroup, doomed);
    else if (pgState == Pathogenesis::STATE_MALARIA) {
      // NOTE: if condition means this doesn't happen if INDIRECT_MORTALITY is
      // included. Validity is debatable, but there's no point changing now.
      // (This does affect tests.)
      effectiveTreatment = uncomplicatedEvent (latestReport, true, ageYears, ageGroup);
    }

    if ((pgState & Pathogenesis::INDIRECT_MORTALITY) && doomed == 0)
      doomed = -Global::interval;

    if (util::ModelOptions::option (util::PENALISATION_EPISODES)) {
      withinHostModel.immunityPenalisation();
    }
  } else if (pgState & Pathogenesis::SICK) { // sick but not from malaria
    effectiveTreatment = uncomplicatedEvent (latestReport, false, ageYears, ageGroup);
  }

  if (effectiveTreatment) {
      withinHostModel.clearInfections (latestReport.getState() == Pathogenesis::STATE_SEVERE);
  }
}


// -----  private  -----

bool OldCaseManagement::uncomplicatedEvent (Episode& latestReport, bool isMalaria, double ageYears, Monitoring::AgeGroup ageGroup)
{
    Regimen::Type regimen = (_tLastTreatment + Episode::healthSystemMemory > Global::simulationTime) ? Regimen::UC2 : Regimen::UC;
    bool successfulTreatment = false;
    
    if (probGetsTreatment[regimen]*_treatmentSeekingFactor > (random::uniform_01())) {
	_tLastTreatment = Global::simulationTime;
	if ( regimen == Regimen::UC )
	    Monitoring::Surveys.current->reportTreatments1( ageGroup, 1 );
	if ( regimen == Regimen::UC2 )
	    Monitoring::Surveys.current->reportTreatments2( ageGroup, 1 );
	
	if (probParasitesCleared[regimen] > random::uniform_01()) {
	successfulTreatment = true;	// Parasites are cleared
	// We don't report out-of-hospital recoveries, so this wouldn't do anything extra:
	//entrypoint = Pathogenesis::State (entrypoint | Pathogenesis::RECOVERY);
	} else {
	// No change in parasitological status: treated outside of hospital
	}
    } else {
	// No change in parasitological status: non-treated
    }
    
    Pathogenesis::State entrypoint = isMalaria ? Pathogenesis::STATE_MALARIA : Pathogenesis::SICK;
    latestReport.update (Global::simulationTime, ageGroup, entrypoint);
    
    return successfulTreatment;
}

bool OldCaseManagement::severeMalaria (Episode& latestReport, double ageYears, Monitoring::AgeGroup ageGroup, int& doomed)
{
  BOOST_STATIC_ASSERT (NUM_SEQUELAE_AGE_GROUPS == 2);	// code setting sequelaeIndex assumes this
  size_t sequelaeIndex = 0;
  if (ageYears >= SEQUELAE_AGE_BOUND[0]) {
    sequelaeIndex = 1;
  }
  
  Regimen::Type regimen = Regimen::SEVERE;

  double p2, p3, p4, p5, p6, p7;
  // Probability of getting treatment (only part which is case managment):
  p2 = probGetsTreatment[regimen] * _treatmentSeekingFactor;
  // Probability of getting cured after getting treatment:
  p3 = cureRate[regimen];
  // p4 is the hospital case-fatality rate from Tanzania
  p4 = caseFatality (ageYears);
  // p5 here is the community threshold case-fatality rate
  p5 = getCommunityCaseFatalityRate (p4);
  p6 = probSequelaeTreated[sequelaeIndex];
  p7 = probSequelaeUntreated[sequelaeIndex];

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

  double prandom = random::uniform_01();

  if (q[2] <= prandom) { // Patient gets in-hospital treatment
    _tLastTreatment = Global::simulationTime;
    Monitoring::Surveys.current->reportTreatments3( ageGroup, 1 );

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

void OldCaseManagement::setParasiteCaseParameters (const scnXml::HSImmediateOutcomes& hsioData)
{
  // --- calculate cureRate ---

  //We get the ACR depending on the name of firstLineDrug.
  cureRate[0] = getHealthSystemACRByName (hsioData.getInitialACR(),
                                          hsioData.getDrugRegimen().getFirstLine());

  //Calculate curerate 0
  double pSeekOfficialCareUncomplicated1 = hsioData.getPSeekOfficialCareUncomplicated1().getValue();
  double pSelfTreatment = hsioData.getPSelfTreatUncomplicated().getValue();
  if (pSeekOfficialCareUncomplicated1 + pSelfTreatment > 0) {
    double cureRateSelfTreatment = hsioData.getInitialACR().getSelfTreatment().getValue();

    cureRate[0] = (cureRate[0] * pSeekOfficialCareUncomplicated1
                   + cureRateSelfTreatment * pSelfTreatment)
                  / (pSeekOfficialCareUncomplicated1 + pSelfTreatment);
  }

  cureRate[1] = getHealthSystemACRByName (hsioData.getInitialACR(),
                                          hsioData.getDrugRegimen().getSecondLine());

  cureRate[2] = getHealthSystemACRByName (hsioData.getInitialACR(),
                                          hsioData.getDrugRegimen().getInpatient());


  // --- calculate probGetsTreatment ---

  probGetsTreatment[0] = hsioData.getPSeekOfficialCareUncomplicated1().getValue() + hsioData.getPSelfTreatUncomplicated().getValue();
  probGetsTreatment[1] = hsioData.getPSeekOfficialCareUncomplicated2().getValue();
  probGetsTreatment[2] = hsioData.getPSeekOfficialCareSevere().getValue();


  // --- calculate probParasitesCleared ---

  string firstLineDrug = hsioData.getDrugRegimen().getFirstLine();
  string secondLineDrug = hsioData.getDrugRegimen().getSecondLine();
  pSeekOfficialCareUncomplicated1 = hsioData.getPSeekOfficialCareUncomplicated1().getValue();

  double complianceFirstLine = getHealthSystemACRByName (hsioData.getCompliance(), firstLineDrug);
  double complianceSecondLine = getHealthSystemACRByName (hsioData.getCompliance(), secondLineDrug);

  double cureRateFirstLine = getHealthSystemACRByName (hsioData.getInitialACR(), firstLineDrug);
  double cureRateSecondLine = getHealthSystemACRByName (hsioData.getInitialACR(), secondLineDrug);

  double nonCompliersEffectiveFirstLine = getHealthSystemACRByName (hsioData.getNonCompliersEffective(), firstLineDrug);
  double nonCompliersEffectiveSecondLine = getHealthSystemACRByName (hsioData.getNonCompliersEffective(), secondLineDrug);

  pSelfTreatment = hsioData.getPSelfTreatUncomplicated().getValue();
  double complianceSelfTreatment = hsioData.getCompliance().getSelfTreatment().getValue();
  double cureRateSelfTreatment = hsioData.getInitialACR().getSelfTreatment().getValue();

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
