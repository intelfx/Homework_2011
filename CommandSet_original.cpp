#include "stdafx.h"
#include "CommandSet_original.h"

#include <uXray/fxhash_functions.h>

// -----------------------------------------------------------------------------
// Library		Homework
// File			CommandSet_original.cpp
// Author		intelfx
// Description	mkI command set implementation.
// -----------------------------------------------------------------------------

namespace ProcessorImplementation
{
	using namespace Processor;

	CommandSet_mkI::~CommandSet_mkI() = default;
	CommandSet_mkI::CommandSet_mkI() :
	by_id()
	{
	}

	void CommandSet_mkI::OnAttach()
	{
		ResetCommandSet();
	}

	void CommandSet_mkI::ResetCommandSet()
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Resetting command set: using mkI");
		by_id.clear();

		for (const ICD* dsc = initial_commands; dsc ->name; ++dsc)
		{
			CommandTraits traits { dsc ->name,
								   dsc ->description,
								   dsc ->arg_type,
								   dsc ->is_service_command,
								   get_id (dsc ->name)
								 };

			msg (E_INFO, E_DEBUG, "mkI command: \"%s\" -> %u", dsc ->name, traits.id);
			auto byid_ins_res = by_id.insert (std::make_pair (traits.id, std::move (traits)));
			__assert (byid_ins_res.second, "Internal inconsistency on \"%s\"", dsc ->name);
		}

