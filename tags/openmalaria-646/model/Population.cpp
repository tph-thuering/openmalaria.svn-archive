/*

 This file is part of OpenMalaria.

 Copyright (C) 2005-2009 Swiss Tropical Institute and Liverpool School Of Tropical Medicine

 OpenMalaria is free software; you can redistribute it and/or modify
 it under the terms of the GNu General Public License as published by
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
#include "Population.h"
#include "inputData.h"
#include "Monitoring/Surveys.h"
#include "Monitoring/Continuous.h"

#include "Transmission/TransmissionModel.h"

#include "Host/Human.h"
#include "Host/NeonatalMortality.h"
#include "Clinical/ClinicalModel.h"
#include "Clinical/CaseManagementCommon.h"
#include "Pathogenesis/PathogenesisModel.h"
#include "PkPd/PkPdModel.h"

#include "util/errors.hpp"
#include "util/random.h"
#include "util/ModelOptions.hpp"

#include <boost/format.hpp>

namespace OM
{
    using namespace OM::util;

// -----  Population: static data / methods  -----

// Optional immediate reporting of population demographics
// Compile with -DOMP_CSV_REPORTING for gcc or /DOMP_CSV_REPORTING for MSVC.
#ifdef OMP_CSV_REPORTING
ofstream csvReporting;
#endif


void Population::init()
{
    Host::Human::initHumanParameters();
    Host::NeonatalMortality::init();
    PkPd::PkPdModel::init();
#ifdef OMP_CSV_REPORTING
    csvReporting.open ("population.csv", ios::app);
#endif
    
    AgeStructure::init ();
}

void Population::clear()
{
    PkPd::PkPdModel::cleanup ();
    Host::Human::clear();
#ifdef OMP_CSV_REPORTING
    csvReporting.close();
#endif
}

void Population::staticCheckpoint (istream& stream)
{
    Host::NeonatalMortality::staticCheckpoint (stream);
    Clinical::ClinicalModel::staticCheckpoint (stream);
    PkPd::PkPdModel::staticCheckpoint (stream);
}
void Population::staticCheckpoint (ostream& stream)
{
    Host::NeonatalMortality::staticCheckpoint (stream);
    Clinical::ClinicalModel::staticCheckpoint (stream);
    PkPd::PkPdModel::staticCheckpoint (stream);
}


// -----  non-static methods: creation/destruction, checkpointing  -----

Population::Population()
    : populationSize (InputData().getDemography().getPopSize())
{
    using Monitoring::Continuous;
    Continuous::registerCallback( "hosts", "\thosts", MakeDelegate( this, &Population::ctsHosts ) );
    Continuous::registerCallback( "recent births", "\trecent births", MakeDelegate( this, &Population::ctsRecentBirths ) );
    Continuous::registerCallback( "patent hosts", "\tpatent hosts", MakeDelegate( this, &Population::ctsPatentHosts ) );
    Continuous::registerCallback( "immunity h", "\timmunity h", MakeDelegate( this, &Population::ctsImmunityh ) );
    Continuous::registerCallback( "immunity Y", "\timmunity Y", MakeDelegate( this, &Population::ctsImmunityY ) );
    
    _transmissionModel = Transmission::TransmissionModel::createTransmissionModel(populationSize);
}

Population::~Population()
{
    for (HumanIter iter = population.begin(); iter != population.end(); ++iter) {
        iter->destroy();
    }
    delete _transmissionModel;
}

void Population::checkpoint (istream& stream)
{
    size_t popSize; // must be type of population.size()
    popSize & stream;
    if (popSize > size_t (populationSize))
        throw util::checkpoint_error( (boost::format("pop size (%1%) exceeds that given in scenario.xml") %popSize).str() );
    for (size_t i = 0; i < popSize && !stream.eof(); ++i) {
        // Note: calling this constructor of Host::Human is slightly wasteful, but avoids the need for another
        // ctor and leaves less opportunity for uninitialized memory.
        population.push_back (Host::Human (*_transmissionModel, 0, 0));
        population.back() & stream;
    }
    if (population.size() != popSize)
        throw util::checkpoint_error( (boost::format("Population: out of data (read %1% humans)") %population.size() ).str() );
}
void Population::checkpoint (ostream& stream)
{
    population.size() & stream;
    for (HumanIter iter = population.begin(); iter != population.end(); ++iter)
        (*iter) & stream;
}

void Population::preMainSimInit ()
{
    _transmissionModel->initMainSimulation();
    Clinical::ClinicalModel::initMainSimulation();
    recentBirths = 0;
}

void Population::createInitialHumans ()
{
    /* We create a whole population here, regardless of whether humans can
    survive until start of vector init (vector model needs a whole population
    structure in any case). However, we don't update humans known not to survive
    until vector init, which saves computation and memory (no infections). */
    
    int cumulativePop = 0;
    for (int iage = AgeStructure::getMaxTimestepsPerLife() - 1; iage >= 0; iage--) {
	int targetPop = AgeStructure::targetCumPop (iage, populationSize);
	while (cumulativePop < targetPop) {
	    newHuman (-iage);
	    ++cumulativePop;
	}
    }
    
    // Vector setup dependant on human population structure (we *want* to
    // include all humans, whether they'll survive to vector init phase or not).
    // This also updates humans' ageGroupData, which _must_ happen here.
    _transmissionModel->updateAgeCorrectionFactor (population, populationSize);
    _transmissionModel->setupNv0 (population, populationSize);
}


