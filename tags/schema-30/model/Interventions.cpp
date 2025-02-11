/* This file is part of OpenMalaria.
 *
 * Copyright (C) 2005-2011 Swiss Tropical Institute and Liverpool School Of Tropical Medicine
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

#include "Interventions.h"
#include "Host/Vaccine.h"
#include "Population.h"
#include "util/random.h"
#include "Clinical/ESCaseManagement.h"
#include "Clinical/ImmediateOutcomes.h"
#include "WithinHost/DescriptiveIPTWithinHost.h"
#include "Clinical/CaseManagementCommon.h"
#include "Monitoring/Surveys.h"

namespace OM {

// -----  AgeIntervention  -----

AgeIntervention::AgeIntervention(
    const ::scnXml::ContinuousDeployment& elt, void(Host::Human::*func) (const OM::Population&)
) :
    begin( elt.getBegin() ),
    end( elt.getEnd() ),
    ageTimesteps( TimeStep::fromYears( elt.getTargetAgeYrs() ) ),
    cohortOnly( elt.getCohort() ),
    coverage( elt.getCoverage() ),
    deploy( func )
{
    if( begin < TimeStep(0) || end < begin ){
        throw util::xml_scenario_error("continuous intervention must have 0 <= begin <= end");
    }
    if( ageTimesteps <= TimeStep(0) ){
        ostringstream msg;
        msg << "continuous intervention with target age "<<elt.getTargetAgeYrs();
        msg << " years corresponds to timestep "<<ageTimesteps;
        msg << "; must be at least timestep 1.";
        throw util::xml_scenario_error( msg.str() );
    }
    if( ageTimesteps > TimeStep::maxAgeIntervals ){
        ostringstream msg;
        msg << "continuous intervention must have target age no greater than ";
        msg << TimeStep::maxAgeIntervals * TimeStep::yearsPerInterval;
        throw util::xml_scenario_error( msg.str() );
    }
    if( !(coverage >= 0.0 && coverage <= 1.0) ){
        throw util::xml_scenario_error("continuous intervention coverage must be in range [0,1]");
    }
}

// -----  TimedIntervention and derivatives  -----

TimedIntervention::TimedIntervention(TimeStep deploymentTime) :
    time( deploymentTime )
{
    if( deploymentTime < TimeStep(0) ){
        throw util::xml_scenario_error("timed intervention deployment: may not be negative");
    }else if( deploymentTime >= Monitoring::Surveys.getFinalTimestep() ){
        cerr << "Warning: timed intervention deployment at time "<<deploymentTime.asInt();
        cerr << " happens after last survey" << endl;
    }
}

class DummyIntervention : public TimedIntervention {
public:
    DummyIntervention() :
        TimedIntervention( TimeStep(0) )
    {
        // TimedIntervention's ctor checks that the deployment time-step is
        // within the intervention period. We want this time to be after the
        // last time-step, so set the time here after TimedIntervention's ctor
        // check has been done (hacky).
        time = TimeStep::future;
    }
    virtual void deploy (OM::Population&) {}
};

class TimedChangeHSIntervention : public TimedIntervention {
public:
    TimedChangeHSIntervention( const scnXml::ChangeHS::TimedDeploymentType& hs ) :
        TimedIntervention( TimeStep( hs.getTime() ) ),
        newHS( hs._clone() )
    {}
    virtual void deploy (OM::Population& population) {
        Clinical::CaseManagementCommon::changeHealthSystem( *newHS );
        delete newHS;
        newHS = 0;
    }
    
private:
    scnXml::HealthSystem *newHS;
};

class TimedChangeEIRIntervention : public TimedIntervention {
public:
    TimedChangeEIRIntervention( const scnXml::ChangeEIR::TimedDeploymentType& nv ) :
        TimedIntervention( TimeStep( nv.getTime() ) ),
        newEIR( nv._clone() )
    {}
    virtual void deploy (OM::Population& population) {
        population.transmissionModel().changeEIRIntervention( *newEIR );
        delete newEIR;
        newEIR = 0;
    }
    
private:
    scnXml::NonVector *newEIR;
};

class TimedUninfectVectorsIntervention : public TimedIntervention {
public:
    TimedUninfectVectorsIntervention( TimeStep deployTime ) :
        TimedIntervention( deployTime )
    {}
    virtual void deploy (OM::Population& population) {
        population.transmissionModel().uninfectVectors();
    }
};

class TimedR_0Intervention : public TimedIntervention {
public:
    TimedR_0Intervention( TimeStep deployTime ) :
        TimedIntervention( deployTime )
    {}
    virtual void deploy (OM::Population& population) {
        int i = (int)std::floor (util::random::uniform_01() * population.getSize());        // pick a human
        Population::HumanIter it = population.getList().begin();
        while (i > 0){  // find human (can't use population[i])
            ++it;
            --i;
        }
        assert( i == 0 );
        assert( it != population.getList().end() );
        it->R_0Vaccines();
        it->addInfection();
    }
};

/// Deployment of mass-to-human interventions
class TimedMassIntervention : public TimedIntervention {
public:
    /** 
     * @param mass XML element specifying the age range and compliance
     * (proportion of eligible individuals who receive the intervention).
     * @param deployIntervention A member-function pointer to a
     *      "void func (const OM::Population&)" function within human which
     *      activates the intervention. Population is passed for acces to static
     *      params. */
    TimedMassIntervention( const scnXml::Mass& mass,
                           void (Host::Human::*deployIntervention)(const OM::Population&) ) :
        TimedIntervention( TimeStep( mass.getTime() ) ),
        minAge( TimeStep::fromYears( mass.getMinAge() ) ),
        maxAge( TimeStep::fromYears( mass.getMaxAge() ) ),
        cohortOnly( mass.getCohort() ),
        coverage( mass.getCoverage() ),
        intervention( deployIntervention )
    {
        if( !(coverage >= 0.0 && coverage <= 1.0) ){
            throw util::xml_scenario_error("timed intervention coverage must be in range [0,1]");
        }
        if( minAge < TimeStep(0) || maxAge < minAge ){
            throw util::xml_scenario_error("timed intervention must have 0 <= minAge <= maxAge");
        }
    }
    
    virtual void deploy (OM::Population& population) {
        Population::HumanPop& popList = population.getList();
        for (Population::HumanIter iter = popList.begin(); iter != popList.end(); ++iter) {
            TimeStep age = TimeStep::simulation - iter->getDateOfBirth();
            if( age >= minAge && age < maxAge ){
                if( !cohortOnly || iter->getInCohort() ){
                    if( util::random::uniform_01() < coverage ){
                        // This is UGLY syntax. It just means call intervention() on the human pointed by iter.
                        ( (*iter).*intervention) (population);
                    }
                }
            }
        }
    }
    
