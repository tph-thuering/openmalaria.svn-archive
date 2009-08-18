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
#ifndef Hmod_population
#define Hmod_population
#include "global.h"
#include <list>
#include <fstream>
#include <vector>

// Forward declarations
class Human;
class TransmissionModel;
namespace scnXml {
  class Mass;
}

//! The simulated human population
class Population{
public:
  Population();
  //! Clears human collection.
  ~Population();
  
  /// Call static inits of sub-models
  static void init();
  
  /// Calls static clear on sub-models to free memory
  static void clear();
  
  /** Initialisation run between initial one-lifespan run of simulation and
   * actual simulation. */
  void preMainSimInit ();

   /*! Estimates demography parameters to define a smooth curve for the target
      population age-distribution (age in years) */
  void estimateRemovalRates();

  /*! Takes the best-fitting demography parameters estimated by
      estimateRemovalRates and sets up the initial population according to
      these */
  void setupPyramid(bool isCheckpoint);

  //! Initialise arrays for recording infant mortality
  void initialiseInfantArrays();
 
  //! Write checkpoint
  void write (ostream& out);
  /// Read checkpoint
  void read (istream& in);
  
  //! Updates all individuals in the list for one time-step
  /*!  Also updates the population-level measures such as infectiousness, and
       the age-distribution by c outmigrating or creating new births if
       necessary */
  void update1();
  
  //! Checks for time-based interventions and implements them
  /*!   
       \param time Current time (in tsteps) 
  */
  void implementIntervention(int time);
  
  //! Initialise human list
  void initialiseHumanList();
  
  //! Creates initializes and add to the population list a new uninfected human
  /*! 
     \param dob date of birth (usually current time)
  */
  void newHuman(int dob);

  //! Makes a survey
  void newSurvey();
 
  /** Determine whether to remove the human current in order to preserve or
   * obtain the desired age structure.
   * 
   * @param current The human in question.
   * @param targetPop The total target population size at this time
   * @param cumPop The total number of individuals younger than or the same age
   *	as this individual (including this individual).
   *
   * @returns True if current should be removed from the population. */
  bool outMigrate(Human& current, int targetPop, int cumPop);
  
  // Static:
  /** For input values for alpha1 and mu1, the fit to field data (residualSS)
   * is calculated and returned function called iteratively by
   * estimateRemovalRates. */
  static double setDemoParameters (double param1, double param2);
  
private:
  /** Generic function to activate some intervention on all humans within the
   * age range and passing the compliance test given by mass.
   * 
   * @param mass XML element specifying the age range and compliance
   *	(proportion of eligible individuals who receive the intervention).
   * @param intervention A member-function pointer to a "void func ()" function
   *	within human which activates the intervention. */
  void massIntervention (const scnXml::Mass& mass, void (Human::*intervention)());
  
  /** This is the maximum age of an individual that the simulation program can
   * handle. Max age for a scenario is given in the  the xml file. */
  static const int maxLifetimeDays= 32855;
  static const int ngroups= 20;

  /** The bounds for each age group and percentage of population in this age
   * group for the field data demography age groups.
   * 
   * ageGroupBounds[i] is the lower-bound for group i, ageGroupBounds[i+1] is
   * the group's upper bound. ageGroupPercent[i] is the percentage of the
   * population in age group i.
   * 
   * Set by estimateRemovalRates() and used internally (by
   * setDemoParameters()). */
  //@{
  static double ageGroupBounds[ngroups+1];
  static double ageGroupPercent[ngroups];
  //@}
  /** Demography variables used in estimating the smooth curve.
   * 
   * Only used in setDemoParameters() calculations. */
  //@{
  static double M1[ngroups];
  static double M2[ngroups];
  static double M[ngroups];
  static double pred[ngroups];
  //@}
  /** Parameters defining smooth curve of target age-distribution.
   * 
   * Set by estimateRemovalRates() (via setDemoParameters()) and used by
   * setupPyramid(). Checkpointed. */
  //@{
  static double mu0;
  static double mu1;
  static double alpha0;
  static double alpha1;
  static double rho;
  //@}
  
  /// Target cumulative percentage of population by age, from oldest age to youngest.
  //TODO5D
  static double *cumpc;

  /// ID passed to last Human created. Checkpointed.
  static int IDCounter;

  //! Size of the human population   
  int _populationSize;

  //! TransmissionModel model
  TransmissionModel* _transmissionModel;
  
  /** The simulated human population
   *
   * The list of all humans, ordered from oldest to youngest. */
  std::list<Human> _population;
  
  /// Iterator type of _population
  typedef std::list<Human>::iterator HumanIter;

  //! max lifespan in intervals
  int _maxTimestepsPerLife;

  /*!
    this is needed to prevent checkpoint cheats. Ideally a unique identifier per workunit, but a
    random integer number should do the job
  */
  int _workUnitIdentifier;
  
  friend class VectorSpeciesSuite;
};

#endif
