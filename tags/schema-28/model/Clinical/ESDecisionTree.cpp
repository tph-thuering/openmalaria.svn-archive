/*
 This file is part of OpenMalaria.
 
 Copyright (C) 2005-2010 Swiss Tropical Institute and Liverpool School Of Tropical Medicine
 
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

#include "Clinical/ESDecisionTree.h"
#include "Clinical/parser.h"
#include "util/random.h"
#include "util/errors.h"

#include <limits>
#include <list>
#include <boost/format.hpp>
#include <boost/assign/std/vector.hpp> // for 'operator+=()'

namespace OM { namespace Clinical {
    using namespace OM::util;
    using namespace boost::assign;
    
    
    // -----  Input processor to set up user-defined decisions  -----
    // (You may want to skip this when reading the file.)
    
    struct DR_processor {
	/** Parses input for an ESDecisionRandom tree.
	 *
	 * @param dvm Decision-value map
	 * @param d The target object.
	 * @param allowP "p" decisions are only allowed when this is true, in
	 * order to enforce the specification.
	 */
	DR_processor (const ESDecisionValueMap& dvm, ESDecisionValueBase& d, bool allowP) : dvMap(dvm), dR(d), allowPDecisions(allowP) {
	    BOOST_FOREACH( const string& dependency, dR.depends ){
		tuple<ESDecisionValue,const ESDecisionValueMap::value_map_t&> decMap = dvMap.getDecision( dependency );
		dR.mask |= decMap.get<0>();	// add this decision's inputs into the mask
		
		// decMap.second is converted to an ESDecisionValueSet:
		inputDependencies.push_back( make_pair( dependency, decMap.get<1>() ) );
	    }
	}
	
	void process (const parser::Outcome& outcome) {
	    processOutcome(outcome,
			   set<string>(),	/* empty set: no decisions yet processed */
			   ESDecisionValue(),	/* 0: start with no outcomes */
			   1.0				/* probability of reaching here, given all required values */
	    );
	    checkProbabilities ();
	}
	
    private:
	void processBranches (const parser::BranchSet& branchSet,
			      set<string> usedInputs,	// needs to take a copy
			      ESDecisionValue dependValues,
			     double dependP
	){
	    if (allowPDecisions && branchSet.decision == "p") {
		
		double cum_p = 0.0;
		BOOST_FOREACH( const parser::Branch& branch, branchSet.branches ) {
		    assert ( boost::get<double>( &branch.dec_value ) != NULL );	// parser should guarantee we have the right value
		    double p = *boost::get<double>( &branch.dec_value );
		    cum_p += p;
		    
		    processOutcome( branch.outcome, usedInputs, dependValues, dependP*p );
		}
		// Test cum_p is approx. 1.0 in case the input tree is wrong. In any case, we force probabilities to add to 1.0.
		if (cum_p < 0.999 || cum_p > 1.001)
		    throw util::xml_scenario_error ( (
			boost::format("decision tree %1%: expected probability sum to be 1.0 but found %2%")
			%dR.decision
			%cum_p
		    ).str() );
		
	    } else {
		// This check is to make sure dependencies are listed (other
		// code depends on this).
		if( find( dR.depends.begin(), dR.depends.end(), branchSet.decision ) == dR.depends.end() )
		    throw xml_scenario_error( (
			boost::format( "decision tree %1%: %2% not listed as a dependency" )
			%dR.decision
			%branchSet.decision
		    ).str() );
		usedInputs.insert( branchSet.decision );
		
		ESDecisionValueMap::value_map_t valMap = dvMap.getDecision( branchSet.decision ).get<1>();	// copy
		BOOST_FOREACH( const parser::Branch& branch, branchSet.branches ) {
		    assert ( boost::get<string>( &branch.dec_value ) != NULL );	// parser should guarantee we have the right value
		    string dec_value = *boost::get<string>( &branch.dec_value );
		    ESDecisionValueMap::value_map_t::iterator valIt = valMap.find( dec_value );
		    if( valIt == valMap.end() )
			throw xml_scenario_error( (
			    boost::format("decision tree %3%: %1%(%2%) encountered: %2% is not an outcome of %1%")
			    %branchSet.decision
			    %dec_value
			    %dR.decision
			).str());
		    
		    processOutcome( branch.outcome, usedInputs, dependValues | valIt->second, dependP );
		    
		    valMap.erase( valIt );
		}
		if( !valMap.empty() ){	// error: not all options were included
		    ostringstream msg;
		    msg << "decision tree "<<dR.decision<<": expected branches:";
		    for (ESDecisionValueMap::value_map_t::iterator it = valMap.begin(); it != valMap.end(); ++it)
			msg <<' '<<branchSet.decision<<'('<<it->first<<')';
		    throw xml_scenario_error( msg.str() );
		}
		
	    }
	}
	void processOutcome (const parser::Outcome& outcome,
			     const set<string>& usedInputs,	// doesn't edit
			     ESDecisionValue dependValues,
			     double dependP
	){
	    if ( const string* val_p = boost::get<string>( &outcome ) ) {
		ESDecisionValue val = dvMap.get( dR.decision, *val_p );
		size_t i = 0;	// get index i in dR.values of this outcome
		while (true) {
		    if (i >= dR.values.size())
			throw logic_error( (
			    boost::format("decision tree %1%: unable to find index for value %2% (code error)")
			    %dR.decision
			    %*val_p
			).str() );
		    if( dR.values[i] == val )
			break;
		    ++i;
		}
		
		// all input values which match the decisions we have made
		ESDecisionValueSet inputValues = dependValues;
		for( list< pair< string, ESDecisionValueSet > >::const_iterator it = inputDependencies.begin(); it != inputDependencies.end(); ++it ){
		    if( usedInputs.count( it->first ) == 0 )
			// This decion was not decided; look at all permutations by it
			inputValues |= it->second;
		}
		
		BOOST_FOREACH( ESDecisionValue inputValue, inputValues.values ){
		    // find/make an entry for dependent decisions:
		    vector<double>& outcomes_cum_p = dR.map_cum_p[ inputValue ];
		    /* print cum-prob-array (part 1):
		    if( outcomes_cum_p.empty() ) cout << "new ";
		    cout << "outcome cumulative probabilities for "<<dvMap.format( val )<<" (index "<<i<<"): ";
		    */
		    // make sure it's size is correct (will need resizing if just inserted):
		    outcomes_cum_p.resize( dR.values.size(), 0.0 );	// any new entries have p(0.0)
		    for (size_t j = i; j < outcomes_cum_p.size(); ++j)
			outcomes_cum_p[j] += dependP;
		    /* print cum-prob-array (part 2):
		    for (size_t j = 0; j < outcomes_cum_p.size(); ++j)
			cout <<" "<<outcomes_cum_p[j];
		    cout<<endl;
		    */
		}
	    } else if ( const parser::BranchSet* bs_p = boost::get<parser::BranchSet>( &outcome ) ) {
		processBranches( *bs_p, usedInputs, dependValues, dependP );
	    } else {
		assert (false);
	    }
	}
	
	void checkProbabilities () {
	    size_t l = dR.values.size();
	    size_t l1 = l - 1;
	    for( ESDecisionRandom::map_cum_p_t::iterator val_cum_p = dR.map_cum_p.begin(); val_cum_p != dR.map_cum_p.end(); ++val_cum_p ) {
		assert( val_cum_p->second.size() == l );
		// We force the last value to 1.0. It should be roughly that anyway due to
		// previous checks; it doesn't really matter if this gives allows a small error.
		val_cum_p->second[l1] = 1.0;
	    }
	}
	
	const ESDecisionValueMap& dvMap;
	ESDecisionValueBase& dR;
	bool allowPDecisions;
	list< pair< string, ESDecisionValueSet > > inputDependencies;
    };
    
    struct DA_processor {
	typedef parser::DoubleRange DoubleRange;
	
	/** Parses input for an ESDecisionAge tree.
	 *
	 * @param dvm Decision-value map
	 * @param d The target object.
	 */
	DA_processor (const ESDecisionValueMap& dvm, ESDecisionAge& d) : dvMap(dvm), dA(d) {
	    assert( dA.depends.empty() );
	}
	
	void process (const parser::Outcome& outcome) {
	    processOutcome( outcome, DoubleRange(0.0, numeric_limits<double>::infinity()), false );
	    checkAndApplyRanges ();
	}
	
    private:
	static DoubleRange intersection( const DoubleRange& lhs, const DoubleRange& rhs ){
	    DoubleRange ret(
		std::max( lhs.first, rhs.first ),
		std::min( lhs.second, rhs.second )
	    );
	    if( ret.first > ret.second )
		ret.second = ret.first;	// empty range
	    return ret;
	}
	
	void processBranches (const parser::BranchSet& branchSet, const DoubleRange& range){
	    if (branchSet.decision == "age") {
		BOOST_FOREACH( const parser::Branch& branch, branchSet.branches ){
		    assert ( boost::get<parser::DoubleRange>( &branch.dec_value ) != NULL );	// parser should guarantee we have the right value
		    processOutcome(
			branch.outcome,
			intersection( range, *boost::get<parser::DoubleRange>( &branch.dec_value ) ),
			true	// to say, don't allow sub-branches
		    );
		}
	    } else {
		throw xml_scenario_error( (
		    boost::format( "decision tree %1%: cannot depend on anything other than age (tried to use %2%)" )
		    %dA.decision
		    %branchSet.decision
		).str() );
	    }
	}
	void processOutcome (const parser::Outcome& outcome, DoubleRange range, bool deep){
	    if ( const string* val_p = boost::get<string>( &outcome ) ) {
		ESDecisionValue val = dvMap.get( dA.decision, *val_p );
		
		ranges.push_back( make_pair( range, val ) );
	    } else if ( const parser::BranchSet* bs_p = boost::get<parser::BranchSet>( &outcome ) ) {
		// Don't allow deeper age-branches. Only reason for this
		// restriction is that we don't check sub-ranges conform to
		// parent range, which would allow some funny-looking input.
		if( deep ) {
		    throw xml_scenario_error( (
			boost::format( "decision tree %1%: age-branches within age-branches not supported" )
			%dA.decision
		    ).str() );
		}
		processBranches( *bs_p, range );
	    } else {
		assert (false);
	    }
	}
	
	typedef pair<DoubleRange,ESDecisionValue> RangeValue;
	struct SortRangeValue {
	    bool operator() (const RangeValue& lhs, const RangeValue& rhs) const{
		return lhs.first.first < rhs.first.second;	// sort according to lower-bound of range
	    }
	};
	void checkAndApplyRanges () {
	    SortRangeValue sortRangeValue;
	    ranges.sort( sortRangeValue );
	    
	    double lastUBound = 0.0;	// required lower bound
	    BOOST_FOREACH( const RangeValue& range, ranges ){
		if( lastUBound != range.first.first ) {	// yes, using == on doubles, but should be ok
		    throw xml_scenario_error( (
			boost::format( "decision tree %1%: age range bounds don't match up; found [a,%2%), [%3%,%4%)" )
			%dA.decision
			%lastUBound
			%range.first.first
			%range.first.second
		    ).str() );
		}
		lastUBound = range.first.second;
		dA.age_upper_bounds[range.first.second] = range.second;
	    }
	    if( lastUBound != numeric_limits<double>::infinity() ){
		throw xml_scenario_error( (
		    boost::format( "decision tree %1%: age range final upper bound should be inf, found: %2%" )
		    %dA.decision
		    %lastUBound
		).str() );
	    }
	}
	
	const ESDecisionValueMap& dvMap;
	ESDecisionAge& dA;
	list<RangeValue> ranges;
    };
    
    
    // -----  Decision constructors and determineImpl() functions  -----
    
    ESDecisionUC2Test::ESDecisionUC2Test (ESDecisionValueMap& dvMap) {
	decision = "case";
	vector< string > valueList;
	valueList += "UC1", "UC2";
	dvMap.add_decision_values( decision, valueList );
	UC1 = dvMap.get( decision, "UC1" );
	UC2 = dvMap.get( decision, "UC2" );
    }
    ESDecisionValue ESDecisionUC2Test::determineImpl (const ESDecisionValue, const ESHostData& hostData) const {
	assert (hostData.pgState & Pathogenesis::SICK && !(hostData.pgState & Pathogenesis::COMPLICATED));
	if (hostData.pgState & Pathogenesis::SECOND_CASE)
	    return UC2;
	else
	    return UC1;
    }
    
    ESDecisionParasiteTest::ESDecisionParasiteTest (ESDecisionValueMap& dvMap) {
	decision = "result";
	
	vector<string> testValues;
	testValues += "none", "microscopy", "RDT";
	// Add values, which (1) lets us create test_none, etc., (2) introduces a
	// check when the "test" decision is added later on output values, and
	// (3) allows us to set mask.
	mask = dvMap.add_decision_values ("test", testValues);
	test_none = dvMap.get("test","none");
	test_microscopy = dvMap.get("test", "microscopy");
	test_RDT = dvMap.get("test","RDT");
	
	depends.resize (1, "test");	// Note: we check in add_decision_values() that this test has the outcomes we expect
	
	vector< string > valueList;
	valueList += "none", "negative", "positive";
	dvMap.add_decision_values( decision, valueList );
	none = dvMap.get( decision, "none" );
	negative = dvMap.get( decision, "negative" );
	positive = dvMap.get( decision, "positive" );
    }
    
    ESDecisionValue ESDecisionParasiteTest::determineImpl (const ESDecisionValue input, const ESHostData& hostData) const {
	if (input == test_none)	// no test
	    return none;
	else {
	    //TODO: reference paper
	    //TODO: move parameters to XML
	    double dens_50;	// parasite density giving 50% chance of positive outcome
	    double specificity;	// chance of a negative outcome given no parasites
	    if (input == test_microscopy) {
		// Microscopy sensitivity/specificity data in Africa;
		// Source: expert opinion — Allan Schapira
		dens_50 = 20.0;
		specificity = .75;
	    } else {
		assert (input == test_RDT);
		// RDT sensitivity/specificity for Plasmodium falciparum in Africa
		// Source: Murray et al (Clinical Microbiological Reviews, Jan. 2008)
		dens_50 = 50.0;
		specificity = .942;
	    }
	    double dens = hostData.withinHost.getTotalDensity ();
	    double pPositive = 1.0 - specificity + specificity * dens / (dens + dens_50);
	    if (random::uniform_01() < pPositive)
		return positive;
	    else
		return negative;
	}
	assert(false);	// should return before here
    }
    
    ESDecisionAge::ESDecisionAge (ESDecisionValueMap& dvm, const ::scnXml::HSESDecision& xmlDc) {
	decision = xmlDc.getName();
	string decErrStr = "decision "+decision;
	
	vector<string> valueList = parser::parseSymbolList( xmlDc.getValues(), decision+" values attribute" );
	dvm.add_decision_values (decision, valueList);
	
	// Get content of this element:
	const ::xml_schema::String *content_p = dynamic_cast< const ::xml_schema::String * > (&xmlDc);
	if (content_p == NULL)
	    throw runtime_error ("ESDecision: bad upcast?!");
	
	DA_processor processor (dvm, *this);
	// 2-stage parse: first produces a parser::Outcome object, second the
	// processor object does the work of converting into age_upper_bounds:
	processor.process( parser::parseTree( *content_p, decision ) );
    }
    ESDecisionValue ESDecisionAge::determineImpl (const ESDecisionValue input, const ESHostData& hostData) const {
	assert (hostData.ageYears >= 0.0 && hostData.ageYears < numeric_limits<double>::infinity());
	map<double, ESDecisionValue>::const_iterator it = age_upper_bounds.upper_bound( hostData.ageYears );
	assert( it != age_upper_bounds.end() );	// should be confirmed by set-up
	return it->second;
    }
    
    ESDecisionValueBase::ESDecisionValueBase (
        ESDecisionValueMap& dvm,
        const ::scnXml::HSESDecision& xmlDc,
        const vector<string>& dependsInput) /*: dvMap(dvm)*/
    {
        decision = xmlDc.getName();
        string decErrStr = "decision "+decision;
        depends = dependsInput;
        
        vector<string> valueList = parser::parseSymbolList( xmlDc.getValues(), decision+" values attribute" );
        dvm.add_decision_values (decision, valueList);
        values.resize (valueList.size());
        for( size_t i = 0; i < valueList.size(); ++i ) {
            values[i] = dvm.get (decision, valueList[i]);
        }
    }
    
    ESDecisionDeterministic::ESDecisionDeterministic (
        ESDecisionValueMap& dvm,
        const ::scnXml::HSESDecision& xmlDc,
        const vector<string>& dependsInput) :
        ESDecisionValueBase(dvm, xmlDc, dependsInput)
    {
        // Get content of this element:
        const ::xml_schema::String *content_p = dynamic_cast< const ::xml_schema::String * > (&xmlDc);
        if (content_p == NULL)
            throw runtime_error ("ESDecision: bad upcast?!");
        
        DR_processor processor (dvm, *this, false);
        // 2-stage parse: first produces a parser::Outcome object, second the
        // processor object does the work of converting into map_cum_p:
        processor.process( parser::parseTree( *content_p, decision ) );
    }
    ESDecisionValue ESDecisionDeterministic::determineImpl (const ESDecisionValue input, const ESHostData& hostData) const {
        map_cum_p_t::const_iterator it = map_cum_p.find (input);
        if (it == map_cum_p.end()){
            // All possible input combinations should be in map_cum_p
            throw logic_error( "ESDecisionRandom: input combination not found in map (code error)" );
        }
        double sample = 0.5;
        size_t i = 0;
        while (it->second[i] <= sample)
            ++i;
        return values[i];
    }
    
    ESDecisionRandom::ESDecisionRandom (
        ESDecisionValueMap& dvm,
        const ::scnXml::HSESDecision& xmlDc,
        const vector<string>& dependsInput) :
        ESDecisionValueBase(dvm, xmlDc, dependsInput)
    {
        // Get content of this element:
        const ::xml_schema::String *content_p = dynamic_cast< const ::xml_schema::String * > (&xmlDc);
        if (content_p == NULL)
            throw runtime_error ("ESDecision: bad upcast?!");
        
        DR_processor processor (dvm, *this, true);
        // 2-stage parse: first produces a parser::Outcome object, second the
        // processor object does the work of converting into map_cum_p:
        processor.process( parser::parseTree( *content_p, decision ) );
    }
    ESDecisionValue ESDecisionRandom::determineImpl (const ESDecisionValue input, const ESHostData& hostData) const {
	map_cum_p_t::const_iterator it = map_cum_p.find (input);
	if (it == map_cum_p.end()){
	    // All possible input combinations should be in map_cum_p
	    throw logic_error( "ESDecisionRandom: input combination not found in map (code error)" );
	}
	double sample = random::uniform_01 ();
	size_t i = 0;
	while (it->second[i] <= sample)
	    ++i;
	return values[i];
    }
    
    ESDecisionTree* ESDecisionTree::create (ESDecisionValueMap& dvm, const ::scnXml::HSESDecision& xmlDc) {
	const string& decision = xmlDc.getName();
	if( decision == "age" || decision == "p" || decision == "case" || decision == "result" )
	    throw xml_scenario_error( (boost::format("error: %1% is a reserved decision name") %decision).str() );
	
	vector<string> depends = parser::parseSymbolList( xmlDc.getDepends(), decision+" depends attribute" );
	
	vector<string>::iterator it = find( depends.begin(), depends.end(), "age" );
	if( it != depends.end() ){
	    if( depends.size() != 1u )
		throw xml_scenario_error( (boost::format(
		    "decision tree %1%: a decision depending on \"age\" may not depend on anything else"
		) %decision).str() );
	    return new ESDecisionAge( dvm, xmlDc );
	}
	
	it = find( depends.begin(), depends.end(), "p" );
        if( it != depends.end() ){
            depends.erase( it );
            return new ESDecisionRandom( dvm, xmlDc, depends );
        }else{
            return new ESDecisionDeterministic( dvm, xmlDc, depends );
        }
    }
} }