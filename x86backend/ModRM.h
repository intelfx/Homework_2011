#ifndef INTERPRETER_X86BACKEND_MODRM_H
#define INTERPRETER_X86BACKEND_MODRM_H

#include "build.h"

#include "Defs.h"
#include "Registers.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		ModRM.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT backend: ModR/M opcode part definitions.
// -------------------------------------------------------------------------------------

namespace x86backend
{

struct ModRM
{
	unsigned char rm : 3;
	unsigned char reg : 3;
	unsigned char mod : 2;
} PACKED;
static_assert( sizeof( ModRM ) == 1, "ModR/M structure is not packed" );

enum class ModField
{
	NoShift = 0, // 00b
	Disp8   = 1, // 01b
	Disp32  = 2, // 10b
	Direct  = 3  // 11b
};

// [xxx]
enum class IndirectNoShift
{
	RAX             = 0,
	RCX             = 1,
	RDX             = 2,
	RBX             = 3,
	UseSIB          = 4,
	Displacement32  = 5, // WARNING: this is RIP-based addressing in x86_64
	RSI             = 6,
	RDI             = 7
};

// [xxx] + disp8
enum class IndirectShiftDisplacement8
{
	RAX    = 0,
	RCX    = 1,
	RDX    = 2,
	RBX    = 3,
	UseSIB = 4,
	RBP    = 5,
	RSI    = 6,
	RDI    = 7
};

// [xxx] + disp32
enum class IndirectShiftDisplacement32
{
	RAX    = 0,
	RCX    = 1,
	RDX    = 2,
	RBX    = 3,
	UseSIB = 4,
	RBP    = 5,
	RSI    = 6,
	RDI    = 7
};

struct ModRMWrapper
{
	union {
		IndirectNoShift no_shift;
		IndirectShiftDisplacement8 disp8;
		IndirectShiftDisplacement32 disp32;
		unsigned char raw;
	};

	bool need_extension;
	ModField mod;

	// Intended for mod == 00b, memory addressing like [RAX] or displacement32
	ModRMWrapper( IndirectNoShift reg ) :
		no_shift( reg ),
		need_extension( false ),
		mod( ModField::NoShift )
	{
	}

	// Intended for mod == 01b, memory addressing like [RAX]+displacement8
	ModRMWrapper( IndirectShiftDisplacement8 reg ) :
		disp8( reg ),
		need_extension( false ),
		mod( ModField::Disp8 )
	{
	}

	// Intended for mod == 10b, memory addressing like [RAX]+displacement32
	ModRMWrapper( IndirectShiftDisplacement32 reg ) :
		disp32( reg ),
		need_extension( false ),
		mod( ModField::Disp32 )
	{
	}

	// Intended for mod == 11b, register-register operations
	ModRMWrapper( RegisterWrapper reg ) :
		raw( reg.raw ),
		need_extension( reg.need_extension ),
		mod( ModField::Direct )
	{
		s_cassert( reg.operand_size == AddressSize::QWORD, "Cannot generate direct r/m field from non-QWORD register" );
	}

	// Intended for any mod-field, addressing of additional registers (R8, [R8], [R8]+displacement8, [R8]+displacement32)
	ModRMWrapper( Reg64E reg, ModField shift ) :
		raw( reinterpret_cast<unsigned char&>( reg ) ),
		need_extension( true ),
		mod( shift )
	{
		// TODO: transform [R13] (mod == 00b, r/m = 101b) into SIB+disp8 form (see 2.2.1.6 of the Intel manual)
	}
};

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_MODRM_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
