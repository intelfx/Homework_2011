#ifndef _FXHASH_FUNCTIONS_H
#define _FXHASH_FUNCTIONS_H

#include "build.h"

#include "fxassert.h"
#include "fxmath.h"

// -----------------------------------------------------------------------------
// Library		FXLib
// File			Hashers.h
// Author		intelfx
// Description	Various hashing functions (C-style generalised).
// -----------------------------------------------------------------------------

size_t hasher_zero (const void*, size_t)
{
	return 0;
}

size_t hasher_length (const void*, size_t length)
{
	return length;
}

size_t hasher_ptr (const void* ptr, size_t)
{
	return reinterpret_cast<size_t> (ptr);
}

size_t hasher_sum (const void* ptr, size_t length)
{
	size_t sum = 0;
	const char* cptr = reinterpret_cast<const char*> (ptr);

	while (length--)
		sum += *cptr++;

	return sum;
}

size_t hasher_bsd (const void* ptr, size_t length)
{
	unsigned short sum = 0;
	const char* cptr = reinterpret_cast<const char*> (ptr);

	while (length--)
	{
		sum  = rotr (sum, 1);
		sum += *cptr++;
	}

	return sum;
}

size_t hasher_stl (const void* ptr, size_t length)
{
	return std::_Hash_impl::hash (ptr, length);
}

size_t hasher_xroll (const void* ptr, size_t length, size_t sum = 0)
{
	const char* cptr = reinterpret_cast<const char*> (ptr);

	size_t initsum = 0;

	static_assert (sizeof (size_t) <= 8, "Invalid size_t");
	switch (length % sizeof (size_t))
	{
		case 7:
			initsum += *cptr++;
		case 6:
			initsum += *cptr++;
		case 5:
			initsum += *cptr++;
		case 4:
			initsum += *cptr++;
		case 3:
			initsum += *cptr++;
		case 2:
			initsum += *cptr++;
		case 1:
			initsum += *cptr++;

			sum ^= initsum;
			sum = rotr (sum, 1);

		case 0:
		default:
			break;
	}

	size_t idx = length / sizeof (size_t);
	const size_t* uptr = reinterpret_cast<const size_t*> (cptr);

	while (idx--)
	{
		sum ^= *uptr++;
		sum = rotr (sum, 1);
	}

	return sum;
}

size_t hasher_xroll_plain (const void* ptr, size_t length)
{
	return hasher_xroll (ptr, length, 0);
}

#endif // _FXHASH_FUNCTIONS_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
