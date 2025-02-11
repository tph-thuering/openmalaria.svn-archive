/*
 This file is part of OpenMalaria.
 
 Copyright (C) 2005-2010 Swiss Tropical Institute and Liverpool School Of Tropical Medicine
 
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

#include "Clinical/CaseManagementCommon.h"
#include "Clinical/OldCaseManagement.h"
#include "Clinical/ESCaseManagement.h"
#include "inputData.h"
#include "util/ModelOptions.h"
#include "util/errors.h"
#include "util/XmlUtils.h"
#include <limits>
#include <boost/format.hpp>

namespace OM { namespace Clinical {
    int CaseManagementCommon::healthSystemSource;
    map<double,double> CaseManagementCommon::caseFatalityRates;
    double CaseManagementCommon::_oddsRatioThreshold;
    map<double,double> CaseManagementCommon::pSeqInpatData;
    
    // -----  functions  -----
    
    void CaseManagementCommon::initCommon (){
	_oddsRatioThreshold = exp (InputData.getParameter (Params::LOG_ODDS_RATIO_CF_COMMUNITY));
	
	changeHealthSystem(-1);
    }
    
    void CaseManagementCommon::changeHealthSystem (int source) {
	healthSystemSource = source;
	const scnXml::HealthSystem& healthSystem = getHealthSystem ();
	
	readCommon (healthSystem);
	
	if (util::ModelOptions::option (util::CLINICAL_EVENT_SCHEDULER))
	    ESCaseManagement::setHealthSystem(healthSystem);
	else
	    OldCaseManagement::setHealthSystem(healthSystem);
    }
    
    const scnXml::HealthSystem& CaseManagementCommon::getHealthSystem () {
	if (healthSystemSource == -1) {
	    return InputData().getHealthSystem();
	} else {
	    const scnXml::Intervention* interv = InputData.getInterventionByTime (healthSystemSource);
	    if (interv == NULL || !interv->getChangeHS().present())
		throw runtime_error ("healthSystemSource invalid");
	    return interv->getChangeHS().get();
	}
	assert(false);	// unreachable
    }
    
    void CaseManagementCommon::readCommon (const scnXml::HealthSystem& healthSystem)
    {
	// -----  case fatality rates  -----
	caseFatalityRates.clear();	// Necessary when re-read from an intervention
	util::XmlUtils::lboundGroups2map(
	    caseFatalityRates,
	    healthSystem.getCFR().getGroup(),
	    "CFR",
	    true
	);
	
	
	// -----  sequelae  -----
	pSeqInpatData.clear();
	util::XmlUtils::lboundGroups2map(
	    pSeqInpatData,
	    healthSystem.getPSequelaeInpatient().getGroup(),
	    "pSequelaeInpatient"
	);
    }
    
    double CaseManagementCommon::caseFatality (double ageYears)
    {
	assert ( ageYears >= 0.0 );
	map<double,double>::const_iterator it = caseFatalityRates.upper_bound( ageYears );
	assert( it != caseFatalityRates.end() );
	double a1 = it->first;
	double f1 = it->second;
	--it;
	double a0 = it->first;	// a0 <=ageYears < a1
	double f0 = it->second;
	return (ageYears - a0) / (a1 - a0) * (f1 - f0) + f0;
    }
    
    double CaseManagementCommon::getCommunityCaseFatalityRate (double caseFatalityRatio)
    {
	double x = caseFatalityRatio * _oddsRatioThreshold;
	return x / (1 - caseFatalityRatio + x);
    }
    
    double CaseManagementCommon::pSequelaeInpatient(double ageYears){
	map<double,double>::const_iterator it = pSeqInpatData.upper_bound( ageYears );
	assert( it != pSeqInpatData.begin() );
	--it;
	return it->second;
    }

    void CaseManagementCommon::staticCheckpoint (ostream& stream) {
	healthSystemSource & stream;
    }
    void CaseManagementCommon::staticCheckpoint (istream& stream) {
	healthSystemSource & stream;
	if (healthSystemSource != -1)
	    changeHealthSystem( healthSystemSource );
    }
} }
