#ifndef INTERPRETER_X86BACKEND_BUILD_H
#define INTERPRETER_X86BACKEND_BUILD_H

// -------------------------------------------------------------------------------------
// Library		Homework
// File			build.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Build-time dependency file for x86 native JIT compiler
// -------------------------------------------------------------------------------------

#include "../build.h"

#ifdef BUILDING_INTERPRETER_X86BACKEND
# define INTERPRETER_X86BACKEND_API EXPORT
# define INTERPRETER_X86BACKEND_TE
#else
# define INTERPRETER_X86BACKEND_API IMPORT
# define INTERPRETER_X86BACKEND_TE extern
#endif

#endif // INTERPRETER_BUILD_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
