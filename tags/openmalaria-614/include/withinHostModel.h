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

#ifndef Hmod_whost
#define Hmod_whost

#include "global.h"
#include "Infection.h"

#include <iostream>
#include <list>

using namespace std;

class Human;
class Event;

/*! Within Host Model abstract class.
 * Dont forget to create friend << and >> for subclasses.
 */
class WithinHostModel {
public:
  /// Initialise static parameters
  static void init();
  
  /// Free memory
  static void clear();
  
  static WithinHostModel* createWithinHostModel ();
  
  WithinHostModel() :
    _cumulativeInfections(0)
  {}
  
  virtual void update(double age) =0;

  virtual void summarize(double age) =0;
  
  //! Create a new infection requires that the human is allocated and current
  virtual void newInfection() =0;
  /*!  Clears all infections which have expired (their startdate+duration is less
  than the current time). */
  virtual void clearOldInfections() =0;
  /** Conditionally clear all infections.
   * Just calls clearAllInfections if IPT isn't present.
   * NOTE: replace Event& with minimal info needed.
   * NOTE: make clearAllInfections protected and replace with this? */
  virtual void IPTClearInfections (Event&);
  //! Clears all infections in an individual
  virtual void clearAllInfections() =0;
  
  virtual void medicate(string drugName, double qty, int time) =0;

  virtual void calculateDensities(Human&) =0;
  
  //! Returns Cumulative Infections
  int getCumulativeInfections() {return _cumulativeInfections;};
  
  /// Only do anything when IPT is present:
  //@{
  /// Conditionally set last SP dose
  virtual void IPTSetLastSPDose (int agetstep, int ageGroup) {}
  /// Prescribe IPTi with probability compliance. Only called if IPT present.
  virtual void IPTiTreatment (double compliance, int ageGroup) {}
  //@}
  
  /*! Until now, this only includes decay of immunity against
  asexual blood stages */
  virtual void updateImmuneStatus() =0;
  
  virtual void immunityPenalisation() =0;
  
  virtual void write(ostream& out) const =0;
  virtual void read(istream& in) =0;
  
protected:
  //!Cumulative number of infections since birth
  int _cumulativeInfections;
  
  /* Static private */
  
  //! Relative weights by age group
  /** Relative weights, based on data in InputTables\wt_bites.csv 
  The data are for Kilombero, Tanzania, taken from the Keiser et al (diploma
  thesis). The original source was anthropometric studies by Inez Azevedo Reads
  in weights by age group. The weights are expressed as proportions of 0.5*those
  in the reference age group. */
  static const double wtprop[nwtgrps];
  
//Standard dev innate immunity for densities
  static double sigma_i;
// contribution of parasite densities to acquired immunity in the presence of fever
  static double immPenalty_22;
/*
  Remaining immunity against asexual parasites(after time step, each of 2 components y and h)
  This variable decays the effectors cumulativeH and cumulativeY in a way that their
  effects on densities (1-Dh and 1-Dy) decay exponentially.
*/
  static double asexImmRemain;
/*
  Remaining immunity against asexual parasites(after each time step, each of 2 components y and h)
  This variable decays the effectors cumulativeH and cumulativeY exponentially.
*/
  static double immEffectorRemain;
};

#endif