// -----  non-static methods: simulation loop  -----

void Population::newHuman (int dob)
{
    population.push_back (Host::Human (*_transmissionModel, dob, Global::simulationTime));
    ++recentBirths;
}

void Population::update1()
{
    // This also updates humans' ageGroupData, which _must_ happen at beginning of timestep.
    // Should probably be whole pop, not just those surviving to vector init.
    _transmissionModel->updateAgeCorrectionFactor (population, populationSize);
    
    // This should only use humans being updated: otherwise too small a proportion
    // will be infected. However, we don't have another number to use instead.
    // TODO: add some value to be used until old-enough humans are updated?
    Host::NeonatalMortality::update (population);
    
    // This should be called before humans contract new infections in the simulation step.
    // This needs the whole population (it is an approximation before all humans are updated).
    _transmissionModel->vectorUpdate (population, Global::simulationTime);

    //NOTE: other parts of code are not set up to handle changing population size. Also
    // populationSize is assumed to be the _actual and exact_ population size by other code.
    //targetPop is the population size at time t allowing population growth
    //int targetPop = (int) (populationSize * exp (AgeStructure::rho * Global::simulationTime));
    int targetPop = populationSize;
    int cumPop = 0;

    // Update each human in turn
    //std::cout<<" time " <<t<<std::endl;
    HumanIter last = population.end();
    --last;
    for (HumanIter iter = population.begin(); iter != population.end();) {
        // Update human, and remove if too old:
        if (iter->update (Global::simulationTime, _transmissionModel,
		/* Only include humans who can survive until vector init.
		Note: we could exclude more humans due to age distribution,
		but how many extra to leave due to deaths isn't obvious. */
		(int)Global::intervalsPerYear + iter->getDateOfBirth() > 0
	    )) {
            iter->destroy();
            iter = population.erase (iter);
            continue;
        }

        //BEGIN Population size & age structure
        ++cumPop;
        int age = (Global::simulationTime - iter->getDateOfBirth());

        // if (Actual number of people so far > target population size for this age) ...
        if (cumPop > AgeStructure::targetCumPop (age, targetPop)) {
            --cumPop;
            iter->destroy();
            iter = population.erase (iter);
            continue;
        }
        //END Population size & age structure
        ++iter;
    } // end of per-human updates

    // increase population size to targetPop
    while (cumPop < targetPop) {
        newHuman (Global::simulationTime);
        //++nCounter;
        ++cumPop;
    }
    
    // Doesn't matter whether non-updated humans are included (value isn't used
    // before all humans are updated).
    _transmissionModel->updateKappa (population, Global::simulationTime);

#ifdef OMP_CSV_REPORTING
    if (Global::simulationTime % (Global::intervalsPerYear*5) == 0) {
        csvReporting << Global::simulationTime << ',';
        list<Host::Human>::reverse_iterator it = population.rbegin();
        for (double ageLim = 0; ageLim <= maxLifetimeDays / 365.0; ageLim += 1) {
            int counter = 0;
            while (it != population.rend() && it->getAgeInYears() < ageLim) {
                ++counter;
                ++it;
            }
            csvReporting << counter << ',';
        }
        csvReporting << endl;
    }
#endif
}


// -----  non-static methods: reporting  -----

void Population::ctsHosts (ostream& stream){
    // this option is intended for debugging human initialization; normally this should equal populationSize.
    stream << '\t' << population.size();
}
void Population::ctsRecentBirths (ostream& stream){
    stream << '\t' << recentBirths;
    recentBirths = 0;
}
void Population::ctsPatentHosts (ostream& stream){
    int patent = 0;
    for (HumanIter iter = population.begin(); iter != population.end(); iter++) {
        if( iter->getWithinHostModel().parasiteDensityDetectible() )
            ++patent;
    }
    stream << '\t' << patent;
}
void Population::ctsImmunityh (ostream& stream){
    double x = 0.0;
    for (HumanIter iter = population.begin(); iter != population.end(); iter++) {
        x += iter->getWithinHostModel().getCumulativeh();
    }
    x /= populationSize;
    stream << '\t' << x;
}
void Population::ctsImmunityY (ostream& stream){
    double x = 0.0;
    for (HumanIter iter = population.begin(); iter != population.end(); iter++) {
        x += iter->getWithinHostModel().getCumulativeY();
    }
    x /= populationSize;
    stream << '\t' << x;
}

