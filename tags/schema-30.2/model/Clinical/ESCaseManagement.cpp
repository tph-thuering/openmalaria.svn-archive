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

#include "Clinical/ESCaseManagement.h"
#include "Clinical/ESDecisionTree.h"
#include "Clinical/EventScheduler.h"
#include "Clinical/parser.h"
#include "inputData.h"
#include "util/errors.h"
#include "util/ModelOptions.h"

#include <set>
#include <sstream>
#include <boost/format.hpp>
#include <boost/assign/std/vector.hpp> // for 'operator+=()'

namespace OM { namespace Clinical {
    using namespace OM::util;
    using namespace boost::assign;
    using boost::format;
    using parser::SymbolValueMap;
    using parser::SymbolRangeMap;

// -----  ESTreatmentSchedule  -----

ESTreatmentSchedule::ESTreatmentSchedule (const scnXml::HSESTreatmentSchedule& sched) {
    const ::scnXml::HSESTreatmentSchedule::MedicateSequence& mSeq = sched.getMedicate();
    medications.resize (mSeq.size ());
    for (size_t j = 0; j < mSeq.size(); ++j) {
	medications[j].abbrev = mSeq[j].getDrug();
	medications[j].qty = mSeq[j].getMg();
	medications[j].cost_qty = medications[j].qty;	// effective quantity w.r.t. cost starts off equal to quantity
	medications[j].time = mSeq[j].getHour() / 24.0;	// convert from hours to days
	if( mSeq[j].getDuration().present() ){
	    if( !(mSeq[j].getDuration().get() > 0.0) ){
		throw util::xml_scenario_error( "duration of an IV dose must be some positive amount of time" );
	    }
	    medications[j].duration = mSeq[j].getDuration().get() / 24.0;
	}
    }
}

void ESTreatmentSchedule::multiplyQty (const SymbolValueMap& m, bool affectsCost, const string& errObj) {
    for( vector<MedicateData>::iterator med = medications.begin(); med != medications.end(); ++med ){
	SymbolValueMap::const_iterator it = m.find( med->abbrev );
	if( it == m.end() )
	    throw xml_scenario_error( (boost::format("%1%: no effect described for drug (ingredient) %2%") %errObj %med->abbrev).str() );
	med->qty *= it->second;
	if( affectsCost )
	    med->cost_qty *= it->second;
    }
}
void ESTreatmentSchedule::delay (const SymbolValueMap& m, const string& errObj) {
    for( vector<MedicateData>::iterator med = medications.begin(); med != medications.end(); ++med ){
	SymbolValueMap::const_iterator it = m.find( med->abbrev );
	if( it == m.end() )
	    throw xml_scenario_error( (boost::format("%1%: no effect described for drug (ingredient) %2%") %errObj %med->abbrev).str() );
	med->time += it->second / 24.0;	// convert to days
    }
}
void ESTreatmentSchedule::selectTimeRange (const SymbolRangeMap& m, bool affectsCost, const string& errObj) {
    for( vector<MedicateData>::iterator med = medications.begin(); med != medications.end(); ){
	SymbolRangeMap::const_iterator it = m.find( med->abbrev );
	if( it == m.end() )
	    throw xml_scenario_error( (boost::format("%1%: no effect described for drug (ingredient) %2%") %errObj %med->abbrev).str() );
	double timeH = med->time * 24.0;	// convert back to hours for comparisons
	if( it->second.first <= timeH && timeH < it->second.second )
	    ++med;
	else {
	    if( affectsCost )
		med = medications.erase( med );	// NOTE: inefficient on a vector... not very important here though
	    else {
		med->qty = 0.0;	// zero effect treatment, but still has cost (cost_qty)
		++med;
	    }
	}
    }
}


// -----  ESTreatment  -----

typedef ESDecisionValueMap::value_map_t value_map_t;

// functions extracted just to avoid repeating this 3 times:
string modFormatErrMsg( const string& elt, const string& dec, const string& val ){
    // Formats an error-message to pass to sub-functions (due to way it's passed, needs to be generated anyway)
    ostringstream msg;
    msg << "treatment \""<<elt
	<<"\" modifier for decision value "<<dec<<'('<<val<<')';
    return msg.str();
}
ESDecisionValue modGetESDecVal( value_map_t& decVals, const scnXml::HSESTreatmentModifierEffect& mod, const string& errObj ){
    value_map_t::iterator it = decVals.find( mod.getValue() );
    if( it == decVals.end() )
	throw xml_scenario_error( errObj+": value doesn't exist" );
    ESDecisionValue val = it->second;
    decVals.erase( it );
    return val;
}

ESTreatment::ESTreatment(const ESDecisionValueMap& dvMap, const scnXml::HSESTreatment& elt, list<string>& required) {
    // We need to apply all select-time-range modifiers before any delay
    // modifiers. The easiest solution is to reorder the list:
    list<const scnXml::HSESTreatmentModifier*> modifierList;
    BOOST_FOREACH( const scnXml::HSESTreatmentModifier& modifier, elt.getModifier() ){
	if( modifier.getSelectTimeRange().size() )
	    modifierList.push_front( &modifier );
	else
	    modifierList.push_back( &modifier );
    }
    
    ESDecisionValue zero;       // "insert" has no support for anonymous objects, so can't declare inline
    schedules.insert( zero, new ESTreatmentSchedule( elt.getSchedule() ) );
    Schedules startSchedules;
    
    BOOST_FOREACH( const scnXml::HSESTreatmentModifier* modifier, modifierList ){
	schedules.swap( startSchedules );
	schedules.clear();
	
	required.push_back( modifier->getDecision() );	// make sure this decision isn't optimised out
	tuple<ESDecisionValue, const value_map_t&> decPair = dvMap.getDecision( modifier->getDecision() );
	schedulesMask |= decPair.get<0>();
	value_map_t decVals = decPair.get<1>();	// copy
        schedules.rehash( (std::size_t)std::ceil(decVals.size() / schedules.max_load_factor()) );
	
	if( modifier->getMultiplyQty().size() ) {
	    assert( modifier->getDelay().size() == 0 );
	    assert( modifier->getSelectTimeRange().size() == 0 );
	    BOOST_FOREACH( const scnXml::HSESTreatmentModifierEffect& mod, modifier->getMultiplyQty() ){
		string errObj = modFormatErrMsg( elt.getName(), modifier->getDecision(), mod.getValue() );
		ESDecisionValue val = modGetESDecVal( decVals, mod, errObj );
		const SymbolValueMap& m = parser::parseSymbolValueMap( mod.getEffect(), errObj );
		bool affectsCost = mod.getAffectsCost().present() ? mod.getAffectsCost().get() : true;
		
		for( Schedules::iterator s = startSchedules.begin(); s != startSchedules.end(); ++s ){
		    ESTreatmentSchedule *ts = new ESTreatmentSchedule( *s->second );
		    ts->multiplyQty( m, affectsCost, errObj );
                    ESDecisionValue key = s->first | val;
		    schedules.insert( key, ts );
		}
	    }
	} else if( modifier->getDelay().size() ) {
	    assert( modifier->getSelectTimeRange().size() == 0 );
	    BOOST_FOREACH( const scnXml::HSESTreatmentModifierEffect& mod, modifier->getDelay() ){
		string errObj = modFormatErrMsg( elt.getName(), modifier->getDecision(), mod.getValue() );
		ESDecisionValue val = modGetESDecVal( decVals, mod, errObj );
		const SymbolValueMap& m = parser::parseSymbolValueMap( mod.getEffect(), errObj );
		
		for( Schedules::iterator s = startSchedules.begin(); s != startSchedules.end(); ++s ){
		    ESTreatmentSchedule *ts = new ESTreatmentSchedule( *s->second );
		    ts->delay( m, errObj );
                    ESDecisionValue key = s->first | val;
                    schedules.insert( key, ts );
		}
	    }
	} else if( modifier->getSelectTimeRange().size() ) {
	    BOOST_FOREACH( const scnXml::HSESTreatmentModifierEffect& mod, modifier->getSelectTimeRange() ){
		string errObj = modFormatErrMsg( elt.getName(), modifier->getDecision(), mod.getValue() );
		ESDecisionValue val = modGetESDecVal( decVals, mod, errObj );
		const SymbolRangeMap& m = parser::parseSymbolRangeMap( mod.getEffect(), errObj );
		bool affectsCost = mod.getAffectsCost().present() ? mod.getAffectsCost().get() : true;
		
		for( Schedules::iterator s = startSchedules.begin(); s != startSchedules.end(); ++s ){
		    ESTreatmentSchedule *ts = new ESTreatmentSchedule( *s->second );
		    ts->selectTimeRange( m, affectsCost, errObj );
                    ESDecisionValue key = s->first | val;
                    schedules.insert( key, ts );
		}
	    }
	} else {
	    ostringstream msg;
	    msg << "treatment \""<<elt.getName()
		<<"\" modifier for decision "<<modifier->getDecision()
		<<" has no sub-elements";
	    throw xml_scenario_error( msg.str() );
	}
	
	if( !decVals.empty() ){
	    ostringstream msg;
	    msg << "modifier for treatment \""<<elt.getName()
		<< "\" by decision "<<modifier->getDecision()
		<<": effect not described for values:";
	    for( value_map_t::iterator it = decVals.begin(); it != decVals.end(); ++it )
		msg<<' '<<it->first;
	    throw xml_scenario_error( msg.str() );
	}
    }
    
    /*
    cout<<"schedules for "<<elt.getName()<<":";
    for( Schedules::iterator it = schedules.begin(); it != schedules.end(); ++it ) {
	cout<<" "<<it->first<<" ["<<dvMap.format( it->first )<<"];";
    }
    cout<<"\nmask: "<<schedulesMask<<endl;
    */
}

ESTreatment::~ESTreatment() {
}

ESTreatmentSchedule* ESTreatment::getSchedule (ESDecisionValue& outcome) {
    outcome = outcome & schedulesMask;
    Schedules::iterator it = schedules.find (outcome);
    if (it == schedules.end ())
	return NULL;
    return it->second;
}

// -----  ESDecisionMap  -----

class ESDecisionMapProcessor {
    ESDecisionMap::TreeType treeType;
    ESDecisionValueMap& dvMap;
    
