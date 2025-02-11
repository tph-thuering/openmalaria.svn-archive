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
#ifndef Hmod_global
#define Hmod_global
#pragma warning(disable: 4290)
#include "Constant.h"
#include <fcntl.h>
#include <math.h>
#include <vector>
#include <string>
#include <stdexcept>
using namespace std;

/// Command-Line Option possibilities
namespace CLO {
  enum CLO {
    NONE		= 0x0,
    
    EARLY_EXIT		= 0x100,	///< Don't run main simulation flag
    
    PRINT_MODEL_VERSION	= 0x1 | EARLY_EXIT,
    TEST_CHECKPOINTING	= 0x2,
  };
}

class Global
{
public:
  /** Looks through all command line options.
   *
   * @returns The name of the scenario XML file to use.
   *
   * Calls exit(1) directly to exit in the case a help message is printed. Help
   * is printed to cout. Both actions necessitate calling this function
   * before BOINC is initialised.
   * 
   * In other cases, sets variables in Global to achieve the desired result. */
  static string parseCommandLine (int argc, char* argv[]);
  
  /** Sets parameters in Global and performs some checks.
   *
   * Also outputs some extra information for command-line options.
   * 
   * @returns: True if we should stop before running the simulation because of
   * an option. */
  static bool initGlobal ();
  
  static int modIntervalsPerYear (int i);

  /// Variables that must be checkpointed.
  //@{

  /** Model version defines which implementations of hard-coded options should be
   * used. The integer value of modelVersion passed from the .xml is converted to
   * binary with each bit corresponding to a different dichotomous option.  The
   * original default model is modelVersion=0 */
  static ModelVersion modelVersion;
  //@}
  
  /// Data read from xml which doesn't need to be checkpointed.
  //@{

  /// temporal resolution of simulation, in days
  static int interval;
   //Simulation time steps per year
  static size_t intervalsPerYear;
  // Maximum age of individuals in a scenario in time intervals
  static int maxAgeIntervals;
  static int simulationMode;
   //pre-erythrocytic latent period, in time steps
  static int latentp;
  
/*
  Size of the human population
  Moved from population.f so that transmission model.f can see it.
*/
  static vector<int> infantDeaths;
  static vector<int> infantIntervalsAtRisk;
  //@}
  
  static CLO::CLO clOptions;
  
private:
  /// Sets modelVersion, checking for incompatible versions.
  static void setModelVersion ();
};

/** Thrown to indicate an error in the scenario.xml file.  */
class xml_scenario_error : public runtime_error
{
  public:
    explicit xml_scenario_error(const string&  __arg);
};

#endif
