#include "stdafx.h"
#include "Executor.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Executor.cpp
// Author		intelfx
// Description	Main virtual executor implementation
// -----------------------------------------------------------------------------

namespace ProcessorImplementation
{
	using namespace Processor;

	enum COMMANDS
	{
		C_INIT = 0,
		C_PUSH,
		C_POP,
		C_TOP,

		C_SLEEP,

		C_LOAD,
		C_WRITE,

		C_ADD,
		C_SUB,
		C_MUL,
		C_DIV,

		C_INC,
		C_DEC,

		C_NEG,
		C_SQRT,

		C_ANAL,
		C_CMP,
		C_SWAP,
		C_DUP,

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

		C_JMP,
		C_CALL,
		C_RET,

		C_DFL,
		C_SNFC,
		C_CNFC,

		C_EXEC,
		C_QUIT,

		C_MAX
	};

	const char* Executor::supported_mnemonics[C_MAX] =
	{
		"init",
		"push",
		"pop",
		"top",

		"sleep",

		"ld",
		"wr",

		"add",
		"sub",
		"mul",
		"div",

		"inc",
		"dec",

		"neg",
		"sqrt",

		"anal", // Fuck yeah
		"cmp",
		"swap",
		"dup",

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

		"jmp",
		"call",
		"ret",

		"dumpfl",
		"snfc",
		"cnfc",

		"exec",
		"quit"
	};

	void Executor::OnAttach()
	{
		ResetImplementations();
	}

	Value::Type Executor::SupportedType() const
	{
		return Value::V_FLOAT;
	}

	void Executor::ResetImplementations()
	{
		ICommandSet* cmdset = proc_ ->CommandSet();

		for (size_t cmd = 0; cmd < C_MAX; ++cmd)
		{
			cmdset ->AddCommandImplementation (supported_mnemonics[cmd], ID(),
											   reinterpret_cast<void*> (cmd));
		}
	}

	inline void Executor::AttemptAnalyze()
	{
		if (temp_flags.analyze_always)
			proc_ ->LogicProvider() ->Analyze (temp[0]);
	}

	inline void Executor::PopArguments (size_t count)
	{
		__assert (count < temp_size, "Too much arguments for temporary processing");

		for (size_t i = 0; i < count; ++i)
			proc_ ->LogicProvider() ->StackPop().Get (temp[i]);
	}

	inline void Executor::TopArgument()
	{
		proc_ ->LogicProvider() ->StackTop().Get (temp[0]);
	}

	inline void Executor::ReadArgument (Reference& ref)
	{
		proc_ ->LogicProvider() ->Read (ref).Get (temp[0]);
	}

	inline void Executor::PushResult()
	{
		proc_ ->LogicProvider() ->StackPush (temp[0]);
	}

	inline void Executor::WriteResult (Reference& ref)
	{
		proc_ ->LogicProvider() ->Write (ref, temp[0]);
	}

	inline void Executor::RetrieveFlags()
	{
		size_t flags = proc_ ->MMU() ->GetContext().flags;

		temp_flags.zero				= flags & MASK (F_ZERO);
		temp_flags.negative			= flags & MASK (F_NEGATIVE);
		temp_flags.invalid_fp		= flags & MASK (F_INVALIDFP);
		temp_flags.analyze_always	= !(flags & MASK (F_NFC));
	}

