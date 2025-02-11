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

#ifndef Hmod_UtilVectors
#define Hmod_UtilVectors

#include "Global.h"
#include <gsl/gsl_vector.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Various utilities acting on vectors. */

namespace OM { namespace util { namespace vectors {

  ///@brief Basic operations on std::vector
  //@{
  /// Scale all elements of a vector by a in-situ
  void scale (vector<double>& vec, double a);
  
  /// Return sum of all elements
  double sum (const vector<double>& vec);
  
  /// Return sum of all elements
  double sum (const gsl_vector *vec);
  
  /// Return arithmetric mean
  inline double mean (const vector<double>& vec){
    return sum(vec) / vec.size();
  }
  /// Return arithmetric mean
  inline double mean (const gsl_vector *vec){
    return sum(vec) / vec->size;
  }
  
  /// Add one vector into another (x += y)
  void addTo (vector<double>& x, vector<double>& y);
  //@}
  
  
  ///@brief Comparissons on std::vector
  //@{
  /** Return true if, approximately, a == b
   *
   * In detail: true when (fabs(a-b) <= max(fabs(a),fabs(b)) * lim_fact). */
  bool approxEqual (const double a, const double b, const double lim_fact = 1e-6);
  
  /// Returns true if vec1 and vec2 have equal length and all elements are
  /// approximately equal (see approxEqual for double parameters).
  bool approxEqual (const vector<double>& vec1, const vector<double>& vec2, const double lim_fact = 1e-6);
  //@}
  
  
  ///@brief Convertions between gsl_vector and std::vector<double>
  //@{
  /** Convert a gsl_vector to a std::vector<double>. */
  vector<double> gsl2std (const gsl_vector* vec);
  /** Convert a gsl_vector to a possibly already allocated std::vector<double>. */
  void gsl2std( const gsl_vector *vec, vector<double>& target );
  
  /** Convert a std::vector<double> to a gsl_vector (newly allocated).
   *
   * @param vec Input vector
   * @param length Allows length to be validated. If
   *	(vec.size() != length) an exception is thrown. */
  gsl_vector* std2gsl (const vector<double>& vec, size_t length);
  /// Ditto, but taking values from a double[].
  gsl_vector* std2gsl (const double* vec, size_t length);
  //@}
  
  /** Given an input sequence of Fourier coefficients, with odd length,
   * calculate the exponential of the corresponding fourier series.
   *
   * Note: output is per-interval in tArray. When length is intervalsPerYear,
   * you may want to scale the output by days-per-interval.
   *
   * @param tArray Array to fill with exponated values from Fourier series.
   * Length should already be set.
   * @param FC Fourier coefficients (a0, a1,b1, a2,b2, ...); can be any length
   * so long as it is odd.
   * @param rAngle Angle to rotate generated series by in radians: [0,2π] */
  void calcExpFourierSeries (vector<double>& tArray, vector<double>& FC, double rAngle);
}

/// Utility to print a vector (operator must be in namespace)
template<class T>
ostream& operator<< (ostream& out, vector<T> vec) {
  out << '[';
  if (vec.size())
    out << vec[0];
  for (size_t i = 1; i < vec.size(); ++i)
    out << ", " << vec[i];
  out << ']';
  return out;
}

} }
#endif
