#ifndef INTERPRETER_X86BACKEND_REX_H
#define INTERPRETER_X86BACKEND_REX_H

#include "build.h"

#include "Defs.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		REX.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT backend: REX prefix part definitions.
// -------------------------------------------------------------------------------------

namespace x86backend
{

struct REX
{
	bool b : 1;
	bool x : 1;
	bool r : 1;
	bool w : 1;
	unsigned char base : 4;

	void Enable()
	{
		// 0100 WRXB
		base = 0x04;

		// Check higher halfword
		// TODO make this check compile-time.
		s_cassert( ( reinterpret_cast<unsigned char&>( *this ) & 0xF0 ) == 0x40,
				   "Build-time error: invalid REX structure memory layout" );
	}

	bool IsSet()
	{
		// Check lower halfword
		return ( reinterpret_cast<unsigned char&>( *this ) & 0x0F );
	}
} PACKED;
static_assert( sizeof( REX ) == 1, "REX structure is not packed" );

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_REX_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; 
