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

#ifndef Hmod_LSTMPkPdModel
#define Hmod_LSTMPkPdModel

#include "PkPd/PkPdModel.h"
#include "PkPd/Drug/LSTMDrug.h"

namespace OM { namespace PkPd {
    
/** Pharmacokinetic and pharmacodynamics interface, used by each human's
 * within-host model.
 *
 * Some of the implementation is contained in the drug.h/drug.cpp files. */
class LSTMPkPdModel : public PkPdModel {
public:
  LSTMPkPdModel ();
  virtual ~LSTMPkPdModel ();
  
  virtual void checkpoint (istream& stream);
  virtual void checkpoint (ostream& stream);
  
  virtual void medicate(string drugAbbrev, double qty, double time, double ageYears);
  virtual void medicateIV(string drugAbbrev, double qty, double duration, double endTime);
  virtual void decayDrugs ();
  virtual double getDrugFactor (uint32_t proteome_ID);
  
  virtual uint32_t new_proteome_ID ();
  
private:
    // Per-individual variables:
  list<LSTMDrug> _drugs;
  
  /// Multiplies the mean weight for age.
  /// Within PkPd class simply because it's not used elsewhere.
  double hetWeightMultiplier;
};

} }
#endif