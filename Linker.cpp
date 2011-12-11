#include "stdafx.h"
#include "Linker.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Linker.cpp
// Author		intelfx
// Description	Linker.
// -----------------------------------------------------------------------------

namespace ProcessorImplementation
{
	using namespace Processor;

	void UATLinker::LinkSymbols (DecodedSet& input)
	{
		msg (E_INFO, E_DEBUG, "Linking symbols [%lu]", input.symbols.size());

		for (symbol_map::value_type& hsymbol: input.symbols)
		{
			Symbol& linked_desc		= hsymbol.second.second;
			const std::string& name	= hsymbol.second.first;
			size_t hash				= hsymbol.first;
			size_t caddress			= proc_ ->MMU() ->GetTextSize();
			size_t daddress			= proc_ ->MMU() ->GetDataSize();
			auto existing_record	= temporary_map.find (hash);

			// If symbol is defined here, link it (set address).
			if (linked_desc.is_resolved)
			{
				if (linked_desc.ref.is_symbol)
				{
					msg (E_INFO, E_DEBUG, "Definition of alias \"%s\": aliasing %p",
						 name.c_str(), linked_desc.ref.symbol_hash);

					__assert (input.symbols.count (linked_desc.ref.symbol_hash),
							  "Input set does not contain aliased symbol");
				}

				else if (linked_desc.ref.direct.address != symbol_auto_placement_addr)
				{
					msg (E_INFO, E_DEBUG, "Definition of symbol \"%s\": not linking address %s:%ld",
						 name.c_str(), Dbg_AddrType (linked_desc.ref.direct.type),
						 linked_desc.ref.direct.address);
				}

				else switch (linked_desc.ref.direct.type)
				{
					case S_CODE:
						msg (E_INFO, E_DEBUG, "Definition of TEXT label \"%s\": assigning address %ld",
							 name.c_str(), caddress);
						linked_desc.ref.direct.address = caddress;
						break;

					case S_DATA:
						msg (E_INFO, E_DEBUG, "Definition of DATA label \"%s\": assigning address %ld",
							 name.c_str(), daddress);
						linked_desc.ref.direct.address = daddress;
						break;

					case S_REGISTER:
						msg (E_INFO, E_DEBUG, "Definition of register alias \"%s\": aliasing \"%s\"",
							 name.c_str(), proc_ ->EncodeRegister (static_cast<Register> (linked_desc.ref.direct.address)));
						break;

					case S_FRAME:
						msg (E_INFO, E_DEBUG, "Definition of stack frame alias \"%s\": aliasing local address %ld",
							 name.c_str(), linked_desc.ref.direct.address);
						break;

					case S_FRAME_BACK:
						msg (E_INFO, E_DEBUG, "Definition of parameter alias \"%s\": aliasing parameter #%ld",
							 name.c_str(), linked_desc.ref.direct.address);
						break;

					default:
					case S_NONE:
						__asshole ("Invalid symbol type");
						break;
				}

				// Attach to any existing record (checking it is reference, not definition).
				if (existing_record != temporary_map.end())
				{
					__verify (!existing_record ->second.second.is_resolved,
							  "Symbol redefinition");

					existing_record ->second.second = linked_desc;
				}
			} // if symbol is resolved (defined)

			else
			{
				msg (E_INFO, E_DEBUG, "Usage of symbol \"%s\"", name.c_str());
			}

			// Add symbol if it does not exist at all.
			if (existing_record == temporary_map.end())
				temporary_map.insert (hsymbol);
		}

		msg (E_INFO, E_DEBUG, "Link completed");
	}

	Reference::Direct& UATLinker::Resolve (Reference& reference)
	{
		Reference* temp_ref = &reference;
		while (temp_ref ->is_symbol)
		{
			symbol_type& sm_rec = proc_ ->MMU() ->ASymbol (temp_ref ->symbol_hash);

			Symbol& sym = sm_rec.second;
			const char* name = sm_rec.first.c_str();

			__assert (sym.is_resolved, "Unresolved symbol \"%s\"", name);
			temp_ref = &sym.ref;
		}

		return temp_ref ->direct;
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
		for (symbol_map::value_type& sym_iter: temporary_map)
		{
			symbol_type* current_sym = &sym_iter.second;

			__assert (current_sym ->second.hash == sym_iter.first,
					  "Internal map inconsistency: hash %p <-> key %p",
					  current_sym ->second.hash, sym_iter.first);

			// Recursively (well, not recursively, but can be treated as such)
			// check circular aliasing if we've got an alias until we reach
			// direct reference, in which case check the ref itself.
			std::set<size_t> c_alias_detection_buffer;
			for (;;)
			{
				Symbol& sym = current_sym ->second;

				__verify (sym.is_resolved,
				          "Linker error: Unresolved symbol [%p] \"%s\"",
				          current_sym ->second.hash, current_sym ->first.c_str());

				msg (E_INFO, E_DEBUG, "Symbol [%p] \"%s\": %s",
					 sym.hash, current_sym ->first.c_str(),
				     sym.ref.is_symbol ? "alias" : "direct reference");


				if (sym.ref.is_symbol)
				{
					msg (E_INFO, E_DEBUG, "...aliasing [%p]", sym.ref.symbol_hash);
					symbol_map::iterator target_iter = temporary_map.find (sym.ref.symbol_hash);

					__assert (target_iter != temporary_map.end(),
							  "Aliased symbol not found in symbol map");

					// Check circular aliasing
					bool circular_detected = c_alias_detection_buffer.insert (target_iter ->first).second;
					__assert (!circular_detected, "Circular alias detected when linking");

					current_sym = &target_iter ->second;
				}

				else
				{
					Reference::Direct& dref = sym.ref.direct;
					__verify (dref.address != symbol_auto_placement_addr,
							  "Reference has a STUB address assigned");

					switch (dref.type)
					{
						case S_CODE:
							msg (E_INFO, E_DEBUG, "Reference to TEXT segment [%lu]", dref.address);
							__verify (dref.address < proc_ ->MMU() ->GetTextSize(),
									  "Reference points beyond TEXT end (%lu)",
									  proc_ ->MMU() ->GetTextSize());
							break;

						case S_DATA:
							msg (E_INFO, E_DEBUG, "Reference to DATA segment [%lu]", dref.address);
							__verify (dref.address < proc_ ->MMU() ->GetDataSize(),
									  "Reference points beyond DATA end (%lu)",
									  proc_ ->MMU() ->GetDataSize());
							break;

						case S_REGISTER:
							msg (E_INFO, E_DEBUG, "Reference of register [%d] \"%s\"",
								 dref.address, proc_ ->EncodeRegister (static_cast<Register> (dref.address)));
							break;

						case S_FRAME:
							msg (E_INFO, E_DEBUG, "Reference of stack frame with offset %ld",
								 dref.address);
							break;

						case S_FRAME_BACK:
							msg (E_INFO, E_DEBUG, "Reference of function parameter #%ld",
								 dref.address);
							break;

						case S_NONE:
							__asshole ("Invalid type");
							break;

						default:
							__asshole ("Switch error");
							break;
					}

					break;
				} // if not symbol
			} // for (while is symbol alias)
		} // for (temporary_map)

		msg (E_INFO, E_DEBUG, "Writing temporary map");
		proc_ ->MMU() ->ReadSyms (&temporary_map);
		temporary_map.clear(); // Well, MMU should move-assign our map, but who knows...
	}
} // namespace ProcessorImplementation