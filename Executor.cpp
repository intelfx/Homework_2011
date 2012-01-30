#include "stdafx.h"
#include "Executor.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Executor.cpp
// Author		intelfx
// Description	Main virtual executor implementation
// -----------------------------------------------------------------------------

ImplementDescriptor (FloatExecutor, "floating-point executor", MOD_APPMODULE);

namespace ProcessorImplementation
{
	using namespace Processor;

	enum COMMANDS
	{
		C_PUSH = 1,
		C_POP,
		C_TOP,

		C_LOAD,
		C_STORE,

		C_FLOAD,
		C_FSTORE,

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

		C_MAX
	};

	const char* FloatExecutor::supported_mnemonics[C_MAX] =
	{
		0,
		"push",
		"pop",
		"top",

		"ld",
		"st",

		"force_ld",
		"force_st",

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
		"dup"
	};

	FloatExecutor::FloatExecutor() = default;
	FloatExecutor::~FloatExecutor() = default;

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
		ICommandSet* cmdset = proc_ ->CommandSet();

		for (size_t cmd = 1; cmd < C_MAX; ++cmd)
		{
			cmdset ->AddCommandImplementation (supported_mnemonics[cmd], ID(),
											   reinterpret_cast<void*> (cmd));
		}
	}

	inline void FloatExecutor::AttemptAnalyze()
	{
		if (need_to_analyze)
			proc_ ->LogicProvider() ->Analyze (temp[0]);
	}

	inline void FloatExecutor::PopArguments (size_t count)
	{
		__assert (count < temp_size, "Too much arguments for temporary processing");

		for (size_t i = 0; i < count; ++i)
			proc_ ->LogicProvider() ->StackPop().Get (temp[i]);
	}

	inline void FloatExecutor::TopArgument()
	{
		proc_ ->LogicProvider() ->StackTop().Get (temp[0]);
	}

	inline void FloatExecutor::ReadArgument (Reference& ref)
	{
		proc_ ->LogicProvider() ->Read (ref).Get (temp[0]);
	}

	inline void FloatExecutor::PushResult()
	{
		proc_ ->LogicProvider() ->StackPush (temp[0]);
	}

	inline void FloatExecutor::WriteResult (Reference& ref)
	{
		proc_ ->LogicProvider() ->Write (ref, temp[0]);
	}

	void FloatExecutor::Execute (void* handle, Command::Argument& argument)
	{
		need_to_analyze	= !(proc_ ->MMU() ->GetContext().flags & MASK (F_NFC));

		COMMANDS cmd = static_cast<COMMANDS> (reinterpret_cast<ptrdiff_t> (handle));
		switch (cmd)
		{
		case C_PUSH:
			argument.value.Get (temp[0]);
			PushResult();
			break;

		case C_POP:
			PopArguments (1);
			msg (E_INFO, E_DEBUG, "Popped value: %lg", temp[0]);
			break;

		case C_TOP:
			TopArgument();
			msg (E_INFO, E_VERBOSE, "Value on top: %lg", temp[0]);
			break;

		case C_FLOAD:
			/* force read value to floating-point */
			temp[0] = proc_ ->LogicProvider() ->Read (argument.ref).fp;

			PushResult();
			break;

		case C_LOAD:
			ReadArgument (argument.ref);
			PushResult();
			break;

		case C_FSTORE:
		{
			PopArguments (1);

			/* retain written value at its original type */
			calc_t forged_value (temp[0]);
			forged_value.type = proc_ ->LogicProvider() ->Read (argument.ref).type;

			proc_ ->LogicProvider() ->Write (argument.ref, forged_value);
			break;
		}

		case C_STORE:
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
			need_to_analyze = 1;
			break;

		case C_CMP: /* second operand is compared against stack top */
			PopArguments (1);
			temp[1] = temp[0];
			TopArgument();
			temp[0] -= temp[1];
			need_to_analyze = 1;
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

		case C_MAX:
		default:
			__asshole ("Switch error");
			break;
		}

		AttemptAnalyze();
	}

}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