		msg (E_INFO, E_DEBUG, "Successfully added %zu commands", by_id.size());
	}

	void CommandSet_mkI::AddCommand (Processor::CommandTraits&& command)
	{
		verify_method;

		command.id = get_id (command.mnemonic);

		msg (E_INFO, E_DEBUG, "Adding custom command: \"%s\" (\"%s\") -> # %u",
			 command.mnemonic, command.description, command.id);

		auto byid_ins_res = by_id.insert (std::make_pair (command.id, std::move (command)));
		__assert (byid_ins_res.second, "Command already exists: \"%s\"", command.mnemonic);
	}

	void CommandSet_mkI::AddCommandImplementation (const char* mnemonic, size_t module, void* handle)
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Registering implementation driver for command \"%s\" -> module %zx",
			 mnemonic, module);

		__assert (mnemonic, "NULL mnemonic");
		cid_t cmd_id = get_id (mnemonic);
		auto cmd_it = by_id.find (cmd_id);

		if (cmd_it == by_id.end())
		{
			msg (E_WARNING, E_VERBOSE, "Registering implementation driver for invalid mnemonic: \"%s\"", mnemonic);
			return;
		}

		auto impl_ins_res = cmd_it ->second.execution_handles.insert (std::make_pair (module, handle));
		__assert (impl_ins_res.second, "Implementation of \"%s\" -> module %zx has already been registered",
				  mnemonic, module);
	}

	void* CommandSet_mkI::GetExecutionHandle (cid_t id, size_t module)
	{
		const CommandTraits& cmd = DecodeCommand (id);
		return GetExecutionHandle (cmd, module);
	}

	void* CommandSet_mkI::GetExecutionHandle (const CommandTraits& cmd, size_t module)
	{
		auto impl_it = cmd.execution_handles.find (module);

		if (impl_it == cmd.execution_handles.end())
			return 0;

		return impl_it ->second;
	}

	const CommandTraits& CommandSet_mkI::DecodeCommand (const char* mnemonic) const
	{
		__assert (mnemonic, "NULL mnemonic");
		cid_t cmd_id = get_id (mnemonic);
		auto cmd_it = by_id.find (cmd_id);
		__assert (cmd_it != by_id.end(), "Invalid or unregistered mnemonic: \"%s\"", mnemonic);

		return cmd_it ->second;
	}

	const CommandTraits& CommandSet_mkI::DecodeCommand (cid_t id) const
	{
		auto cmd_it = by_id.find (id);
		__assert (cmd_it != by_id.end(), "Invalid or unregistered ID: %hhu", id);

		return cmd_it ->second;
	}

	const CommandSet_mkI::ICD CommandSet_mkI::initial_commands[] =
	{
		{
			"init",
			"Stack: initialize stack and execution environment",
			A_NONE,
			1
		},
		{
			"push",
			"Stack: push a value onto the stack",
			A_VALUE,
			0
		},
		{
			"pop",
			"Stack: remove a value from the stack",
			A_NONE,
			0
		},
		{
			"top",
			"Stack: peek a value from the stack",
			A_NONE,
			0
		},
		{
			"sleep",
			"System: hand off control to the OS",
			A_NONE,
			1
		},
		{
			"ld",
			"Data: read (load) data memory/register",
			A_REFERENCE,
			0
		},
		{
			"st",
			"Data: write (store) data memory/register",
			A_REFERENCE,
			0
		},
		{
			"force_ld",
			"Data: read (load) data memory/register, forcing type cast",
			A_REFERENCE,
			0
		},
		{
			"force_st",
			"Data: write (store) data memory/register, forcing type cast",
			A_REFERENCE,
			0
		},
		{
			"add",
			"Arithmetic: addition",
			A_NONE,
			0
		},
		{
			"sub",
			"Arithmetic: subtraction (minuend on top)",
			A_NONE,
			0
		},
		{
			"mul",
			"Arithmetic: multiplication",
			A_NONE,
			0
		},
		{
			"div",
			"Arithmetic: division (dividend on top)",
			A_NONE,
			0
		},
		{
			"mod",
			"Arithmetic: modulo (dividend on top)",
			A_NONE,
			0
		},
		{
			"inc",
			"Arithmetic: increment by one",
			A_NONE,
			0
		},
		{
			"dec",
			"Arithmetic: decrement by one",
			A_NONE,
			0
		},
		{
			"neg",
			"Arithmetic: negation",
			A_NONE,
			0
		},
		{
			"sqrt",
			"Arithmetic: square root extraction",
			A_NONE,
			0
		},
		{
			"anal",
			"Accumulator: analyze top of the stack",
			A_NONE,
			0
		},
		{
			"cmp",
			"Accumulator: compare two values on the stack (R/O subtraction)",
			A_NONE,
			0
		},
		{
			"swap",
			"Accumulator: swap two values on the stack",
			A_NONE,
			0
		},
		{
			"dup",
			"Accumulator: duplicate the value on the stack",
			A_NONE,
			0
		},
		{
			"je",
			"Address: jump if ZERO",
			A_REFERENCE,
			1
		},
		{
			"jne",
			"Address: jump if not ZERO",
			A_REFERENCE,
			1
		},
		{
			"ja",
			"Address: jump if ABOVE",
			A_REFERENCE,
			1
		},
		{
			"jna",
			"Address: jump if not ABOVE",
			A_REFERENCE,
			1
		},
		{
			"jae",
			"Address: jump if ABOVE or EQUAL",
			A_REFERENCE,
			1
		},
		{
			"jnae",
			"Address: jump if not ABOVE or EQUAL",
			A_REFERENCE,
			1
		},
		{
			"jb",
			"Address: jump if BELOW",
			A_REFERENCE,
			1
		},
		{
			"jnb",
			"Address: jump if not BELOW",
			A_REFERENCE,
			1
		},
		{
			"jbe",
			"Address: jump if BELOW or EQUAL",
			A_REFERENCE,
			1
		},
		{
			"jnbe",
			"Address: jump if not BELOW or EQUAL",
			A_REFERENCE,
			1
		},
		{
			"jmp",
			"Address: unconditional jump",
			A_REFERENCE,
			1
		},
		{
			"call",
			"Address: unconditional call",
			A_REFERENCE,
			1
		},
		{
			"ret",
			"Address: unconditional return",
			A_NONE,
			1
		},
		{
			"dump",
			"System: dump context",
			A_NONE,
			1
		},
		{
			"snfc",
			"Flags: set No-Flag-Change flag",
			A_NONE,
			1
		},
		{
			"cnfc",
			"Flags: clear No-Flag-Change flag",
			A_NONE,
			1
		},
		{
			"exec",
			"Management: execute next context buffer",
			A_NONE,
			1
		},
		{
			"quit",
			"Management: stop execution/quit context",
			A_NONE,
			1
		},
		{
			0,
			0,
			A_NONE,
			0
		}
	};
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
