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
#include "event.h"
#include "simulation.h"
#include "inputData.h"
#include "GSLWrapper.h"
#include <algorithm>
#include "CaseManagementModel.h"
#include "summary.h"
#include "morbidityModel.h"

void Event::update(int simulationTime, int ageGroup, int diagnosis, int outcome){
  if ((diagnosis == Diagnosis::INDIRECT_MALARIA_DEATH) || (simulationTime>(_time + CaseManagementModel::caseManagementMemory))){
    if (_time!=missing_value){
      Simulation::gMainSummary->report(*this);
    }
    _time=simulationTime;
    _surveyPeriod=Simulation::gMainSummary->getSurveyPeriod();
    _ageGroup=ageGroup;
    _diagnosis=diagnosis;
    _outcome=outcome;
    _recurrence=1;
  }
  else {
    _outcome=std::max(outcome, _outcome);
    _diagnosis=std::max(diagnosis, _diagnosis);
    _recurrence++;
  }
}


ostream& operator<<(ostream& out, const Event& event){
  out << event._time << endl;
  out << event._surveyPeriod << endl;
  out << event._ageGroup << endl;
  out << event._diagnosis << endl;
  out << event._outcome << endl;
  return out << event._recurrence << endl;
  
}

istream& operator>>(istream& in, Event& event){
  in >> event._time;
  in >> event._surveyPeriod;
  in >> event._ageGroup;
  in >> event._diagnosis;
  in >> event._outcome;
  in >> event._recurrence;

  return in;
}
