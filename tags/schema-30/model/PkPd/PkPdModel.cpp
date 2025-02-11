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

#include "PkPd/PkPdModel.h"
// #include "PkPd/Drug/HoshenDrugType.h"
#include "Global.h"
// #include "PkPd/Proteome.h"
#include "util/ModelOptions.h"
#include "util/errors.h"
#include "inputData.h"

// submodels:
// #include "PkPd/HoshenPkPdModel.h"
#include "PkPd/LSTMPkPdModel.h"
#include "PkPd/VoidPkPdModel.h"

#include <assert.h>
#include <stdexcept>
#include <limits.h>

namespace OM { namespace PkPd {

PkPdModel::ActiveModel PkPdModel::activeModel = PkPdModel::NON_PKPD;


// -----  static functions  -----

void PkPdModel::init () {
    if (util::ModelOptions::option (util::INCLUDES_PK_PD)) {
	if (InputData().getPharmacology().present()) {
	    activeModel = LSTM_PKPD;
	    LSTMDrugType::init(InputData().getPharmacology().get ());
	} else {
	    activeModel = HOSHEN_PKPD;
            // Hoshen model has been removed.
            throw util::xml_scenario_error( "drugDescription element required in XML" );
// 	    ProteomeManager::init ();
// 	    HoshenDrugType::init();
	}
    }
}
void PkPdModel::cleanup () {
    if (activeModel == LSTM_PKPD) {
	LSTMDrugType::cleanup();
    } else if (activeModel == HOSHEN_PKPD) {
        assert( false );
// 	HoshenDrugType::cleanup();
// 	ProteomeManager::cleanup ();
    }
}

PkPdModel* PkPdModel::createPkPdModel () {
    if (activeModel == NON_PKPD) {
	return new VoidPkPdModel();
    } else if (activeModel == LSTM_PKPD) {
	return new LSTMPkPdModel ();
    } else if (activeModel == HOSHEN_PKPD) {
        
// 	return new HoshenPkPdModel ();
    }
    throw util::traced_exception("bad PKPD model");
}


} }
