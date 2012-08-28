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
		CommandTraits traits { dsc->name,
		                       dsc->description,
		                       dsc->arg_type,
		                       dsc->is_service_command,
		                       get_id( dsc->name ),
		                       {}
		                     };

		msg( E_INFO, E_DEBUG, "mkI command: \"%s\"-> %u", dsc->name, traits.id );
		auto byid_ins_res = by_id.insert( std::make_pair( traits.id, std::move( traits ) ) );
		cassert( byid_ins_res.second, "Internal inconsistency on \"%s\"", dsc->name );
	}

	msg( E_INFO, E_DEBUG, "Successfully added %zu commands", by_id.size() );
}

void CommandSet_mkI::AddCommand( Processor::CommandTraits && command )
{
	verify_method;

	command.id = get_id( command.mnemonic );

	msg( E_INFO, E_DEBUG, "Adding custom command: \"%s\" (\"%s\")-> 0x%04hx",
	     command.mnemonic, command.description, command.id );

	auto command_iresult = by_id.insert( std::make_pair( command.id, std::move( command ) ) );
	cassert( command_iresult.second, "Command already exists: \"%s\"", command.mnemonic );
}

void CommandSet_mkI::AddCommandImplementation( const char* mnemonic, size_t module, void* handle )
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Registering implementation driver for command \"%s\"-> module %zx",
	     mnemonic, module );

	cassert( mnemonic, "NULL mnemonic" );
	auto cmd_iterator = by_id.find( get_id( mnemonic ) );

	if( cmd_iterator == by_id.end() ) {
		msg( E_WARNING, E_VERBOSE, "Registering implementation driver for invalid mnemonic: \"%s\"", mnemonic );
		return;
	}

	auto handle_iresult = cmd_iterator->second.execution_handles.insert( std::make_pair( module, handle ) );
	cassert( handle_iresult.second, "Implementation of \"%s\"-> module %zx has already been registered",
	         mnemonic, module );
}

void* CommandSet_mkI::GetExecutionHandle( const CommandTraits& cmd, size_t module )
{
	auto handle_iterator = cmd.execution_handles.find( module );

	if( handle_iterator != cmd.execution_handles.end() )
		return handle_iterator->second;

	return 0;
}

const CommandTraits* CommandSet_mkI::DecodeCommand( const char* mnemonic ) const
{
	cassert( mnemonic, "NULL mnemonic" );
	auto cmd_iterator = by_id.find( get_id( mnemonic ) );

	if( cmd_iterator != by_id.end() )
		return &cmd_iterator->second;

	return 0;
}

const CommandTraits* CommandSet_mkI::DecodeCommand( Processor::cid_t id ) const
{
	auto cmd_iterator = by_id.find( id );

	if( cmd_iterator != by_id.end() )
		return &cmd_iterator->second;

	return 0;
}

const CommandSet_mkI::InternalCommandDescriptor CommandSet_mkI::initial_commands[] = {
	{
		"init",
		"System: initialize stack and execution environment",
		A_NONE,
		1
	},
	{
		"sleep",
		"System: hand off control to the OS",
		A_NONE,
		1
	},
	{
		"sys",
		"System: invoke execution environment",
		A_VALUE,
		1
	},
	{
		"dump",
		"System: dump context",
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
		"cmp",
		"Stack: compare two values on the stack (R/O subtraction)",
		A_NONE,
		0
	},
	{
		"swap",
		"Stack: swap two values on the stack",
		A_NONE,
		0
	},
	{
		"dup",
		"Stack: duplicate the value on the stack",
		A_NONE,
		0
	},
	{
		"lea",
		"Data: load effective address",
		A_REFERENCE,
		1
	},
	{
		"ld",
		"Data: read (load) memory/register",
		A_REFERENCE,
		0
	},
	{
		"st",
		"Data: write (store) memory/register",
		A_REFERENCE,
		0
	},
	{
		"ldint",
		"Data: read (load) integer from memory/register",
		A_REFERENCE,
		0
	},
	{
		"stint",
		"Data: write (store) integer memory/register",
		A_REFERENCE,
		0
	},
	{
		"settype",
		"Data: change type of memory location",
		A_REFERENCE,
		1
	},
	{
		"abs",
		"Arithmetic: absolute value",
		A_NONE,
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
		"sin",
		"Trigonometry: sine",
		A_NONE,
		0
	},
	{
		"cos",
		"Trigonometry: cosine",
		A_NONE,
		0
	},
	{
		"tan",
		"Trigonometry: tangent",
		A_NONE,
		0
	},
	{
		"asin",
		"Trigonometry: arcsine",
		A_NONE,
		0
	},
	{
		"acos",
		"Trigonometry: arccosine",
		A_NONE,
		0
	},
	{
		"atan",
		"Trigonometry: arctangent",
		A_NONE,
		0
	},
	{
		"anal", /* fuck yeah. */
		"Stack: analyze top of the stack",
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

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;