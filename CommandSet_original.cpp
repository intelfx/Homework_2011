#include "stdafx.h"
#include "CommandSet_original.h"

#include <uXray/fxhash_functions.h>

// -------------------------------------------------------------------------------------
// Library		Homework
// File			CommandSet_original.cpp
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Original (Processor mkI) command set plugin implementation.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

void CommandSet_mkI::OnAttach()
{
	ResetCommandSet();
}

void CommandSet_mkI::ResetCommandSet()
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Resetting command set: using mkI" );
	by_id.clear();

	for( const InternalCommandDescriptor* dsc = initial_commands; dsc->name; ++dsc ) {
		CommandTraits traits ( dsc->name,
		                       dsc->description,
		                       dsc->arg_type,
		                       dsc->is_service_command );
		traits.id = get_id( dsc->name );

		msg( E_INFO, E_DEBUG, "mkI command: \"%s\" -> %u", dsc->name, traits.id );
		auto byid_ins_res = by_id.insert( std::make_pair( traits.id, std::move( traits ) ) );
		cassert( byid_ins_res.second, "Internal inconsistency on \"%s\"", dsc->name );
	}

	msg( E_INFO, E_DEBUG, "Successfully added %zu commands", by_id.size() );
}

void CommandSet_mkI::AddCommand( CommandTraits && command )
{
	verify_method;

	command.id = get_id( command.mnemonic );

	msg( E_INFO, E_DEBUG, "Adding custom command: \"%s\" (\"%s\") -> 0x%04hx",
	     command.mnemonic, command.description, command.id );

	auto command_iresult = by_id.insert( std::make_pair( command.id, std::move( command ) ) );
	cassert( command_iresult.second, "Command already exists: \"%s\"", command.mnemonic );
}

void CommandSet_mkI::AddCommandImplementation( const char* mnemonic, size_t module, void* handle )
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Registering implementation driver for command \"%s\" -> module %zx",
	     mnemonic, module );

	cassert( mnemonic, "NULL mnemonic" );
	auto cmd_iterator = by_id.find( get_id( mnemonic ) );

	if( cmd_iterator == by_id.end() ) {
		msg( E_WARNING, E_VERBOSE, "Registering implementation driver for invalid mnemonic: \"%s\"", mnemonic );
		return;
	}

	auto handle_iresult = cmd_iterator->second.execution_handles.insert( std::make_pair( module, handle ) );
	cassert( handle_iresult.second, "Implementation of \"%s\" -> module %zx has already been registered",
	         mnemonic, module );
}

void* CommandSet_mkI::GetExecutionHandle( const CommandTraits& cmd, size_t module )
{
	auto handle_iterator = cmd.execution_handles.find( module );

	if( handle_iterator != cmd.execution_handles.end() )
		return handle_iterator->second;

	return nullptr;
}

const CommandTraits* CommandSet_mkI::DecodeCommand( const char* mnemonic ) const
{
	cassert( mnemonic, "NULL mnemonic" );
	auto cmd_iterator = by_id.find( get_id( mnemonic ) );

	if( cmd_iterator != by_id.end() )
		return &cmd_iterator->second;

	return nullptr;
}

const CommandTraits* CommandSet_mkI::DecodeCommand( cid_t id ) const
{
	auto cmd_iterator = by_id.find( id );

	if( cmd_iterator != by_id.end() )
		return &cmd_iterator->second;

	return nullptr;
}

