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
			"parameter",
			"byte-granular pool"
		};

		const char* ValueType_ids[Value::V_MAX + 1] =
		{
			"integer",
			"floating-point",
			"undefined"
		};

		char debug_buffer[STATIC_LENGTH];

		void PrintReference (const Reference& ref, IMMU* mmu)
		{
			switch (ref.type)
			{
			case Reference::RT_SYMBOL:
			{
				if (!mmu)
					snprintf (debug_buffer, STATIC_LENGTH, "symbol (hash %zx + offset %zu)",
							ref.symbol.hash, ref.symbol.offset);

				else
					snprintf (debug_buffer, STATIC_LENGTH, "symbol %s+%zu",
							  mmu ->ASymbol (ref.symbol.hash).first.c_str(), ref.symbol.offset);
			}

				break;

			case Reference::RT_DIRECT:
				__sassert (ref.plain.type < S_MAX, "Invalid reference type: %zu", ref.plain.type);

				snprintf (debug_buffer, STATIC_LENGTH, "direct %s address %zu",
						  AddrType_ids[ref.plain.type], ref.plain.address);

				break;

			case Reference::RT_INDIRECT:
				__sassert (ref.plain.type < S_MAX, "Invalid reference type: %zu", ref.plain.type);

				snprintf (debug_buffer, STATIC_LENGTH, "indirect %s",
						  AddrType_ids[ref.plain.type]);

				break;

			default:
				__sasshole ("Switch error");
				break;
			}
		}


		void PrintValue (const Value& val)
		{
			switch (val.type)
			{
			case Value::V_INTEGER:
				snprintf (debug_buffer, STATIC_LENGTH, "int:%ld", val.integer);
				break;

			case Value::V_FLOAT:
				snprintf (debug_buffer, STATIC_LENGTH, "float:%lg", val.fp);
				break;

			case Value::V_MAX:
				snprintf (debug_buffer, STATIC_LENGTH, "<uninitialised>");
				break;

			default:
				__sasshole ("Switch error");
				break;
			}
		}

		void PrintArgument (ArgumentType arg_type, const Command::Argument& argument, IMMU* mmu)
		{
			switch (arg_type)
			{
			case A_NONE:
				strncpy (debug_buffer, "no argument", STATIC_LENGTH);
				break;

			case A_REFERENCE:
				PrintReference (argument.ref, mmu);
				break;

			case A_VALUE:
				PrintValue (argument.value);
				break;

			default:
				__sasshole ("Switch error");
				break;
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

	typedef abiret_t(*abi_gate_pointer)(unsigned*);

	calc_t ExecuteGate (void* address)
	{
		__sassert (address, "Invalid image");

		abiret_t return_value;
		Value::Type return_type = Value::V_MAX;

		abi_gate_pointer jit_compiled_function = reinterpret_cast<abi_gate_pointer> (address);
		unsigned* jit_compiled_function_argument = reinterpret_cast<unsigned*> (&return_type);

		// ----
		// Say your goodbyes
		// That was your life
		// You'll pay all your penance
		// JIT compiler death sequence
		return_value = jit_compiled_function (jit_compiled_function_argument);
		// ----

		if (return_type == Value::V_MAX)
			return calc_t (); /* uninitialised */

		else
		{
			calc_t result;
			result.type = return_type;
			result.SetFromABI (return_value);
			return result;
		}
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

	void IMMU::SelectStack (Value::Type type)
	{
		msg (E_INFO, E_DEBUG, "Stack change request (req. \"%s\") is unsupported by MMU",
			 ProcDebug::ValueType_ids[type]);
	}
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
