#include "stdafx.h"
#include "Executor.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Executor.cpp
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Floating-point interpreter plugin implementation.
// -------------------------------------------------------------------------------------

ImplementDescriptor( FloatExecutor, "floating-point executor", MOD_APPMODULE )

namespace ProcessorImplementation
{
using namespace Processor;

enum COMMANDS {
	C_PUSH = 1,
	C_POP,
	C_TOP,

	C_LOAD,
	C_STORE,
	C_LDINT,
	C_STINT,

	C_ABS,
	C_ADD,
	C_SUB,
	C_MUL,
	C_DIV,

	C_INC,
	C_DEC,

	C_NEG,
	C_SQRT,

	C_SIN,
	C_COS,

	C_ANAL,
	C_CMP,
	C_SWAP,
	C_DUP,

	C_MAX
};

const char* FloatExecutor::supported_mnemonics[C_MAX] = {
	nullptr,
	"push",
	"pop",
	"top",

	"ld",
	"st",
	"ldint",
	"stint",

	"abs",
	"add",
	"sub",
	"mul",
	"div",

	"inc",
	"dec",

	"neg",
	"sqrt",

	"sin",
	"cos",

	"anal",
	"cmp",
	"swap",
	"dup"
};

void FloatExecutor::OnAttach()
{
	ResetImplementations();
}

Value::Type FloatExecutor::SupportedType() const
{
	return Value::V_FLOAT;
}

void FloatExecutor::ResetImplementations()
{
	ICommandSet* cmdset = proc_->CommandSet();

	for( size_t cmd = 1; cmd < C_MAX; ++cmd ) {
		cmdset->AddCommandImplementation( supported_mnemonics[cmd], ID(),
		                                  reinterpret_cast<void*>( cmd ) );
	}
}

inline void FloatExecutor::AttemptAnalyze()
{
	if( need_to_analyze )
		proc_->LogicProvider()->Analyze( temp[0] );
}

inline void FloatExecutor::PopArguments( size_t count )
{
	cassert( count < temp_size, "Too much arguments for temporary processing" );

	for( size_t i = 0; i < count; ++i )
		proc_->LogicProvider()->StackPop().Get( Value::V_FLOAT, temp[i] );
}

inline void FloatExecutor::TopArgument()
{
	proc_->LogicProvider()->StackTop().Get( Value::V_FLOAT, temp[0] );
}

inline void FloatExecutor::ReadArgument( Reference& ref, Value::Type type )
{
	proc_->LogicProvider()->Read( proc_->Linker()->Resolve( ref ) ).Get( type, temp[0] );
}

inline void FloatExecutor::PushResult()
{
	proc_->LogicProvider()->StackPush( temp[0] );
}

inline void FloatExecutor::WriteResult( Reference& ref )
{
	proc_->LogicProvider()->Write( proc_->Linker()->Resolve( ref ), temp[0] );
}

inline void FloatExecutor::WriteIntResult( Reference& ref )
{
	proc_->LogicProvider()->Write( proc_->Linker()->Resolve( ref ), static_cast<int_t>( temp[0] ) );
}

void FloatExecutor::Execute( void* handle, Command& command )
{
	need_to_analyze	= !( proc_->CurrentContext().flags & MASK( F_NFC ) );

	COMMANDS cmd = static_cast<COMMANDS>( reinterpret_cast<ptrdiff_t>( handle ) );

	switch( cmd ) {
	case C_PUSH:
		command.arg.value.Get( Value::V_FLOAT, temp[0] ); // not expecting type here
		PushResult();
		break;

	case C_POP:
		PopArguments( 1 );
		break;

	case C_TOP:
		TopArgument();
		msg( E_INFO, E_VERBOSE, "Value on top (float): %lg", temp[0] );
		break;

	case C_LOAD:
		ReadArgument( command.arg.ref );
		PushResult();
		break;

	case C_STORE:
		PopArguments( 1 );
		WriteResult( command.arg.ref );
		break;

	case C_LDINT:
		ReadArgument( command.arg.ref, Value::V_INTEGER );
		PushResult();
		break;

	case C_STINT:
		PopArguments( 1 );
		WriteIntResult( command.arg.ref );
		break;

	case C_ABS:
		PopArguments( 1 );
		temp[0] = fabs( temp[0] );
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

	case C_SQRT:
		PopArguments( 1 );
		temp[0] = sqrt( temp[0] );
		PushResult();
		break;

	case C_SIN:
		PopArguments( 1 );
		temp[0] = sin( temp[0] );
		PushResult();
		break;

	case C_COS:
		PopArguments( 1 );
		temp[0] = cos( temp[0] );
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
