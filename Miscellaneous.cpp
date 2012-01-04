#include "stdafx.h"
#include "Interfaces.h"

namespace Processor
{

	namespace ProcDebug
	{
		const char* FileSectionType_ids[SEC_MAX] =
		{
			"non-uniform",
			"symbol map",
			"text image"
			"data image",
			"stack image",
			"invalid type"
		};

		const char* AddrType_ids[S_MAX] =
		{
			"invalid type",
			"code",
			"data",
			"register",
			"stack frame",
			"parameter"
		};

		char debug_buffer[STATIC_LENGTH];

		void PrintReference (const Reference& ref)
		{
			if (ref.is_symbol)
				snprintf (debug_buffer, STATIC_LENGTH, "symbol (hash %p)", ref.symbol_hash);

			else
			{
				__sassert (ref.direct.type < S_MAX, "Invalid reference type: %zu", ref.direct.type);

				snprintf (debug_buffer, STATIC_LENGTH, "direct %s address %zu",
						  AddrType_ids[ref.direct.type], ref.direct.address);
			}
		}
	}

	symbol_map::value_type PrepareSymbol (const char* label, Symbol sym, size_t* hash)
	{
		std::string prep_label (label);
		size_t prep_hash = std::hash<std::string>() (prep_label);

		sym.hash = prep_hash;

		if (hash)
			*hash = prep_hash;

		return std::make_pair (prep_hash, std::make_pair (std::move (label), sym));
	}

	abiret_t ICommandSet::ExecFallbackCallback (Processor::Command* cmd)
	{
		ICommandSet* cset = default_exec_ ->GetProcessor() ->CommandSet();
		void* handle = cset ->GetExecutionHandle (cmd ->id, default_exec_ ->ID());
// 		__sassert (handle, "Invalid handle for command \"%s\"", cset ->DecodeCommand (cmd ->id).mnemonic);

		default_exec_ ->Execute (handle, cmd ->arg);
		calc_t temporary_result = default_exec_ ->GetProcessor() ->LogicProvider() ->StackPop();
		abiprep_t prep_result = temporary_result;

		return *reinterpret_cast<abiret_t*> (&prep_result);
	}

	void IMMU::SetTemporaryContext (size_t ctx_id)
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Setting up temporary context [buffer %zu depth %zu] -> %zu",
			 GetContext().buffer, GetContext().depth, ctx_id);

		SaveContext();
		ClearContext();
		GetContext().buffer = ctx_id;
		ResetBuffers (ctx_id);
	}
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;