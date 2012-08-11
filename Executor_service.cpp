#include "stdafx.h"
#include "Executor_service.h"

#include <uXray/time_ops.h>

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Executor_service.cpp
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Untyped interpreter plugin implementation.
// -------------------------------------------------------------------------------------

ImplementDescriptor( ServiceExecutor, "service executor", MOD_APPMODULE );

namespace ProcessorImplementation
{
using namespace Processor;

enum COMMANDS {
	C_INIT = 1,
	C_SLEEP,
	C_SETTYPE,

	C_JE,
	C_JNE,
	C_JA,
	C_JNA,
	C_JAE,
	C_JNAE,
	C_JB,
	C_JNB,
	C_JBE,
	C_JNBE,

	C_LEA,
	C_SYSCALL,

	C_JMP,
	C_CALL,
	C_RET,

	C_DCX,
	C_SNFC,
	C_CNFC,

	C_QUIT,

	C_MAX
};

const char* ServiceExecutor::supported_mnemonics[C_MAX] = {
	0,
	"init",
	"sleep",
	"settype",

	"je",
	"jne",
	"ja",
	"jna",
	"jae",
	"jnae",
	"jb",
	"jnb",
	"jbe",
	"jnbe",

	"lea",
	"sys",

	"jmp",
	"call",
	"ret",

	"dump",
	"snfc",
	"cnfc",

	"quit"
};

Value::Type ServiceExecutor::SupportedType() const
{
	return Value::V_MAX;
}

void ServiceExecutor::OnAttach()
{
	ResetImplementations();
}

void ServiceExecutor::ResetImplementations()
{
	ICommandSet* cmdset = proc_->CommandSet();

	for( size_t cmd = 1; cmd < C_MAX; ++cmd ) {
		cmdset->AddCommandImplementation( supported_mnemonics[cmd], ID(),
		                                  reinterpret_cast<void*>( cmd ) );
	}
}

void ServiceExecutor::Execute( void* handle, Command& command )
{
	struct {
		bool negative;
		bool zero;
		bool analyze_always;
		bool invalid_fp;
	} temp_flags;

	size_t flags = proc_->MMU()->GetContext().flags;
	temp_flags.negative = flags & MASK( F_NEGATIVE );
	temp_flags.zero = flags & MASK( F_ZERO );
	temp_flags.analyze_always = !( flags & MASK( F_NFC ) );
	temp_flags.invalid_fp = flags & MASK( F_INVALIDFP );

	COMMANDS cmd = static_cast<COMMANDS>( reinterpret_cast<ptrdiff_t>( handle ) );

	switch( cmd ) {
	case C_INIT:
		proc_->MMU()->ResetEverything();
		proc_->MMU()->AllocContextBuffer();
		break;

	case C_SLEEP:
		timeops::sleep( 0 );
		break;

	case C_SETTYPE: {
		proc_->LogicProvider()->UpdateType( proc_->Linker()->Resolve( command.arg.ref ), command.type );
		break;
	}

	case C_JNE:

		if( !temp_flags.zero )
			proc_->LogicProvider()->Jump( proc_->Linker()->Resolve( command.arg.ref ) );

		break;

	case C_JE:

		if( temp_flags.zero )
			proc_->LogicProvider()->Jump( proc_->Linker()->Resolve( command.arg.ref ) );

		break;

	case C_JA:
	case C_JNBE:

		if( !temp_flags.zero && !temp_flags.negative )
			proc_->LogicProvider()->Jump( proc_->Linker()->Resolve( command.arg.ref ) );

		break;

	case C_JNA:
	case C_JBE:

		if( temp_flags.zero || temp_flags.negative )
			proc_->LogicProvider()->Jump( proc_->Linker()->Resolve( command.arg.ref ) );

		break;

	case C_JAE:
	case C_JNB:

		if( !temp_flags.negative )
			proc_->LogicProvider()->Jump( proc_->Linker()->Resolve( command.arg.ref ) );

		break;

	case C_JNAE:
	case C_JB:

		if( temp_flags.negative )
			proc_->LogicProvider()->Jump( proc_->Linker()->Resolve( command.arg.ref ) );

		break;

	case C_LEA:
		proc_->MMU()->ARegister( indirect_addressing_register ) =
		    static_cast<int_t>( proc_->Linker()->Resolve( command.arg.ref ).address );
		break;

	case C_SYSCALL:
		cassert( command.arg.value.type == Value::V_INTEGER, "Non-integer syscall argument" );
		proc_->LogicProvider()->Syscall( command.arg.value.integer );
		break;


	case C_JMP:
		proc_->LogicProvider()->Jump( proc_->Linker()->Resolve( command.arg.ref ) );
		break;

	case C_CALL:
		proc_->MMU()->SaveContext();
		proc_->LogicProvider()->Jump( proc_->Linker()->Resolve( command.arg.ref ) );
		break;

	case C_RET:
		proc_->MMU()->RestoreContext();
		break;

	case C_DCX: {
		const char* ctx_dump, *reg_dump, *stack_dump;
		proc_->MMU()->DumpContext( &ctx_dump, &reg_dump, &stack_dump );

		msg( E_INFO, E_USER, "Context dump:" );
		msg( E_INFO, E_USER, "%s", ctx_dump );
		msg( E_INFO, E_USER, "%s", reg_dump );
		msg( E_INFO, E_USER, "%s", stack_dump );
	}
	break;

	case C_SNFC:
		proc_->MMU()->GetContext().flags |=  MASK( F_NFC );
		break;

	case C_CNFC:
		proc_->MMU()->GetContext().flags &= ~MASK( F_NFC );
		break;

	case C_QUIT:
		proc_->MMU()->GetContext().flags |= MASK( F_EXIT );
		break;

	case C_MAX:
	default:
		casshole( "Switch error" );
		break;
	}
}

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
