#include "Backend.h"

#include "Insn.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		Codegen.cpp
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
	else if( CompileCommand_ExtArithmetic( cmd ) ) { }
	else if( CompileCommand_Control( cmd ) ) { }
	else if( CompileCommand_Conditionals( cmd ) ) { }
	else if( CompileCommand_System( cmd ) ) { }
	else {
		msg( E_WARNING, E_VERBOSE, "Using an interpreter gate call to execute instruction." );
		cassert( cmd.type == Value::V_MAX, "Cannot use an interpreter gate call on a non-stack-less command (type: %s)",
		         ProcDebug::Print( cmd.type ).c_str() );
		CompileBinaryGateCall( BinaryFunction::BF_COMMANDGATE, reinterpret_cast<abiret_t>( &cmd ) );
	}
}

#define HANDLE_WRONG_TYPE casshole( "Wrong/unsupported value type when compiling command: %s", ProcDebug::Print( cmd.type ).c_str() )
#define WRONG_TYPE(type) case Value::type: HANDLE_WRONG_TYPE
#define WRONG_DEFAULT    default:          HANDLE_WRONG_TYPE

#define CheckMnemonic( c, m )      CheckCmd< crc32( m ) >( c, m )
#define OnCmd( c, m )              if( CheckMnemonic( c, m ) )
#define OnMultipleCmd( c, m1, m2 ) if( CheckMnemonic( c, m1 ) || CheckMnemonic( c, m2 ) )

bool x86Backend::CompileCommand_Control( Command& cmd )
{
	OnCmd( cmd, "jmp" ) {
		CompileControlTransferInstruction( cmd.arg.ref,
		                                   Insn().AddOpcode( 0xE9 ),
		                                   Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x4 ) );

		return true;
	} // jmp

	OnCmd( cmd, "call" ) {
		CompileControlTransferInstruction( cmd.arg.ref,
		                                   Insn().AddOpcode( 0xE8 ),
		                                   Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x2 ) );

		return true;
	} // call

	OnCmd( cmd, "ret" ) {
		Insn()
			.AddOpcode( 0xC3 )
			.Emit( this );

		return true;
	} // ret

	return false;
}

