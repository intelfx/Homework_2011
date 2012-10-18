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
	NONE = 0,
    BYTE,
    WORD,
    DWORD,
    QWORD
};

enum class OperandType
{
	None = 0,
	Register,       // modrm byte, reg field
	OpcodeRegister, // opcode, inline register spec
	RegMem,         // modrm byte, mod+r/m fields
	Immediate
};

AddressSize EncodeSize( size_t size )
{
	switch( size ) {
	case 1:
		return AddressSize::BYTE;

	case 2:
		return AddressSize::WORD;

	case 4:
		return AddressSize::DWORD;

	case 8:
		return AddressSize::QWORD;

	default:
		s_casshole( "Invalid size of an immediate value: %zu", size );
	}
}

namespace Prefixes
{

enum class SizeOverride : unsigned char
{
    Operand = 0x66,
    Address = 0x67
};

enum class GeneralPurpose : unsigned char
{
    LOCK = 0xF0,
    REPNE = 0xF2,
    REPE = 0xF3
};

enum class SegmentOverride : unsigned char
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

enum class Special : unsigned char
{
	ESC = 0x0F
};

} // namespace Prefixes

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_DEFS_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
