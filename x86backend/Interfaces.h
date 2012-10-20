#ifndef INTERPRETER_X86BACKEND_INTERFACES_H
#define INTERPRETER_X86BACKEND_INTERFACES_H

#include "../build.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		Interfaces.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT backend: instruction emitter interfaces.
// -------------------------------------------------------------------------------------

namespace x86backend
{

class IEmissionDestination
{
public:
	// The emission target buffer.
	virtual llarray& Target() = 0;

	// Inserts a stub to be then converted into a valid offset to an instruction
	// within the same native image.
	// relative: whether to insert a relative 32-bit displacement to the instruction
	//           (wrt. next instruction address) or an absolute 64-bit address.
	// The caller of Insn::Emit() is expceted to maintain instruction offset table
	// and count emitted instructions manually (as well as provide valid pseudo-displacements).
	virtual void AddCodeReference( size_t insn, bool relative ) = 0;
};

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_INTERFACES_H
