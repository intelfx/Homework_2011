#include "stdafx.h"
#include "Executor_int.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Executor_int.cpp
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Integer interpreter plugin implementation.
// -------------------------------------------------------------------------------------

ImplementDescriptor( IntegerExecutor, "integer executor", MOD_APPMODULE );

namespace ProcessorImplementation
{
using namespace Processor;

enum COMMANDS {
	C_PUSH = 1,
	C_POP,
	C_TOP,

	C_LOAD,
	C_STORE,

	C_ABS,
	C_ADD,
	C_SUB,
	C_MUL,
	C_DIV,
	C_MOD,

	C_INC,
	C_DEC,

	C_NEG,

	C_ANAL,
	C_CMP,
	C_SWAP,
	C_DUP,

	C_MAX
};

const char* IntegerExecutor::supported_mnemonics[C_MAX] = {
	0,
	"push",
	"pop",
	"top",

	"ld",
	"st",

	"abs",
	"add",
	"sub",
	"mul",
	"div",
	"mod",

	"inc",
	"dec",

	"neg",

	"anal", // Fuck yeah
	"cmp",
	"swap",
	"dup"
};

void IntegerExecutor::OnAttach()
{
	ResetImplementations();
}

Value::Type IntegerExecutor::SupportedType() const
{
	return Value::V_INTEGER;
}

void IntegerExecutor::ResetImplementations()
{
	ICommandSet* cmdset = proc_->CommandSet();

	for( size_t cmd = 1; cmd < C_MAX; ++cmd ) {
		cmdset->AddCommandImplementation( supported_mnemonics[cmd], ID(),
		                                  reinterpret_cast<void*>( cmd ) );
	}
}

inline void IntegerExecutor::AttemptAnalyze()
{
	if( need_to_analyze )
		proc_->LogicProvider()->Analyze( temp[0] );
}

inline void IntegerExecutor::PopArguments( size_t count )
{
	cassert( count < temp_size, "Too much arguments for temporary processing" );

	for( size_t i = 0; i < count; ++i )
		proc_->LogicProvider()->StackPop().Get( Value::V_INTEGER, temp[i] );
}

inline void IntegerExecutor::TopArgument()
{
	proc_->LogicProvider()->StackTop().Get( Value::V_INTEGER, temp[0] );
}

inline void IntegerExecutor::ReadArgument( Reference& ref )
{
	proc_->LogicProvider()->Read( proc_->Linker()->Resolve( ref ) ).Get( Value::V_INTEGER, temp[0] );
}

inline void IntegerExecutor::PushResult()
{
	proc_->LogicProvider()->StackPush( temp[0] );
}

inline void IntegerExecutor::WriteResult( Reference& ref )
{
	proc_->LogicProvider()->Write( proc_->Linker()->Resolve( ref ), temp[0] );
}

void IntegerExecutor::Execute( void* handle, Command& command )
{
	need_to_analyze = !( proc_->CurrentContext().flags & MASK( F_NFC ) );

	COMMANDS cmd = static_cast<COMMANDS>( reinterpret_cast<ptrdiff_t>( handle ) );

	switch( cmd ) {
	case C_PUSH:
		command.arg.value.Get( Value::V_MAX, temp[0] );
		PushResult();
		break;

	case C_POP:
		PopArguments( 1 );
		break;

	case C_TOP:
		TopArgument();
		msg( E_INFO, E_VERBOSE, "Value on top (integer): %ld", temp[0] );
		break;

	case C_LOAD:
		ReadArgument( command.arg.ref );
		PushResult();
		break;

	case C_STORE:
		PopArguments( 1 );
		WriteResult( command.arg.ref );
		break;

	case C_ABS:
		PopArguments( 1 );
		temp[0] = labs( temp[0] );
		PushResult();
		break;

	case C_ADD:
		PopArguments( 2 );
		temp[0] += temp[1];
		PushResult();
		break;

	case C_SUB: /* top is subtrahend */
		PopArguments( 2 );
		temp[0] = temp[1] - temp[0];
		PushResult();
		break;

	case C_MUL:
		PopArguments( 2 );
		temp[0] *= temp[1];
		PushResult();
		break;

	case C_DIV: /* top is divisor */
		PopArguments( 2 );
		temp[0] = temp[1] / temp[0];
		PushResult();
		break;

	case C_MOD:
		PopArguments( 2 );
		temp[0] = temp[1] % temp[0];
		PushResult();
		break;

	case C_INC:
		PopArguments( 1 );
		++temp[0];
		PushResult();
		break;

	case C_DEC:
		PopArguments( 1 );
		--temp[0];
		PushResult();
		break;

	case C_NEG:
		PopArguments( 1 );
		temp[0] = -temp[0];
		PushResult();
		break;

	case C_ANAL:
		TopArgument();
		need_to_analyze = 1;
		break;

	case C_CMP: /* second operand is compared against stack top */
		PopArguments( 1 );
		temp[1] = temp[0];
		TopArgument();
		temp[0] -= temp[1];
		need_to_analyze = 1;
		break;

	case C_SWAP:
		PopArguments( 2 );
		PushResult();
		temp[0] = temp[1];
		PushResult();
		break;

	case C_DUP:
		TopArgument();
		PushResult();
		break;

	case C_MAX:
	default:
		casshole( "Switch error" );
		break;
	}

	AttemptAnalyze();
}

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
