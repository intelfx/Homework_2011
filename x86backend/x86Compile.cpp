#include "stdafx.h"
#include "x86Backend.h"

#include "x86backend/Insn.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		x86Compile.cpp
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT compiler backend: semantic part (code generation worker).
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;
using namespace x86backend;

void x86Backend::CompilePrologue()
{
	// mov r11, rdi
	Insn()
		.AddOpcode( 0x8B )
		.AddRegister( Reg64E::R11 )
		.AddRegister( Reg64::RDI )
		.Emit( this );

	// push rbx
	Insn()
		.AddOpcode( 0x50 )
		.AddOpcodeRegister( Reg64::RBX )
		.SetIsDefault64Bit()
		.Emit( this );

	// enter 0, 0
	Insn()
		.AddOpcode( 0xC8 )
		.AddImmediate<uint16_t>( 0 )
		.AddImmediate<uint8_t>( 0 )
		.SetOperandSize()
		.Emit( this );

	// finit
	Insn()
		.AddOpcode( 0x9B, 0xDB, 0xE3 )
		.Emit( this );
}

void x86Backend::CompileCommand( Command& cmd )
{
	if( CompileCommand_Arithmetic( cmd ) ) { }
	else if( CompileCommand_Control( cmd ) ) { }
	else if( CompileCommand_Conditionals( cmd ) ) { }
	else if( CompileCommand_System( cmd ) ) { }
	else {
		casshole( "Unsupported command. Interpreter command gate is not implemented." );
	}
}

#define WRONG_TYPE(type) case Value::type: casshole( "Wrong/unsupported value type when compiling command: %s", ProcDebug::Print( Value::type ).c_str() )

bool x86Backend::CompileCommand_Control( Command& cmd )
{
	if( IsCmd( cmd, "jmp" ) ) {
		CompileControlTransferInstruction( cmd.arg.ref,
										   Insn().AddOpcode( 0xE9 ),
										   Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x4 ) );
	} else if( IsCmd( cmd, "call" ) ) {
		CompileControlTransferInstruction( cmd.arg.ref,
										   Insn().AddOpcode( 0xE8 ),
										   Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x2 ) );
	} else if( IsCmd( cmd, "ret" ) ) {
		Insn()
		.AddOpcode( 0xC3 )
		.Emit( this );
	} else {
		return false;
	}
	return true;
}

bool x86Backend::CompileCommand_Arithmetic( Command& cmd )
{
	return false;
}

bool x86Backend::CompileCommand_Conditionals( Command& cmd )
{
	return false;
}

