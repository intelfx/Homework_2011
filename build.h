#ifndef INTERPRETER_BUILD_H
#define INTERPRETER_BUILD_H

// -------------------------------------------------------------------------------------
// Library		Homework
// File			build.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Build-time dependency file.
// -------------------------------------------------------------------------------------

#include <uXray/build.h>

#include <uXray/encap.h>
#include <uXray/fxassert.h>
#include <uXray/fxmisc.h>
#include <uXray/fxhash_functions.h>
#include <uXray/fxcrc32.h>
#include <uXray/fxjitruntime.h>

#ifdef INTERPRETER_STDAFX_H
# define INTERPRETER_API EXPORT
# define INTERPRETER_TE
#else
# define INTERPRETER_API IMPORT
# define INTERPRETER_TE extern
#endif

#include "Defs.h"

#endif // INTERPRETER_BUILD_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
