#include "stdafx.h"
#include "Interfaces.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		Miscellaneous.cpp
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	Miscellaneous (mostly debug data) definitions.
// -------------------------------------------------------------------------------------

namespace Processor
{

namespace ProcDebug
{

const char* FileSectionType_ids[SEC_MAX] = {
	"text image",
	"data image",
	"byte-granular pool image",
	"symbol map",
	"stack image"
};

const char* AddrType_ids[S_MAX] = {
	"invalid type",
	"code",
	"data",
	"register",
	"byte-granular pool",
	"stack frame",
	"parameter"
};

const char* ValueType_ids[Value::V_MAX + 1] = {
	"integer",
	"floating-point",
	"undefined"
};

char debug_buffer[STATIC_LENGTH];

std::string Print( MemorySectionType arg )
{
	return arg >= SEC_MAX ? ( snprintf( debug_buffer, STATIC_LENGTH, "<wrong section type: %u>", arg ), debug_buffer )
	                      : FileSectionType_ids[arg];
}

std::string Print( AddrType arg )
{
	return arg >= S_MAX ? ( snprintf( debug_buffer, STATIC_LENGTH, "<wrong address type: %u>", arg ), debug_buffer )
	                    : AddrType_ids[arg];
}

std::string Print( Value::Type arg )
{
	return arg >= Value::V_MAX + 1 ? ( snprintf( debug_buffer, STATIC_LENGTH, "<wrong value type: %u>", arg ), debug_buffer )
	                               : ValueType_ids[arg];
}

void PrintSingleReference( char*& output, const Reference::SingleRef& ref, IMMU* mmu )
{
	if( ref.indirection_section != S_NONE ) {
		if( ref.indirection_section == S_MAX ) {
			output += snprintf( output, STATIC_LENGTH - ( output - debug_buffer ), "(indirect to unknown section) " );
		} else {
			output += snprintf( output, STATIC_LENGTH - ( output - debug_buffer ), "(indirect to section \"%s\") ",
			                    Print( ref.indirection_section ).c_str() );
		}
	}

	switch( ref.target.type ) {
	case Reference::BaseRef::BRT_SYMBOL:
		if( mmu )
			output += snprintf( output, STATIC_LENGTH - ( output - debug_buffer ), "symbol \"%s\"",
			                    mmu->ASymbol( ref.target.symbol_hash ).first.c_str() );

		else
			output += snprintf( output, STATIC_LENGTH - ( output - debug_buffer ), "symbol [%zx]",
			                    ref.target.symbol_hash );
		break;

	case Reference::BaseRef::BRT_MEMORY_REF:
		output += snprintf( output, STATIC_LENGTH - ( output - debug_buffer ), "address %zu", ref.target.memory_address );
		break;

	case Reference::BaseRef::BRT_DEFINITION:
		output += snprintf( output, STATIC_LENGTH - ( output - debug_buffer ),
							"unplaced definition" ); 
		break;
	}
}

std::string PrintReference( const DirectReference& ref )
{
	snprintf( debug_buffer, STATIC_LENGTH, "section \"%s\" address %zu",
	          Print( ref.section ).c_str(), ref.address );
	return debug_buffer;
}

std::string PrintReference( const Reference& ref, IMMU* mmu )
{
	char* output = debug_buffer;

	if( ref.global_section != S_NONE )
		output += snprintf( output, STATIC_LENGTH - ( output - debug_buffer ),
		                    "section \"%s\" ", Print( ref.global_section ).c_str() );

	PrintSingleReference( output, ref.components[0], mmu );

	if( ref.has_second_component ) {
		strcpy( output, " + " );
		output += 3;

		PrintSingleReference( output, ref.components[1], mmu );
	}
	return debug_buffer;
}


std::string PrintValue( const Value& val )
{
	switch( val.type ) {
	case Value::V_INTEGER:
		snprintf( debug_buffer, STATIC_LENGTH, "int:%ld", val.integer );
		break;

	case Value::V_FLOAT:
		snprintf( debug_buffer, STATIC_LENGTH, "float:%lg", val.fp );
		break;

	case Value::V_MAX:
		snprintf( debug_buffer, STATIC_LENGTH, "<unset>" );
		break;

	default:
		s_casshole( "Switch error" );
		break;
	}
	return debug_buffer;
}

std::string PrintArgument( ArgumentType arg_type, const Command::Argument& argument, IMMU* mmu )
{
	switch( arg_type ) {
	case A_NONE:
		return "absent";

	case A_REFERENCE:
		return "reference to " + PrintReference( argument.ref, mmu );

	case A_VALUE:
		return PrintValue( argument.value );

	default:
		s_casshole( "Switch error" );
		break;
	}
}

} // namespace ProcDebug

calc_t ExecuteGate( abi_native_fn_t jit_compiled_function )
{
	s_cassert( jit_compiled_function, "Invalid image" );

	abiret_t return_value;
	Value::Type return_type = Value::V_MAX;

	unsigned* jit_compiled_function_argument = reinterpret_cast<unsigned*>( &return_type );

	/*
	 * Say your goodbyes
	 * That was your life
	 * Pay all your penance
	 * JIT-compiler death sentence
	 */
	smsg( E_WARNING, E_DEBUG, "Going hot now..." );
	return_value = jit_compiled_function( jit_compiled_function_argument );

	if( return_type == Value::V_MAX )
		return calc_t(); /* uninitialised */

	else {
		calc_t result;
		result.type = return_type;
		result.SetFromABI( return_value );
		return result;
	}
}


void ILogic::SwitchToContextBuffer( ctx_t id, bool clear_on_switch )
{
	SaveCurrentContext();
	ResetCurrentContextState();
	SetCurrentContextBuffer( id );
	if( clear_on_switch ) {
		proc_->MMU()->ResetContextBuffer( id );
	}
}

} // namespace Processor
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