bool x86Backend::CompileCommand_System( Command& cmd )
{
	if( IsCmd( cmd, "push" ) ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// push rax
			Insn()
				.AddOpcode( 0x50 )
				.AddOpcodeRegister( Reg64::RAX )
				.SetIsDefault64Bit()
				.Emit( this );

			// mov rax, {cmd.arg.value.integer}
			Insn()
				.AddOpcode( 0xB8 )
				.AddOpcodeRegister( Reg64::RAX )
				.AddImmediate( cmd.arg.value.integer )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// mov rcx, {cmd.arg.value.fp as cmd.arg.value.integer}
			Insn()
				.AddOpcode( 0xB8 )
				.AddOpcodeRegister( Reg64::RCX )
				.AddImmediate( cmd.arg.value.integer )
				.Emit( this );

			// push rcx
			Insn()
				.AddOpcode( 0x50 )
				.AddOpcodeRegister( Reg64::RCX )
				.SetIsDefault64Bit()
				.Emit( this );

			// fld qword [rsp]
			Insn()
				.AddOpcode( 0xDD )
				.SetOpcodeExtension( 0x0 )
				.AddRM( ModRMWrapper( BaseRegs::RSP, IndexRegs::None, 1, ModField::NoShift ) )
				.Emit( this );

			// pop rcx
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RCX )
				.SetIsDefault64Bit()
				.Emit( this );
			break;

		WRONG_TYPE(V_MAX);
		}
	} // push

	else if( IsCmd( cmd, "pop" ) ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// pop rax -- overwrite
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RAX )
				.SetIsDefault64Bit()
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// ffree st(0)
			Insn()
				.AddOpcode( 0xDD, 0xC0 )
				.AddOpcodeRegister( RegX87::ST0 )
				.Emit( this );

			// fincstp
			Insn()
				.AddOpcode( 0xD9, 0xF7 )
				.Emit( this );
			break;

		WRONG_TYPE(V_MAX);
		}
	} // pop

	else if( IsCmd( cmd, "ld" ) ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// push rax
			Insn()
				.AddOpcode( 0x50 )
				.AddOpcodeRegister( Reg64::RAX )
				.SetIsDefault64Bit()
				.Emit( this );

			// mov rax, [reference]
			Insn()
				.AddOpcode( 0x8B )
				.AddRegister( Reg64::RAX )
				.AddRM( CompileReferenceResolution( cmd.arg.ref ) )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fld m64
			Insn()
				.AddOpcode( 0xDD )
				.SetOpcodeExtension( 0x0 )
				.AddRM( CompileReferenceResolution( cmd.arg.ref ) )
				.Emit( this );
			break;

		WRONG_TYPE(V_MAX);
		}
	}

	else if( IsCmd( cmd, "st" ) ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// mov [reference], rax
			Insn()
				.AddOpcode( 0x89 )
				.AddRM( CompileReferenceResolution( cmd.arg.ref ) )
				.AddRegister( Reg64::RAX )
				.Emit( this );

			// pop rax
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RAX )
				.SetIsDefault64Bit()
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fstp m64
			Insn()
				.AddOpcode( 0xDD )
				.SetOpcodeExtension( 0x3 )
				.AddRM( CompileReferenceResolution( cmd.arg.ref ) )
				.Emit( this );
			break;

		WRONG_TYPE(V_MAX);
		}
	} // st

	else if( IsCmd( cmd, "dup" ) ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// push rax
			Insn()
				.AddOpcode( 0x50 )
				.AddOpcodeRegister( Reg64::RAX )
				.SetIsDefault64Bit()
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fld st(0)
			Insn()
				.AddOpcode( 0xD9, 0xC0 )
				.AddOpcodeRegister( RegX87::ST0 )
				.Emit( this );
			break;

		WRONG_TYPE(V_MAX);
		}
	} // dup

	else if( IsCmd( cmd, "swap" ) ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// xchg rax, [rsp]
			Insn()
				.AddOpcode( 0x87 )
				.AddRegister( Reg64::RAX )
				.AddRM( ModRMWrapper( BaseRegs::RSP, IndexRegs::None, 1, ModField::NoShift ) )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fxch
			Insn()
				.AddOpcode( 0xD9, 0xC9 )
				.Emit( this );
			break;

		WRONG_TYPE(V_MAX);
		}
	} // swap

	else if( IsCmd( cmd, "quit" ) ) {
		// mov dword [r11], {cmd.type}
		Insn()
			.AddOpcode( 0xC7 )
			.AddRM( ModRMWrapper( Reg64E::R11, ModField::NoShift ) )
			.AddImmediate<uint32_t>( cmd.type )
			.Emit( this );

		switch( cmd.type ) {
		case Value::V_INTEGER:
			// no-op, value in rax
			break;

		case Value::V_FLOAT:
			// sub rsp, 8
			Insn()
				.AddOpcode( 0x83 )
				.SetOpcodeExtension( 0x5 )
				.AddRM( RegisterWrapper( Reg64::RSP ) )
				.AddImmediate<uint8_t>( 8 )
				.Emit( this );

			// fstp qword [rsp]
			Insn()
				.AddOpcode( 0xDD )
				.SetOpcodeExtension( 0x3 )
				.AddRM( ModRMWrapper( BaseRegs::RSP, IndexRegs::None, 1, ModField::NoShift ) )
				.Emit( this );

			// pop rax
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RAX )
				.SetIsDefault64Bit()
				.Emit( this );
			break;

		case Value::V_MAX:
			// xor rax, rax
			Insn()
				.AddOpcode( 0x33 )
				.AddRegister( Reg64::RAX )
				.AddRegister( Reg64::RAX )
				.Emit( this );
			break;
		}

		// finit
		Insn()
			.AddOpcode( 0x9B, 0xDB, 0xE3 )
			.Emit( this );

		// leave
		Insn()
			.AddOpcode( 0xC9 )
			.Emit( this );

		// pop rbx
		Insn()
			.AddOpcode( 0x58 )
			.AddOpcodeRegister( Reg64::RBX )
			.SetIsDefault64Bit()
			.Emit( this );

		// retn
		Insn()
			.AddOpcode( 0xC3 )
			.Emit( this );
	} // quit

	else {
		return false;
	}

	return true;
}

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
