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

size_t hasher_stl (const void* ptr, size_t length)
{
	return std::_Hash_impl::hash (ptr, length);
}

size_t hasher_xroll (const void* ptr, size_t length)
{
	size_t sum = 0;
	const char* cptr = reinterpret_cast<const char*> (ptr);

	if (size_t initial_mod = length % sizeof (size_t))
	{
		while (initial_mod--)
		{
			__sassert (reinterpret_cast<const char*> (cptr) - reinterpret_cast<const char*> (ptr) < length,
					   "Hasher selfcheck: out-of-bounds");

			sum += *cptr++;
			sum = sum << 8;
		}
	}

	size_t idx = length / sizeof (size_t);
	const size_t* uptr = reinterpret_cast<const size_t*> (cptr);

	while (idx--)
	{
		__sassert ( (reinterpret_cast<const char*> (uptr) - reinterpret_cast<const char*> (ptr)) < length,
					"Hasher selfcheck: out-of-bounds");

		sum ^= *uptr++;
		sum = rotr (sum, 1);
	}

	return sum;
}

#endif // _FXHASH_FUNCTIONS_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
