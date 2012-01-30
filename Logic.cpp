#include "stdafx.h"
#include "Logic.h"

#include <uXray/fxhash_functions.h>
#include <math.h>

namespace ProcessorImplementation
{
	using namespace Processor;

	void Logic::ExecuteSingleCommand (Command& command)
	{
		verify_method;

		if (!command.cached_executor || !command.cached_handle)
		{
			// Perform caching of executor/handle since execution must be O(1)

			ICommandSet* command_set = proc_ ->CommandSet();
			const CommandTraits& command_traits = command_set ->DecodeCommand (command.id);

			// Select valid executor based on command flavor
			if (command_traits.is_service_command)
				command.cached_executor = proc_ ->Executor (Value::V_MAX);

			else
				command.cached_executor = proc_ ->Executor (command.type);

			__assert (command.cached_executor, "Was unable to select executor for command type \"%s\"",
					  ProcDebug::ValueType_ids[command.type]);

			command.cached_handle = command_set ->GetExecutionHandle (command_traits, command.cached_executor ->ID());
			__assert (command.cached_handle,
					  "Was unable to retrieve exec handle for command \"%s\" [type \"%s\" execid %zx]",
					  command_traits.mnemonic,
					  ProcDebug::ValueType_ids[command.type],
					  command.cached_executor ->ID());
		}

		IMMU* mmu = proc_ ->MMU();

		mmu ->GetContext().flags &= ~MASK (F_WAS_JUMP);
		mmu ->SelectStack (command.type);

		command.cached_executor ->Execute (command.cached_handle, command.arg);
	}

	size_t Logic::ChecksumState()
	{
		verify_method;

		size_t checksum = 0;

		/*
		 * Checksum contains:
		 * - current context
		 * - current code image
		 * - current symbol map
		 */

		IMMU* mmu = proc_ ->MMU();

		Context& ctx = mmu ->GetContext();
		checksum = hasher_xroll (&ctx, sizeof (ctx), checksum);

		size_t text_length = mmu ->GetTextSize();
		while (text_length--)
		{
			checksum = hasher_xroll (&mmu ->ACommand (text_length), sizeof (Command), checksum);
		}

		// TODO hash symbol map
		return checksum;
	}

	void Logic::Analyze (Processor::calc_t value)
	{
		verify_method;

		Context& ctx = proc_ ->MMU() ->GetContext();
		ctx.flags &= ~(MASK (F_ZERO) | MASK (F_NEGATIVE) | MASK (F_INVALIDFP));

		switch (value.type)
		{
		case Value::V_FLOAT:
		{
			int classification = fpclassify (value.fp);
			switch (classification)
			{
			case FP_NAN:
			case FP_INFINITE:
				ctx.flags |= MASK (F_INVALIDFP);
				break;

			case FP_ZERO:
				ctx.flags |= MASK (F_ZERO);
				break;

			case FP_SUBNORMAL:
			case FP_NORMAL:
				if (value.fp < 0)
					ctx.flags |= MASK (F_NEGATIVE);
				break;

			default:
				__asshole ("fpclassify() error - returned %d", classification);
				break;
			}

			break;
		}

		case Value::V_INTEGER:
		{
			if (value.integer == 0)
				ctx.flags |= MASK (F_ZERO);

			else if (value.integer < 0)
				ctx.flags |= MASK (F_NEGATIVE);

			break;
		}

		case Value::V_MAX:
			__asshole ("Uninitialised value");
			break;

		default:
			__asshole ("Switch error");
			break;
		}
	}

	Processor::Register Logic::DecodeRegister (const char* reg)
	{
		verify_method;
		__assert (reg && reg[0], "Invalid or NULL register string to decode");

		for (unsigned rid = R_A; rid < R_MAX; ++rid)
			if (!strcmp (reg, RegisterIDs[rid]))
				return static_cast<Register> (rid);

		__asshole ("Unknown register string: \"%s\"", reg);
		return R_MAX; /* for GCC to be quiet */
	}

	const char* Logic::EncodeRegister (Register reg)
	{
		verify_method;

		__assert (reg < R_MAX, "Invalid register ID to encode: %d", reg);
		return RegisterIDs[reg];
	}

	void Logic::Jump (Processor::Reference& ref)
	{
		verify_method;

		Reference::Direct dref = proc_ ->Linker() ->Resolve (ref);
		__verify (dref.type == S_CODE, "Cannot jump to non-CODE reference to %s",
				  (ProcDebug::PrintReference (ref), ProcDebug::debug_buffer));

		msg (E_INFO, E_DEBUG, "Jumping -> %zu", dref.address);

		Context& ctx = proc_ ->MMU() ->GetContext();

		ctx.ip = dref.address;
		ctx.flags |= MASK (F_WAS_JUMP);
 }