	void Executor::Execute (void* handle, Command::Argument& argument)
	{
		RetrieveFlags();

		COMMANDS cmd = static_cast<COMMANDS> (reinterpret_cast<ptrdiff_t> (handle));
		switch (cmd)
		{
		case C_INIT:
			proc_ ->MMU() ->ResetEverything();
			proc_ ->MMU() ->AllocContextBuffer();
			break;

		case C_PUSH:
			argument.value.Get (temp[0]);
			PushResult();
			break;

		case C_POP:
			PopArguments (1);
			msg (E_INFO, E_VERBOSE, "Popped value: %lg", temp[0]);
			break;

		case C_TOP:
			TopArgument();
			msg (E_INFO, E_VERBOSE, "Value on top: %lg", temp[0]);
			break;

		case C_SLEEP:
			sleep (0);
			break;

		case C_LOAD:
			ReadArgument (argument.ref);
			PushResult();
			break;

		case C_WRITE:
			PopArguments (1);
			WriteResult (argument.ref);
			break;

		case C_ADD:
			PopArguments (2);
			temp[0] += temp[1];
			PushResult();
			break;

		case C_SUB: /* top is subtrahend */
			PopArguments (2);
			temp[0] = temp[1] - temp[0];
			PushResult();
			break;

		case C_MUL:
			PopArguments (2);
			temp[0] *= temp[1];
			PushResult();
			break;

		case C_DIV: /* top is divisor */
			PopArguments (2);
			temp[0] = temp[1] / temp[0];
			PushResult();
			break;

		case C_INC:
			PopArguments (1);
			++temp[0];
			PushResult();
			break;

		case C_DEC:
			PopArguments (1);
			--temp[0];
			PushResult();
			break;

		case C_NEG:
			PopArguments (1);
			temp[0] = -temp[0];
			PushResult();
			break;

		case C_SQRT:
			PopArguments (1);
			temp[0] = sqrt (temp[0]);
			PushResult();
			break;

		case C_ANAL:
			TopArgument();
			temp_flags.analyze_always = 1;
			break;

		case C_CMP: /* second operand is compared against stack top */
			PopArguments (1);
			temp[1] = temp[0];
			TopArgument();
			temp[0] -= temp[1];
			temp_flags.analyze_always = 1;
			break;

		case C_SWAP:
			PopArguments (2);
			proc_ ->LogicProvider() ->StackPush (temp[0]);
			temp[0] = temp[1];
			PushResult();
			break;

		case C_DUP:
			TopArgument();
			PushResult();
			break;

		case C_JNE:
			if (!temp_flags.zero)
				proc_ ->LogicProvider() ->Jump (argument.ref);

			break;

		case C_JE:
			if (temp_flags.zero)
				proc_ ->LogicProvider() ->Jump (argument.ref);

			break;

		case C_JA:
		case C_JNBE:
			if (!temp_flags.zero && !temp_flags.negative)
				proc_ ->LogicProvider() ->Jump (argument.ref);

			break;

		case C_JNA:
		case C_JBE:
			if (temp_flags.zero || temp_flags.negative)
				proc_ ->LogicProvider() ->Jump (argument.ref);

			break;

		case C_JAE:
		case C_JNB:
			if (!temp_flags.negative)
				proc_ ->LogicProvider() ->Jump (argument.ref);

			break;

		case C_JNAE:
		case C_JB:
			if (temp_flags.negative)
				proc_ ->LogicProvider() ->Jump (argument.ref);

			break;

		case C_JMP:
			proc_ ->LogicProvider() ->Jump (argument.ref);
			break;

		case C_CALL:
			proc_ ->MMU() ->SaveContext();
			proc_ ->LogicProvider() ->Jump (argument.ref);
			break;

		case C_RET:
			proc_ ->MMU() ->RestoreContext();
			break;

		case C_DFL:
			proc_ ->MMU() ->DumpContext();
			break;

		case C_SNFC:
			proc_ ->MMU() ->GetContext().flags |=  MASK (F_NFC);
			break;

		case C_CNFC:
			proc_ ->MMU() ->GetContext().flags &= ~MASK (F_NFC);
			break;

		case C_EXEC:
			proc_ ->MMU() ->NextContextBuffer();
			proc_ ->Exec();
			break;

		case C_QUIT:
			proc_ ->MMU() ->GetContext().flags |= MASK (F_EXIT);
			break;

		case C_MAX:
		default:
			__asshole ("Switch error");
			break;
		}

		AttemptAnalyze();
	}

}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