protected:
    // restrictions on deployment
    TimeStep minAge;
    TimeStep maxAge;
    bool cohortOnly;
    double coverage;    // proportion coverage within group meeting above restrictions
    void (Host::Human::*intervention) (const OM::Population&);       // callback: per-human deployment
};

/// Deployment of mass-to-human interventions with cumulative-deployment support
class TimedMassCumIntervention : public TimedMassIntervention {
public:
    /** As massIntervention, but supports "increase to target coverage" mode:
     * Deployment is only to unprotected humans and brings
     * total coverage up to the level given in description.
     * 
     * @param mass XML element specifying the age range and compliance
     * (proportion of eligible individuals who receive the intervention).
     * @param deployIntervention A member-function pointer to a
     *      "void func (const OM::Population&)" function within human which
     *      activates the intervention. Population is passed for acces to static
     *      params.
     * @param isProtectedCb A member-function pointer to a
     * "bool func (TimeStep maxAge)" function on a Human which returns true if
     * the Human is still protected by an intervention of the type in question
     * which is no older than maxAge. */
    TimedMassCumIntervention( const scnXml::MassCum& mass,
                              void (Host::Human::*deployIntervention)(const OM::Population&),
                              bool (Host::Human::*isProtectedCb) (TimeStep) const ) :
        TimedMassIntervention( mass, deployIntervention ),
        isProtected( isProtectedCb ),
        maxInterventionAge( TimeStep::fromYears( mass.getCumulativeWithMaxAge().get() ) )
    {}
    
