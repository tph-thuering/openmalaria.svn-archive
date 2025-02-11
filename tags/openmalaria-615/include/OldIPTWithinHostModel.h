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

#ifndef Hmod_oldITNWHM
#define Hmod_oldITNWHM

#include "oldWithinHostModel.h"

/** Extension to the OldWithinHostModel, including IPT (intermittent
 * preventative treatment) plus a simple drug-action model (SPAction). */
class OldIPTWithinHostModel : public OldWithinHostModel {
public:
  ///@name Static init/cleanup
  //@{
  /** Determines whether IPT is present or not (iptActive), and if so
   * initialises parameters here and in OldIPTInfection. */
  static void initParameters();
  static void clearParameters();
  //@}
  
  OldIPTWithinHostModel ();
  
  //! Create a new infection requires that the human is allocated and current
  virtual void newInfection();
  
  /// Conditionally clear all infections
  virtual void clearInfections (Event&);
  /// Conditionally set last SP dose
  virtual void IPTSetLastSPDose (int agetstep, int ageGroup);
  /// Prescribe IPTi with probability compliance. Only called if IPT present.
  virtual void IPTiTreatment (double compliance, int ageGroup);
  
  /// Is IPT present?
  static bool iptActive;
  
protected:
  /*!  SP drug action applies to each infection depending on genotype and when
  the individual had their last dose of SP */
  void SPAction(Human&);
  
  void IPTattenuateAsexualDensity (DescriptiveInfection& infec);
  void IPTattenuateAsexualMinTotalDensity (Human&);
  
  virtual void write(ostream& out) const;
  virtual void read(istream& in);
  
private:
  //! time at which attenuated infection 'would' end if SP present
  int _SPattenuationt;
  //!last SP Dose given
  int _lastSPDose;
  //!last IPTi or placebo dose given 
  int _lastIptiOrPlacebo;
  
  
  // -----  static data  -----
  
  ///Number of IPTi doses
  static int numberOfIPTiDoses;
  ///Target age for IPTi doses, in time steps
  static int *iptiTargetagetstep;
  ///Coverage , as a proportion of the poulation in the target age range
  static double *iptiCoverage;
  /// Values (codes)
  static int iptiEffect;
};

#endif
