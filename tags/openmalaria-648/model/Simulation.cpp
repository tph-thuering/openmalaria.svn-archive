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

#include "Simulation.h"

#include "util/BoincWrapper.h"
#include "util/timer.h"
#include "Monitoring/Continuous.h"
#include "Monitoring/Surveys.h"
#include "Global.h"
#include "Transmission/TransmissionModel.h"
#include "inputData.h"
#include "util/CommandLine.hpp"
#include "util/ModelOptions.hpp"
#include "util/errors.hpp"
#include "util/random.h"
#include "util/StreamValidator.h"
#include "PopulationStats.h"

#include <fstream>
#include <gzstream.h>


namespace OM {
    using Monitoring::Continuous;
    using Monitoring::Surveys;

// -----  Set-up & tear-down  -----

Simulation::Simulation(util::Checksum ck)
: simPeriodEnd(0), totalSimDuration(0), phase(STARTING_PHASE), _population(NULL), workUnitIdentifier(0), cksum(ck)
{
    OM::Global::init ();
    
    Global::simulationTime = 0;
    Global::timeStep = numeric_limits<int>::min();
    
    // Initialize input variables and allocate memory.
    // We try to make initialization hierarchical (i.e. most classes initialise
    // through Population::init).
    util::random::seed (InputData().getModel().getParameters().getIseed());
    util::ModelOptions::init ();
    Surveys.init();
    Population::init();
    _population = new Population();
    
    workUnitIdentifier = InputData().getWuID();
}

Simulation::~Simulation(){
    //free memory
    delete _population;
    Population::clear();
}


// -----  run simulations  -----

int Simulation::start(){
    totalSimDuration = Global::lifespanInitIntervals	// ONE_LIFE_SPAN
	+ _population->_transmissionModel->transmissionInitDuration()	// initial run of TRANSMISSION_INIT
	+ Surveys.getFinalTimestep() + 1;	// MAIN_PHASE: surveys; +1 to let final survey run
    
    if (isCheckpoint()) {
	Continuous::init( true );
	readCheckpoint();
    } else {
	Continuous::init( false );
	_population->createInitialHumans();
    }
    // Set to either a checkpointing timestep or min int value. We only need to
    // set once, since we exit after a checkpoint triggered this way.
    int testCheckpointStep = util::CommandLine::getNextCheckpointTime( Global::simulationTime );
    int testCheckpointDieStep = testCheckpointStep;	// kill program at same time
    
    // phase loop
    while (true) {
	// loop for steps within a phase
	while (Global::simulationTime < simPeriodEnd) {
	    // checkpoint
	    if (util::BoincWrapper::timeToCheckpoint() || Global::simulationTime == testCheckpointStep) {
		writeCheckpoint();
		util::BoincWrapper::checkpointCompleted();
	    }
	    if (Global::simulationTime == testCheckpointDieStep)
		throw util::cmd_exit ("Checkpoint test: checkpoint written");
	    
	    // Reporting: effectively happens at end of time-step due to 
	    Continuous::update();
	    if (Global::timeStep == Surveys.currentTimestep) {
		_population->newSurvey();
		Surveys.incrementSurveyPeriod();
	    }
	    _population->implementIntervention(Global::timeStep);
	    
	    // Simulation-time used to be 1-based (from Fortran implementation).
	    // It is now zero-based with regards to checkpoints but one-based
	    // with regards to everything else: rather messy.
	    ++Global::simulationTime;
	    
	    // update
	    _population->update1();
	    
	    util::BoincWrapper::reportProgress (double(Global::simulationTime) / totalSimDuration);
	    
	    ++Global::timeStep;	// zero-based: zero on first time-step of intervention period
	}
	
	if (phase == STARTING_PHASE) {
	    // Start ONE_LIFE_SPAN:
	    ++phase;
	    simPeriodEnd = Global::lifespanInitIntervals;
	} else if (phase == ONE_LIFE_SPAN) {
	    // Start vector-initialisation:
	    ++phase;
	    simPeriodEnd += _population->_transmissionModel->transmissionInitDuration();
	} else if (phase == TRANSMISSION_INIT) {
	    int extend = _population->_transmissionModel->transmissionInitIterate();
	    if (extend > 0) {	// repeat phase
		simPeriodEnd += extend;
		totalSimDuration += extend;
	    } else {		// next phase
		// Start MAIN_PHASE:
		++phase;
		simPeriodEnd = totalSimDuration;
		Global::timeStep=0;
		_population->preMainSimInit();
		_population->newSurvey();	// Only to reset TransmissionModel::innoculationsPerAgeGroup
		Surveys.incrementSurveyPeriod();
	    }
	} else if (phase == MAIN_PHASE) {
	    ++phase;
	    cerr << "sim end" << endl;
	    break;
	}
	if (util::CommandLine::option (util::CommandLine::TEST_CHECKPOINTING)){
	    // First of middle of next phase, or current value (from command line) triggers a checkpoint.
	    int phase_mid = Global::simulationTime + (simPeriodEnd - Global::simulationTime) / 2;
	    // don't checkpoint 0-length phases or do mid-phase checkpointing when timed checkpoints were specified:
	    if( testCheckpointStep == numeric_limits< int >::min()
		&& phase_mid > Global::simulationTime
	    ){
		testCheckpointStep = phase_mid;
		// Test checkpoint: die a bit later than checkpoint for better
		// resume testing (specifically, ctsout.txt).
		testCheckpointDieStep = testCheckpointStep + 2;
	    }
	}
    }
    
    // Open a critical section; should prevent app kill while/after writing
    // output.txt, which we don't currently handle well.
    // Note: we don't end this critical section; we simply exit.
    util::BoincWrapper::beginCriticalSection();
    
    PopulationStats::print();
    
    _population->flushReports();	// ensure all Human instances report past events
    Surveys.writeSummaryArrays();
    
    // Write scenario checksum, only if simulation completed.
    cksum.writeToFile (util::BoincWrapper::resolveFile ("scenario.sum"));
    
# ifdef OM_STREAM_VALIDATOR
    util::StreamValidator.saveStream();
# endif
    
    return 0;
}


// -----  checkpointing: set up read/write stream  -----

const char* CHECKPOINT = "checkpoint";

bool Simulation::isCheckpoint(){
  ifstream checkpointFile(CHECKPOINT,ios::in);
  // If not open, file doesn't exist (or is inaccessible)
  return checkpointFile.is_open();
}
int readCheckpointNum () {
    ifstream checkpointFile;
    checkpointFile.open(CHECKPOINT, fstream::in);
    int checkpointNum;
    checkpointFile >> checkpointNum;
    checkpointFile.close();
    if (!checkpointFile)
	throw util::checkpoint_error ("error reading from file \"checkpoint\"");
    return checkpointNum;
}

void Simulation::writeCheckpoint(){
    // We alternate between two checkpoints, in case program is closed while writing.
    const int NUM_CHECKPOINTS = 2;
    
    int oldCheckpointNum = 0, checkpointNum = 0;
    if (isCheckpoint()) {
	oldCheckpointNum = readCheckpointNum();
	// Get next checkpoint number:
	checkpointNum = (oldCheckpointNum + 1) % NUM_CHECKPOINTS;
    }
    
    {	// Open the next checkpoint file for writing:
	ostringstream name;
	name << CHECKPOINT << checkpointNum;
	//Writing checkpoint:
// 	cerr << Global::simulationTime << " WC: " << name.str();
	if (util::CommandLine::option (util::CommandLine::COMPRESS_CHECKPOINTS)) {
	    name << ".gz";
	    ogzstream out(name.str().c_str(), ios::out | ios::binary);
	    checkpoint (out, checkpointNum);
	    out.close();
	} else {
	    ofstream out(name.str().c_str(), ios::out | ios::binary);
	    checkpoint (out, checkpointNum);
	    out.close();
	}
    }
    
    {	// Indicate which is the latest checkpoint file.
	ofstream checkpointFile;
	checkpointFile.open(CHECKPOINT,ios::out);
	checkpointFile << checkpointNum;
	checkpointFile.close();
	if (!checkpointFile)
	    throw util::checkpoint_error ("error writing to file \"checkpoint\"");
    }
    // Truncate the old checkpoint to save disk space, when it existed
    if( oldCheckpointNum != checkpointNum
	&& !util::CommandLine::option (
	    util::CommandLine::TEST_DUPLICATE_CHECKPOINTS
	)	/* need original in this case */
    ) {
	ostringstream name;
	name << CHECKPOINT << oldCheckpointNum;
	if (util::CommandLine::option (util::CommandLine::COMPRESS_CHECKPOINTS)) {
	    name << ".gz";
	}
	ofstream out(name.str().c_str(), ios::out | ios::binary);
	out.close();
    }
//     cerr << " OK" << endl;
}

void Simulation::readCheckpoint() {
    int checkpointNum = readCheckpointNum();
    
  // Open the latest file
  ostringstream name;
  name << CHECKPOINT << checkpointNum;	// try uncompressed
  ifstream in(name.str().c_str(), ios::in | ios::binary);
  if (in.good()) {
    checkpoint (in, checkpointNum);
    in.close();
  } else {
    name << ".gz";				// then compressed
    igzstream in(name.str().c_str(), ios::in | ios::binary);
    //Note: gzstreams are considered "good" when file not open!
    if ( !( in.good() && in.rdbuf()->is_open() ) )
      throw util::checkpoint_error ("Unable to read file");
    checkpoint (in, checkpointNum);
    in.close();
  }
  
  // Keep size of stderr.txt minimal with a short message, since this is a common message:
  cerr <<Global::simulationTime<<" RC"<<endl;
  
  // On resume, write a checkpoint so we can tell whether we have identical checkpointed state
  if (util::CommandLine::option (util::CommandLine::TEST_DUPLICATE_CHECKPOINTS))
    writeCheckpoint();
}


//   -----  checkpointing: Simulation data  -----

void Simulation::checkpoint (istream& stream, int checkpointNum) {
    try {
	util::checkpoint::header (stream);
	util::CommandLine::staticCheckpoint (stream);
	Population::staticCheckpoint (stream);
	Surveys & stream;
	Continuous::staticCheckpoint (stream);
#	ifdef OM_STREAM_VALIDATOR
	util::StreamValidator & stream;
#	endif
	
	Global::simulationTime & stream;
	Global::timeStep & stream;
	simPeriodEnd & stream;
	totalSimDuration & stream;
	phase & stream;
	(*_population) & stream;
	PopulationStats::staticCheckpoint( stream );
	
	// read last, because other loads may use random numbers:
	util::random::checkpoint (stream, checkpointNum);
	
	// Check scenario.xml and checkpoint files correspond:
	int oldWUID(workUnitIdentifier);
	util::Checksum oldCksum(cksum);
	workUnitIdentifier & stream;
	cksum & stream;
	if (workUnitIdentifier != oldWUID || cksum != oldCksum)
	    throw util::checkpoint_error ("mismatched checkpoint");
    } catch (const util::checkpoint_error& e) {	// append " (pos X of Y bytes)"
	ostringstream pos;
	pos<<" (pos "<<stream.tellg()<<" of ";
	stream.ignore (numeric_limits<streamsize>::max()-1);	// skip to end of file
	pos<<stream.tellg()<<" bytes)";
	throw util::checkpoint_error( e.what() + pos.str() );
    }
    
    
    stream.ignore (numeric_limits<streamsize>::max()-1);	// skip to end of file
    if (stream.gcount () != 0) {
	ostringstream msg;
	msg << "Checkpointing file has " << stream.gcount() << " bytes remaining." << endl;
	throw util::checkpoint_error (msg.str());
    } else if (stream.fail())
	throw util::checkpoint_error ("stream read error");
}

void Simulation::checkpoint (ostream& stream, int checkpointNum) {
    util::checkpoint::header (stream);
    if (stream == NULL || !stream.good())
	throw util::checkpoint_error ("Unable to write to file");
    util::timer::startCheckpoint ();
    
    util::CommandLine::staticCheckpoint (stream);
    Population::staticCheckpoint (stream);
    Surveys & stream;
    Continuous::staticCheckpoint (stream);
# ifdef OM_STREAM_VALIDATOR
    util::StreamValidator & stream;
# endif
    
    Global::simulationTime & stream;
    Global::timeStep & stream;
    simPeriodEnd & stream;
    totalSimDuration & stream;
    phase & stream;
    (*_population) & stream;
    PopulationStats::staticCheckpoint( stream );
    
    util::random::checkpoint (stream, checkpointNum);
    workUnitIdentifier & stream;
    cksum & stream;
    
    util::timer::stopCheckpoint ();
    if (stream.fail())
	throw util::checkpoint_error ("stream write error");
}

}