    void deploy(OM::Population& population){
        // Cumulative case: bring target group's coverage up to target coverage
        Population::HumanPop& popList = population.getList();
        vector<Host::Human*> unprotected;
        size_t total = 0;       // number of humans within age bound and optionally cohort
        for (Population::HumanIter iter = popList.begin(); iter != popList.end(); ++iter) {
            TimeStep age = TimeStep::simulation - iter->getDateOfBirth();
            if( age >= minAge && age < maxAge ){
                if( !cohortOnly || iter->getInCohort() ){
                    total+=1;
                    if( !((*iter).*isProtected)(maxInterventionAge) )
                        unprotected.push_back( &*iter );
                }
            }
        }
        
        double propProtected = static_cast<double>( total - unprotected.size() ) / static_cast<double>( total );
        if( propProtected < coverage ){
            // Proportion propProtected are already covered, so need to
            // additionally cover the proportion (coverage - propProtected),
            // selected from the list unprotected.
            double additionalCoverage = (coverage - propProtected) / (1.0 - propProtected);
            for (vector<Host::Human*>::iterator iter = unprotected.begin();
                 iter != unprotected.end(); ++iter)
            {
                if( util::random::uniform_01() < additionalCoverage ){
                    ( (**iter).*intervention) (population);
                }
            }
        }
    }
    
private:
    // callback to ascertain whether a human is still under protection from an
    // intervention young enough not to need replacement
    bool (Host::Human::*isProtected) (TimeStep) const;
    // max age at which an intervention is considered not to need replacement
    TimeStep maxInterventionAge;
};

class TimedLarvicideIntervention : public TimedIntervention {
public:
    TimedLarvicideIntervention( TimeStep deployTime, const scnXml::Larviciding& larviciding) :
        TimedIntervention( deployTime ),
        _larviciding(larviciding)
	
    {}
    virtual void deploy (OM::Population& population) {
      population.transmissionModel().intervLarviciding(_larviciding);
    }
    
private:
  scnXml::Larviciding _larviciding;
};

/** Create either a TimedMassCumIntervention or a TimedMassIntervention,
 * depending on whether the cumulativeWithMaxAge attribute is present.
 * 
 * @param mass XML element specifying the age range and compliance
 * (proportion of eligible individuals who receive the intervention).
 * @param deployIntervention A member-function pointer to a
 *      "void func (const OM::Population&)" function within human which
 *      activates the intervention. Population is passed for acces to static params.
 * @param isProtectedCb A member-function pointer to a
 * "bool func (TimeStep maxAge)" function on a Human which returns true if
 * the Human is still protected by an intervention of the type in question
 * which is no older than maxAge. */
TimedMassIntervention* createTimedMassCumIntervention(
    const scnXml::MassCum& mass,
    void (Host::Human::*deployIntervention)(const OM::Population&),
    bool (Host::Human::*isProtectedCb) (TimeStep) const
){
    if( mass.getCumulativeWithMaxAge().present() ){
        return new TimedMassCumIntervention( mass, deployIntervention, isProtectedCb );
    }else{
        return new TimedMassIntervention( mass, deployIntervention );
    }
}


// -----  InterventionManager  -----