    // Collection of all decisions not yet added into decisions or culled.
    // Filled by constructor, emptied by process().
    typedef map<string,ESDecisionTree*> DecisionList;
    DecisionList pending;
    
    // Set of all tests required. Filled by process().
    set<ESDecisionTree*> required;
    
    // Add by key "decision", and return true if added
    bool addToPending( ESDecisionTree* d ){
	return pending.insert( make_pair(d->decision, d) ).second;
    }
    void addRequires (const string& x){
	DecisionList::const_iterator it = pending.find( x );
	if( it == pending.end() ) {
	    ostringstream msg;
	    msg << "ESCaseManagement: decision " << x << " required (for ";
            if( treeType == ESDecisionMap::MDA ){
                msg << "MDA";
            }else if( treeType == ESDecisionMap::Uncomplicated ){
                msg << "uncomplicated";
            }else if( treeType == ESDecisionMap::Complicated ){
                msg << "complicated";
            }else{
                assert (false);
            }
            msg << " tree)";
	    throw xml_scenario_error(msg.str());
	}
	if( required.insert( it->second ).second ){	// if newly inserted...
	    BOOST_FOREACH( const string& dep, it->second->depends )
		addRequires( dep );
	}	// else it was already considered
    }
    
    inline bool hasAllDependencies (const ESDecisionTree* decision, const set<string>& dependencies) {
	BOOST_FOREACH ( const string& n, decision->depends ) {
	    if (dependencies.count(n) == 0) {
		return false;
	    }
	}
	return true;
    }
    
public:
    /* Read from XML into a temporary list. */
    ESDecisionMapProcessor (ESDecisionValueMap& dvm,
                            const ::scnXml::HSESCaseManagement& xmlCM,
                            ESDecisionMap::TreeType tt) :
        treeType(tt), dvMap(dvm)
    {
	// Assemble a list of all tests we might need to add
	if ( treeType == ESDecisionMap::Uncomplicated ) {
	    addToPending (new ESDecisionUC2Test (dvMap));	// this test only makes sense for UC case
	}
	addToPending (new ESDecisionParasiteTest (dvMap));	// optimised away if not used
	BOOST_FOREACH ( const ::scnXml::HSESDecision& xmlDc, xmlCM.getDecisions().getDecision() ) {
	    if( !addToPending (ESDecisionTree::create (dvMap, xmlDc)) )
		throw xml_scenario_error((format("Case management: decision %1% described twice") %xmlDc.getName()).str());
	}
    }
    
