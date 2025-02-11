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

#ifndef Hmod_PkPdModel
#define Hmod_PkPdModel

#include "PkPd/Proteome.h"
#include <fstream>
using namespace std;

namespace OM { namespace PkPd {

/** Encapsulates both the static operations for PKPD models and the per-human
 * drug proxies.
 * 
 * Note that there currently needn't be a PKPD model, in which case an instance
 * of this class is created (allowing nicer code). Therefore all methods have an
 * empty implementation in this class.
 * 
 * Calling order within a timestep (see doc for medicate for details):
 *  * getDrugFactor() for each infection
 *  * decayDrugs()
 *  * medicate()
 */
class PkPdModel {
public:
  ///@brief Static functions
  //@{
  static void init ();
  static void cleanup ();
  
  static void staticCheckpoint (istream& stream);
  static void staticCheckpoint (ostream& stream);
  
  /** Factory function to create a drug interface, type dependant on run-time
   * options.
   * 
   * Currently may return one of: PkPdModel, HoshenPkPdModel, IhKwPkPdModel. */
  static PkPdModel* createPkPdModel ();
  //@}
  
  /// @brief Constructors, destructor and checkpointing
  //@{
  /// Create a new instance
  PkPdModel () {}
  /// Destroy an instance
  virtual ~PkPdModel () {}
  
  /// Checkpointing
  template<class S>
  void operator& (S& stream) {
      checkpoint (stream);
  }
  //@}
  
  /** Medicate drugs to an individual, which act on infections the following
   * timesteps, until rendered ineffective by decayDrugs().
   *
   * \param drugAbbrev - The drug abbreviation.
   * \param qty        - the quantity (which units?).
   * \param time       - Time in minutes since start of this time step to medicate at
   * \param age        - Age of human in years
   * \param weight     - Weight (mass) of human in kg
   * 
   * Due to the fact we're using a discrete timestep model, the case-management
   * update (calling medicate) and within-host model update (calling
   * getDrugFactor) cannot [easily] have immediate effects on each other. The
   * implementation we use is that the within-host model update (calculating
   * new infection densities) happens first; hence medicate() will always be
   * called after getDrugFactor in a timestep, and a time of zero means the
   * dose has effect from the start of the following timestep. */
  virtual void medicate(string drugAbbrev, double qty, int time, double age, double weight)  {};
  
  /// Called each timestep immediately after the drug acts on any infections.
  //NOTE: does calling after applying drug effects make the most sense for all models?
  virtual void decayDrugs () {};
  
  /** This is how drugs act on infections.
   *
   * Each timestep, on each infection, the parasite density is multiplied by
   * the return value of this infection. The WithinHostModels are responsible
   * for clearing infections once the parasite density is negligible. */
  virtual double getDrugFactor (const ProteomeInstance* infProteome) {
    return 1.0;
  }
  
protected:
  virtual void checkpoint (istream& stream) {}
  virtual void checkpoint (ostream& stream) {}
};

} }
#endif