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

#ifndef Hmod_DescriptiveIPT
#define Hmod_DescriptiveIPT

#include "WithinHost/DescriptiveWithinHost.h"
#include "schema/interventions.h"

namespace OM { namespace WithinHost {
    
/** Extension to the DescriptiveWithinHostModel, including IPT (intermittent
 * preventative treatment) using a simple drug-action model (SPAction).
 *
 * NOTE: This IPT code (this class and DescriptiveIPTInfection) are
 * unmaintained in order to keep results comparable with previous experiments
 * run. */
class DescriptiveIPTWithinHost : public DescriptiveWithinHostModel {
public:
  ///@name Static init/cleanup
  //@{
  /** Determines whether IPT is present or not (iptActive), and if so
   * initialises parameters here and in DescriptiveIPTInfection. */
  static void init(const scnXml::IPTDescription& xmlIPTI);
  //@}
  
  DescriptiveIPTWithinHost ();
  
  //! Create a new infection requires that the human is allocated and current
  virtual void newInfection();
  virtual void loadInfection(istream& stream);
  
  /// Conditionally clear all infections
  virtual void clearInfections (bool isSevere);
  /// Continuous intervention: give an IPTi dose
  virtual void deployIptDose (Monitoring::AgeGroup ageGroup, bool inCohort);
  /// Prescribe IPTi with probability compliance. Only called if IPT present.
  virtual void IPTiTreatment (Monitoring::AgeGroup ageGroup, bool inCohort);
  /// Last IPTi dose recent enough to give protection?
  virtual bool hasIPTiProtection (TimeStep maxInterventionAge) const;
  
protected:
  virtual bool eventSPClears (DescriptiveInfection* inf);
  virtual void IPTattenuateAsexualMinTotalDensity ();
  virtual void IPTattenuateAsexualDensity (DescriptiveInfection* inf);
  
  virtual void checkpoint (istream& stream);
  virtual void checkpoint (ostream& stream);
  
private:
  //! time at which attenuated infection 'would' end if SP present
  TimeStep _SPattenuationt;
  /** Timestep of last SP Dose given (TIMESTEP_NEVER if no SP dose given). */
  TimeStep _lastSPDose;
  /// Timestep of last IPTi or placebo dose given (TIMESTEP_NEVER if never given).
  TimeStep _lastIptiOrPlacebo;
  
  //!Cumulative number of infections since birth
  //NOTE: we also have _cumulativeh; why both?
  int _cumulativeInfections;
  
  /// @brief Static data set by initParameters
  //@{
    // When 10≤effect<30: IPT dose
    // 2 or 12: SP; 3 or 13: short-acting drug
    // 14-22: something to do with seasonality
    // NOTE: if the logic in clearInfections() is anything to go by, other
    // values are valid for SP but totally undocumented
    enum IPTiEffects {
        NO_IPT = 0,
        PLACEBO_SP  = 2,
        PLACEBO_CLEAR_INFECTIONS = 3,
        IPT_MIN = 10,   // likely 10 and 11 are unused
        IPT_SP = 12,
        IPT_CLEAR_INFECTIONS = 13,
        IPT_SEASONAL_MIN = 14,
        IPT_SEASONAL_MAX = 22,
        IPT_MAX = 30    // no idea what values greater than 22 mean... maybe nothing
    };
  /// Values (codes)
  static IPTiEffects iptiEffect;
  //@}
};

} }
#endif