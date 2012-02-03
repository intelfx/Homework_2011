#include "stdafx.h"
#include "fxmath.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxmath.cpp
// Author		intelfx
// Description	Mathematic functions
// -----------------------------------------------------------------------------


unsigned int gcd (unsigned int u, unsigned int v)
{
	int shift;

	/* GCD(0,x) := x */
	if (u == 0 || v == 0)
		return u | v;

	/* Let shift := lg K, where K is the greatest power of 2
	 *      dividing both u and v. */
	for (shift = 0; ( (u | v) & 1) == 0; ++shift)
	{
		u >>= 1;
		v >>= 1;
	}

	while ( (u & 1) == 0)
		u >>= 1;

	/* From here on, u is always odd. */
	do
	{
		while ( (v & 1) == 0) /* Loop X */
			v >>= 1;

		/* Now u and v are both odd, so diff(u, v) is even.
		 *          Let u = min(u, v), v = diff(u, v)/2. */
		if (u < v)
		{
			v -= u;
		}
		else
		{
			unsigned int diff = u - v;
			u = v;
			v = diff;
		}
		v >>= 1;
	}
	while (v != 0);

	return u << shift;
}

FXLIB_API int rand (int limit, int shift)
{
	return (rand() % limit) + shift;
}

FXLIB_API void printDivLayout (const int* input, int inputlen)
{
	int c_div = input[0];
	int div_c = 0;
	int value = 1;

	for (int i = 0; i < inputlen; ++i)
		value *= input[i];

	printf ("%d =", value);

	for (int i = 0; i < inputlen; ++i)
	{
		if (input[i] == c_div)
			div_c++;
		else
		{
			printf (" %d^%d ", c_div, div_c);
			putchar ('*');
			c_div = input[i];
			div_c = 1;
		}

		if ( (i + 1) == inputlen)
			//last value isn't printed
			printf (" %d^%d", c_div, div_c);
	}
}

FXLIB_API int getDivLayout (int src, int* output, const int* input, int inputlen)
{
	__sassert (src > 1, "Invalid source");

	int part = src;
	int count = 0;
	while (part != 1)
	{
		for (int i = 0; i < inputlen; ++i)
		{
			int cur_value = input[i];

			if (! (part % cur_value))
			{
				part /= cur_value;
				output[count++] = cur_value;
				break;
			}
		}
	}

	return count;
}

FXLIB_API int getDividers (int src, int* output, bool only_prime, const int* input, int inputlen)
{
	__sassert (src > 1, "Invalid source");
	__sassert (input || (!only_prime), "Invalid pointer");
	__sassert (inputlen || (!only_prime), "Invalid pointer");

	int count = 0;
	int cur_value = 0;
	int for_limit = 0;

	for_limit = (only_prime) ? inputlen : (src / 2 + 1);

	for (int i = 0; i < for_limit; ++i)
	{
		cur_value = (only_prime) ? input[i] : i;

		if (cur_value < 2) continue;
		if (! (src % cur_value)) output[count++] = cur_value;
	}

	return count;
}

FXLIB_API int makePrimeTable (int lim, int* output)
{
	__sassert (lim > 1, "Invalid source");

	for (int i = 0; i <= lim; ++i)
		output[i] = i;

	output[0] = 0;
	output[1] = 0;

	int cpv = 2; //current prime for computing the table
	int lgv = 4; //last "good" value in the table at current iteration

	smsg (E_INFO, E_DEBUG, "Calculating primes table with limit %d", lim);

	do
	{
		for (int i = (cpv * 2); i <= lim; i += cpv)
			output[i] = 0; //cut values that are multiplies of current prime

		for (int i = cpv + 1; i < lgv; ++i)
			if (output[i])
			{
				cpv = output[i];
				break;
			}

		lgv = cpv * cpv;
	}
	while (lgv <= lim);

	// Now compress array, deleting all the zeroes from it.
	int cci = -1; // current compressed index
	for (int i = 0; i <= lim; ++i)
		if (output[i])
		{
			output[++cci] = output[i];
			output[i] = 0;
		}

	return cci;
}

FXLIB_API unsigned parse_char (char ch, unsigned radix)
{
	unsigned result = 0;

	if (ch >= '0' && ch <= '9')
		result = ch - '0';

	else if (ch >= 'a' && ch <= 'z')
		result = ch - 'a' + 10;

	else if (ch >= 'A' && ch <= 'Z')
		result = ch - 'A' + 10;

	__sverify (result, "Invalid char: '%c': Non-numeric", ch);
	__sverify (result < radix, "Invalid char: '%c': Doesn't fit in radix %d", ch, radix);
	return result;
}

FXLIB_API char make_char (unsigned val)
{
	char result;

	if (val < 10)
		result = val + '0';

	else
		result = val + 'A' - 10;

	__sassert (result <= 'Z', "Invalid value: %d: Doesn't fit in alphabet", val);
	return result;
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
