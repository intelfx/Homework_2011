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
		msg (E_INFO, E_DEBUG, "Linking symbols: %zu", input.mentioned_symbols.size());

		char sym_nm_buf[STATIC_LENGTH];

		for (symbol_map::value_type & symbol_record: input.mentioned_symbols)
		{
			Symbol& symbol		= symbol_record.second.second;
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
				switch (symbol.ref.type)
				{
				case Reference::RT_SYMBOL:
					msg (E_INFO, E_DEBUG, "Definition of alias %s: aliasing hash %p",
						 sym_nm_buf, symbol.ref.symbol_hash);

					__assert (input.mentioned_symbols.count (symbol.ref.symbol_hash),
							  "Input set does not contain aliased symbol");

					break;

				case Reference::RT_DIRECT:
					if (symbol.ref.plain.address != symbol_auto_placement_addr)
						switch (symbol.ref.plain.type)
						{
						case S_CODE:
						case S_DATA:
							msg (E_INFO, E_DEBUG, "Definition of %s label \"%s\": user-defined address %lu",
								 ProcDebug::AddrType_ids [symbol.ref.plain.type], sym_nm_buf, symbol.ref.plain.address);
							break;

						case S_REGISTER:
							msg (E_INFO, E_DEBUG, "Definition of register alias \"%s\": aliasing register %s",
								 sym_nm_buf,
								 proc_ ->LogicProvider() ->EncodeRegister (static_cast<Register> (symbol.ref.plain.address)));
							break;

						case S_FRAME:
						case S_FRAME_BACK:
							msg (E_INFO, E_DEBUG, "Definition of %s stack alias \"%s\": local address %lu",
								 ProcDebug::AddrType_ids [symbol.ref.plain.type], sym_nm_buf, symbol.ref.plain.address);
							break;

						case S_NONE:
						case S_MAX:
						default:
							__asshole ("Invalid address type or switch error");
							break;
						}

					else switch (symbol.ref.plain.type)
						{
						case S_CODE:
							msg (E_INFO, E_DEBUG, "Definition of TEXT label %s: assigning address %lu",
								 sym_nm_buf, caddress);
							symbol.ref.plain.address = caddress;
							break;

						case S_DATA:
							msg (E_INFO, E_DEBUG, "Definition of DATA label %s: assigning address %lu",
								 sym_nm_buf, daddress);
							symbol.ref.plain.address = daddress;
							break;

						case S_REGISTER:
						case S_FRAME:
						case S_FRAME_BACK:
							__asshole ("Definition of symbol \"%s\" : type %s with STUB address, cannot auto-assign",
									   sym_nm_buf, ProcDebug::AddrType_ids [symbol.ref.plain.type]);
							break;

						default:
						case S_NONE:
						case S_MAX:
							__asshole ("Invalid symbol type");
							break;
						}

					break;

				case Reference::RT_INDIRECT:
					__assert (symbol.ref.plain.type < S_MAX, "Invalid symbol type");

					msg (E_INFO, E_DEBUG, "Definition of %s alias \"%s\": indirect addressing",
						 ProcDebug::AddrType_ids[symbol.ref.plain.type], sym_nm_buf);

					break;

				default:
					__asshole ("Switch error");
					break;
				} // switch (symbol's reference type)

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

	Reference::Direct UATLinker::Resolve (Reference& reference)
	{
		Reference* temp_ref = &reference;
		Reference::Direct plain_ref;

		while (temp_ref ->type == Reference::RT_SYMBOL)
		{
			symbol_type& symbol_record = proc_ ->MMU() ->ASymbol (temp_ref ->symbol_hash);

			Symbol& symbol = symbol_record.second;
			const char* name = symbol_record.first.c_str();

			__assert (symbol.is_resolved, "Unresolved symbol \"%s\"", name);
			temp_ref = &symbol.ref;
		}

		if (temp_ref ->type == Reference::RT_INDIRECT)
		{
			plain_ref.type = temp_ref ->plain.type;

			calc_t address = proc_ ->MMU() ->ARegister (indirect_addressing_register);
			__assert (address.type == Value::V_INTEGER, "Cannot access float address");

			plain_ref.address = address.integer;
		}

		else
			plain_ref = temp_ref ->plain;

		proc_ ->MMU() ->VerifyReference (plain_ref);
		return plain_ref;
	}

	void UATLinker::InitLinkSession()
	{
		msg (E_INFO, E_VERBOSE, "Initializing linker");
		temporary_map.clear();
	}

	void UATLinker::Finalize()
	{
		msg (E_INFO, E_VERBOSE, "Finishing linkage");
		msg (E_INFO, E_VERBOSE, "Image size - %lu symbols", temporary_map.size());

		msg (E_INFO, E_DEBUG, "Checking image completeness");

		char sym_nm_buf[STATIC_LENGTH];

		for (symbol_map::value_type& symbol_iterator: temporary_map)
		{
			symbol_type* current_symbol_record = &symbol_iterator.second;

			snprintf (sym_nm_buf, STATIC_LENGTH, "\"%s\" (hash %zx)",
					  current_symbol_record ->first.c_str(), current_symbol_record ->second.hash);

			__assert (current_symbol_record ->second.hash == symbol_iterator.first,
					  "Internal map inconsistency (%s): hash %p <-> key %p",
					  sym_nm_buf, current_symbol_record ->second.hash, symbol_iterator.first);

			// Recursively (well, not recursively, but can be treated as such)
			// check circular aliasing if we've got an alias until we reach
			// plain reference, in which case check the ref itself.
			std::set<size_t> c_alias_detection_buffer;
			Processor::Reference::Type last_encountered_type;

			do
			{
				Symbol& symbol = current_symbol_record ->second;

				__verify (symbol.is_resolved,
						  "Linker error: Unresolved symbol %s", sym_nm_buf);

				msg (E_INFO, E_DEBUG, "Symbol %s: %s",
					 sym_nm_buf, (symbol.ref.type == Reference::RT_SYMBOL) ? "alias" : "plain reference");

				last_encountered_type = symbol.ref.type; // for do-while loop exit condition
				switch (symbol.ref.type)
				{
				case Reference::RT_SYMBOL:
				{
					msg (E_INFO, E_DEBUG, "...aliasing hash %p", symbol.ref.symbol_hash);
					symbol_map::iterator target_iter = temporary_map.find (symbol.ref.symbol_hash);

					__assert (target_iter != temporary_map.end(),
							  "Aliased symbol not found in symbol map");

					// Check circular aliasing
					bool circular_detected = c_alias_detection_buffer.insert (target_iter ->first).second;
					__assert (!circular_detected, "Circular alias detected when linking");

					current_symbol_record = &target_iter ->second;
					break;
				}

				case Reference::RT_DIRECT:
				{

					Reference::Direct& direct_ref = symbol.ref.plain;
					__verify (direct_ref.address != symbol_auto_placement_addr,
							  "Reference has a STUB address assigned");

					switch (direct_ref.type)
					{
					case S_CODE:
						msg (E_INFO, E_DEBUG, "Reference to TEXT segment [%lu]", direct_ref.address);
						__verify (direct_ref.address < proc_ ->MMU() ->GetTextSize(),
								  "Reference points beyond TEXT end (%lu)",
								  proc_ ->MMU() ->GetTextSize());
						break;

					case S_DATA:
						msg (E_INFO, E_DEBUG, "Reference to DATA segment [%lu]", direct_ref.address);
						__verify (direct_ref.address < proc_ ->MMU() ->GetDataSize(),
								  "Reference points beyond DATA end (%lu)",
								  proc_ ->MMU() ->GetDataSize());
						break;

					case S_REGISTER:
						msg (E_INFO, E_DEBUG, "Reference of register [%d] \"%s\"",
							 direct_ref.address,
							 proc_ ->LogicProvider() ->EncodeRegister (static_cast<Register> (direct_ref.address)));
						break;

					case S_FRAME:
						msg (E_INFO, E_DEBUG, "Reference of stack frame with offset %ld",
							 direct_ref.address);
						break;

					case S_FRAME_BACK:
						msg (E_INFO, E_DEBUG, "Reference of function parameter #%ld",
							 direct_ref.address);
						break;

					case S_NONE:
					case S_MAX:
						__asshole ("Invalid type");
						break;

					default:
						__asshole ("Switch error");
						break;
					}

					break;
				}

				case Reference::RT_INDIRECT:
					msg (E_INFO, E_DEBUG, "Reference of type %s - indirect",
						 ProcDebug::AddrType_ids[symbol.ref.plain.type]);

					break;

				default:
					__asshole ("Switch error");
					break;
				}

			} while (last_encountered_type == Reference::RT_SYMBOL); // for (while is symbol alias)
		} // for (temporary_map)

		msg (E_INFO, E_DEBUG, "Writing temporary map");
		proc_ ->MMU() ->InsertSyms (std::move (temporary_map));
		temporary_map.clear(); // Well, MMU should move-assign our map, but who knows...
	}
} // namespace ProcessorImplementation
// kate: indent-mode cstyle; replace-tabs off; indent-width 4; tab-width 4;
