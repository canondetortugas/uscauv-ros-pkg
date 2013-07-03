/***************************************************************************
 *  include/uscauv_common/simple_math.h
 *  --------------------
 *
 *  Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, Dylan Foster
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of USC AUV nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************/


#ifndef USCAUV_USCAUVCOMMON_SIMPLEMATH
#define USCAUV_USCAUVCOMMON_SIMPLEMATH

#include <cmath>
#include <algorithm>

namespace uscauv
{
  static double const     PI = 3.1415926535897932384626433832795028841971693993751;
  static double const TWO_PI = 6.2831853071795864769252867665590057683943387987502;
  static double const PI_TWO = 1.5707963267948965579989817342720925807952880859375;
  
  /// Integer case
  int algebraic_mod(int const & a, int const & b = 0)
  {
    if( b == 0 )
      return a;
    
    return (a%b+b)%b;
  }
  
  /// Floating point case
  double algebraic_mod(double  const & a, double const & b = 0)
    {
      if( b == 0.0 )
	return a;

      double m = a - b*floor(a/b);

      if( b > 0)
	{
	  if( m < 0 )
	    {
	      if( b+m >= b)
		return 0.0;
	      else return b+m;
	    }
	}
      else
	{
	  if( m <= b)
	    return 0;
	  if( m > 0 )
	    {
	      if(b + m == b)
		return 0;
	      else return b+m;
	    }
	}
      return m;
    }
  
  template<typename __NumericType>
    __NumericType ring_difference(__NumericType const & a, __NumericType const & b = 0, __NumericType const & range = TWO_PI)
  {
    __NumericType const x = algebraic_mod(a - b, range);
    __NumericType const y = algebraic_mod(b - a, range);

    return (y > x) ? -x : y;
  }

  template<typename __NumericType>
    __NumericType ring_distance(__NumericType const & a, __NumericType const & b = 0, __NumericType const & range = TWO_PI)
  {
    return std::min( algebraic_mod(a - b, range), algebraic_mod(b - a, range) );
  }

}

#endif // USCAUV_USCAUVCOMMON_SIMPLEMATH