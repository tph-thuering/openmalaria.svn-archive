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

#ifndef CONSTANT_H
#define CONSTANT_H

/** Flags signalling which versions of some models to use. */
enum ModelVersion {
  /** @brief Clinical episodes reduce the level of acquired immunity
   * 
   * Effective cumulative exposure to blood stage parasites is reduced during a
   * clinical episode, so that clinical episodes have a negative effect on
   * blood stage immunity.
   * 
   * Default: Clinical events have no effect on immune status except
   * secondarily via effects of treatment. */
  PENALISATION_EPISODES = 1 << 1,
  
  /** @brief Baseline availability of humans is sampled from a gamma distribution
   * Infections introduced by mass action with negative binomial
   * variation in numbers of infection and no acquired preerythrocytic immunity.
   * 
   * Default: New infections are introduced via a Poisson process as described
   * in AJTMH 75 (suppl 2) pp11-18. */
  NEGATIVE_BINOMIAL_MASS_ACTION = 1 << 2,
  
  /** @brief 
   * 
   * Does nothing if IPT is not present. */
  ATTENUATION_ASEXUAL_DENSITY = 1 << 3,
  
  /** @brief Baseline availability of humans is sampled from a log normal distribution
   * 
   * Infections introduced by mass action with log normal variation in
   * infection rate with no acquired preerythrocytic immunity.
   * 
   * Default: New infections are introduced via a Poisson process as described
   * in AJTMH 75 (suppl 2) pp11-18. */
  LOGNORMAL_MASS_ACTION = 1 << 4,
  
  /** Infections introduced by mass action with log normal variation in
   * infection rate modulated by preerythrocytic immunity.
   * 
   * Default: New infections are introduced via a Poisson process as described
   * in AJTMH 75 (suppl 2) pp11-18. */
  LOGNORMAL_MASS_ACTION_PLUS_PRE_IMM = 1 << 5,
  
  /// BugFix in previous versions.  This option is not currently implemented.
  // @{
  MAX_DENS_CORRECTION = 1 << 6,
  INNATE_MAX_DENS = 1 << 7,
  MAX_DENS_RESET = 1 << 8,
  //@}
  
  /** @brief Parasite densities are predicted from an autoregressive process
   *
   * Default: Parasite densities are determined from the descriptive model
   * given in AJTMH 75 (suppl 2) pp19-31 .*/
  DUMMY_WITHIN_HOST_MODEL = 1 << 9,
  
  /** Clinical episodes occur if parasitaemia exceeds the pyrogenic threshold.
   * 
   * Default: Clinical episodes are a stochastic function as described in AJTMH
   * 75 (suppl 2) pp56-62. */
  PREDETERMINED_EPISODES = 1 << 10,
  
  /** @brief The presentation model includes simulation of non-malaria fevers
   * 
   * Default: Non-malaria fevers are not simulated. */
  NON_MALARIA_FEVERS = 1 << 11,
  
  /** @brief Pharmacokinetic and pharmacodynamics of drugs are simulated
   * 
   * Default: Drugs have all or nothing effects (except in certain IPTi
   * models). */
  INCLUDES_PK_PD = 1 << 12,
  
  /** @brief Use revised case management model
   * 
   * Default: use the Tediosi et al case management model (Case management as
   * described in AJTMH 75 (suppl 2) pp90-103. */
  CASE_MANAGEMENT_V2 = 1 << 13,
  
  /** @brief Clinical episodes occur in response to a simple parasite density trigger
   * 
   * Default: Use the Ross et al presentation model (Clinical episodes are a
   * stochastic function as described in AJTMH 75 (suppl 2) pp56-62). */
  MUELLER_PRESENTATION_MODEL = 1 << 14,
  
  /** @brief Simple heterogeneity
   * 
   * Defaults: No heterogeneity. */
  // @{
  /// @brief Allow simple heterogeneity in transmission
  TRANS_HET = 1 << 15,
  /// @brief Allow simple heterogeneity in comorbidity
  COMORB_HET = 1 << 16,
  /// @brief Allow simple heterogeneity in treatment seeking
  TREAT_HET = 1 << 17,
  /// @brief Allow correlated heterogeneities in transmission and comorbidity
  COMORB_TRANS_HET = 1 << 18,
  /// @brief Allow correlated heterogeneities in transmission and treatment seeking
  TRANS_TREAT_HET = 1 << 19,
  /// @brief Allow correlated heterogeneities comorbidity and treatment seeking
  COMORB_TREAT_HET = 1 << 20,
  /// @brief Allow correlated heterogeneities in transmission, comorbidity and treatment seeking
  TRIPLE_HET = 1 << 21,
  // @}
  
  // Used by tests; should be 1 plus highest left-shift value of 1
  NUM_VERSIONS = 22,
};

namespace Diagnosis {
  enum Value { NON_MALARIA_FEVER,
               UNCOMPLICATED_MALARIA,
               SEVERE_MALARIA,
               INDIRECT_MALARIA_DEATH };
}

namespace Outcome {
/*
  Possibilities for outcomes are:
  for non-treated
*/
  enum Value {
    
