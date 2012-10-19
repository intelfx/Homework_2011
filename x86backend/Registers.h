#ifndef INTERPRETER_X86BACKEND_REGISTERS_H
#define INTERPRETER_X86BACKEND_REGISTERS_H

#include "build.h"

#include "Defs.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		Registers.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT backend: register definitions.
// -------------------------------------------------------------------------------------

namespace x86backend
{

enum class Reg8 : unsigned char
{
	AL = 0,
	CL = 1,
	DL = 2,
	BL = 3,
	AH = 4,
	CH = 5,
	DH = 6,
	BH = 7
};

enum class Reg16 : unsigned char
{
	AX = 0,
	CX = 1,
	DX = 2,
	BX = 3,
	SP = 4,
	BP = 5,
	SI = 6,
	DI = 7
};

enum class Reg32 : unsigned char
{
	EAX = 0,
	ECX = 1,
	EDX = 2,
	EBX = 3,
	ESP = 4,
	EBP = 5,
	ESI = 6,
	EDI = 7
};

enum class Reg64 : unsigned char
{
	RAX = 0,
	RCX = 1,
	RDX = 2,
	RBX = 3,
	RSP = 4,
	RBP = 5,
	RSI = 6,
	RDI = 7
};

enum class Reg64E : unsigned char
{
	R8  = 0,
	R9  = 1,
	R10 = 2,
	R11 = 3,
	R12 = 4,
	R13 = 5,
	R14 = 6,
	R15 = 7
};

enum class RegX87 : unsigned char
{
	ST0 = 0,
	ST1 = 1,
	ST2 = 2,
	ST3 = 3,
	ST4 = 4,
	ST5 = 5,
	ST6 = 6,
	ST7 = 7
};

struct RegisterWrapper
{
	union {
		Reg8 reg8;
		Reg32 reg32;
		Reg64 reg64;
		Reg64E reg64e;
		RegX87 regx87;
		unsigned char raw;
	};

	AddressSize operand_size;
	bool need_extension;

	RegisterWrapper( Reg8 reg ) :
		reg8( reg ),
		operand_size( AddressSize::BYTE ),
		need_extension( false )
	{
	}

	RegisterWrapper( Reg32 reg ) :
		reg32( reg ),
		operand_size( AddressSize::DWORD ),
		need_extension( false )
	{
	}

	RegisterWrapper( Reg64 reg ) :
		reg64( reg ),
		operand_size( AddressSize::QWORD ),
		need_extension( false )
	{
	}

	RegisterWrapper( Reg64E reg ) :
		reg64e( reg ),
		operand_size( AddressSize::QWORD ),
		need_extension( true )
	{
	}

	RegisterWrapper( RegX87 reg ) :
		regx87( reg ),
		operand_size( AddressSize::NONE ),
		need_extension( false )
	{
	}
};

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_REGISTERS_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
