#include "stdafx.h"
#include "Linker.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Linker.cpp
// Author		intelfx
// Description	Unit-At-a-Time linker implementation
// -----------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

void UATLinker::LinkSymbols (DecodeResult& input)
{
	verify_method;
	msg (E_INFO, E_DEBUG, "Linking symbols: %zu", input.mentioned_symbols.size());

	char sym_nm_buf[STATIC_LENGTH];

	for (symbol_map::value_type & symbol_record: input.mentioned_symbols)
	{
		Symbol& symbol			= symbol_record.second.second;
		const std::string& name	= symbol_record.second.first;
		size_t hash				= symbol.hash;
		size_t caddress			= proc_ ->MMU() ->GetTextSize();
		size_t daddress			= proc_ ->MMU() ->GetDataSize();
		auto existing_record	= temporary_map.find (hash);

		snprintf (sym_nm_buf, STATIC_LENGTH, "\"%s\" (hash %zx)", name.c_str(), hash);

		__assert (symbol_record.first == symbol.hash,
				  "Input symbol inconsistence (%s): hash %x <-> key %x",
				  sym_nm_buf, symbol.hash, symbol_record.first);

		// If symbol is defined here, link it (set address).
		if (symbol.is_resolved)
		{
			if (symbol.ref.needs_linker_placement)
			{
				__assert (!symbol.ref.has_second_component, "Invalid label %s", sym_nm_buf);
				__assert (!symbol.ref.components[0].is_indirect, "Invalid label %s", sym_nm_buf);
				__assert (!symbol.ref.components[0].target.is_symbol, "Invalid label %s", sym_nm_buf);

				switch (symbol.ref.global_section)
				{
				case S_CODE:
					msg (E_INFO, E_DEBUG, "Definition of TEXT label %s: assigning address %zu",
						 sym_nm_buf, caddress);
					symbol.ref.components[0].target.memory_address = caddress;
					break;

				case S_DATA:
					msg (E_INFO, E_DEBUG, "Definition of DATA label %s: assigning address %zu",
						 sym_nm_buf, daddress);
					symbol.ref.components[0].target.memory_address = daddress;
					break;

				case S_REGISTER:
				case S_FRAME:
				case S_FRAME_BACK:
				case S_BYTEPOOL:
					__asshole ("Definition of symbol \"%s\" : type %s, cannot auto-assign",
							   sym_nm_buf, ProcDebug::AddrType_ids [symbol.ref.global_section]);
					break;

				default:
				case S_NONE:
				case S_MAX:
					__asshole ("Invalid symbol type");
					break;
				}

				symbol.ref.needs_linker_placement = 0;
			} // needs linker placement

			else
			{
				msg (E_INFO, E_DEBUG, "Definition of symbol \"%s\"", sym_nm_buf);
			}

			// Attach to any existing record (checking it is reference, not definition).
			if (existing_record != temporary_map.end())
			{
				__verify (!existing_record ->second.second.is_resolved,
						  "Symbol redefinition: \"%s\"", sym_nm_buf);

				existing_record ->second.second = symbol;
			}
		} // if symbol is resolved (defined)

		else
		{
			msg (E_INFO, E_DEBUG, "Usage of symbol \"%s\"", sym_nm_buf);
		}

		// Add symbol if it does not exist at all.
		if (existing_record == temporary_map.end())
			temporary_map.insert (symbol_record);
	}

	msg (E_INFO, E_DEBUG, "Link completed");
}

DirectReference UATLinker::Resolve (Reference& reference)
{
	verify_method;

	DirectReference result;
	init (result);

	result.section = reference.global_section;

	for (unsigned i = 0; i <= reference.has_second_component; ++i)
	{
		DirectReference tmp_reference;
		init (tmp_reference);

		const Reference::SingleRef& cref = reference.components[i];
		const Reference::BaseRef& bref = cref.is_indirect ? cref.indirect.target : cref.target;

		/* resolve main base address of the component */
		if (bref.is_symbol)
			tmp_reference = Resolve (proc_ ->MMU() ->ASymbol (bref.symbol_hash).second.ref);

		else
			tmp_reference.address = bref.memory_address;


		/* resolve indirection */
		if (reference.components[i].is_indirect)
		{
			/* load explicitly specified section (if it is not specified in symbol) */
			if (tmp_reference.section == S_NONE)
				tmp_reference.section = cref.indirect.section;

			else
				__assert (cref.indirect.section == S_NONE, "Duplicate specified section in indirection");

			/* verify we have section set */
			__assert (tmp_reference.section != S_NONE, "No section specified to resolve indirect address");

			/* load address value and reset section */
			proc_ ->LogicProvider() ->Read (tmp_reference).Get (Value::V_INTEGER, tmp_reference.address);
			tmp_reference.section = S_NONE;
		}

		/* assign to result reference */
		if (tmp_reference.section != S_NONE)
		{
			__assert (result.section == S_NONE, "Duplicate specified section");
			result.section = tmp_reference.section;
		}

		result.address += tmp_reference.address;
	}

	return result;
}

void UATLinker::InitLinkSession()
{
	verify_method;

	msg (E_INFO, E_DEBUG, "Starting link session");
	temporary_map.clear();
}

void UATLinker::Finalize()
{
	verify_method;

	msg (E_INFO, E_VERBOSE, "Linking %lu symbols", temporary_map.size());

	msg (E_INFO, E_DEBUG, "Checking image completeness");

	char sym_nm_buf[STATIC_LENGTH];

	for (symbol_map::value_type & symbol_iterator: temporary_map)
	{
		symbol_type* current_symbol_record = &symbol_iterator.second;

		snprintf (sym_nm_buf, STATIC_LENGTH, "\"%s\" (hash %zx)",
				  current_symbol_record ->first.c_str(), current_symbol_record ->second.hash);

		__assert (current_symbol_record ->second.hash == symbol_iterator.first,
				  "Internal map inconsistency in %s: key %zx",
				  sym_nm_buf, symbol_iterator.first);

		Symbol& symbol = current_symbol_record ->second;
		__verify (symbol.is_resolved, "Linker error: Unresolved symbol %s", sym_nm_buf);
		__assert (!symbol.ref.needs_linker_placement, "Unplaced symbol %s", sym_nm_buf);

	} // for (temporary_map)

	proc_ ->MMU() ->InsertSyms (std::move (temporary_map));
	temporary_map.clear(); // Well, MMU should move-assign our map, but who knows...

	msg (E_INFO, E_VERBOSE, "Linking session completed");
}
} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