void Population::newSurvey ()
{
    for (HumanIter iter = population.begin(); iter != population.end(); iter++) {
        iter->summarize();
    }
    _transmissionModel->summarize( *Monitoring::Surveys.current );
}

void Population::flushReports (){
    for (HumanIter iter = population.begin(); iter != population.end(); iter++) {
        iter->flushReports();
    }
}    


// -----  non-static methods: interventions  -----

void Population::implementIntervention (int time)
{
    const scnXml::Intervention* interv = InputData.getInterventionByTime (time);
    if (interv == NULL)
        return;

    // Given an intervention descriptor for this time point, check which
    // interventions are included. All should check when data loading that they
    // have data if used according to getActiveInterventions().

    if (interv->getChangeHS().present()) {
        Clinical::CaseManagementCommon::changeHealthSystem(time);
    }

    if (interv->getChangeEIR().present()) {
        _transmissionModel->changeEIRIntervention (interv->getChangeEIR().get());
    }

    if (interv->getVaccinate().present()) {
        massIntervention (interv->getVaccinate().get(), &Host::Human::massVaccinate);
    }
    if (interv->getMDA().present()) {
	massIntervention (interv->getMDA().get().getCoverage(), &Host::Human::massDrugAdministration);
    }
    if (interv->getIpti().present()) {
        massIntervention (interv->getIpti().get(), &Host::Human::IPTiTreatment);
    }

    if (interv->getITN().present()) {
        massIntervention (interv->getITN().get(), &Host::Human::setupITN);
    }
    if (interv->getIRS().present()) {
        massIntervention (interv->getIRS().get(), &Host::Human::setupIRS);
    }
    if (interv->getVectorAvailability().present()) {
        massIntervention (interv->getVectorAvailability().get(), &Host::Human::setupVA);
    }
    if (interv->getImmuneSuppression().present()) {
	massIntervention (interv->getImmuneSuppression().get(), &Host::Human::immuneSuppression);
    }
    if (interv->getCohort().present()) {
	massIntervention (interv->getCohort().get(), &Host::Human::addToCohort);
    }
    
    if (interv->getLarviciding().present()) {
        _transmissionModel->intervLarviciding (interv->getLarviciding().get());
    }
    
    if (interv->getInsertR_0Case().present()){
	int i = (int)std::floor (random::uniform_01() * populationSize);	// pick a human
	HumanIter it = population.begin();
	while (i > 0){	// find human (can't use population[i])
	    ++it;
	    --i;
	}
	assert( i == 0 );
	assert( it != population.end() );
	it->R_0Vaccines();
	it->addInfection();
    }
    if (interv->getImportedInfectionsPerThousandHosts().present()){
    	importedInfections(interv->getImportedInfectionsPerThousandHosts().get());
    }
    if (interv->getUninfectVectors().present()){
	_transmissionModel->uninfectVectors();
    }
}

void Population::importedInfections(double importedInfectionsPerThousandHosts)
{
	double prop = (double)populationSize/1000.0;
	double importedInfectionsNbr = importedInfectionsPerThousandHosts * prop;
	double importedInfectionsProb = importedInfectionsNbr/(double) populationSize;
	int totalImportedInfections = 0;

	if(importedInfectionsNbr>0)
		for(HumanIter humanIterator = population.begin(); humanIterator!=population.end(); humanIterator++)
			if(random::bernoulli(importedInfectionsProb)==1)
			{
				humanIterator->addInfection();
				totalImportedInfections++;
			}
}

void Population::massIntervention (const scnXml::Mass& mass, void (Host::Human::*intervention) ())
{
    double minAge = mass.getMinAge().present() ?
                    mass.getMinAge().get() : 0.0;
    double maxAge = mass.getMaxAge().present() ?
                    mass.getMaxAge().get() : 100.0;
    double coverage = mass.getCoverage();
    bool cohortOnly = mass.getCohort().present() ?
		    mass.getCohort().get() : false;

    for (HumanIter iter = population.begin(); iter != population.end(); ++iter) {
        double ageYears = iter->getAgeInYears();
        if( ageYears > minAge && ageYears < maxAge ){
	    if( !cohortOnly || iter->getInCohort() ){
		if( random::uniform_01() < coverage ){
		    // This is UGLY syntax. It just means call intervention() on the human pointed by iter.
		    ( (*iter).*intervention) ();
		}
	    }
	}
    }
}

}
