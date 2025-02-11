/*
 This file is part of OpenMalaria.
 
 Copyright (C) 2005-2009 Swiss Tropical Institute and Liverpool School Of Tropical Medicine
 
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

#ifndef Hmod_TransmissionModel 
#define Hmod_TransmissionModel 

#include "human.h"
#include <string.h>

// Define these to print out various arrays:
//#define TransmissionModel_PrintEIRaIDFT
//#define TransmissionModel_PrintSmoothArray
#define TransmissionModel_PrintRotateArray

class Summary;

//! Abstract base class, defines behaviour of transmission models
class TransmissionModel {
public:
  ///@brief Creation, destruction and checkpointing
  //@{
  /// Creates a derived class
  static TransmissionModel* createTransmissionModel ();
  
  //! Reads all entomological parameters from the input datafile. 
  TransmissionModel();
  //!Deallocate memory for TransmissionModel parameters and clean up
  virtual ~TransmissionModel();
  
  //FIXME: do implementing models need checkpointing?
  void write(ostream& out) const;
  void read(istream& in);
  //@}
  
  /// Set a couple of summary items
  void summarize (Summary&);
  
  //NOTE: doesn't really belong here
  /// Get the appropriate index within ageSpecificRelativeAvailability, etc.,
  /// for this age (in years). Also used by Human.
  static size_t  getAgeGroup (double age);
  
  //! get the number of infections for a specific human at a given time step 
  virtual double getExpectedNumberOfInfections (Human& human, double age_adj_EIR) = 0;

  //! Calculates the adjustment for body size in exposure to mosquitoes 
  /*! 
  The bites are assumed proportional to average surface area for hosts of the given age. 
  Linear interpolation is used to calculate this from the input array of surface areas. 
  \param ageyrs age in years 
  \return the ratio of bites received by the host to the average for an adult 
  */ 
  double getRelativeAvailability (double ageyrs); 


  //! initialise the main simulation
  virtual void initMainSimulation (int populationSize)=0; 
  
  /// Needs to be called each step of the simulation
  virtual void advancePeriod (const std::list<Human>& population, int simulationTime) {}

  /** @brief Initialization function, setting up EIR arrays
   *
   * Needs to be called after EIR data is changed ("changeEIR intervention").
   *
   * Reads in the estimates of the EIR for each village and each day
   * and converts this into EIR estimates per five day period
   * assuming that the annual cycle repeated during the pre-intervention period
   */
  virtual void inputEIR () {}
  
  /** Set kappa for current interval in year from infectiousness of humans.
   *
   * Also updates _annualAverageKappa.
   * 
   * NOTE: could be combined with advancePeriod(), but is currently called at
   * a different time. */
  void updateKappa (double sumWeight, double sumWt_kappa);
  
  /** Little function to copy kappa to initialKappa. */
  inline void copyToInitialKappa () {
    initialKappa = kappa;
  }
  
  /** Returns the EIR, per host and per time step.
   *
   * During the pre-intervention phase, the EIR is forced, using values from
   * the XML file (possibly generated from fourier coefficients). During the
   * main simulation phase, it may be calculated or obtained from data in the
   * XML file. */
  double getEIR (int simulationTime, PerHostTransmission& host);
  
protected:
  /** Calculates the EIR (in adults), during the main simulation phase.
   * 
   * \param simulationTime Time since start of simulation.
   * \param host The human to calculate EIR for (not used by all models). */
  virtual double calculateEIR(int simulationTime, PerHostTransmission& host) = 0; 
  
  /** EIR per time step during the pre-intervention phase 
   *
   * Only NonVectorTransmission?
   * Not checkpointed; doesn't need to be for NonVectorTransmission (unless
   * changeEIR intervention occurred). */
  vector<double> initialisationEIR; 

  /** kappa[] is the probability of infection of a mosquito at each bite.
   *
   * Checkpointed. */
  vector<double> kappa; 

  /** initialKappa[] is the value of kappa during the pre-intervention phase.
   * 
   * Doesn't need to be checkpointed. */
  vector<double> initialKappa; 
  
private:
  /*! annAvgKappa is the overall proportion of mosquitoes that get infected
   * allowing for the different densities in different seasons (approximating
   * relative mosquito density with the EIR).
   *
   * Checkpointed. */
  double _annualAverageKappa;
  
  /*! Used to calculate annAvgKappa.
   *
   * Checkpointed. */
  double _sumAnnualKappa;
  
protected:
  /*! Total annual EIR.
   *
   * Checkpointed. */
  double annualEIR; 

  //TODO: the entire code for specifying availability should be part of the human
  //! initialisation of the vector of expected surface area as a function of age
  void initAgeExposureConversion(); 
  
  
  /* Not used - uncomment code in initAgeExposureConversion() to initialise.
  // ratio of the number of bites received relative to the number received at age 6 years 
  double biteratio_6; */
  
  
  /** Duration of the extrinsic incubation period (sporozoite development time)
   * (θ_s).
   * Units: Days.
   * 
   * Doesn't need checkpointing. */
  int EIPDuration;
  
  //! Number of age groups for which the surface area calculations apply 
  static const size_t nages= 22;

  // NOTE: perhaps all these age-specific constants should be moved to Human. It depends which part of the simulation each class is meant to simulate...
  /** Average number of bites for each age as a proportion of the maximum.
   *
   * Set by constructor. */
  double ageSpecificRelativeAvailability[nwtgrps];

  //! Cutpoints of the age categories (minima) used for storing relative
  //! weights? surface areas?
  static const double agemin[nwtgrps]; 

  //! Cutpoints of the age categories (maxima) used for storing relative
  //! weights? surface areas?
  static const double agemax[nwtgrps];  

  //! Proportionate body surface area
 /* 
  The body surface area is expressed as proportions of 0.5*those in 
  the reference age group.In some models we have used calculations of weight and in others surface area, based on 
  Mosteller RD: Simplified Calculation of Body Surface Area. N Engl J Med 1987 Oct 22;317(17):1098 (letter) 
  These values are retained here should they be required for future comparisons 
 */ 
  static const double bsa_prop[nwtgrps]; 
};

#endif
