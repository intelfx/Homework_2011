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

FXLIB_API size_t hasher_zero (const void*, size_t);
FXLIB_API size_t hasher_length (const void*, size_t length);
FXLIB_API size_t hasher_ptr (const void* ptr, size_t);
FXLIB_API size_t hasher_sum (const void* ptr, size_t length);
FXLIB_API size_t hasher_bsd (const void* ptr, size_t length);
FXLIB_API size_t hasher_bsd_string (const void* ptr);
FXLIB_API size_t hasher_stl (const void* ptr, size_t length);
FXLIB_API size_t hasher_xroll (const void* ptr, size_t length, size_t sum = 0);
FXLIB_API size_t hasher_xroll_plain (const void* ptr, size_t length);

#endif // _FXHASH_FUNCTIONS_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
