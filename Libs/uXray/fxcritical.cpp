#include "stdafx.h"
#include "fxcritical.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxcritical.cpp
// Author		intelfx
// Description	Some critical definitions and functions.
// -----------------------------------------------------------------------------

FXLIB_API bool fx_vasprintf (char** dest, const char* fmt, va_list args)
{
#ifdef _GNU_SOURCE
	int vasprintf_result = vasprintf (dest, fmt, args);

	if (vasprintf_result != -1)
		return 1;
#endif

	// Fallback to another implementation when vasprintf is either unavailable or failed.

	// Try to determine length via vsnprintf with NULL pointer and size, as by POSIX/C99.
	// Otherwise, if runtime does not conform, use static method.
	int length = vsnprintf (0, 0, fmt, args);
	if (length < 1)
		length = STATIC_LENGTH;

	// Allocate the memory.
	char* staticmem = reinterpret_cast<char*> (malloc (length +1));
	if (!staticmem)
	{
		// We can't do much if calloc() fails.
		*dest = 0;
		return 0;
	}

	// We won't check if our length is sufficient, we'll just cut off what's excess.
	int vsnprintf_result = vsnprintf (staticmem, length, fmt, args);

	if (vsnprintf_result != -1)
		return 1;

	// Everything fails.
	return 0;
}

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
