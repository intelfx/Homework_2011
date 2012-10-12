#include "stdafx.h"
#include "Linker.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Linker.cpp
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Default linker plugin implementation.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

void UATLinker::DirectLink_Add( Processor::symbol_map& symbols, size_t offsets[SEC_MAX] )
{
	verify_method;
	msg( E_INFO, E_DEBUG, "Linking symbols: %zu", symbols.size() );

	char sym_nm_buf[STATIC_LENGTH];

	for( symbol_map::value_type & symbol_record: symbols ) {
		Symbol& symbol			= symbol_record.second.second;
		const std::string& name	= symbol_record.second.first;
		size_t hash				= symbol.hash;

		snprintf( sym_nm_buf, STATIC_LENGTH, "\"%s\" (hash %zx)", name.c_str(), hash );

		cassert( symbol_record.first == symbol.hash,
		         "Input symbol inconsistence (%s): hash %zx <-> key %zx",
		         sym_nm_buf, symbol.hash, symbol_record.first );

		// If symbol is defined here, link it (set address).
		if( symbol.is_resolved ) {
			if( symbol.ref.needs_linker_placement ) {
				cassert( !symbol.ref.has_second_component, "Invalid label %s", sym_nm_buf );
				cassert( !symbol.ref.components[0].is_indirect, "Invalid label %s", sym_nm_buf );
				cassert( !symbol.ref.components[0].target.is_symbol, "Invalid label %s", sym_nm_buf );

				switch( symbol.ref.global_section ) {
				case S_CODE:
					msg( E_INFO, E_DEBUG, "Definition of TEXT label %s: assigning address %zu",
					     sym_nm_buf, offsets[SEC_CODE_IMAGE] );
					symbol.ref.components[0].target.memory_address = offsets[SEC_CODE_IMAGE];
					break;

				case S_DATA:
					msg( E_INFO, E_DEBUG, "Definition of DATA label %s: assigning address %zu",
					     sym_nm_buf, offsets[SEC_DATA_IMAGE] );
					symbol.ref.components[0].target.memory_address = offsets[SEC_DATA_IMAGE];
					break;

				case S_REGISTER:
				case S_FRAME:
				case S_FRAME_BACK:
				case S_BYTEPOOL:
					casshole( "Definition of symbol %s : type %s, cannot auto-assign",
					          sym_nm_buf, ProcDebug::AddrType_ids [symbol.ref.global_section] );
					break;

				default:
				case S_NONE:
				case S_MAX:
					casshole( "Invalid symbol type" );
					break;
				}

				symbol.ref.needs_linker_placement = 0;
			} // needs linker placement

			else {
				msg( E_INFO, E_DEBUG, "Definition of symbol %s", sym_nm_buf );
			}
		} // if symbol is resolved (defined)

		else {
			msg( E_INFO, E_DEBUG, "Usage of symbol %s", sym_nm_buf );
		}

		temporary_map.insert( symbol_record );
	}

	msg( E_INFO, E_DEBUG, "Link completed" );
}

DirectReference UATLinker::Resolve( Reference& reference )
{
	verify_method;

	DirectReference result; mem_init( result );
	result.section = reference.global_section;

	for( unsigned i = 0; i <= reference.has_second_component; ++i ) {
		DirectReference tmp_reference; mem_init( tmp_reference );

		const Reference::SingleRef& cref = reference.components[i];
		const Reference::BaseRef& bref = cref.is_indirect ? cref.indirect.target : cref.target;

		/* resolve main base address of the component */
		if( bref.is_symbol )
			tmp_reference = Resolve( proc_->MMU()->ASymbol( bref.symbol_hash ).second.ref );

		else
			tmp_reference.address = bref.memory_address;

		/* resolve indirection */
		if( reference.components[i].is_indirect ) {
			/* load explicitly specified section (if it is not specified in symbol) */
			if( tmp_reference.section == S_NONE )
				tmp_reference.section = cref.indirect.section;

			else
				cassert( cref.indirect.section == S_NONE, "Duplicate specified section in indirection" );

			/* verify we have section set */
			cassert( tmp_reference.section != S_NONE, "No section specified to resolve indirect address" );

			/* load address value and reset section */
			proc_->LogicProvider()->Read( tmp_reference ).Get( Value::V_INTEGER, tmp_reference.address );
			tmp_reference.section = S_NONE;
		}

		/* assign to result reference */
		if( tmp_reference.section != S_NONE ) {
			cassert( result.section == S_NONE, "Duplicate specified section" );
			result.section = tmp_reference.section;
		}

		result.address += tmp_reference.address;
	}

	return result;
}

void UATLinker::DirectLink_Init()
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Starting link session" );
	temporary_map.clear();
}

void UATLinker::DirectLink_Commit( bool UAT )
{
	verify_method;

	msg( E_INFO, E_VERBOSE, "Linking %lu symbols", temporary_map.size() );
	symbol_map target_map;

	char sym_nm_buf[STATIC_LENGTH];

	for( symbol_map::value_type& symbol_iterator: temporary_map ) {
		symbol_type* current_symbol_record = &symbol_iterator.second;
		size_t hash                        = current_symbol_record->second.hash;
		auto existing_record               = target_map.find( hash );

		snprintf( sym_nm_buf, STATIC_LENGTH, "\"%s\" (hash %zx)",
		          current_symbol_record->first.c_str(), hash );

		cassert( hash == symbol_iterator.first,
		         "Internal map inconsistency in %s: hash %zx <-> key %zx",
		         sym_nm_buf, hash, symbol_iterator.first );

		Symbol& symbol = current_symbol_record->second;
		cassert( !symbol.ref.needs_linker_placement, "Unplaced symbol %s", sym_nm_buf );

		// Replace any reference already present in the map.
		if( existing_record != target_map.end() ) {
			bool existing_is_definition = existing_record->second.second.is_resolved;
			bool new_is_definition = symbol.is_resolved;

			if( existing_is_definition ) {
				cverify( !new_is_definition, "Symbol redefinition: %s", sym_nm_buf );
			} else if( new_is_definition ) {
				existing_record->second.second = symbol;
			}
		}
	} // for (temporary_map)

	if( UAT ) {
		casshole( "Not implemented" );
	}

	proc_->MMU()->ReadSymbolImage( std::move( target_map ) );
	temporary_map.clear(); // Well, MMU should move-assign our map, but who knows...

	msg( E_INFO, E_VERBOSE, "Linking session completed" );
}

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
