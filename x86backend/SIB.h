#ifndef INTERPRETER_X86BACKEND_SIB_H
#define INTERPRETER_X86BACKEND_SIB_H

#include "build.h"

#include "Defs.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		SIB.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT backend: SIB opcode part definitions.
// -------------------------------------------------------------------------------------

namespace x86backend
{

struct SIB
{
	unsigned char base : 3;
	unsigned char index : 3;
	unsigned char scale : 2;
} PACKED;
static_assert( sizeof( SIB ) == 1, "SIB structure is not packed" );

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_SIB_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; 