	void Logic::Write (Reference& ref, calc_t value)
	{
		verify_method;

		Reference::Direct dref = proc_ ->Linker() ->Resolve (ref);
		switch (dref.type)
		{
		case S_CODE:
			msg (E_CRITICAL, E_VERBOSE, "Attempt to write to CODE section: reference to %s",
				 (ProcDebug::PrintReference (ref), ProcDebug::debug_buffer));
			msg (E_CRITICAL, E_VERBOSE, "Write will not be performed");
			break;

		case S_DATA:
			proc_ ->MMU() ->AData (dref.address) = value;
			break;

		case S_FRAME:
			proc_ ->MMU() ->AStackFrame (dref.address) = value;

		case S_FRAME_BACK:
			msg (E_WARNING, E_VERBOSE, "Attempt to write to function parameter: reference to %s",
				 (ProcDebug::PrintReference (ref), ProcDebug::debug_buffer));

			proc_ ->MMU() ->AStackFrame (-dref.address) = value;
			break;

		case S_REGISTER:
			proc_ ->MMU() ->ARegister (static_cast<Register> (dref.address)) = value;
			break;

		case S_BYTEPOOL:
			__assert (value.type == Value::V_INTEGER, "Attempt to write non-integer to the byte pool at address %zx", dref.address);
			*proc_ ->MMU() ->ABytepool (dref.address) = reinterpret_cast<char&> (value.integer); // truncation
			break;

		case S_NONE:
		case S_MAX:
		default:
			__asshole ("Invalid address type or switch error");
			break;
		}
	}

	Processor::calc_t Logic::Read (Reference& ref)
	{
		verify_method;

		Reference::Direct dref = proc_ ->Linker() ->Resolve (ref);
		switch (dref.type)
		{
		case S_CODE:
			msg (E_WARNING, E_VERBOSE, "Attempt to read from CODE section: reference to %s",
				 (ProcDebug::PrintReference (ref), ProcDebug::debug_buffer));
			return static_cast<int_t> (proc_ ->MMU() ->ACommand (dref.address).id);

		case S_DATA:
			return proc_ ->MMU() ->AData (dref.address);

		case S_FRAME:
			return proc_ ->MMU() ->AStackFrame (dref.address);

		case S_FRAME_BACK:
			return proc_ ->MMU() ->AStackFrame (-dref.address);

		case S_REGISTER:
			return proc_ ->MMU() ->ARegister (static_cast<Register> (dref.address));

		case S_BYTEPOOL:
			return static_cast<int_t> (*proc_ ->MMU() ->ABytepool (dref.address));

		case S_NONE:
		case S_MAX:
		default:
			__asshole ("Invalid address type or switch error");
			break;
		}

		return strtod ("NAN", 0); /* for GCC not to complain */
	}

	Processor::calc_t Logic::StackPop()
	{
		verify_method;

		IMMU* mmu = proc_ ->MMU();

		calc_t data = mmu ->AStackTop (0);
		mmu ->AlterStackTop (-1);

		verify_method;
		return data;
	}

	void Logic::StackPush (calc_t value)
	{
		verify_method;

		IMMU* mmu = proc_ ->MMU();

		mmu ->AlterStackTop (1);
		mmu ->AStackTop (0) = value;

		verify_method;
	}

	calc_t Logic::StackTop()
	{
		verify_method;
		return proc_ ->MMU() ->AStackTop (0);
	}

	void Logic::Syscall (size_t index)
	{
		msg (E_INFO, E_VERBOSE, "System call with index %zu", index);

		switch (index)
		{
			case 0:
			{
				calc_t input_data = proc_ ->MMU() ->ARegister (R_F);
				__assert (input_data.type == Value::V_INTEGER, "Non-integer value in register $rf");

				msg (E_INFO, E_USER, "Application output: \"%s\"", proc_ ->MMU() ->ABytepool (input_data.integer));
				break;
			}

			case 1:
			{
				msg (E_INFO, E_USER, "Reading integer from command-line");
				char buffer[STATIC_LENGTH];
				gets (buffer);
				StackPush (static_cast<int_t> (atoi (buffer)));
			}

			default:
				msg (E_WARNING, E_VERBOSE, "Undefined syscall index: %zu", index);
				break;
		}
	}

	const char* Logic::RegisterIDs[R_MAX] =
	{
		"ra",
		"rb",
		"rc",
		"rd",
		"re",
		"rf"
	};
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
