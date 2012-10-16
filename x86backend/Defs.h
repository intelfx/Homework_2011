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

} // namespace Prefixes

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_DEFS_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
