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

#include "simulation.h"

#include "boincWrapper.h"

#include "GSLWrapper.h"
#include "population.h"
#include "summary.h"
#include "proteome.h"
#include "drug.h"
#include "global.h"
#include "inputData.h"
#include <fstream>

/*

  Defines the duration of the simulation runs and the pre-timestep call-sequence for both the
  warmup and the main simulation.

  Duration of the main simulation (post-warmup), in 5 day intervals.
*/
int simulationDuration;
double relTimeInMainSim;

int Simulation::simulationTime;
Summary* Simulation::gMainSummary;
int Simulation::timeStep;

static const char* const CHECKPOINT = "checkpoint";
static const int NUM_CHECKPOINTS = 2;

Simulation::Simulation() :
    checkpointName(CHECKPOINT)
{
  // Initialize input variables and allocate memory.
  // We try to make initialization hierarchical (i.e. most classes initialise
  // through Population::init).
  GSL_SETUP();
  
  gMainSummary = new Summary();
  
  Population::init();
  _population = new Population();
  Global::simulationMode=equilibriumMode;
  simulationDuration=get_simulation_duration();
  relTimeInMainSim=simulationDuration/(1.0*simulationDuration+Global::maxAgeIntervals);
  if (Global::modelVersion & INCLUDES_PK_PD) {
    initProteomeModule();
    initDrugModule(5*24*60, 24*60);
  }

}

Simulation::~Simulation(){
  //free memory
  gMainSummary->clearSummaryParameters();
  Population::clear();
  delete gMainSummary;
  
  GSL_TEARDOWN();
}

int Simulation::start(){
  if (isCheckpoint()) {
    readCheckpoint();
    _population->setupPyramid(true);
  }
  else {
    simulationTime = 0;
    _population->estimateRemovalRates();
    _population->initialiseHumanList();
    _population->setupPyramid(false);
  }
  updateOneLifespan();

  _population->preMainSimInit();
  mainSimulation();
  return 0;
}

void Simulation::mainSimulation(){
  //TODO5D
  timeStep=0;
  gMainSummary->initialiseSummaries();
  Global::simulationMode=get_mode();
  
  while( timeStep <=  simulationDuration) {
    _population->implementIntervention(timeStep);
    //Calculate the current progress
    BoincWrapper::reportProgress(relTimeInMainSim*(double(timeStep)/simulationDuration)+(1-relTimeInMainSim));
    //Here would be another place to write checkpoints. But then we need to save state of the surveys/events.
    ++simulationTime;
    ++timeStep;
    _population->update1();
    if ( timeStep ==  gMainSummary->getSurveyTimeInterval(gMainSummary->getSurveyPeriod()-1)) {
      _population->newSurvey();
    }
  }
  delete _population;
  gMainSummary->writeSummaryArrays();
}

void Simulation::updateOneLifespan () {
  while( simulationTime <  Global::maxAgeIntervals) {
    BoincWrapper::reportProgress (double(simulationTime) / Global::maxAgeIntervals * (1-relTimeInMainSim));
    if (BoincWrapper::timeToCheckpoint()) {
      writeCheckpoint();
      BoincWrapper::checkpointCompleted();
    }
    
    ++simulationTime;
    _population->update1();
  }
}


bool Simulation::isCheckpoint(){
  ifstream checkpointFile(CHECKPOINT,ios::in);
  // If not open, file doesn't exist (or is inaccessible)
  return checkpointFile.is_open();
}

void Simulation::writeCheckpoint(){
  // Set so that first checkpoint has number 0
  int checkpointNum = NUM_CHECKPOINTS - 1;
  {	// Get checkpoint number, if any
    ifstream checkpointFile;
    checkpointFile.open(CHECKPOINT, fstream::in);
    if(checkpointFile.is_open()){
      checkpointFile >> checkpointNum;
      checkpointFile.close();
    }
  }
  // Get next checkpoint number:
  checkpointNum = (checkpointNum + 1) % NUM_CHECKPOINTS;
  
  save_rng_state (checkpointNum);
  
  // Open the next checkpoint file for writing:
  ofstream f_checkpoint;
  {
    ostringstream name (checkpointName);
    name << checkpointNum;
    f_checkpoint.open(name.str().c_str(), ios::out | ios::binary);
  }
  
  // Write checkpoint
  f_checkpoint.precision(20);
  f_checkpoint << simulationTime << endl;
  if (Global::modelVersion & INCLUDES_PK_PD) {
    f_checkpoint << ProteomeManager::getManager();
  }
  _population->write (f_checkpoint);
  f_checkpoint.close();
  
  {	// Indicate which is the latest checkpoint file.
    ofstream checkpointFile;
    checkpointFile.open(CHECKPOINT,ios::out);
    checkpointFile << checkpointNum;
    checkpointFile.close();
  }
}

void Simulation::readCheckpoint() {
  int checkpointNum;
  {	// Find out which checkpoint file is current
    ifstream checkpointFile;
    checkpointFile.open(CHECKPOINT, ios::in);
    checkpointFile >> checkpointNum;
    checkpointFile.close();
  }
  // Open the latest file
  ifstream f_checkpoint;
  {
    ostringstream name (checkpointName);
    name << checkpointNum;
    f_checkpoint.open(name.str().c_str(), ios::in | ios::binary);
  }
  
  // Read checkpoint
  f_checkpoint >> simulationTime;
  if (Global::modelVersion & INCLUDES_PK_PD) {
    ProteomeManager* manager = ProteomeManager::getManager();
    f_checkpoint >> *manager;
  }
  _population->read(f_checkpoint);
  f_checkpoint.close();
  
  load_rng_state(checkpointNum);
}
