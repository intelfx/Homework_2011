#ifndef _ANTIDED_BUILD_H
#define _ANTIDED_BUILD_H

// -----------------------------------------------------------------------------
// Library		Homework
// File			build.h
// Author		intelfx
// Description	Build-time dependencies
// -----------------------------------------------------------------------------

#include <uXray/build.h>

#include <uXray/encap.h>
#include <uXray/fxassert.h>
#include <uXray/fxmisc.h>
#include <uXray/fxhash_functions.h>

// ----

// For WINDOWS target (DLL) : dllexport/dllimport
#ifdef TARGET_WINDOWS

# ifdef __GNUC__
#  ifdef _INTERPRETER_STDAFX_H
#   define INTERPRETER_API __attribute__ ((dllexport))
#  else
#   define INTERPRETER_API __attribute__ ((dllimport))
#  endif
# endif

# ifdef _MSC_VER
#  ifdef _INTERPRETER_STDAFX_H
#   define INTERPRETER_API __declspec (dllexport)
#  else
#   define INTERPRETER_API __declspec (dllimport)
#  endif
# endif

#endif

// For POSIX target (DSO) : visibility default
#ifdef TARGET_POSIX
# ifdef __GNUC__
#   define INTERPRETER_API __attribute__ ((visibility ("default")))
# endif
#endif

// Fallback to nothing
#ifndef INTERPRETER_API
# define INTERPRETER_API
#endif

// ----

#endif // _ANTIDED_BUILD_H