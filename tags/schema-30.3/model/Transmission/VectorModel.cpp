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

#include "Transmission/VectorModel.h"
#include "inputData.h"
#include "Monitoring/Continuous.h"
#include "util/vectors.h"
#include "util/ModelOptions.h"

#include <fstream>
#include <map>
#include <cmath>

namespace OM {
namespace Transmission {
using namespace OM::util;

double VectorModel::meanPopAvail (const std::list<Host::Human>& population, int populationSize) {
    double sumRelativeAvailability = 0.0;
    for (std::list<Host::Human>::const_iterator h = population.begin(); h != population.end(); ++h){
        sumRelativeAvailability += h->perHostTransmission.relativeAvailabilityAge (h->getAgeInYears());
    }
    if( populationSize > 0 ){
        return sumRelativeAvailability / populationSize;     // mean-rel-avail
    }else{
        // value should be unimportant when no humans are available, though inf/nan is not acceptable
        return 1.0;
    }
}

void VectorModel::ctsCbN_v0 (ostream& stream) {
    for (size_t i = 0; i < numSpecies; ++i)
        stream << '\t' << species[i].getLastN_v0();
}
void VectorModel::ctsCbP_A (ostream& stream) {
    for (size_t i = 0; i < numSpecies; ++i)
        stream << '\t' << species[i].getLastVecStat(Anopheles::PA);
}
void VectorModel::ctsCbP_df (ostream& stream) {
    for (size_t i = 0; i < numSpecies; ++i)
        stream << '\t' << species[i].getLastVecStat(Anopheles::PDF);
}
void VectorModel::ctsCbP_dif (ostream& stream) {
    for (size_t i = 0; i < numSpecies; ++i)
        stream << '\t' << species[i].getLastVecStat(Anopheles::PDIF);
}
void VectorModel::ctsCbN_v (ostream& stream) {
    for (size_t i = 0; i < numSpecies; ++i)
        stream << '\t' << species[i].getLastVecStat(Anopheles::NV);
}
void VectorModel::ctsCbO_v (ostream& stream) {
    for (size_t i = 0; i < numSpecies; ++i)
        stream << '\t' << species[i].getLastVecStat(Anopheles::OV);
}
void VectorModel::ctsCbS_v (ostream& stream) {
    for (size_t i = 0; i < numSpecies; ++i)
        stream << '\t' << species[i].getLastVecStat(Anopheles::SV);
}
void VectorModel::ctsCbAlpha (const Population& population, ostream& stream){
    for( size_t i = 0; i < numSpecies; ++i){
        const Anopheles::PerHostBase& params = species[i].getHumanBaseParams();
        double total = 0.0;
        for (Population::ConstHumanIter iter = population.getList().begin(),
                end = population.getList().end(); iter != end; ++iter) {
            total += iter->perHostTransmission.entoAvailabilityFull( params, i, iter->getAgeInYears() );
        }
        stream << '\t' << total / population.getSize();
    }
}
void VectorModel::ctsCbP_B (const Population& population, ostream& stream){
    for( size_t i = 0; i < numSpecies; ++i){
	const Anopheles::PerHostBase& params = species[i].getHumanBaseParams();
        double total = 0.0;
        for (Population::ConstHumanIter iter = population.getList().begin(),
                end = population.getList().end(); iter != end; ++iter) {
            total += iter->perHostTransmission.probMosqBiting( params, i );
        }
        stream << '\t' << total / population.getSize();
    }
}
void VectorModel::ctsCbP_CD (const Population& population, ostream& stream){
    for( size_t i = 0; i < numSpecies; ++i){
	const Anopheles::PerHostBase& params = species[i].getHumanBaseParams();
        double total = 0.0;
        for (Population::ConstHumanIter iter = population.getList().begin(),
                end = population.getList().end(); iter != end; ++iter) {
            total += iter->perHostTransmission.probMosqResting( params, i );
        }
        stream << '\t' << total / population.getSize();
    }
}
void VectorModel::ctsNetInsecticideContent (const Population& population, ostream& stream){
    double meanVar = 0.0;
    int n = 0;
    for (Population::ConstHumanIter iter = population.getList().begin(),
            end = population.getList().end(); iter != end; ++iter) {
        if( iter->perHostTransmission.getITN().timeOfDeployment() >= TimeStep(0) ){
            ++n;
            meanVar += iter->perHostTransmission.getITN().getInsecticideContent(_ITNParams);
        }
    }
    stream << '\t' << meanVar/n;
}
void VectorModel::ctsIRSInsecticideContent (const Population& population, ostream& stream) {
    double totalInsecticide = 0.0;
    for (Population::ConstHumanIter iter = population.getList().begin(),
            end = population.getList().end(); iter != end; ++iter) {
        totalInsecticide += iter->perHostTransmission.getIRS().getInsecticideContent(_IRSParams);
    }
    stream << '\t' << totalInsecticide / population.getSize();
}
void VectorModel::ctsIRSEffects (const Population& population, ostream& stream) {
    for( size_t i = 0; i < numSpecies; ++i ){
        const IRSAnophelesParams& params = species[i].getHumanBaseParams().irs;
        double totalRA = 0.0, totalPrePSF = 0.0, totalPostPSF = 0.0;
        for (Population::ConstHumanIter iter = population.getList().begin(),
                end = population.getList().end(); iter != end; ++iter) {
            totalRA += iter->perHostTransmission.getIRS().relativeAttractiveness(params);
            totalPrePSF += iter->perHostTransmission.getIRS().preprandialSurvivalFactor(params);
            totalPostPSF += iter->perHostTransmission.getIRS().postprandialSurvivalFactor(params);
        }
        stream << '\t' << totalRA / population.getSize()
            << '\t' << totalPrePSF / population.getSize()
            << '\t' << totalPostPSF / population.getSize();
    }
}

void VectorModel::ctsCbResAvailability (ostream& stream) {
    for (size_t i = 0; i < numSpecies; ++i)
        stream << '\t' << species[i].getResAvailability();
}
void VectorModel::ctsCbResRequirements (ostream& stream) {
    for (size_t i = 0; i < numSpecies; ++i)
        stream << '\t' << species[i].getResRequirements();
}

const string& reverseLookup (const map<string,size_t>& m, size_t i) {
    for ( map<string,size_t>::const_iterator it = m.begin(); it != m.end(); ++it ) {
        if ( it->second == i )
            return it->first;
    }
    throw TRACED_EXCEPTION_DEFAULT( "reverseLookup: key not found" );        // shouldn't ever happen
}

VectorModel::VectorModel (const scnXml::Vector vectorData, int populationSize)
    : initIterations(0), numSpecies(0)
{
    // Each item in the AnophelesSequence represents an anopheles species.
    // TransmissionModel::createTransmissionModel checks length of list >= 1.
    const scnXml::Vector::AnophelesSequence anophelesList = vectorData.getAnopheles();
    const scnXml::Vector::NonHumanHostsSequence nonHumansList = vectorData.getNonHumanHosts();

    map<string, double> nonHumanHostPopulations;

    for (size_t i = 0; i<nonHumansList.size(); i++)
        nonHumanHostPopulations[nonHumansList[i].getName()] = nonHumansList[i].getNumber();

    numSpecies = anophelesList.size();
    if (numSpecies < 1)
        throw util::xml_scenario_error ("Can't use Vector model without data for at least one anopheles species!");
    species.resize (numSpecies, AnophelesModel(&_ITNParams, &_IRSParams));

    for (size_t i = 0; i < numSpecies; ++i) {
        string name = species[i].initialise (anophelesList[i],
                                             initialisationEIR,
                                             nonHumanHostPopulations,
                                             populationSize);
        speciesIndex[name] = i;
    }
    
    // Calculate total annual EIR
    annualEIR = vectors::sum( initialisationEIR );


    if( interventionMode == forcedEIR ) {
        // We don't need these anymore (now we have initialisationEIR); free memory
        numSpecies = 0;
        species.clear();
        speciesIndex.clear();
    }


    // -----  Continuous reporting  -----
    ostringstream ctsNv0, ctsPA, ctsPdf, ctsPdif,
        ctsNv, ctsOv, ctsSv,
        ctsAlpha, ctsPB, ctsPCD,
        ctsIRSEffects,
        ctsRA, ctsRR;
    // Output in order of species so that (1) we can just iterate through this
    // list when outputting and (2) output is in order specified in XML.
    for (size_t i = 0; i < numSpecies; ++i) {
        // Unfortunately, we need to reverse-lookup the name.
        const string& name = reverseLookup( speciesIndex, i );
        ctsNv0<<"\tN_v0("<<name<<")";
        ctsPA<<"\tP_A("<<name<<")";
        ctsPdf<<"\tP_df("<<name<<")";
        ctsPdif<<"\tP_dif("<<name<<")";
        ctsNv<<"\tN_v("<<name<<")";
        ctsOv<<"\tO_v("<<name<<")";
        ctsSv<<"\tS_v("<<name<<")";
        ctsAlpha<<"\talpha_i("<<name<<")";
        ctsPB<<"\tP_B("<<name<<")";
        ctsPCD<<"\tP_C*P_D("<<name<<")";
        ctsIRSEffects<<"\tIRS rel attr ("<<name<<")"
            <<"\tIRS preprand surv factor ("<<name<<")"
            <<"\tIRS postprand surv factor ("<<name<<")";
        ctsRA<<"\tres avail("<<name<<")";
        ctsRR<<"\tres req("<<name<<")";
    }
    using Monitoring::Continuous;
    Continuous.registerCallback( "N_v0", ctsNv0.str(), MakeDelegate( this, &VectorModel::ctsCbN_v0 ) );
    Continuous.registerCallback( "P_A", ctsPA.str(), MakeDelegate( this, &VectorModel::ctsCbP_A ) );
    Continuous.registerCallback( "P_df", ctsPdf.str(), MakeDelegate( this, &VectorModel::ctsCbP_df ) );
    Continuous.registerCallback( "P_dif", ctsPdif.str(), MakeDelegate( this, &VectorModel::ctsCbP_dif ) );
    Continuous.registerCallback( "N_v", ctsNv.str(), MakeDelegate( this, &VectorModel::ctsCbN_v ) );
    Continuous.registerCallback( "O_v", ctsOv.str(), MakeDelegate( this, &VectorModel::ctsCbO_v ) );
    Continuous.registerCallback( "S_v", ctsSv.str(), MakeDelegate( this, &VectorModel::ctsCbS_v ) );
    // availability to mosquitoes relative to other humans, excluding age factor
    Continuous.registerCallback( "alpha", ctsAlpha.str(), MakeDelegate( this, &VectorModel::ctsCbAlpha ) );
    Continuous.registerCallback( "P_B", ctsPB.str(), MakeDelegate( this, &VectorModel::ctsCbP_B ) );
    Continuous.registerCallback( "P_C*P_D", ctsPCD.str(), MakeDelegate( this, &VectorModel::ctsCbP_CD ) );
    Continuous.registerCallback( "mean insecticide content",
        "\tmean insecticide content",
        MakeDelegate( this, &VectorModel::ctsNetInsecticideContent ) );
    // Mean IRS insecticide across whole population
    Continuous.registerCallback( "IRS insecticide content",
        "\tIRS insecticide content",
        MakeDelegate( this, &VectorModel::ctsIRSInsecticideContent ) );
    // Mean values of relative attractiveness, pre- and post-prandial survival
    // IRS-induced factors of mosquitoes (i.e. only the portion of deterrent
    // and killing effects attributable to IRS).
    Continuous.registerCallback( "IRS effects", ctsIRSEffects.str(),
        MakeDelegate( this, &VectorModel::ctsIRSEffects ) );
    Continuous.registerCallback( "resource availability", ctsRA.str(),
        MakeDelegate( this, &VectorModel::ctsCbResAvailability ) );
    Continuous.registerCallback( "resource requirements", ctsRR.str(),
        MakeDelegate( this, &VectorModel::ctsCbResRequirements ) );
}
VectorModel::~VectorModel () {
}

void VectorModel::init2 (const std::list<Host::Human>& population, int populationSize) {
    double mPA = meanPopAvail(population, populationSize);
    for (size_t i = 0; i < numSpecies; ++i) {
        species[i].init2 (i, population, populationSize, mPA);
    }
    simulationMode = forcedEIR;   // now we should be ready to start
}


void VectorModel::scaleEIR (double factor) {
    for ( size_t i = 0; i < numSpecies; ++i )
        species[i].scaleEIR( factor );
    vectors::scale( initialisationEIR, factor );
    annualEIR = vectors::sum( initialisationEIR );
}
#if 0
void VectorModel::scaleXML_EIR (scnXml::EntoData& ed, double factor) const {
    // XML values are exponentiated; so we add some factor to existing a0 values:
    double add_to_a0 = std::log( factor );

    assert( ed.getVector().present() );
    scnXml::Vector::AnophelesSequence& anophelesList = ed.getVector().get().getAnopheles();

    for ( scnXml::Vector::AnophelesIterator it = anophelesList.begin();
            it != anophelesList.end(); ++it ) {
        if( it->getEIR().present() ){
            double old_a0 = it->getEIR().get().getA0();
            it->getEIR().get().setA0( old_a0 + add_to_a0 );
        }else{
            // schema should enforce that one of the two is here
            assert( it->getMonthlyEIR().present() );
            double oldAnnual = it->getMonthlyEIR().get().getAnnualEIR();
            it->getMonthlyEIR().get().setAnnualEIR( oldAnnual * factor );
        }
    }
}
#endif


TimeStep VectorModel::minPreinitDuration () {
    if ( interventionMode == forcedEIR ) {
        return TimeStep(0);
    }
    // Data is summed over 5 years; add an extra 50 for stabilization.
    // 50 years seems a reasonable figure from a few tests
    return TimeStep::fromYears( 55 );
}
TimeStep VectorModel::expectedInitDuration (){
    return TimeStep::fromYears( 1 );
}

TimeStep VectorModel::initIterate () {
    if( interventionMode != dynamicEIR ) {
        // allow forcing equilibrium mode like with non-vector model
        return TimeStep(0); // no initialization to do
    }
    if( initIterations < 0 ){
        assert( interventionMode = dynamicEIR );
        simulationMode = dynamicEIR;
        return TimeStep(0);
    }
    
    ++initIterations;
    
    bool needIterate = false;
    for (size_t i = 0; i < numSpecies; ++i) {
        needIterate = needIterate || species[i].initIterate ();
    }
    if( needIterate == false ){
        initIterations = -1;
    }
    if( initIterations > 10 ){
        throw TRACED_EXCEPTION("Transmission warmup exceeded 10 iterations!",util::Error::VectorWarmup);
    }
    
    // Time to let parameters settle after each iteration. I would expect one year
    // to be enough (but I may be wrong).
    if( needIterate )
        // stabilization + 5 years data-collection time:
        return TimeStep::fromYears( 1 ) + TimeStep::fromYears(5);
    else
        return TimeStep::fromYears( 1 );
}

double VectorModel::calculateEIR(PerHost& host, double ageYears) {
    host.update(_ITNParams);
    if (simulationMode == forcedEIR){
        return initialisationEIR[TimeStep::simulation % TimeStep::stepsPerYear]
               * host.relativeAvailabilityHetAge (ageYears);
    }else{
        assert( simulationMode == dynamicEIR );
        double simEIR = 0.0;
        for (size_t i = 0; i < numSpecies; ++i) {
            simEIR += species[i].calculateEIR (i, host);
        }
        simEIR *= host.relativeAvailabilityAge (ageYears);
        return simEIR;
    }
}


// Every Global::interval days:
void VectorModel::vectorUpdate (const std::list<Host::Human>& population, int populationSize) {
    for (size_t i = 0; i < numSpecies; ++i){
        species[i].advancePeriod (population, populationSize, i, simulationMode == dynamicEIR);
    }
}
void VectorModel::update (const std::list<Host::Human>& population, int populationSize) {
    TransmissionModel::updateKappa( population );
}

void VectorModel::checkSimMode() const{
    if( interventionMode != dynamicEIR ){
        throw xml_scenario_error("vector interventions can only be used in "
            "dynamic transmission mode (mode=\"dynamic\")");
    }
}

void VectorModel::setITNDescription (const scnXml::ITNDescription& elt){
    checkSimMode();
    double proportionUse = _ITNParams.init( elt );
    typedef scnXml::ITNDescription::AnophelesParamsSequence AP;
    const AP& ap = elt.getAnophelesParams();
    if( ap.size() != numSpecies ){
        throw util::xml_scenario_error(
            "ITN.description.anophelesParams: must have one element for each "
            "mosquito species described in entomology"
        );
    }
    for( AP::const_iterator it = ap.begin(); it != ap.end(); ++it ){
        species[getSpeciesIndex(it->getMosquito())].setITNDescription (_ITNParams, *it, proportionUse);
    }
}
void VectorModel::setIRSDescription (const scnXml::IRS& elt){
    checkSimMode();
    if( elt.getDescription().present() ){
        _IRSParams.init( elt.getDescription().get() );
        
        typedef scnXml::IRSDescription_v1::AnophelesParamsSequence AP;
        const AP& ap = elt.getDescription().get().getAnophelesParams();
        if( ap.size() != numSpecies ){
            throw util::xml_scenario_error(
                "IRS.simpleDescription.anophelesParams: must have one element "
                "for each mosquito species described in entomology"
            );
        }
        for( AP::const_iterator it = ap.begin(); it != ap.end(); ++it ) {
            species[getSpeciesIndex(it->getMosquito())].setIRSDescription (_IRSParams, *it);
        }
    }else{
        assert( elt.getDescription_v2().present() );   // choice: one or the other
        _IRSParams.init( elt.getDescription_v2().get() );
        
        typedef scnXml::IRSDescription_v2::AnophelesParamsSequence AP;
        const AP& ap = elt.getDescription_v2().get().getAnophelesParams();
        if( ap.size() != numSpecies ){
            throw util::xml_scenario_error(
                "IRS.description.anophelesParams: must have one element for "
                "each mosquito species described in entomology"
            );
        }
        for( AP::const_iterator it = ap.begin(); it != ap.end(); ++it ) {
            species[getSpeciesIndex(it->getMosquito())].setIRSDescription (_IRSParams, *it);
        }
    }
}
void VectorModel::setVADescription (const scnXml::VectorDeterrent& elt){
    checkSimMode();
    PerHost::setVADescription (elt);
    typedef scnXml::VectorDeterrent::AnophelesParamsSequence AP;
    const AP& ap = elt.getAnophelesParams();
    if( ap.size() != numSpecies ){
        throw util::xml_scenario_error(
            "vectorDeterrent.anophelesParams: must have one element for each "
            "mosquito species described in entomology"
        );
    }
    for( AP::const_iterator it = ap.begin(); it != ap.end(); ++it ) {
        species[getSpeciesIndex(it->getMosquito())].setVADescription (*it);
    }
}

void VectorModel::intervLarviciding (const scnXml::Larviciding::DescriptionType& desc) {
    checkSimMode();
    
    typedef scnXml::Larviciding::DescriptionType::AnophelesSequence AnophSeq;
    const AnophSeq& seq = desc.getAnopheles();
    
    if( seq.size() != numSpecies ){
        throw util::xml_scenario_error(
            "larviciding.anopheles: must have one element for each mosquito "
            "species described in entomology"
        );
    }
    
    for (AnophSeq::const_iterator it = seq.begin();
            it != seq.end(); ++it) {
        species[getSpeciesIndex(it->getMosquito())].intervLarviciding(*it);
    }
    
}
void VectorModel::uninfectVectors() {
    for (size_t i = 0; i < numSpecies; ++i)
        species[i].uninfectVectors();
}

void VectorModel::summarize (Monitoring::Survey& survey) {
    TransmissionModel::summarize (survey);

    for (map<string,size_t>::const_iterator it = speciesIndex.begin(); it != speciesIndex.end(); ++it)
        species[it->second].summarize (it->first, survey);
}


void VectorModel::checkpoint (istream& stream) {
    TransmissionModel::checkpoint (stream);
    initIterations & stream;
    util::checkpoint::checkpoint (species, stream, AnophelesModel (&_ITNParams, &_IRSParams));
}
void VectorModel::checkpoint (ostream& stream) {
    TransmissionModel::checkpoint (stream);
    initIterations & stream;
    species & stream;
}

}
}
