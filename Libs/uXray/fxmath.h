#ifndef _FXMATH_H_
#define _FXMATH_H_

#include "build.h"

#include "fxassert.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxmath.h
// Author		intelfx
// Description	Mathematic functions
// -----------------------------------------------------------------------------

FXLIB_API int makePrimeTable (int lim, int* output);
FXLIB_API int getDividers (int src, int* output, bool only_prime_divs, const int* input, int inputlen);
FXLIB_API int getDivLayout (int src, int* output, const int* input, int inputlen);
FXLIB_API void printDivLayout (const int* input, int inputlen);
FXLIB_API int rand (int limit, int shift = 0);

FXLIB_API unsigned gcd (unsigned u, unsigned v);

FXLIB_API unsigned parse_char (char ch, unsigned radix);
FXLIB_API char make_char (unsigned val);

inline unsigned int rotl (const unsigned int value, int shift)
{
	if ( (shift &= sizeof (value) * 8 - 1) == 0)
		return value;

	return (value << shift) | (value >> (sizeof (value) * 8 - shift));
}

inline unsigned int rotr (const unsigned int value, int shift)
{
	if ( (shift &= sizeof (value) * 8 - 1) == 0)
		return value;

	return (value >> shift) | (value << (sizeof (value) * 8 - shift));
}

#endif // _FXMATH_H_
