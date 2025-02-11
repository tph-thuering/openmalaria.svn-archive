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

#ifndef Hmod_LSTMDrugType
#define Hmod_LSTMDrugType

#include <string>
#include <deque>
#include <map>
#include <vector>
#include "Dose.h"
#include "Global.h"
#include "PkPd/Proteome.h"
#include "DrugType.h"

using namespace std;

namespace OM { namespace PkPd {
    
/** Information about each (type of) drug (rather than each use of a drug).
 *
 * Static data contains a list of all available drug types.
 * 
 * No DrugType data is checkpointed, because it is loaded by init() from XML
 * data. (Although if it cannot be reproduced by reloading it should be
 * checkpointed.) */
class LSTMDrugType : public DrugType {
public:
  ///@brief Static functions
  //@{
  /** Initialise the drug model. Called at start of simulation. */
  //TODO: data from XML.
  static void init ();
  
  
  ///@brief Non static (per instance) functions
  //@{
  /** Create a new DrugType.
   *
   * @param name	Name of the drug
   * @param abbreviation	Abbreviated name (e.g. CQ)
   * @param absorptionFactor	
   * @param halfLife	Half life of decay, in minutes
   */
  LSTMDrugType (string name, string abbreviation, double absorptionFactor, double halfLife);
  ~LSTMDrugType ();
  
  //! Adds a PD Rule.
  /*! The order of rule adding is important! The first add should be the
   *  one with most mutations (typically the most resistant), the last
   *  one should be the sensitive (ie vector<mutation> = 0).
   */
  void addPDRule(vector<Mutation*> requiredMutations, double pdFactor);

  //! Parses the proteme instances.
  /*! Creates an association between ProteomeInstance and PD factor.
   *  This is solely for performance purposes.
   */
  virtual void parseProteomeInstances();
  //@}
  
private:
  // The list of available drugs. Not checkpointed; should be set up by init().
  //static map<string,LSTMDrugType> available;
  
  //! Absorption factor.
  /*! Absorption = dose * factor / weight
   */
  double absorptionFactor;
  //! Half-life (in minutes)
  double halfLife;
  //! Pharma dynamic list of parameters.
  /*! A ordered list of required mutations.
   * The parameter value can be found on pdParameters.
   * The order is important, the first one takes precedence
   *   (a map cannot impement this).
   */
  vector< vector<Mutation*> > requiredMutations;
  //! PD parameters (check requiredMutations)
  vector<double> pdParameters;
  //! Fast data structure to know the PD param per proteome
  map<int,double> proteomePDParameters;
  //END
  
  // Allow the Drug class to access private members
  friend class Drug;
  friend class LSTMDrug;
};

} }
#endif