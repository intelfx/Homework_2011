#ifndef _UX_BUILD_H
#define _UX_BUILD_H

// -----------------------------------------------------------------------------
// Library		FXLib
// File			build.h
// Author		intelfx
// Description	Build-time dependencies. To include in every header file.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Definitions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// -------------------------warning-control-------------------------------------
// -----------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning (disable: 4996) // Using unsafe functions
#pragma warning (disable: 4800) // Forcing value to boolean
#pragma warning (disable: 4251) // No DLL-interface found
#pragma warning (disable: 4355) // Using "this" in base class initializer list
#pragma warning (disable: 4250) // Inherits <...> via dominance
#endif // _MSC_VER

// -----------------------------------------------------------------------------
// -------------------------platform-control------------------------------------
// -----------------------------------------------------------------------------

#if defined (__x86_64__) || defined (__LP64) || defined (__IA64__) ||					\
	defined (_M_X64)     || defined (_WIN64)
# define TARGET_X64 1
#elif defined (_M_IX86) || defined (__INTEL__) || defined (__i386__)
# define TARGET_X86 1
#else
# error "FX Target Arch Control module: Can't match target architecture."
#endif

#if defined (WINDOWS) || defined (__WINDOWS__) || defined (_WIN32) ||					\
	defined (_WIN64) || defined (_WINDOWS) || defined (WIN32) || defined (WIN64)
# define TARGET_WINDOWS 1
#elif defined (__linux__)     || defined (__unix__)    ||								\
	  defined (__OpenBSD__)   || defined (__FreeBSD__) || defined (__NetBSD__) ||		\
	  defined (__DragonFly__) || defined (__BSD__)     || defined (__FREEBSD__) ||		\
	  defined (__APPLE__)
# define TARGET_POSIX 1
#else
# error "FX Target OS Control module: Can't match target operating system."
#endif


// -----------------------------------------------------------------------------
// -----------------------------API-control-------------------------------------
// -----------------------------------------------------------------------------

// FXLIB_API is a macro used to seamlessly export/import symbols in this library.
// _FXLIB_STDAFX_H_ is used to determine whether we are building the library,
// because in sources this file gets included with "stdafx.h" in first line.

/*
 * Exporting templates is still a pain in the ass.
 * We can't just write FXLIB_API in the template definition -
 * it works for __attribute(visibility(default)),
 * but in dllexport/dllimport case that will work only
 * for template instantiations exported from the library
 * and imported in the application,
 * but if the template is used (instantiated) in application too,
 * application linker will still try to import all the instantiations.
 *
 * And we can't write it in the template explicit instantiation -
 * GCC requires the __attribute__(visibility(default)) to be in definition.
 * Though __attribute__((dllexport)) will work from instantiation.
 */

// For WINDOWS target (DLL) : dllexport/dllimport
#ifdef TARGET_WINDOWS

# ifdef __GNUC__
#  ifdef _FXLIB_STDAFX_H_
#   define FXLIB_API __attribute__ ((dllexport))
#  else
#   define FXLIB_API __attribute__ ((dllimport))
#  endif
#  define PACKED __attribute__ ((packed))
# endif

# ifdef _MSC_VER
#  ifdef _FXLIB_STDAFX_H_
#   define FXLIB_API __declspec (dllexport)
#  else
#   define FXLIB_API __declspec (dllimport)
#  endif
# endif

#endif

// For POSIX target (DSO) : visibility default
#ifdef TARGET_POSIX
# ifdef __GNUC__
#   define PACKED __attribute__ ((packed))
#   define FXLIB_API __attribute__ ((visibility ("default")))
# endif
#endif

// Fallback to nothing
#ifndef FXLIB_API
# define FXLIB_API
#endif

#ifndef PACKED
# define PACKED
#endif

// -----------------------------------------------------------------------------
// Dependencies
// -----------------------------------------------------------------------------

#include <utility>
#include <stdexcept>
#include <map>
#include <set>
#include <unordered_map>
#include <stack>
#include <string>
#include <vector>
#include <list>
#include <typeinfo>

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#ifndef va_start
# define va_start(list, arg) __sasshole ("VA_START unsupported!")
#endif

#ifndef va_end
# define va_end(list) __sasshole ("VA_END unsupported!")
#endif

#ifndef va_arg
# define va_arg(list,type) __sasshole ("VA_ARG unsupported!")
#endif

#include "fxcritical.h"

#endif // _UX_BUILD_H
