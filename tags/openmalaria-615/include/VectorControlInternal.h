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

/// This is a header for VectorControl's functions which are only used internally.

#include <gsl/gsl_vector.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_multiroots.h>

//#define VectorControl_PRINT_CalcUpsilonOneHost
//#define VectorControl_PRINT_CalcSvDiff
//#define VectorControl_PRINT_CalcLambda
//#define VectorControl_PRINT_CalcXP
//#define VectorControl_PRINT_CalcSpectralRadius
//#define VectorControl_PRINT_CalcInv1minusA


/***************************************************************************
 *********************** STRUCTURE DEFINITIONS *****************************
 ***************************************************************************/

// Structure that contains the parameters for the function used in the 
// root-finding algorithm to find the emergence rate that matches the 
// number of infectious host-seeking mosquitoes.
struct SvDiffParams
{
  gsl_vector* S_vFromEIR;
  gsl_matrix** Upsilon;
  gsl_matrix* inv1Xtp;
  size_t eta;
  size_t mt;
  size_t thetap;
};



/** CalcUpsilonOneHost returns a pointer to an array of thetap 
 * GSL matrices assuming there is only one host of humans..
 * Each matrix is Upsilon(t).
 *
 * $Upsilon(t)$ is the evolution of the mosquito population over one
 * time step. There are three main system variables:
 * $N_v$: The total number of host-seeking mosquitoes.
 * $O_v$: The number of infected host-seeking mosquitoes.
 * $S_v$: The number of infectious host-seeking mosquitoes.
 *
 * As the difference equations go back more than one time step, 
 * the size of the system is larger than 3.
 * For $N_v$ and $O_v$, we need to go back mt steps.
 * For $S_v$ we need to go back tau steps.
 * So the size of the system, eta = 2 mt + tau.
 * The first column of Upsilon(t) (indexed by 0 in C) corresponds to
 * $N_v(t)$ - as it depends on the other paramters at previous times.
 * The (mt+1)^th column of Upsilon(t) (indexed by mt in C) corresponds to
 * $O_v(t)$ - as it depends on the other paramters at previous times.
 * The (2mt+1)^th column of Upsilon(t) (indexed by 2mt in C) corresponds to
 * $S_v(t)$ - as it depends on the other paramters at previous times.
 * All other columns have 1 in the subdiagonal.
 *
 * For now, we write this code assuming that the parameters where we
 * are ignoring dependence on host type, or phase of the period, (and
 * have defined as doubles) will remove doubles. We do not code for
 * generality. If we make changes to these data types later, we will
 * change the code then. We code this, as is, to make it easier now, 
 * as we do not know what parameters we will change. It should 
 * hopefully, not be too difficult to change the code later (and
 * create a new general CalcUpsilon). Let's hope....
 *
 * 
 * Upsilon, PAPtr, and PAiPtr are OUT parameters.
 * All other parameters are IN parameters. */
void CalcUpsilonOneHost(gsl_matrix** Upsilon, double* PAPtr, 
                        double* PAiPtr, size_t thetap, size_t eta, size_t mt, size_t tau, 
                        size_t thetas, size_t n, size_t m, double Ni, double alphai, 
                        double muvA, double thetad, double PBi, double PCi, double PDi, 
                        double PEi, gsl_vector* Kvi, char fntestentopar[]);

/** CalcSvDiff_rf returns the difference between Sv for the periodic 
 * orbit for the given Nv0 and from the EIR data.
 * 
 * Given the input parameters to the entomological model, this routine
 * calculates the number of infectious host-seekign mosquitoes for the 
 * resulting periodic orbit. It then calculates the difference between 
 * this Sv and the periodic Sv calculated from the EIR data (which is 
 * the Sv from the periodic orbit of the system with the final 
 * calculated Nv0.
 *
 * This routine performs the same calculations as CalcSvDiff() but it
 * does so in the format required by the GSL multiroot-finding
 * algorithms.
 * 
 * f is an OUT parameter.
 * All other parameters are IN parameters. */
int CalcSvDiff_rf(const gsl_vector* x, void* p, gsl_vector* f);

// NOTE: Unused
//static size_t staticCalcSvDiff_rf(const gsl_vector* x, void* p, gsl_vector* f,void* obj);

/** CalcSvDiff returns the difference between Sv for the periodic 
 * orbit for the given Nv0 and from the EIR data.
 * 
 * Given the input parameters to the entomological model, this routine
 * calculates the number of infectious host-seekign mosquitoes for the 
 * resulting periodic orbit. It then calculates the difference between 
 * this Sv and the periodic Sv calculated from the EIR data (which is 
 * the Sv from the periodic orbit of the system with the final 
 * calculated Nv0.
 * 
 * SvDiff is an OUT parameter.
 * All other parameters are IN parameters. */
void CalcSvDiff(gsl_vector* SvDiff, gsl_vector* SvfromEIR, 
                gsl_matrix** Upsilon, gsl_vector* Nv0, gsl_matrix* inv1Xtp, 
                size_t eta, size_t mt, size_t thetap, char fntestentopar[]);

/** CalcLambda() returns a pointer to an array of thetap 
 * GSL vectors.
 * Each vector is Lambda(t).
 *
 * $Lambda(t)$ is the forcing of the mosquito population
 * at each time step, that is, it is the number of new
 * mosquitoes that enter the population at each time, $t$.
 *
 * We note here that Nv0 is a gsl_vector where the index, t, refers
 * to the mosquito emergence rate at time, t. Lambda[t] is a
 * gsl_vector that denotes the forcing at time t, where the index, i,
 * refers to the forcing to the i^th dimension of the system.
 *
 * God moves over the face of the waters,
 * Looking to the left and looking to the right,
 * But there is only water to see.
 * 
 * Lambda is an OUT parameter.
 * All other parameters are IN parameters. */
