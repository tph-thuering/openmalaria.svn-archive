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
// Unittest for the LSTM drug model

#ifndef Hmod_LSTMPkPdSuite
#define Hmod_LSTMPkPdSuite

#include <cxxtest/TestSuite.h>
#include "PkPd/LSTMPkPdModel.h"
#include "UnittestUtil.h"
#include "ExtraAsserts.h"
#include <limits>

using namespace OM;
using namespace OM::PkPd;

class LSTMPkPdSuite : public CxxTest::TestSuite
{
public:
    void setUp () {
	agd.update( 21.0 );
	UnittestUtil::PkPdSuiteSetup(PkPdModel::LSTM_PKPD);
	proxy = new LSTMPkPdModel ();
	proteome_ID = 0;		// 0 should work; we definitely don't want random allocation
    }
    void tearDown () {
	delete proxy;
	UnittestUtil::PkPdSuiteTearDown();
    }
    
    void testNone () {
	TS_ASSERT_EQUALS (proxy->getDrugFactor (proteome_ID), 1.0);
    }
    
    void testCq () {
	proxy->medicate ("MF", 3000, 0, agd, 21);
	TS_ASSERT_APPROX (proxy->getDrugFactor (proteome_ID), 0.03564073617400945);
    }
    
    void testCqHalves () {	// the point being: check it can handle two doses at the same time-point correctly
	proxy->medicate ("MF", 1500, 0, agd, 21);
	proxy->medicate ("MF", 1500, 0, agd, 21);
	TS_ASSERT_APPROX (proxy->getDrugFactor (proteome_ID), 0.03564073617400945);
    }
    
    void testCqSplit () {
	proxy->medicate ("MF", 3000, 0, agd, 21);
	proxy->medicate ("MF", 0, 0.5, agd, 21);	// insert a second dose half way through the day: forces drug calculation to be split into half-days but shouldn't affect result
	TS_ASSERT_APPROX (proxy->getDrugFactor (proteome_ID), 0.03564073617400945);
    }
    
    void testCqDecayed () {
	proxy->medicate ("MF", 3000, 0, agd, 21);
	proxy->decayDrugs ();
	TS_ASSERT_APPROX (proxy->getDrugFactor (proteome_ID), 0.03601694155274731);
    }
    
    void testCq2Doses () {
	proxy->medicate ("MF", 3000, 0, agd, 21);
	proxy->decayDrugs ();
	proxy->medicate ("MF", 3000, 0, agd, 21);
	TS_ASSERT_APPROX (proxy->getDrugFactor (proteome_ID), 0.03245158219000328);
    }
    
    AgeGroupData agd;
    LSTMPkPdModel *proxy;
    uint32_t proteome_ID;
};

#endif