    /* Filter and output. */
    void process (ESDecisionMap::Decisions& decisions, list<string>& requiredOutputs) {
	BOOST_FOREACH( const string& req, requiredOutputs ){
	    addRequires( req );
	}
	
	decisions.reserve (pending.size());
	set<string> added;	// names of decisions added; used since it's faster to lookup decision names here than linearly in "decisions"
	for (DecisionList::iterator it = pending.begin(); ;) {
	    if (it == pending.end ()) {
		if (pending.empty ())	// good, we're done
		    break;
		// else: some elements had unresolved dependencies; this should already have been caught by addRequires though
		throw TRACED_EXCEPTION_DEFAULT("ESCaseManagement: didn't catch all dependencies (code error)");
	    }
	    //cout << "Considering " << (*it)->decision << " with " << (*it)->depends.size()<<" dependencies"<<endl;
	    if (required.count( it->second ) == 0) {
		//Note: optimising out unwanted decisions like this has two
		// possible sources of error, which should be checked:
		// 1. empty slots in the decisions list
		// 2. That required decisions are mistakenly optimised out.
#ifdef WITHOUT_BOINC	// only print in non-BOINC mode to reduce stderr.txt size on server; this isn't a crucial message
		if( it->first != "result" && it->first != "case" ) {	// these are built-in tests which may not be used
		    cerr << "Warning: ESCaseManagement: decision " << it->first << " is unused (for ";
                    if( treeType == ESDecisionMap::MDA ){
                        cerr << "MDA";
                    }else if( treeType == ESDecisionMap::Uncomplicated ){
                        cerr << "uncomplicated";
                    }else if( treeType == ESDecisionMap::Complicated ){
                        cerr << "complicated";
                    }else{
                        assert (false);
                    }
                    cerr << " tree)"<<endl;
		}
#endif
		delete it->second;
		pending.erase(it);
		it = pending.begin();	// after erase, we must restart from beginning
	    } else if (hasAllDependencies (it->second, added)) {
		decisions.push_back( it->second );
		added.insert (it->second->decision);
		pending.erase (it);
		it = pending.begin ();	// restart from beginning as above; also means we know if we reach the end there shouldn't be any elements left
	    } else
		++it;
	}
    }
};
inline ESDecisionValue treatmentGetValue (const ESDecisionValueMap::value_map_t& vmap, const string& value) {
    ESDecisionValueMap::value_map_t::const_iterator it = vmap.find (value);
    if (it == vmap.end())	// value is unknown
	throw xml_scenario_error((format("Treatment description given for treatment %1% which isn't an output of \"treatment\" decision") %value).str());
    return it->second;
}
void ESDecisionMap::initialize (const ::scnXml::HSESCaseManagement& xmlCM, TreeType treeType) {
    // This function is also used to load a new health-system from intervention data; therefore clear old data:
    dvMap.clear();
    decisions.clear();
    treatments.clear();
    
    // Construct processor & read from XML.
    // Fills dvMap (which must be done before evaluating treatments).
    ESDecisionMapProcessor processor( dvMap, xmlCM, treeType );
    
    list<string> required;	// list required decisions, to avoid optimising out
    
    if( treeType == Complicated ){
	// Register required "hospitalisation" output, before decisions are created
	// (by ESDecisionMapProcessor). Effects: no transmission in hospital, delay
	// simply adds one day to time before returning to treatment seeking.
	required.push_back( "hospitalisation" );
	vector<string> tfValues;
	tfValues += "none","immediate","delayed";
	hospitalisation_mask = dvMap.add_decision_values( "hospitalisation", tfValues );
	hospitalisation_immediate = dvMap.get( "hospitalisation", "immediate" );
	hospitalisation_delayed = dvMap.get( "hospitalisation", "delayed" );
    }
    
    // "test" decision: determines diagnostic usage and reported.
    required.push_back( "test" );
    test_mask = dvMap.getDecisionMask( "test" );
    test_RDT = dvMap.get( "test", "RDT" );
    test_microscopy = dvMap.get( "test", "microscopy" );
    
    // "result" decision: result of diagnostic (added automatically, but we need the keys)
    diagnostic_mask = dvMap.getDecisionMask( "result" );
    diagnostic_negative = dvMap.get( "result", "negative" );
    diagnostic_positive = dvMap.get( "result", "positive" );
    
    if( util::ModelOptions::option(util::NON_MALARIA_FEVERS) ){
        required.push_back( "AB_provider" );
        vector<string> outcomes;
        outcomes += "none","facility","informal";
        AB_provider_mask = dvMap.add_decision_values( "AB_provider", outcomes );
        AB_provider_facility = dvMap.get( "AB_provider", "facility" );
        AB_provider_informal = dvMap.get( "AB_provider", "informal" );
    }
    
    // Read treatments
    required.push_back( "treatment" );
    tuple< ESDecisionValue, ESDecisionValueMap::value_map_t > mask_vmap_pair = dvMap.getDecision("treatment");
    treatmentsMask = mask_vmap_pair.get<0>();
    
    const ESDecisionValueMap::value_map_t& treatmentCodes = mask_vmap_pair.get<1>();
    BOOST_FOREACH( const ::scnXml::HSESTreatment& treatment, xmlCM.getTreatments().getTreatment() ){
        ESDecisionValue key = treatmentGetValue( treatmentCodes, treatment.getName() );
	treatments.insert( key, new ESTreatment( dvMap, treatment, required ) );
    }
    
    // Filter and add decisions (must be done after reading treatments):
    processor.process( decisions, required );
}

ESDecisionMap::~ESDecisionMap () {
}

ESDecisionValue ESDecisionMap::determine (const OM::Clinical::ESHostData& hostData) const {
    ESDecisionValue outcomes;	// initialized to 0
    // At this point, decisions is ordered such that all dependencies should be
    // met if evaluated in order, so we just do that.
    for( Decisions::const_iterator it=decisions.begin(); it!=decisions.end(); ++it ){
    // Pass determine the outcomes of previous decisions, filtered to the decisions it depends on.
	// Get back another outcome and add it into the outcomes set.
	outcomes |= it->determine (outcomes, hostData);
    }
    return outcomes;
}
ESTreatmentSchedule& ESDecisionMap::getSchedule (ESDecisionValue outcome) {
    ESDecisionValue masked = outcome & treatmentsMask;
    Treatments::iterator it = treatments.find (masked);
    if (it != treatments.end ()) {
	ESTreatmentSchedule* ret = it->second->getSchedule( outcome );
	if( ret != NULL )
	    return *ret;
	throw TRACED_EXCEPTION_DEFAULT( "a required modifier decision's output is unexpected (code error)" );
    }
    
    ostringstream msg;
    msg<<"decision outcome "<<dvMap.format( masked )<<" not found in list of treatments";
    throw xml_scenario_error (msg.str());
}


// -----  ESCaseManagement  -----

ESDecisionMap ESCaseManagement::uncomplicated, ESCaseManagement::complicated;
ESDecisionMap ESCaseManagement::mda;

void ESCaseManagement::setHealthSystem (const scnXml::HealthSystem& healthSystem) {
    if( !healthSystem.getEventScheduler().present() )
	throw util::xml_scenario_error ("Expected EventScheduler section in healthSystem data (initial or intervention)");
    const scnXml::HSEventScheduler& esData = healthSystem.getEventScheduler().get();
    uncomplicated.initialize (esData.getUncomplicated (), ESDecisionMap::Uncomplicated);
    complicated.initialize (esData.getComplicated (), ESDecisionMap::Complicated);
    
    // Calling our parent class like this is messy. Changing this would require
    // moving change-of-health-system handling into ClinicalModel.
    ClinicalEventScheduler::setParameters( esData );
}

pair<ESDecisionValue, bool> executeTree(
        ESDecisionMap* map,
        const ESHostData& hostData,
        list<MedicateData>& medicateQueue,
        bool inCohort
){
    ESDecisionValue outcome = map->determine (hostData);
    ESTreatmentSchedule& schedule = map->getSchedule(outcome);
    schedule.apply (medicateQueue);
    
    if ( map->RDT_used(outcome) ) {
        Monitoring::Surveys.getSurvey(inCohort).report_Clinical_RDTs (1);
    }
    if ( map->microscopy_used(outcome) ) {
        Monitoring::Surveys.getSurvey(inCohort).report_Clinical_Microscopy (1);
    }
    
    return make_pair( outcome, schedule.anyTreatments() );
}

void ESCaseManagement::initMDA (const scnXml::MDA::DescriptionType& desc){
    mda.initialize( desc, ESDecisionMap::MDA );
}

void ESCaseManagement::massDrugAdministration(
        const ESHostData& hostData,
        list<MedicateData>& medicateQueue,
        bool inCohort,
        Monitoring::AgeGroup ageGroup
){
    Monitoring::Surveys.getSurvey(inCohort).reportMassScreening(ageGroup, 1);
    bool anyTreatment = executeTree( &mda, hostData, medicateQueue, inCohort ).second;
    if( anyTreatment ){
        Monitoring::Surveys.getSurvey(inCohort).reportMDA(ageGroup, 1);
    }
}

CMAuxOutput ESCaseManagement::execute (
        const ESHostData& hostData,
        list<MedicateData>& medicateQueue,
        bool inCohort
) {
    assert (hostData.pgState & Pathogenesis::SICK);
    // We always remove any queued medications.
    medicateQueue.clear();
    
    ESDecisionMap *map = (hostData.pgState & Pathogenesis::COMPLICATED) ? &complicated : &uncomplicated;
    ESDecisionValue outcome = executeTree( map, hostData, medicateQueue, inCohort ).first;
    
    CMAuxOutput auxOut;
    auxOut.hospitalisation = CMAuxOutput::NONE;
    if( hostData.pgState & Pathogenesis::COMPLICATED )
	auxOut.hospitalisation = map->hospitalisation(outcome);
    auxOut.diagnostic = map->diagnostic(outcome);
    auxOut.AB_provider = map->AB_provider(outcome);
    return auxOut;
}

} }