const CommandSet_mkI::InternalCommandDescriptor CommandSet_mkI::initial_commands[] = {
	{
		"init",
		"System: initialize stack and execution environment",
		A_NONE,
		true
	},
	{
		"sleep",
		"System: hand off control to the OS",
		A_NONE,
		true
	},
	{
		"sys",
		"System: invoke execution environment",
		A_VALUE,
		true
	},
	{
		"dump",
		"System: dump context",
		A_NONE,
		true
	},
	{
		"push",
		"Stack: push a value onto the stack",
		A_VALUE,
		false
	},
	{
		"pop",
		"Stack: remove a value from the stack",
		A_NONE,
		false
	},
	{
		"top",
		"Stack: peek a value from the stack",
		A_NONE,
		false
	},
	{
		"cmp",
		"Stack: compare two values on the stack by subtraction (subtrahend on top (popped), minuend not touched)",
		A_NONE,
		false
	},
	{
		"swap",
		"Stack: swap two values on the stack",
		A_NONE,
		false
	},
	{
		"dup",
		"Stack: duplicate the value on the stack",
		A_NONE,
		false
	},
	{
		"lea",
		"Data: load effective address",
		A_REFERENCE,
		true
	},
	{
		"ld",
		"Data: read (load) memory/register",
		A_REFERENCE,
		false
	},
	{
		"st",
		"Data: write (store) memory/register",
		A_REFERENCE,
		false
	},
	{
		"ldint",
		"Data: read (load) integer from memory/register",
		A_REFERENCE,
		false
	},
	{
		"stint",
		"Data: write (store) integer memory/register",
		A_REFERENCE,
		false
	},
	{
		"settype",
		"Data: change type of memory location",
		A_REFERENCE,
		true
	},
	{
		"abs",
		"Arithmetic: absolute value",
		A_NONE,
		false
	},
	{
		"add",
		"Arithmetic: addition",
		A_NONE,
		false
	},
	{
		"sub",
		"Arithmetic: subtraction (minuend on top)",
		A_NONE,
		false
	},
	{
		"mul",
		"Arithmetic: multiplication",
		A_NONE,
		false
	},
	{
		"div",
		"Arithmetic: division (dividend on top)",
		A_NONE,
		false
	},
	{
		"mod",
		"Arithmetic: modulo (dividend on top)",
		A_NONE,
		false
	},
	{
		"inc",
		"Arithmetic: increment by one",
		A_NONE,
		false
	},
	{
		"dec",
		"Arithmetic: decrement by one",
		A_NONE,
		false
	},
	{
		"neg",
		"Arithmetic: negation",
		A_NONE,
		false
	},
	{
		"sqrt",
		"Arithmetic: square root extraction",
		A_NONE,
		false
	},
	{
		"sin",
		"Trigonometry: sine",
		A_NONE,
		false
	},
	{
		"cos",
		"Trigonometry: cosine",
		A_NONE,
		false
	},
	{
		"tan",
		"Trigonometry: tangent",
		A_NONE,
		false
	},
	{
		"asin",
		"Trigonometry: arcsine",
		A_NONE,
		false
	},
	{
		"acos",
		"Trigonometry: arccosine",
		A_NONE,
		false
	},
	{
		"atan",
		"Trigonometry: arctangent",
		A_NONE,
		false
	},
	{
		"anal", /* fuck yeah. */
		"Stack: analyze top of the stack",
		A_NONE,
		false
	},
	{
		"je",
		"Address: jump if ZERO",
		A_REFERENCE,
		true
	},
	{
		"jne",
		"Address: jump if not ZERO",
		A_REFERENCE,
		true
	},
	{
		"ja",
		"Address: jump if ABOVE",
		A_REFERENCE,
		true
	},
	{
		"jna",
		"Address: jump if not ABOVE",
		A_REFERENCE,
		true
	},
	{
		"jae",
		"Address: jump if ABOVE or EQUAL",
		A_REFERENCE,
		true
	},
	{
		"jnae",
		"Address: jump if not ABOVE or EQUAL",
		A_REFERENCE,
		true
	},
	{
		"jb",
		"Address: jump if BELOW",
		A_REFERENCE,
		true
	},
	{
		"jnb",
		"Address: jump if not BELOW",
		A_REFERENCE,
		true
	},
	{
		"jbe",
		"Address: jump if BELOW or EQUAL",
		A_REFERENCE,
		true
	},
	{
		"jnbe",
		"Address: jump if not BELOW or EQUAL",
		A_REFERENCE,
		true
	},
	{
		"jmp",
		"Address: unconditional jump",
		A_REFERENCE,
		true
	},
	{
		"call",
		"Address: unconditional call",
		A_REFERENCE,
		true
	},
	{
		"ret",
		"Address: unconditional return",
		A_NONE,
		true
	},
	{
		"snfc",
		"Flags: set No-Flag-Change flag",
		A_NONE,
		true
	},
	{
		"cnfc",
		"Flags: clear No-Flag-Change flag",
		A_NONE,
		true
	},
	{
		"quit",
		"Management: stop execution/quit context",
		A_NONE,
		true
	},
	{
		nullptr,
		nullptr,
		A_NONE,
		false
	}
};

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;