InterventionManager::InterventionManager (const scnXml::Interventions& intervElt, OM::Population& population) :
    nextTimed(0)
{
    if( intervElt.getChangeHS().present() ){
        const scnXml::ChangeHS& chs = intervElt.getChangeHS().get();
        if( chs.getTimedDeployment().size() > 0 ){
            activeInterventions.set (Interventions::CHANGE_HS, true);
            // timed deployments:
            typedef scnXml::ChangeHS::TimedDeploymentSequence::const_iterator It;
            for( It it = chs.getTimedDeployment().begin(); it != chs.getTimedDeployment().end(); ++it ){
                timed.push_back( new TimedChangeHSIntervention( *it ) );
            }
        }
    }
    if( intervElt.getChangeEIR().present() ){
        const scnXml::ChangeEIR& eir = intervElt.getChangeEIR().get();
        if( eir.getTimedDeployment().size() > 0 ){
            activeInterventions.set (Interventions::CHANGE_EIR, true);
            // timed deployments:
            typedef scnXml::ChangeEIR::TimedDeploymentSequence::const_iterator It;
            for( It it = eir.getTimedDeployment().begin(); it != eir.getTimedDeployment().end(); ++it ){
                timed.push_back( new TimedChangeEIRIntervention( *it ) );
            }
        }
    }
    if( intervElt.getMDA().present() ){
        const scnXml::MDA& mda = intervElt.getMDA().get();
        if( mda.getTimed().present() ){
            activeInterventions.set( Interventions::MDA, true );
            // read description:
            if( TimeStep::interval == 5 ){
                if( !mda.getDiagnostic().present() ){
                    // Note: allow no description for now to avoid XML changes.
                    //throw util::xml_scenario_error( "error: interventions.MDA.diagnostic element required for MDA with 5-day timestep" );
                    scnXml::HSDiagnostic diagnostic;
                    scnXml::Deterministic det(0.0);
                    diagnostic.setDeterministic(det);
                    Clinical::ClinicalImmediateOutcomes::initMDA(diagnostic);
                }else{
                    Clinical::ClinicalImmediateOutcomes::initMDA( mda.getDiagnostic().get() );
                }
            }else{
                if( !mda.getDescription().present() ){
                    throw util::xml_scenario_error( "error: interventions.MDA.description element required for MDA with 1-day timestep" );
                }
                Clinical::ESCaseManagement::initMDA( mda.getDescription().get() );
            }
            // timed deployments:
            const scnXml::MassList::DeploySequence& seq = mda.getTimed().get().getDeploy();
            typedef scnXml::MassList::DeploySequence::const_iterator It;
            for( It it = seq.begin(); it != seq.end(); ++it ){
                timed.push_back( new TimedMassIntervention( *it, &Host::Human::massDrugAdministration ) );
            }
        }
    }
    if( intervElt.getVaccine().present() ){
        const scnXml::Vaccine& vacc = intervElt.getVaccine().get();
        if( vacc.getContinuous().present() || vacc.getTimed().present() ){
            activeInterventions.set (Interventions::VACCINE, true);
            // read descriptions:
            Host::Vaccine::init( vacc );
            // continuous deployments:
            // We let ctsVaccinate determine whether according to past vaccinations this dose should be administered
            if( vacc.getContinuous().present() ){
                const scnXml::ContinuousList::DeploySequence& seq = vacc.getContinuous().get().getDeploy();
                typedef scnXml::ContinuousList::DeploySequence::const_iterator CIt;
                for( CIt it = seq.begin(); it != seq.end(); ++it ){
                    ctsIntervs.push_back( AgeIntervention( *it, &Host::Human::ctsVaccinate ) );
                }
            }
            // timed deployments:
            if( vacc.getTimed().present() ){
                const scnXml::MassCumList::DeploySequence& seq = vacc.getTimed().get().getDeploy();
                typedef scnXml::MassCumList::DeploySequence::const_iterator It;
                for( It it = seq.begin(); it != seq.end(); ++it ){
                    timed.push_back( createTimedMassCumIntervention( *it, &Host::Human::massVaccinate, &Host::Human::hasVaccineProtection ) );
                }
            }
        }
    }
    if( intervElt.getIPT().present() ){
        const scnXml::IPT& ipt = intervElt.getIPT().get();
        // read description (note: this is required by IPT WIH model, so do it even if there is no deployment)
        activeInterventions.set (Interventions::IPTI, true);
        WithinHost::DescriptiveIPTWithinHost::init( ipt.getDescription() );
        // continuous deployments:
        if( ipt.getContinuous().present() ){
            const scnXml::ContinuousList::DeploySequence& seq = ipt.getContinuous().get().getDeploy();
            typedef scnXml::ContinuousList::DeploySequence::const_iterator CIt;
            for( CIt it = seq.begin(); it != seq.end(); ++it ){
                ctsIntervs.push_back( AgeIntervention( *it, &Host::Human::continuousIPT ) );
            }
        }
        // timed deployments:
        if( ipt.getTimed().present() ){
            const scnXml::MassCumList::DeploySequence& seq = ipt.getTimed().get().getDeploy();
            typedef scnXml::MassCumList::DeploySequence::const_iterator It;
            for( It it = seq.begin(); it != seq.end(); ++it ){
                timed.push_back( createTimedMassCumIntervention( *it, &Host::Human::timedIPT, &Host::Human::hasIPTiProtection ) );
            }
        }
    }
    if( intervElt.getITN().present() ){
        const scnXml::ITN& itn = intervElt.getITN().get();
        if( itn.getTimed().present() || itn.getContinuous().present() ){
            activeInterventions.set (Interventions::ITN, true);
            // read description
            population.transmissionModel().setITNDescription( itn.getDescription() );
            // continuous deployments:
            if( itn.getContinuous().present() ){
                const scnXml::ContinuousList::DeploySequence& seq = itn.getContinuous().get().getDeploy();
                typedef scnXml::ContinuousList::DeploySequence::const_iterator CIt;
                for( CIt it = seq.begin(); it != seq.end(); ++it ){
                    ctsIntervs.push_back( AgeIntervention( *it, &Host::Human::ctsITN ) );
                }
            }
            // timed deployments:
            if( itn.getTimed().present() ){
                const scnXml::MassCumList::DeploySequence& seq = itn.getTimed().get().getDeploy();
                typedef scnXml::MassCumList::DeploySequence::const_iterator It;
                for( It it = seq.begin(); it != seq.end(); ++it ){
                    timed.push_back( createTimedMassCumIntervention( *it, &Host::Human::massITN, &Host::Human::hasITNProtection ) );
                }
            }
        }
    }
    if( intervElt.getIRS().present() ){
        const scnXml::IRS& irs = intervElt.getIRS().get();
        if( irs.getTimed().present() ){
            activeInterventions.set (Interventions::IRS, true);
            // read description
            population.transmissionModel().setIRSDescription( irs );
            // timed deployments:
            const scnXml::MassCumList::DeploySequence& seq = irs.getTimed().get().getDeploy();
            typedef scnXml::MassCumList::DeploySequence::const_iterator It;
            for( It it = seq.begin(); it != seq.end(); ++it ){
                timed.push_back( createTimedMassCumIntervention( *it, &Host::Human::massIRS, &Host::Human::hasIRSProtection ) );
            }
        }
    }
    if( intervElt.getVectorDeterrent().present() ){
        const scnXml::VectorDeterrent& va = intervElt.getVectorDeterrent().get();
        if( va.getTimed().present() ){
            activeInterventions.set (Interventions::VEC_AVAIL, true);
            // read description
            population.transmissionModel().setVADescription( va );
            // timed deployments:
            const scnXml::MassCumList::DeploySequence& seq = va.getTimed().get().getDeploy();
            typedef scnXml::MassCumList::DeploySequence::const_iterator It;
            for( It it = seq.begin(); it != seq.end(); ++it ){
                timed.push_back( createTimedMassCumIntervention( *it, &Host::Human::massVA, &Host::Human::hasVAProtection ) );
            }
        }
    }
    if( intervElt.getCohort().present() ){
        const scnXml::Cohort& ch = intervElt.getCohort().get();
        if( ch.getTimed().present() || ch.getContinuous().present() ){
            activeInterventions.set (Interventions::COHORT, true);
            // continuous deployments:
            if( ch.getContinuous().present() ){
                const scnXml::ContinuousList::DeploySequence& seq = ch.getContinuous().get().getDeploy();
                typedef scnXml::ContinuousList::DeploySequence::const_iterator CIt;
                for( CIt it = seq.begin(); it != seq.end(); ++it ){
                    ctsIntervs.push_back( AgeIntervention( *it, &Host::Human::addToCohort ) );
                }
            }
            // timed deployments:
            if( ch.getTimed().present() ){
                const scnXml::MassCumList::DeploySequence& seq = ch.getTimed().get().getDeploy();
                typedef scnXml::MassCumList::DeploySequence::const_iterator It;
                for( It it = seq.begin(); it != seq.end(); ++it ){
                    timed.push_back( createTimedMassCumIntervention( *it, &Host::Human::addToCohort, &Host::Human::getInCohort ) );
                }
            }
        }
    }
    if( intervElt.getImportedInfections().present() ){
        const scnXml::ImportedInfections& ii = intervElt.getImportedInfections().get();
        if( importedInfections.init( ii ) ){
            // init() returns true when infections get imported
            activeInterventions.set (Interventions::IMPORTED_INFECTIONS, true);
        }
    }
    if( intervElt.getImmuneSuppression().present() ){
        const scnXml::ImmuneSuppression& elt = intervElt.getImmuneSuppression().get();
        if( elt.getTimed().present() ){
            activeInterventions.set (Interventions::IMMUNE_SUPPRESSION, true);
            // timed deployments:
            const scnXml::MassList::DeploySequence& seq = elt.getTimed().get().getDeploy();
            typedef scnXml::MassList::DeploySequence::const_iterator It;
            for( It it = seq.begin(); it != seq.end(); ++it ){
                timed.push_back( new TimedMassIntervention( *it, &Host::Human::immuneSuppression ) );
            }
        }
    }
    if( intervElt.getInsertR_0Case().present() ){
        const scnXml::InsertR_0Case& elt = intervElt.getInsertR_0Case().get();
        if( elt.getTimedDeployment().size() > 0 ){
            activeInterventions.set (Interventions::R_0_CASE, true);
            // uses vaccines but see note in Vaccine::initParameters()
            // activeInterventions.set (Interventions::VACCINE, true);
            // timed deployments:
            typedef scnXml::InsertR_0Case::TimedDeploymentSequence::const_iterator It;
            for( It it = elt.getTimedDeployment().begin(); it != elt.getTimedDeployment().end(); ++it ){
                timed.push_back( new TimedR_0Intervention( TimeStep( it->getTime() ) ) );
            }
        }
    }
    if( intervElt.getUninfectVectors().present() ){
        const scnXml::UninfectVectors& elt = intervElt.getUninfectVectors().get();
        if( elt.getTimedDeployment().size() > 0 ){
            activeInterventions.set (Interventions::UNINFECT_VECTORS, true);
            // timed deployments:
            typedef scnXml::UninfectVectors::TimedDeploymentSequence::const_iterator It;
            for( It it = elt.getTimedDeployment().begin(); it != elt.getTimedDeployment().end(); ++it ){
                timed.push_back( new TimedUninfectVectorsIntervention( TimeStep( it->getTime() ) ) );
            }
        }
    }
    if( intervElt.getLarviciding().present() ){

      const scnXml::Larviciding& elt = intervElt.getLarviciding().get();

	if (elt.getTimed().present() ) {
	    activeInterventions.set ( Interventions::LARVICIDING, true );

	    const scnXml::TimedBaseList::DeploySequence& seq = elt.getTimed().get().getDeploy();
	    typedef scnXml::TimedBaseList::DeploySequence::const_iterator It;
	    for ( It it = seq.begin(); it != seq.end(); ++it ) {
		timed.push_back( new TimedLarvicideIntervention(TimeStep( it->getTime() ),intervElt.getLarviciding().get()) );
	    }
	}

      }
    
    // lists must be sorted, increasing
    // For reproducability, we need to use stable_sort, not sort.
    stable_sort( ctsIntervs.begin(), ctsIntervs.end() );
    // NOTE: I'd rather use stable_sort, but it's not available. Results are
    // the same without as with a hacked BOOST version including stable_sort.
    timed.sort();
    
    // make sure the list ends with something always in the future, so we don't
    // have to check nextTimed is within range:
    timed.push_back( new DummyIntervention() );
}

