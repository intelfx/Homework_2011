#ifndef INTERPRETER_X86BACKEND_DEFS_H
#define INTERPRETER_X86BACKEND_DEFS_H

#include "build.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		Defs.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT backend: common definitions.
// -------------------------------------------------------------------------------------

namespace x86backend
{

enum class AddressSize
{
    BYTE,
    WORD,
    DWORD,
    QWORD
};

enum class OperandEncoding
{
    Reg_Memory,    // First operand: reg.                Second operand: r/m.
    Memory_Reg,    // First operand: r/m.                Second operand: reg.
    Memory_Imm,    // First operand: r/m.                Second operand: imm.
    OpcodeReg_Imm, // First operand: register in opcode. Second operand: imm.
    Acc_Imm,       // First operand: accumulator.        Second operand: imm.
};

enum class OperandType
{
	None = 0,
	Register,       // modrm byte, reg field
	OpcodeRegister, // opcode, inline register spec
	RegMem,         // modrm byte, mod+r/m fields
	Immediate
};

struct ImmediateSignedWrapper
{
	union {
		int8_t i8;
		int16_t i16;
		int32_t i32;
		int64_t i64;
	};
	AddressSize size;

	ImmediateSignedWrapper( int8_t i ) :
	i8( i ),
	size( AddressSize::BYTE )
	{
	}

	ImmediateSignedWrapper( int16_t i ) :
	i16( i ),
	size( AddressSize::WORD )
	{
	}

	ImmediateSignedWrapper( int32_t i ) :
	i32( i ),
	size( AddressSize::DWORD )
	{
	}

	ImmediateSignedWrapper( int64_t i ) :
	i64( i ),
	size( AddressSize::QWORD )
	{
	}
};

struct ImmediateUnsignedWrapper
{
	union {
		uint8_t i8;
		uint16_t i16;
		uint32_t i32;
		uint64_t i64;
	};
	AddressSize size;

	ImmediateUnsignedWrapper( uint8_t i ) :
	i8( i ),
	size( AddressSize::BYTE )
	{
	}

	ImmediateUnsignedWrapper( uint16_t i ) :
	i16( i ),
	size( AddressSize::WORD )
	{
	}

	ImmediateUnsignedWrapper( uint32_t i ) :
	i32( i ),
	size( AddressSize::DWORD )
	{
	}

	ImmediateUnsignedWrapper( uint64_t i ) :
	i64( i ),
	size( AddressSize::QWORD )
	{
	}
};

namespace Prefixes
{

enum class SizeOverride
{
    Operand = 0x66,
    Address = 0x67
};

enum class GeneralPurpose
{
    LOCK = 0xF0,
    REPNE = 0xF2,
    REPE = 0xF3
};

enum class SegmentOverride
{
    CS = 0x2E,
    SS = 0x36,
    DS = 0x3E,
    ES = 0x26,
    FS = 0x64,
    GS = 0x65,

    // These are not segment override, but in group 2 as per spec.
    BranchNotTaken = 0x2E,
    BranchTaken = 0x3E
};

enum class Special
{
	ESC = 0x0F
};

} // namespace Prefixes

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_DEFS_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