bool x86Backend::CompileCommand_ExtArithmetic( Command& cmd )
{
	OnCmd( cmd, "sqrt" ) {
		switch( cmd.type ) {
		case Value::V_FLOAT:
			// fsqrt
			Insn()
				.AddOpcode( 0xD9, 0xFA )
				.Emit( this );
			break;

		WRONG_TYPE(V_INTEGER);
		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // sqrt

	OnCmd( cmd, "sin" ) {
		switch( cmd.type ) {
		case Value::V_FLOAT:
			// fsin
			Insn()
				.AddOpcode( 0xD9, 0xFE )
				.Emit( this );
			break;

		WRONG_TYPE(V_INTEGER);
		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // sin

	OnCmd( cmd, "cos" ) {
		switch( cmd.type ) {
		case Value::V_FLOAT:
			// fcos
			Insn()
				.AddOpcode( 0xD9, 0xFF )
				.Emit( this );
			break;

		WRONG_TYPE(V_INTEGER);
		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // cos

	OnCmd( cmd, "tan" ) {
		switch( cmd.type ) {
		case Value::V_FLOAT:
			// fptan
			Insn()
				.AddOpcode( 0xD9, 0xF2 )
				.Emit( this );

			// ffree st(0) -- fptan pushes 1.0 after the tangent, pop it
			Insn()
				.AddOpcode( 0xDD, 0xC0 )
				.AddOpcodeRegister( RegX87::ST0 )
				.Emit( this );

			// fincstp
			Insn()
				.AddOpcode( 0xD9, 0xF7 )
				.Emit( this );
			break;

		WRONG_TYPE(V_INTEGER);
		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // tan

	OnCmd( cmd, "atan" ) {
		switch( cmd.type ) {
		case Value::V_FLOAT:
			// fpatan
			Insn()
				.AddOpcode( 0xD9, 0xF3 )
				.Emit( this );
			break;

		WRONG_TYPE(V_INTEGER);
		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // atan

	return false;
}

bool x86Backend::CompileCommand_Arithmetic( Command& cmd )
{
	OnCmd( cmd, "add" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// pop rdx
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RDX )
				.SetIsDefault64Bit()
				.Emit( this );

			// add rax, rdx
			Insn()
				.AddOpcode( 0x03 )
				.AddRegister( Reg64::RAX )
				.AddRegister( Reg64::RDX )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// faddp
			Insn()
				.AddOpcode( 0xDE, 0xC1 )
				.Emit( this );
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // add

	OnCmd( cmd, "sub" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// pop rdx
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RDX )
				.SetIsDefault64Bit()
				.Emit( this );

			// sub rax, rdx
			Insn()
				.AddOpcode( 0x2B )
				.AddRegister( Reg64::RAX )
				.AddRegister( Reg64::RDX )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fsubp
			Insn()
				.AddOpcode( 0xDE, 0xE9 )
				.Emit( this );
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // sub

	OnCmd( cmd, "mul" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// pop rcx
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RCX )
				.Emit( this );

			// imul rcx
			Insn()
				.AddOpcode( 0xF7 )
				.SetOpcodeExtension( 0x5 )
				.AddRM( RegisterWrapper( Reg64::RCX ) )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fmulp
			Insn()
				.AddOpcode( 0xDE, 0xC9 )
				.Emit( this );
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // mul

	OnCmd( cmd, "div" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// pop rcx
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RCX )
				.SetIsDefault64Bit()
				.Emit( this );

			// cqo
			Insn()
				.AddOpcode( 0x99 )
				.SetOperandSize( AddressSize::QWORD )
				.Emit( this );

			// idiv rcx
			Insn()
				.AddOpcode( 0xF7 )
				.SetOpcodeExtension( 0x7 )
				.AddRM( RegisterWrapper( Reg64::RCX ) )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fdivp
			Insn()
				.AddOpcode( 0xDE, 0xF9 )
				.Emit( this );
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // div

	OnCmd( cmd, "mod" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// pop rcx
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RCX )
				.SetIsDefault64Bit()
				.Emit( this );

			// cqo
			Insn()
				.AddOpcode( 0x99 )
				.SetOperandSize( AddressSize::QWORD )
				.Emit( this );

			// idiv rcx
			Insn()
				.AddOpcode( 0xF7 )
				.SetOpcodeExtension( 0x7 )
				.AddRM( RegisterWrapper( Reg64::RCX ) )
				.Emit( this );

			// mov rax, rdx
			Insn()
				.AddOpcode( 0x8B )
				.AddRegister( Reg64::RAX )
				.AddRegister( Reg64::RDX )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fprem1
			Insn()
				.AddOpcode( 0xD9, 0xF5 )
				.Emit( this );

			// TODO, FIXME: the remainder can be partial; check C2 and repeat if so.

			// fxch
			Insn()
				.AddOpcode( 0xD9, 0xC9 )
				.Emit( this );

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

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // mod

	OnCmd( cmd, "inc" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// inc rax
			Insn()
				.AddOpcode( 0xFF )
				.SetOpcodeExtension( 0x0 )
				.AddRM( RegisterWrapper( Reg64::RAX ) )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fld1
			Insn()
				.AddOpcode( 0xD9, 0xE8 )
				.Emit( this );

			// faddp
			Insn()
				.AddOpcode( 0xDE, 0xC1 )
				.Emit( this );
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // inc

	OnCmd( cmd, "dec" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// dec rax
			Insn()
				.AddOpcode( 0xFF )
				.SetOpcodeExtension( 0x1 )
				.AddRM( RegisterWrapper( Reg64::RAX ) )
				.Emit( this );
		break;

		case Value::V_FLOAT:
			// fld1
			Insn()
				.AddOpcode( 0xD9, 0xE8 )
				.Emit( this );

			// fsubp
			Insn()
				.AddOpcode( 0xDE, 0xE9 )
				.Emit( this );
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // dec

	OnCmd( cmd, "neg" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// neg rax
			Insn()
				.AddOpcode( 0xF7 )
				.SetOpcodeExtension( 0x3 )
				.AddRM( RegisterWrapper( Reg64::RAX ) )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fchs
			Insn()
				.AddOpcode( 0xD9, 0xE0 )
				.Emit( this );
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // neg

	OnCmd( cmd, "abs" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// algorithm taken from http://www.strchr.com/optimized_abs_function
			// cqo
			Insn()
				.AddOpcode( 0x99 )
				.SetOperandSize( AddressSize::QWORD )
				.Emit( this );

			// xor rax, rdx
			Insn()
				.AddOpcode( 0x33 )
				.AddRegister( Reg64::RAX )
				.AddRegister( Reg64::RDX )
				.Emit( this );

			// sub rax, rdx
			Insn()
				.AddOpcode( 0x2B )
				.AddRegister( Reg64::RAX )
				.AddRegister( Reg64::RDX )
				.Emit( this );
			break;

		case Value::V_FLOAT:
			// fabs
			Insn()
				.AddOpcode( 0xD9, 0xE1 )
				.Emit( this );
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // abs

	return false;
}

bool x86Backend::CompileCommand_Conditionals( Command& cmd )
{
	OnCmd( cmd, "je" ) {
		// push bx
		// popf word
		CompileRestoreFlags();

		// the conditional trampoline (jz rel32 / jnz rel8)
		CompileConditionalControlTransferInstruction( cmd.arg.ref,
		                                              Insn().AddOpcode( 0x0F, 0x84 ),
		                                              Insn().AddOpcode( 0x75 ),
		                                              Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x4 ) );

		return true;
	} // je

	OnCmd( cmd, "jne" ) {
		// push bx
		// popf word
		CompileRestoreFlags();

		// the conditional trampoline (jnz rel32 / jz rel8)
		CompileConditionalControlTransferInstruction( cmd.arg.ref,
		                                              Insn().AddOpcode( 0x0F, 0x85 ),
		                                              Insn().AddOpcode( 0x74 ),
		                                              Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x4 ) );

		return true;
	} // je

	OnMultipleCmd( cmd, "ja", "jnbe" ) {
		// push bx
		// popf word
		CompileRestoreFlags();

		// the conditional trampoline set (ja rel32 / jna rel8)
		CompileConditionalControlTransferInstruction( cmd.arg.ref,
		                                              Insn().AddOpcode( 0x0F, 0x87 ),
		                                              Insn().AddOpcode( 0x76 ),
		                                              Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x4 ) );

		return true;
	} // ja/jnbe

	OnMultipleCmd( cmd, "jae", "jnb" ) {
		// push bx
		// popf word
		CompileRestoreFlags();

		// the conditional trampoline set (jnb rel32 / jb rel8)
		CompileConditionalControlTransferInstruction( cmd.arg.ref,
		                                              Insn().AddOpcode( 0x0F, 0x83 ),
		                                              Insn().AddOpcode( 0x72 ),
		                                              Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x4 ) );

		return true;
	} // jae/jnb

	OnMultipleCmd( cmd, "jna", "jbe" ) {
		// push bx
		// popf word
		CompileRestoreFlags();

		// the conditional trampoline set (jna rel32 / ja rel8)
		CompileConditionalControlTransferInstruction( cmd.arg.ref,
		                                              Insn().AddOpcode( 0x0F, 0x86 ),
		                                              Insn().AddOpcode( 0x77 ),
		                                              Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x4 ) );

		return true;
	} // jna/jbe

	OnMultipleCmd( cmd, "jnae", "jb" ) {
		// push bx
		// popf word
		CompileRestoreFlags();

		// the conditional trampoline set (jb rel32 / jnb rel8)
		CompileConditionalControlTransferInstruction( cmd.arg.ref,
		                                              Insn().AddOpcode( 0x0F, 0x82 ),
		                                              Insn().AddOpcode( 0x73 ),
		                                              Insn().AddOpcode( 0xFF ).SetOpcodeExtension( 0x4 ) );

		return true;
	} // jnae/jb

	return false;
}

bool x86Backend::CompileCommand_System( Command& cmd )
{
	OnCmd( cmd, "push" ) {
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

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // push

	OnCmd( cmd, "pop" ) {
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

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // pop

	OnCmd( cmd, "ld" ) {
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

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // ld

	OnCmd( cmd, "st" ) {
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

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // st

	OnCmd( cmd, "ldint" ) {
		switch( cmd.type ) {
		case Value::V_FLOAT:
			// fild m64
			Insn()
				.AddOpcode( 0xDF )
				.SetOpcodeExtension( 0x5 )
				.AddRM( CompileReferenceResolution( cmd.arg.ref ) )
				.Emit( this );
			break;

		WRONG_TYPE(V_INTEGER);
		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // ldint

	OnCmd( cmd, "stint" ) {
		switch( cmd.type ) {
		case Value::V_FLOAT:
			// fistp m64
			Insn()
				.AddOpcode( 0xDF )
				.SetOpcodeExtension( 0x7 )
				.AddRM( CompileReferenceResolution( cmd.arg.ref ) )
				.Emit( this );
			break;

		WRONG_TYPE(V_INTEGER);
		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // stint

	OnCmd( cmd, "dup" ) {
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

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // dup

	OnCmd( cmd, "swap" ) {
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

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // swap

	OnCmd( cmd, "quit" ) {
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

		WRONG_DEFAULT;
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

		return true;
	} // quit

	OnCmd( cmd, "cmp" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// mov rdx, rax -- subtrahend
			Insn()
				.AddOpcode( 0x8B )
				.AddRegister( Reg64::RDX )
				.AddRegister( Reg64::RAX )
				.Emit( this );

			// pop rax -- minuend
			Insn()
				.AddOpcode( 0x58 )
				.AddOpcodeRegister( Reg64::RAX )
				.SetIsDefault64Bit()
				.Emit( this );

			// cmp rax, rdx
			Insn()
				.AddOpcode( 0x3B )
				.AddRegister( Reg64::RAX )
				.AddRegister( Reg64::RDX )
				.Emit( this );

			CompileIntegerCompare();
			break;

		case Value::V_FLOAT:
			CompileFPCompare();
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // cmp

	OnCmd( cmd, "anal" ) {
		switch( cmd.type ) {
		case Value::V_INTEGER:
			// cmp rax, 0
			Insn()
				.AddOpcode( 0x3D )
				.AddImmediate<int32_t>( 0 )
				.SetOperandSize( AddressSize::QWORD )
				.Emit( this );

			CompileIntegerCompare();
			break;

		case Value::V_FLOAT:
			// fldz
			Insn()
				.AddOpcode( 0xD9, 0xEE )
				.Emit( this );

			CompileFPCompare();
			break;

		default: WRONG_TYPE(V_MAX);
		}

		return true;
	} // anal

	OnCmd( cmd, "snfc" ) {
		msg( E_WARNING, E_VERBOSE, "NFC is not implemented in JIT mode. It acts like NFC=1, \"snfc\" becomes no-op." );

		return true;
	} // snfc

	OnCmd( cmd, "cnfc" ) {
		msg( E_WARNING, E_VERBOSE, "NFC is not implemented in JIT mode. It acts like NFC=1, \"cnfc\" becomes no-op." );

		return true;
	} // cnfc

	OnCmd( cmd, "top" ) {
// 		msg( E_WARNING, E_VERBOSE, "\"top\" is a no-op in JIT mode." );

		return true;
	} // top

	return false;
}

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
