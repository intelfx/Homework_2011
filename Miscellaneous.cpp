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

void PrintSingleReference (char*& output, const Reference::SingleRef& ref, IMMU* mmu)
{
	if (ref.is_indirect)
	{
		output += snprintf (output, STATIC_LENGTH - (output - debug_buffer), "indirect: ");

		if (ref.indirect.section != S_NONE)
			output += snprintf (output, STATIC_LENGTH - (output - debug_buffer), "section %s ", AddrType_ids[ref.indirect.section]);
	}

	const Reference::BaseRef& bref = (ref.is_indirect) ? ref.indirect.target : ref.target;

	if (bref.is_symbol)
	{
		if (mmu)
			output += snprintf (output, STATIC_LENGTH - (output - debug_buffer), "symbol \"%s\"",
								mmu ->ASymbol (bref.symbol_hash).first.c_str());

		else
			output += snprintf (output, STATIC_LENGTH - (output - debug_buffer), "symbol [0x%zx]",
								bref.symbol_hash);
	}

	else
		output += snprintf (output, STATIC_LENGTH - (output - debug_buffer), "address %zu", bref.memory_address);
}

void PrintReference (const DirectReference& ref)
{
	snprintf (debug_buffer, STATIC_LENGTH, "section %s address %zu",
			  AddrType_ids[ref.section], ref.address);
}

void PrintReference (const Reference& ref, IMMU* mmu)
{
	char* output = debug_buffer;

	if (ref.global_section != S_NONE)
		output += snprintf (output, STATIC_LENGTH - (output - debug_buffer),
							"section %s ", AddrType_ids[ref.global_section]);

	if (ref.needs_linker_placement)
		output += snprintf (output, STATIC_LENGTH - (output - debug_buffer),
							"unresolved");

	else
	{
		PrintSingleReference (output, ref.components[0], mmu);

		if (ref.has_second_component)
		{
			strcpy (output, " + ");
			output += 3;

			PrintSingleReference (output, ref.components[1], mmu);
		}
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

typedef abiret_t (*abi_gate_pointer) (unsigned*);

calc_t ExecuteGate (void* address)
{
	__sassert (address, "Invalid image");

	abiret_t return_value;
	Value::Type return_type = Value::V_MAX;

	abi_gate_pointer jit_compiled_function = reinterpret_cast<abi_gate_pointer> (address);
	unsigned* jit_compiled_function_argument = reinterpret_cast<unsigned*> (&return_type);

	/*
	 * Say your goodbyes
	 * That was your life
	 * Pay all your penance
	 * JIT-compiled death sentence
	 */
	return_value = jit_compiled_function (jit_compiled_function_argument);

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