void CalcLambda(gsl_vector** Lambda, gsl_vector* Nv0, size_t eta,
                size_t thetap, char fntestentopar[]);

/** CalcXP returns a pointer to an array of thetap 
 * GSL vectors.
 * Each vector is is the periodic orbit solution to the main system
 * of equations at time, t.
 *
 * The size of each xp[t] is eta: the order of the system.
 *
 * This routine uses Theorem 2 of Cushing (1998) JDEA 3.
 *
 * We can probably improve the speed of this algorithm. We could replace
 * the vectors for Lambda[i] by simply using Nv0[i] and multiplying the first
 * column of the matrices, X[t,i] by Nv0[i]. 
 *
 * But for now we don't worry about speed and try to continue with the route
 * finding. There may be more work that we need to do to improve speed.
 * 
 * xp is an OUT parameter.
 * All other parameters are IN parameters. */
void CalcXP(gsl_vector** xp, gsl_matrix** Upsilon, 
            gsl_vector** Lambda, gsl_matrix* inv1Xtp, size_t eta,
            size_t thetap, char fntestentopar[]);


/** CalcPSTS() calculates probabilities of surviving the extrinsic
 * incubation period (or part of). The returned variables are the sums
 * to $k_+$ and $k_{l+}$ (including the binomial coefficients and 
 * probabilities in (2.3c) of the paper. 
 *
 * Currently, this returns scalar values because neither $P_A$, nor
 * $P_{df}$, depend on the phase of the period.
 *
 * Note that sumklplus here is defined as sumlv in MATLAB.
 * 
 * sumkplusPtr and sumklplus are OUT parameters.
 * All other parameters are IN parameter. */
void CalcPSTS(double* sumkplusPtr, double* sumklplus, size_t thetas,
              size_t tau, double PA, double Pdf);

/** FuncX() calculates X(t,s).
 *
 * Note that we have to be careful with indices here. 
 * Cushing (1995) has indices starting at 0 and ending at $\theta_p -1$.
 * In our notes, and in MATLAB, the indices start at 1 and end at $\theta_p$.
 *
 *       X(t,s) = \Upsilon(t-1)*...*Upsilon(s) for t \geq s+1
 *              = I                            for t = s.
 *
 * Here, FuncX() is defined for s>=0 and t>=1.
 * 
 * X is an OUT parameter.
 * All other parameters are IN parameters. */
void FuncX(gsl_matrix* X, gsl_matrix** Upsilon, size_t t, size_t s, size_t n);

/** CalcSpectralRadius() calculates the spectral radius of a given matrix.
 *
 * Given an n by n, real, nonsymmetric matrix, A, 
 * this routine calcultes its spectral radius,
 * that is, the eigenvalue with the largest absolute value.
 * 
 * A, n, and fntestentopar are IN parameters. */
double CalcSpectralRadius(gsl_matrix* A, size_t n, char fntestentopar[]);

/** CalcInv1minusA() calculates the inverse of (I-A) where A is a 
 * given matrix.
 *
 * Given an n by n, real matrix, A, 
 * this routine calcultes the inverse of (I-A) where I is the 
 * n by n identity matrix.
 * 
 * A, n, and fntestentopar are IN parameters.
 * inv1A is an OUT parameter. */
void CalcInv1minusA(gsl_matrix* inv1A, gsl_matrix* A, size_t n, char fntestentopar[]);

/** CalcSvfromEIRdata() calculates Sv, given the EIR.
 *
 * Given EIR, and the parameters that determine host-biting,
 * this routine calculates the number of infectious host-seeking
 * mosquitoes, Sv.
 *
 * The EIR is assumed to be periodic so the resulting vector for Sv
 * is also periodic.
 *
 * The other parameters are constant. 
 *
 * Once the periodic paper is written, we should add a refernece to the 
 * equation that we use.
 * 
 * PAi, PBi, Ni and Xii are IN parameters.
 * Sv is an OUT parameter. */
void CalSvfromEIRdata(gsl_vector* Sv, double PAi, double PBi, double Ni, 
                      gsl_vector* Xii);


/** binomial() calculates the binomial coefficient given two integers.
 * 
 * Note that we do not check for errors. */
double binomial(int n, int k);


/******************************************************************************
 Printing routines below. Most are only optionally compiled in.
******************************************************************************/
void PrintRootFindingStateTS(size_t iter, gsl_multiroot_fsolver* srootfind, 
                             size_t thetap, char fnrootfindingstate[]);

void PrintParameters(char fntestentopar[], size_t thetap, size_t tau, size_t thetas, 
                     size_t n, size_t m, double Ni, double alphai, double muvA, 
                     double thetad, double PBi, double PCi, double PDi, double PEi, 
                     gsl_vector* Kvi, gsl_vector* Xii, gsl_vector* Nv0guess);

void PrintUpsilon(char fntestentopar[], gsl_matrix** Upsilon, size_t thetap,
                  size_t eta, double PA, double PAi, double Pdf, gsl_vector* Pdif,
                  gsl_vector* Pduf);

void PrintXP(gsl_vector** xp, size_t eta, size_t thetap, char fntestentopar[]);

void PrintLambda(gsl_vector** Lambda, size_t eta, char fntestentopar[]);

void PrintEigenvalues(char fntestentopar[], gsl_vector_complex* eval, size_t n);

void PrintMatrix(char fntestentopar[], char matrixname[], gsl_matrix* A, 
                 size_t RowLength, size_t ColLength);


void PrintVector(char fntestentopar[], char vectorname[], gsl_vector* v, size_t n);
