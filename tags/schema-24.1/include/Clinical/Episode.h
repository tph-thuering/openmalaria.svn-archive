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

#ifndef Hmod_Episode
#define Hmod_Episode

#include "Global.h"
#include "Pathogenesis/State.h"
#include "Monitoring/Survey.h"	//Monitoring::AgeGroup
#include <ostream>

namespace OM { namespace Clinical {
    
/** Summary of clinical events during a caseManagementMemory period, in one individual.
 *
 * Terminology:
 * An "event" is an instantaeneous alteration of state.
 * A "bout" is a single fever or other sickness (falling sick to recovery).
 * An "episode" is a clinical-view of sickness caused by a malaria infection.
 * There's no reason an "episode" can't span multiple infections and multiple
 * bouts of sickness and recovery (the most severe is reported). */
class Episode{
public:
    /** Set healthSystemMemory. */
    static void init();
    
  Episode() : _time(Global::TIMESTEP_NEVER), _ageGroup() {};
  ~Episode();
  
  /// Report anything pending, as on destruction
  void flush();
  
  /** Report an episode, its severity, and any outcomes it entails.
   *
   * @param simulationTime Time of report (i.e. now)
   * @param ageGroup Monitoring agegroup
   * @param newState The severity (diagnosis) and outcome.
   */
  void update(int simulationTime, bool inCohort, Monitoring::AgeGroup ageGroup, Pathogenesis::State newState);
  
  Pathogenesis::State getState() const {return _state;};
  
  /// Checkpointing
  void operator& (istream& stream);
  void operator& (ostream& stream);	///< ditto
  
  
  /** The maximum age, in timesteps, of when a sickness bout occurred, for
   * another bout to be considered part of the same episode.
   * 
   * Used by both the clinical models in roughly the same way, but will have
   * different values in each to match Global::interval. */
  static int healthSystemMemory;
  
private:
  /** Report a clinical episode.
   *
   * From _state, an episode is reported based on severity (SICK,
   * MALARIA or COMPLICATED), and any outcomes are reported: RECOVERY (in
   * hospital, i.e. with EVENT_IN_HOSPITAL, only), SEQUELAE and DIRECT_DEATH
   * (both in and out of hospital). */
  void report();
  
  /// Timestep of event (TIMESTEP_NEVER if no event).
  int _time;
  /// Survey during which the event occured
  int _surveyPeriod;
  /// Age group of the individual when the episode's first bout occurred
  Monitoring::AgeGroup _ageGroup;
  /// Descriptor of state, containing reporting info. Not all information will
  /// be reported (e.g. indirect deaths are reported independantly).
  Pathogenesis::State _state;
};

} }
#endif