    // non treatedsimulationMode
    NO_CHANGE_IN_PARASITOLOGICAL_STATUS_NON_TREATED,
    //for outpatients
    NO_CHANGE_IN_PARASITOLOGICAL_STATUS_OUTPATIENTS,
    //for inpatients
    NO_CHANGE_IN_PARASITOLOGICAL_STATUS_INPATIENTS,
    //for non-treated
    PARASITES_ARE_CLEARED_PATIENT_RECOVERS_NON_TREATED,
    //for outpatients
    PARASITES_ARE_CLEARED_PATIENT_RECOVERS_OUTPATIENTS,
    //for inpatients
    PARASITES_ARE_CLEARED_PATIENT_RECOVERS_INPATIENTS,
    //for non-treated
    PARASITES_ARE_CLEARED_PATIENT_HAS_SEQUELAE_NON_TREATED,
    //for inpatients
    PARASITES_ARE_CLEARED_PATIENT_HAS_SEQUELAE_INPATIENTS,
    //for non-treated
    PARASITES_NOT_CLEARED_PATIENT_HAS_SEQUELAE_NON_TREATED,
    //for inpatients
    PARASITES_NOT_CLEARED_PATIENT_HAS_SEQUELAE_INPATIENTS,
    //for non-treated
    PATIENT_DIES_NON_TREATED,
    //for inpatients
    PATIENT_DIES_INPATIENTS,
    INDIRECT_DEATH,
    //for outpatients in models of pk/pD
    PARASITES_PKPD_DEPENDENT_RECOVERS_OUTPATIENTS
  };
}

namespace Params {
  enum Params {
    /// Used in NoVectorControl
    //@{
    NEG_LOG_ONE_MINUS_SINF = 1,
    E_STAR = 2,
    SIMM = 3,
    X_STAR_P = 4,
    GAMMA_P = 5,
    //@}
    
    SIGMA_I_SQ = 6,			///< Used in Human
    
    /// Used in Infection
    //@{
    CUMULATIVE_Y_STAR = 7,
    CUMULATIVE_H_STAR = 8,
    NEG_LOG_ONE_MINUS_ALPHA_M = 9,
    DECAY_M = 10,
    SIGMA0_SQ = 11,
    X_NU_STAR = 12,
    //@}
    
    /// Used in Human
    //@{
    Y_STAR_SQ = 13,
    ALPHA = 14,
    DENSITY_BIAS_NON_GARKI = 15,
    BASELINE_AVAILABILITY_SHAPE = 16,	///< Also used in TransmissionModel
    //@}
    
    LOG_ODDS_RATIO_CF_COMMUNITY = 17,	///< Used in CaseManagementModel
    INDIRECT_RISK_COFACTOR = 18,	///< Used in Human
    NON_MALARIA_INFANT_MORTALITY = 19,	///< Used in Summary
    
    /// Used in Human
    //@{
    DENSITY_BIAS_GARKI = 20,
    SEVERE_MALARIA_THRESHHOLD = 21,
    IMMUNITY_PENALTY = 22,
    IMMUNE_EFFECTOR_DECAY = 23,
    COMORBIDITY_INTERCEPT = 24,
    Y_STAR_HALF_LIFE = 25,
    Y_STAR_1 = 26,
    ASEXUAL_IMMUNITY_DECAY = 27,
    Y_STAR_0 = 28,
    
    CRITICAL_AGE_FOR_COMORBIDITY = 30,
    MUELLER_RATE_MULTIPLIER = 31,
    MUELLER_DENSITY_EXPONENT = 32,
    //@}
    
    MAX
  };
}

/** Value used as the timestep for an event which has never happened.
 *
 * For any simulation timestep, we must have:
 * ( TIMESTEP_NEVER + simulationTime < 0 )
 * but since (x - TIMESTEP_NEVER >= y) is often checked, x - TIMESTEP_NEVER
 * must not overflow for any timestep x (int represents down to -0x7FFFFFFF).
 */
const int TIMESTEP_NEVER = -0x3FFFFFFF;

/// Days in a year. Should be a multiple of interval.
const int daysInYear= 365;

/** There are 3 simulation modes. */
enum SimulationMode {
  /** Equilibrium mode
   * 
   * This is used for the warm-up period and if we want to separate direct
   * effect of an intervention from indirect effects via transmission
   * intensity. The seasonal pattern and intensity of the EIR do not change
   * over years. */
  equilibriumMode = 2,
  /** Transient EIR known
   * 
   * This is used to simulate an intervention that changes EIR, and where we
   * have measurements of the EIR over time during the intervention period. */
  transientEIRknown = 3,
  /** EIRchanges
   * 
   * The EIR changes dynamically during the intervention phase as a function of
   * the characteristics of the interventions. */
  dynamicEIR = 4,
};

/// The mean of the base line availability, is used by both human.f and entomology.f
const double BaselineAvailabilityMean= 1.0;

/** The relative risk of non malaria fever is used in human.f
 * TODO: This is currently set arbitrarily to 1.0, but should be defined in the
 * .xml */
const double RelativeRiskNonMalariaFever= 1.0;

  //NOTE: relocated to avoid circular dependency. Age-group stuff should be central..
  /* nwtgrps is the number of age groups for which expected weights are read in
  for use in the age adjustment of the EIR.
  It is used both by Human and TransmissionModel. */
  static const int nwtgrps= 27; 

#endif
