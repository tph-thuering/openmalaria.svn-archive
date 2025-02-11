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

#ifndef Hmod_empiricalwithinhost
#define Hmod_empiricalwithinhost

#include "Global.h"
#include "WithinHost/WithinHostModel.h"
#include "WithinHost/EmpiricalInfection.h"
#include "PkPd/PkPdModel.h"

using namespace std;

namespace OM { namespace WithinHost {
    
/*! EmpiricalDummy Within Host Model class.
 */
class EmpiricalWithinHostModel : public WithinHostModel
{
public:
    EmpiricalWithinHostModel();
    ~EmpiricalWithinHostModel();
    
    
    virtual void newInfection();
    virtual void clearAllInfections();
    
    virtual void medicate (string drugName, double qty, int time, double age);
    
    /** Update densities for timestep (taking into account blood-stage vaccine
     * and drug efficacies. */
    virtual void calculateDensities (double ageInYears, double BSVEfficacy);
    
protected:
    virtual int countInfections (int& patentInfections);
    
    virtual void checkpoint (istream& stream);
    virtual void checkpoint (ostream& stream);
    
private:
    /// Encapsulates drug code for each human
    PkPd::PkPdModel* pkpdModel;

    /** The list of all infections this human has.
     *
     * Since infection models and within host models are very much intertwined,
     * the idea is that each WithinHostModel has its own list of infections. */
    std::list<EmpiricalInfection> infections;
};

} }
#endif