void InterventionManager::loadFromCheckpoint( OM::Population& population, TimeStep interventionTime ){
    // We need to re-deploy changeHS and changeEIR interventions, but nothing
    // else. nextTimed should be zero so we can go through all past interventions.
    // Only redeploy those which happened before this timestep.
    assert( nextTimed == 0 );
    while( timed[nextTimed].time < interventionTime ){
        if( dynamic_cast<TimedChangeHSIntervention*>(&timed[nextTimed])!=0 ||
            dynamic_cast<TimedChangeEIRIntervention*>(&timed[nextTimed])!=0 ){
            //Note: neither changeHS nor changeEIR interventions care what the
            //current timestep is when they are deployed, so we don't need to
            //tell them the deployment time.
            timed[nextTimed].deploy( population );
        }
        nextTimed += 1;
    }
}


void InterventionManager::deploy(OM::Population& population) {
    if( TimeStep::interventionPeriod < TimeStep(0) )
        return;
    
    // deploy imported infections (not strictly speaking an intervention)
    if( activeInterventions[ Interventions::IMPORTED_INFECTIONS ] ){
        importedInfections.import( population );
    }
    
    // deploy timed interventions
    while( timed[nextTimed].time <= TimeStep::interventionPeriod ){
        timed[nextTimed].deploy( population );
        nextTimed += 1;
    }
    
    // deploy continuous interventions
    for( Population::HumanIter it = population.getList().begin();
        it != population.getList().end(); ++it )
    {
        uint32_t nextCtsDist = it->getNextCtsDist();
        TimeStep ageTimeSteps = TimeStep::simulation - it->getDateOfBirth();
        // deploy continuous interventions
        while( nextCtsDist < ctsIntervs.size() )
        {
            if( ctsIntervs[nextCtsDist].ageTimesteps > ageTimeSteps )
                break;      // remaining intervs happen in future
            // If interv for now, do it. (If we missed the time, ignore it.)
            if( ctsIntervs[nextCtsDist].ageTimesteps == ageTimeSteps )
            {
                if( ctsIntervs[nextCtsDist].begin <= TimeStep::interventionPeriod &&
                    TimeStep::interventionPeriod < ctsIntervs[nextCtsDist].end )
                {
                    if( !ctsIntervs[nextCtsDist].cohortOnly || it->getInCohort() )
                    {
                        if (util::random::uniform_01() < ctsIntervs[nextCtsDist].coverage)
                        {
                            // Call member function ctsIntervs[nextCtsDist].deploy
                            ((*it).*(ctsIntervs[nextCtsDist].deploy)) (population);
                        }
                    }
                }
            }
            nextCtsDist = it->incrNextCtsDist();
        }
    }
}

}
