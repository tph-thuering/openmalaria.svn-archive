/* This file is part of OpenMalaria.
 * 
 * Copyright (C) 2005-2009 Swiss Tropical Institute and Liverpool School Of Tropical Medicine
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


//parse the xml scenario file
//

#ifndef INPUTDATABOINC_H
#define INPUTDATABOINC_H

#include "Global.h"
#include "util/BoincWrapper.h"
#include <scenario.hxx>
#include <string>
#include <bitset>

namespace OM {

namespace Params {
  enum Params {
    /// @b Used in NoVectorControl
    //@{
    NEG_LOG_ONE_MINUS_SINF = 1,
    E_STAR = 2,
    SIMM = 3,
    X_STAR_P = 4,
    GAMMA_P = 5,
    //@}
    SIGMA_I_SQ = 6,			///< Used in WithinHostModel
    /// @b Used in Infection
    //@{
    CUMULATIVE_Y_STAR = 7,
    CUMULATIVE_H_STAR = 8,
    NEG_LOG_ONE_MINUS_ALPHA_M = 9,
    DECAY_M = 10,
    SIGMA0_SQ = 11,
    X_NU_STAR = 12,
    //@}
    /// @b Used in PathogenesisModel
    //@{
    Y_STAR_SQ = 13,
    ALPHA = 14,
    //@}
    DENSITY_BIAS_NON_GARKI = 15,	///< Used in WithinHostModel
    BASELINE_AVAILABILITY_SHAPE = 16,	///< Used in InfectionIncidenceModel
    LOG_ODDS_RATIO_CF_COMMUNITY = 17,	///< Used in CaseManagementModel
    INDIRECT_RISK_COFACTOR = 18,	///< Used in PathogenesisModel
    NON_MALARIA_INFANT_MORTALITY = 19,	///< Used in Summary
    DENSITY_BIAS_GARKI = 20,		///< Used in WithinHostModel
    SEVERE_MALARIA_THRESHHOLD = 21,	///< Used in PathogenesisModel
    IMMUNITY_PENALTY = 22,		///< Used in WithinHostModel
    IMMUNE_EFFECTOR_DECAY = 23,		///< Used in WithinHostModel
    /// @b Used in PathogenesisModel
    //@{
    COMORBIDITY_INTERCEPT = 24,
    Y_STAR_HALF_LIFE = 25,
    Y_STAR_1 = 26,
    //@}
    ASEXUAL_IMMUNITY_DECAY = 27,	///< Used in WithinHostModel
    /// @b Used in PathogenesisModel
    //@{
    Y_STAR_0 = 28,
    
    CRITICAL_AGE_FOR_COMORBIDITY = 30,
    MUELLER_RATE_MULTIPLIER = 31,
    MUELLER_DENSITY_EXPONENT = 32,
    //@}
    /// v in "Case Fatality Rate proposal" TODO: reference
    CFR_PAR_REDUCTION_SCALAR = 33,
    // Parameters fitting for Molineaux within host model
    MEAN_LOCAL_MAX_DENSITY = 34,
    SD_LOCAL_MAX_DENSITY = 35,
    MEAN_DIFF_POS_DAYS = 36,
    SD_DIFF_POS_DAYS = 37,
    MAX
  };
}

/** Used to describe which interventions are in use. */
namespace Interventions {
    enum Flags {
	CHANGE_HS,
	CHANGE_EIR,
	IMPORTED_INFECTIONS,
	VACCINE,	// any vaccine
	MDA,
	IPTI,
	ITN,
	IRS,
	VEC_AVAIL,
	LARVICIDING,
	SIZE
    };
}
    
    
    class InputDataType {
    public:
	InputDataType () : scenario(NULL) {}
	
	/** @brief Reads the document in the xmlFile
	* 
	* Throws on failure. */
	util::Checksum createDocument(std::string);

	/**
	* Some elements in memory have been created. This function deletes the object in memory
	*/
	void cleanDocument();
	
	/** Get the base scenario element.
	 *
	 * Is an operator for brevity: InputData().getModel()...
	 */
	inline const scnXml::Scenario& operator()() {
	    return *scenario;
	}

	/** Get a mutable version of scenario element.
	*
	* This is the only entry point for changing the scenario document.
	* 
	* You should set "documentChanged = true;" if you want your changes saved. */
	inline scnXml::Scenario& getMutableScenario() {
	    return *scenario;
	}

	/// Get the intervention from interventions->timed with time time.
	/// @returns NULL if not available
	const scnXml::Intervention* getInterventionByTime(int time);

	/// Returns and enum representing which interventions are active.
	const bitset<Interventions::SIZE> getActiveInterventions ();


	/// Get a parameter from the parameter list. i should be less than Params::MAX.
	double getParameter (size_t i);

	/** Set true if the xml document has been changed and should be saved.
	 *
	 * Currently this is unused. If it is used again, we should be careful about checkpointing since
	 * none of the scenario element is currently checkpointed. */
	bool documentChanged;

	
    private:
	void initParameterValues ();
	void initTimedInterventions ();
	
	/// Sometimes used to save changes to the xml.
	std::string xmlFileName;
	
	/** @brief The xml data structure. */
	scnXml::Scenario* scenario;
	
	// Initialized (derived) values:
	std::map<int, double> parameterValues;
	std::map<int, const scnXml::Intervention*> timedInterventions;
	bitset<Interventions::SIZE> activeInterventions;
    };
    /// InputData entry point.
    extern InputDataType InputData;
}
#endif
