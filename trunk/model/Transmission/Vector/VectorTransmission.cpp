/* This file is part of OpenMalaria.
 * 
 * Copyright (C) 2005-2009 Swiss Tropical Institute and Liverpool School Of Tropical Medicine
 * 
 * OpenMalaria is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "Transmission/Vector/VectorTransmission.h"
#include "inputData.h"
#include "Monitoring/Continuous.h"
#include "util/vectors.h"
#include "util/ModelOptions.h"

#include <fstream>
#include <map>
#include <cmath>

namespace OM { namespace Transmission {
    using namespace OM::util;

void VectorTransmission::ctsCbN_v0 (ostream& stream){
    for (size_t i = 0; i < numSpecies; ++i)
	stream << '\t' << species[i].getLastN_v0()/TimeStep::interval;
}
void VectorTransmission::ctsCbN_v (ostream& stream){
    for (size_t i = 0; i < numSpecies; ++i)
	stream << '\t' << species[i].getLastN_v()/TimeStep::interval;
}
void VectorTransmission::ctsCbO_v (ostream& stream){
    for (size_t i = 0; i < numSpecies; ++i)
	stream << '\t' << species[i].getLastO_v()/TimeStep::interval;
}
void VectorTransmission::ctsCbS_v (ostream& stream){
    for (size_t i = 0; i < numSpecies; ++i)
	stream << '\t' << species[i].getLastS_v()/TimeStep::interval;
}
const string& reverseLookup (const map<string,size_t>& m, size_t i){
    for( map<string,size_t>::const_iterator it = m.begin(); it != m.end(); ++it ){
	if( it->second == i )
	    return it->first;
    }
    throw logic_error( "reverseLookup: key not found" );	// shouldn't ever happen
}

VectorTransmission::VectorTransmission (const scnXml::Vector vectorData, int populationSize)
{
  // Each item in the AnophelesSequence represents an anopheles species.
  // TransmissionModel::createTransmissionModel checks length of list >= 1.
  const scnXml::Vector::AnophelesSequence anophelesList = vectorData.getAnopheles();
  const scnXml::Vector::NonHumanHostsSequence nonHumansList = vectorData.getNonHumanHosts();

  map<string, double> nonHumanHostsPopulations;

  for(size_t i = 0; i<nonHumansList.size(); i++)
	  nonHumanHostsPopulations[nonHumansList[i].getName()] = nonHumansList[i].getNumber();

  numSpecies = anophelesList.size();
  if (numSpecies < 1)
    throw util::xml_scenario_error ("Can't use Vector model without data for at least one anopheles species!");
  species.resize (numSpecies, VectorAnopheles(this));
  
  for (size_t i = 0; i < numSpecies; ++i) {
    string name = species[i].initialise (anophelesList[i], i,
					 initialisationEIR, nonHumanHostsPopulations, populationSize);
    speciesIndex[name] = i;
  }
  
    // Calculate total annual EIR
    annualEIR = vectors::sum( initialisationEIR );
  
  
  // -----  Initialise interventions  -----
  const scnXml::Descriptions::AnophelesSequence& intervSeq =
    InputData().getInterventions().getDescriptions().getAnopheles();
  for (scnXml::Descriptions::AnophelesSequence::const_iterator it =
      intervSeq.begin(); it != intervSeq.end(); ++it) {
    species[getSpeciesIndex(it->getMosquito())].setInterventionDescription (*it);
  }
  for (map<string,size_t>::const_iterator it = speciesIndex.begin(); it != speciesIndex.end(); ++it)
      species[it->second].checkInterventionDescriptions (it->first);
  
  
  // -----  Continuous reporting  -----
  ostringstream ctsNv0, ctsNv, ctsOv, ctsSv;
  // Output in order of species so that (1) we can just iterate through this
  // list when outputting and (2) output is in order specified in XML.
  for (size_t i = 0; i < numSpecies; ++i){
    // Unfortunately, we need to reverse-lookup the name.
    const string& name = reverseLookup( speciesIndex, i );
    ctsNv0<<"\tN_v0("<<name<<")";
    ctsNv<<"\tN_v("<<name<<")";
    ctsOv<<"\tO_v("<<name<<")";
    ctsSv<<"\tS_v("<<name<<")";
  }
  using Monitoring::Continuous;
  Continuous::registerCallback( "N_v0", ctsNv0.str(), MakeDelegate( this, &VectorTransmission::ctsCbN_v0 ) );
  Continuous::registerCallback( "N_v", ctsNv.str(), MakeDelegate( this, &VectorTransmission::ctsCbN_v ) );
  Continuous::registerCallback( "O_v", ctsOv.str(), MakeDelegate( this, &VectorTransmission::ctsCbO_v ) );
  Continuous::registerCallback( "S_v", ctsSv.str(), MakeDelegate( this, &VectorTransmission::ctsCbS_v ) );
}
VectorTransmission::~VectorTransmission () {
  for (size_t i = 0; i < numSpecies; ++i)
    species[i].destroy();
}

void VectorTransmission::setupNv0 (const std::list<Host::Human>& population, int populationSize) {
  for (size_t i = 0; i < numSpecies; ++i) {
    species[i].setupNv0 (i, population, populationSize);
  }
}

void VectorTransmission::scaleEIR (double factor){
    for( size_t i = 0; i < numSpecies; ++i )
	species[i].scaleEIR( factor );
    vectors::scale( initialisationEIR, factor );
    annualEIR = vectors::sum( initialisationEIR );
}
void VectorTransmission::scaleXML_EIR (scnXml::EntoData& ed, double factor) const{
    // XML values are exponentiated; so we add some factor to existing a0 values:
    double add_to_a0 = std::log( factor );
    
    assert( ed.getVector().present() );
    scnXml::Vector::AnophelesSequence& anophelesList = ed.getVector().get().getAnopheles();
    
    for( scnXml::Vector::AnophelesIterator it = anophelesList.begin();
	it != anophelesList.end(); ++it ){//FIXME
	double old_a0 = it->getEir().get().getA0();
	it->getEir().get().setA0( old_a0 + add_to_a0 );
    }
}

TimeStep VectorTransmission::transmissionInitDuration () {
    // Data is summed over the last year of human initialisation
    // plus an extra 4 years.
    return TimeStep::fromYears( 4 );
}
TimeStep VectorTransmission::transmissionInitIterate () {
  bool iterate = false;
  for (size_t i = 0; i < numSpecies; ++i)
    iterate |= species[i].vectorInitIterate ();
  if (iterate) {
    simulationMode = equilibriumMode;
    return TimeStep::fromYears(2);	// Data is summed over one year, so allow extra time for some stabilisation first.
  } else {
    simulationMode = dynamicEIR;
    return TimeStep(0);
  }
}

void VectorTransmission::initMainSimulation() {
  // Check every time at end of init that, to a very low tolerence, the average
  // EIR produced is what was expected. Note: it's become clear the simulated
  // EIR is never going to very accurately match the input EIR.
  if (!vectors::approxEqual(initialisationEIR, innoculationsPerDayOfYear, 1)) {
    cerr << "Generated EIR not as expected (expected, generated):\n";
    cerr << initialisationEIR << '\n' << innoculationsPerDayOfYear << endl;
  }
  
  simulationMode = InputData().getEntoData().getMode();	// allow forcing equilibrium mode like with non-vector model
  if (simulationMode != 2 && simulationMode != 4)
    throw util::xml_scenario_error("mode attribute has invalid value (expected: 2 or 4)");
}

double VectorTransmission::calculateEIR(PerHostTransmission& host, double ageYears) {
  if (simulationMode == equilibriumMode)
    return initialisationEIR[TimeStep::simulation % TimeStep::stepsPerYear]
	 * host.relativeAvailabilityHetAge (ageYears);
  
  double EIR = 0.0;
  for (size_t i = 0; i < numSpecies; ++i) {
    EIR += species[i].calculateEIR (i, host);
  }
  return EIR * host.relativeAvailabilityAge (ageYears);
}


// Every Global::interval days:
void VectorTransmission::vectorUpdate (const std::list<Host::Human>& population) {
  for (size_t i = 0; i < numSpecies; ++i)
    species[i].advancePeriod (population, i, simulationMode == dynamicEIR);
}

void VectorTransmission::intervLarviciding (const scnXml::Larviciding& anoph) {
  const scnXml::Larviciding::AnophelesSequence& seq = anoph.getAnopheles();
  for (scnXml::Larviciding::AnophelesSequence::const_iterator it = seq.begin(); it != seq.end(); ++it)
    species[getSpeciesIndex(it->getMosquito())].intervLarviciding(*it);
}
void VectorTransmission::uninfectVectors(){
  for (size_t i = 0; i < numSpecies; ++i)
    species[i].uninfectVectors();
}

void VectorTransmission::summarize (Monitoring::Survey& survey) {
    TransmissionModel::summarize (survey);
    
    for (map<string,size_t>::const_iterator it = speciesIndex.begin(); it != speciesIndex.end(); ++it)
	species[it->second].summarize (it->first, survey);
}


void VectorTransmission::checkpoint (istream& stream) {
    TransmissionModel::checkpoint (stream);
    util::checkpoint::checkpoint (species, stream, VectorAnopheles (this));
}
void VectorTransmission::checkpoint (ostream& stream) {
    TransmissionModel::checkpoint (stream);
    species & stream;
}

} }
