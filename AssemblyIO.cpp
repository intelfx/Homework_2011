#include "stdafx.h"
#include "AssemblyIO.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			AssemblyIO.cpp
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Assembly language plugin implementation.
// -------------------------------------------------------------------------------------

ImplementDescriptor( AsmHandler, "assembly reader/writer", MOD_APPMODULE );

namespace ProcessorImplementation
{
using namespace Processor;

AsmHandler::AsmHandler() :
	reading_file_( 0 ),
	writing_file_( 0 ),
	current_line_num( 0 )
{
}

AsmHandler::~AsmHandler()
{
	if( reading_file_ )
		RdReset();

	if( writing_file_ )
		WrReset();
}

bool AsmHandler::_Verify() const
{
	if( writing_file_ )
		verify_statement( !ferror( writing_file_ ), "Error in writing stream" );

	if( reading_file_ )
		verify_statement( !ferror( reading_file_ ), "Error in reading stream" );

	return 1;
}

FileType AsmHandler::RdSetup( FILE* file )
{
	reading_file_ = file;
	verify_method;

	return FT_STREAM;
}

void AsmHandler::RdReset()
{
	verify_method;

	if( reading_file_ ) {
		msg( E_INFO, E_DEBUG, "Resetting reader" );

		fclose( reading_file_ );
		reading_file_ = 0;

		current_line_num = 0;
		last_statement_type = Value::V_MAX;
		decode_output.Clear();
	}
}

void AsmHandler::WrReset()
{
	verify_method;

	if( writing_file_ ) {
		msg( E_INFO, E_DEBUG, "Resetting writing file" );

		fclose( writing_file_ );
		writing_file_ = 0;
	}
}

void AsmHandler::WrSetup( FILE* file )
{
	writing_file_ = file;
	verify_method;

	msg( E_INFO, E_DEBUG, "Writer set up" );
}

void AsmHandler::Write( ctx_t id )
{
	verify_method;
	cassert( writing_file_, "Writer has not been set up" );

	msg( E_INFO, E_VERBOSE, "Beginning ASM write of context %zu", id );

	proc_->LogicProvider()->SwitchToContextBuffer( id );
	InternalWriteFile();
	proc_->LogicProvider()->RestoreCurrentContext();
}

void AsmHandler::InternalWriteFile()
{
	casshole( "Not implemented" );
}

std::pair< MemorySectionIdentifier, size_t > AsmHandler::NextSection()
{
	casshole( "Not implemented" );
}

llarray AsmHandler::ReadSectionImage()
{
	casshole( "Not implemented" );
}

symbol_map AsmHandler::ReadSymbols()
{
	casshole( "Not implemented" );
}

AddrType AsmHandler::DecodeSectionType( char id )
{
	switch( id ) {
	case 'c':
		return S_CODE;
	case 'd':
		return S_DATA;
	case 'f':
		return S_FRAME;
	case 'p':
		return S_FRAME_BACK;
	case 'r':
		return S_REGISTER;
	default:
		casshole( "Invalid plain address specifier: '%c'", id );
	}
	return S_NONE; /* for compiler not to complain */
}

Reference::SingleRef AsmHandler::ParseIndirectReference( char* arg )
{
	verify_method;

	Reference::SingleRef result; mem_init( result );

	msg( E_INFO, E_DEBUG, "Parsing indirect reference \"%s\"", arg );

	if( arg[0] == '$' ) {
		result.indirection_section = S_REGISTER;
		result.target = ParseRegisterReference( arg + 1 );
	}

	else {
		// Read section specifier (if present)
		if( arg[1] == ':' ) {
			result.indirection_section = DecodeSectionType( arg[0] );
			arg += 2;
		} else {
			result.indirection_section = S_MAX;
		}

		result.target = ParseBaseReference( arg );
	}

	return result;
}

Reference AsmHandler::ParseFullReference( char* arg )
{
	verify_method;

	Reference result; mem_init( result );

	simplify( arg );
	msg( E_INFO, E_DEBUG, "Parsing complete reference \"%s\"", arg );

	char* second_component = 0;

	// Read section specifier (if present)
	if( arg[1] == ':' ) {
		result.global_section = DecodeSectionType( arg[0] );
		arg += 2;
	}

	// Get pointer to second component (if present)
	if( ( second_component = strchr( arg, '+' ) ) ) {
		*second_component++ = '\0';
	}

	result.components[0] = ParseSingleReference( arg, &result.global_section );

	if( second_component ) {
		result.has_second_component = 1;
		result.components[1] = ParseSingleReference( second_component, &result.global_section );
	}

	return result;
}

Reference::BaseRef AsmHandler::ParseRegisterReference( char* arg )
{
	Reference::BaseRef result; mem_init( result );

	result.type = Reference::BaseRef::BRT_MEMORY_REF;
	result.memory_address = proc_->LogicProvider()->DecodeRegister( arg );

	return result;
}

Reference::SingleRef AsmHandler::ParseSingleReference( char* arg, AddrType* global_section )
{
	Reference::SingleRef result; mem_init( result );

	msg( E_INFO, E_DEBUG, "Parsing single reference \"%s\"", arg );

	switch( arg[0] ) {
	default: /* direct reference */
		result.indirection_section = S_NONE;
		result.target = ParseBaseReference( arg );
		break;

	case '(': { /* indirect reference */
		++arg; // skip opening bracket
		char* closing_bracket = strchr( arg, ')' );
		cverify( closing_bracket, "Malformed indirect reference: \"%s\"", arg );
		*closing_bracket = '\0';

		result = ParseIndirectReference( arg );
		break;
	}

	case '"': /* string reference */
		cverify( *global_section == S_NONE, "Cannot use string reference when global section is specified" );
		*global_section = S_BYTEPOOL;

		result.indirection_section = S_NONE;
		result.target.type = Reference::BaseRef::BRT_DEFINITION;

		ParseInsertString( arg /* no increment */ );
		break;

	case '$': /* register reference */
		cverify( *global_section == S_NONE, "Cannot use register reference when global section is specified" );
		*global_section = S_REGISTER;

		result.indirection_section = S_NONE;
		result.target = ParseRegisterReference( arg + 1 );
		break;
	}

	return result;
}

Reference::BaseRef AsmHandler::ParseBaseReference( char* arg )
{
	Reference::BaseRef result; mem_init( result );

	msg( E_INFO, E_DEBUG, "Parsing basic reference \"%s\"", arg );

	errno = 0;
	char* endptr;

	long immediate = strtol( arg, &endptr, 0 ); // base autodetermine

	if( endptr != arg ) {
		cverify( !errno, "Invalid address: \"%s\": strtol() says \"%s\"",
		         arg, strerror( errno ) );

		cverify( *endptr == '\0', "Invalid address: \"%s\": garbage at end-of-input", arg );

		result.type = Reference::BaseRef::BRT_MEMORY_REF;
		result.memory_address = immediate;

		msg( E_INFO, E_DEBUG, "Basic reference: immediate \"%lu\"", immediate );
	}

	else {
		Symbol referenced_symbol( arg );
		InsertSymbol( referenced_symbol, arg, decode_output.mentioned_symbols );

		result.type = Reference::BaseRef::BRT_SYMBOL;
		result.symbol_hash = referenced_symbol.hash;

		msg( E_INFO, E_DEBUG, "Basic reference: symbol \"%s\" (%zx)", arg, referenced_symbol.hash );
	}

	return result;
}

void AsmHandler::ParseInsertString( char* arg )
{
	std::string output_string;

	msg( E_INFO, E_DEBUG, "Parsing string literal %s", arg );
	char* input_ptr = arg + 1, current, output;

	while( ( current = *input_ptr++ ) != '"' ) {
		if( current == '\\' ) { // escape-sequence
			switch( current = *input_ptr++ ) {
			case '\\': // backward slash
			case '\'': // single-quote
			case '\"': // double-quote
			case '\0': // NUL
				output = current;
				break;

			case 't': // Horizontal tabulator
				output = '\t';
				break;

			case 'v': // Vertical tabulator
				output = '\v';
				break;

			case 'n': // Newline
				output = '\n';
				break;

			case 'r': // Carriage return
				output = '\r';
				break;

			case 'f': // Form-feed
				output = '\f';
				break;

			case 'a': // Audible bell
				output = '\a';
				break;

			case 'b': // Backspace
				output = '\b';
				break;

			case 'x': // Hexadecimal
				output = strtol( input_ptr, &input_ptr, 16 );
				break;

			default: // Octal
				output = strtol( input_ptr, &input_ptr, 8 );
				break;
			} // escape sequence parse
		} else {
			output = current;
		}

		// Commit the char
		output_string.push_back( output );
	} // while not closing double-apostrophe

	output_string.push_back( '\0' );

	msg( E_INFO, E_DEBUG, "Decoded string \"%s\" (length %zu)",
	     output_string.c_str(), output_string.size() );

	cassert( decode_output.bytepool.empty(), "More than one string in single decode unit" );

	decode_output.bytepool.reserve( output_string.size() + 1 );
	decode_output.bytepool.assign( output_string.begin(), output_string.end() );
}

char* AsmHandler::PrepLine( char* read_buffer )
{
	char* begin, *tmp = read_buffer, *last_non_whitespace;

	while( isspace( * ( begin = tmp++ ) ) );

	if( *begin == '\0' )
		return 0;

	tmp = begin;
	last_non_whitespace = begin;

	/* drop unused parts of decoded string */
	do {
		if( *tmp == ';' )
			*tmp = '\0';

		else if( *tmp == '\n' )
			*tmp = '\0';

		else {
			if( !isspace( *tmp ) )
				last_non_whitespace = tmp;

			*tmp = tolower( *tmp );
		}

	} while( *tmp++ );

	*( last_non_whitespace + 1 ) = '\0'; // cut traling whitespace

	return begin;
}

void AsmHandler::ReadSingleDeclaration( const char* decl_data )
{
	char name[STATIC_LENGTH], initialiser[STATIC_LENGTH], type;

	// Declaration is generally a reference to something, so declare it
	Reference declaration_reference; mem_init( declaration_reference );

	// we may get an unnamed declaration
	if( decl_data[0] == '=' ) {
		++decl_data;

		cassert( last_statement_type != Value::V_MAX,
		         "Declaration: unnamed DATA entry = \"%s\": type not set!",
		         decl_data );

		calc_t declaration_data;
		declaration_data.type = last_statement_type;
		declaration_data.Parse( decl_data );

		msg( E_INFO, E_DEBUG, "Declaration: unnamed DATA entry = %s",
		     ProcDebug::PrintValue( declaration_data ).c_str() );

		decode_output.data.push_back( declaration_data );
		return;
	}

	int arguments = sscanf( decl_data, "%[^:= ] %c %s", name, &type, initialiser );

	switch( arguments ) {
	case 3:
		switch( type ) {
		case '=': {
			// Create reference to data
			declaration_reference.global_section = S_DATA;
			declaration_reference.has_second_component = 0;
			declaration_reference.components[0].indirection_section = S_NONE;
			declaration_reference.components[0].target.type = Reference::BaseRef::BRT_DEFINITION;

			// Parse the initialiser
			cassert( last_statement_type != Value::V_MAX,
			         "Declaration: DATA entry \"%s\" = \"%s\": type not set!",
			         name, initialiser );

			calc_t declaration_data;
			declaration_data.type = last_statement_type;
			declaration_data.Parse( initialiser );

			msg( E_INFO, E_DEBUG, "Declaration: DATA entry \"%s\" = %s",
			     name, ProcDebug::PrintValue( declaration_data ).c_str() );

			decode_output.data.push_back( declaration_data );
			break;
		}

		case ':': {
			// Parse aliased reference
			declaration_reference = ParseFullReference( initialiser );

			msg( E_INFO, E_DEBUG, "Declaration: alias \"%s\" to %s",
			     name, ProcDebug::PrintReference( declaration_reference ).c_str() );

			// No data is decoded
			break;
		}

		default:
			casshole( "Invalid declaration: \"%s\"", decl_data );
			break;
		}

		break;

	case 1: {
		// Create reference to data
		declaration_reference.global_section = S_DATA;
		declaration_reference.has_second_component = 0;
		declaration_reference.components[0].indirection_section = S_NONE;
		declaration_reference.components[0].target.type = Reference::BaseRef::BRT_DEFINITION;

		// Create "uninitialised" value and set its type
		calc_t uninitialised_data;
		uninitialised_data.type = last_statement_type;

		// if type is known, initialise to 0 in appropriate type.
		if( last_statement_type != Value::V_MAX ) {
			uninitialised_data.Set( Value::V_MAX, 0 );

			msg( E_INFO, E_DEBUG, "Declaration: DATA entry \"%s\" uninitialised (%s)",
			     name, ProcDebug::PrintValue( uninitialised_data ).c_str() );
		}

		// otherwise, leave value uninitialised
		else {
			msg( E_INFO, E_DEBUG, "Declaration: DATA entry \"%s\" uninitialised (untyped)", name );
		}

		decode_output.data.push_back( uninitialised_data );
		break;
	}

	default:
		casshole( "Invalid declaration: \"%s\"", decl_data );
		break;
	}

	// Create symbol from reference and insert it to the map
	Symbol declaration_symbol( name, declaration_reference );
	InsertSymbol( declaration_symbol, name, decode_output.mentioned_symbols );
}

void AsmHandler::ReadSingleCommand( const char* command,
                                    char* argument )
{
	const CommandTraits* desc = proc_->CommandSet()->DecodeCommand( command );
	cverify( desc, "Command: \"%s\": unsupported command", command );

	Command output_command; mem_init( output_command );

	// Set command opcode
	output_command.id = desc->id;

	// Set command type to default value if it is unspecified.
	if( last_statement_type == Value::V_MAX )
		output_command.type = desc->is_service_command ? Value::V_MAX : Value::V_FLOAT;

	// Set command type if it is specified.
	else
		output_command.type = last_statement_type;

	// Parse arguments
	if( !argument ) { /* no argument */
		cverify( desc->arg_type == A_NONE,
		         "No argument required for command \"%s\" while given argument \"%s\"",
		         command, argument );
	}

	else { /* have argument */
		cverify( desc->arg_type != A_NONE,
		         "Argument needed for command \"%s\"", command );

		switch( desc->arg_type ) {
		case A_REFERENCE:
			output_command.arg.ref = ParseFullReference( argument );
			break;

		case A_VALUE:
			// Set argument type to default value if command type is unspecified.
			if( output_command.type == Value::V_MAX )
				output_command.arg.value.type = desc->is_service_command ? Value::V_INTEGER : Value::V_FLOAT;

			// Set argument type to command type if latter is specified.
			else
				output_command.arg.value.type = output_command.type;

			output_command.arg.value.Parse( argument );
			break;

		case A_NONE:
		default:
			casshole( "Switch error" );
			break;
		} // switch (argument type)

	} // have argument

	msg( E_INFO, E_DEBUG, "Command: \"%s\" (0x%04hx) argument %s",
	     desc->mnemonic, desc->id,
	     ProcDebug::PrintArgument( desc->arg_type, output_command.arg ).c_str() );

	// Write decode result
	decode_output.commands.push_back( output_command );
}

char* AsmHandler::ParseLabel( char* string )
{
	do { // TODO rewrite to while()
		if( *string == ' ' || *string == '\0' )
			return 0; // no label

		if( *string == ':' ) {
			*string = '\0';
			return string + 1; // found label
		}
	} while( *string++ );

	return 0;
}

void AsmHandler::ReadSingleStatement( char* input )
{
	verify_method;
	size_t labels = 0;

	char* command, *argument, typespec = 0, *current_position = PrepLine( input );
	last_statement_type = Value::V_MAX;

	/* skip leading space and return if empty string */
	if( !current_position ) {
		msg( E_INFO, E_DEBUG, "Decoding line %u: empty line", current_line_num );
		return;
	}

	msg( E_INFO, E_DEBUG, "Decoding line %u: \"%s\"", current_line_num, current_position );

	/* parse labels */
	while( char* next_chunk = ParseLabel( current_position ) ) {
		Reference label_reference; mem_init( label_reference );
		label_reference.global_section = S_CODE;
		label_reference.has_second_component = 0;
		label_reference.components[0].indirection_section = S_NONE;
		label_reference.components[0].target.type = Reference::BaseRef::BRT_DEFINITION;

		Symbol label_symbol( current_position, label_reference );
		InsertSymbol( label_symbol, current_position, decode_output.mentioned_symbols );

		msg( E_INFO, E_DEBUG, "Label: \"%s\"", current_position );
		++labels;
		current_position = next_chunk;

		while( isspace( *current_position ) ) ++current_position;
	}

	msg( E_INFO, E_DEBUG, "Labels: %zu", labels );

	/* get remaining statement */
	command = strtok( current_position, " \t" );
	argument = strtok( 0, "" );

	if( !command ) {
		msg( E_INFO, E_DEBUG, "Statement: no statement" );
		return; // no command
	}

	/* parse explicit type-specifier */
	if( char* dot = strchr( command, '.' ) ) {
		*dot++ = '\0';

		switch( tolower( *dot ) ) {
		case 'f':
			last_statement_type = Value::V_FLOAT;
			break;

		case 'd':
		case 'i':
			last_statement_type = Value::V_INTEGER;
			break;

		default:
			casshole( "Invalid command type specification: '%c'", typespec );
			break;
		}
	}

	/* determine if we have a declaration or a command and parse accordingly */

	if( !strcmp( command, "decl" ) ) {
		cverify( argument, "Empty declaration" );
		ReadSingleDeclaration( argument );
	}

	else {
		ReadSingleCommand( command, argument );
	}
}

DecodeResult* AsmHandler::ReadStream()
{
	verify_method;
	char read_buffer[STATIC_LENGTH];

	try {
		decode_output.Clear();

		if( !fgets( read_buffer, STATIC_LENGTH, reading_file_ ) )
			return 0;

		++current_line_num;

		ReadSingleStatement( read_buffer );
		return &decode_output;
	}

	catch( Debug::Exception& e ) {
		switch( e.Type() ) {
		case Debug::EX_INPUT:
			msg( E_CRITICAL, E_USER, "Syntax error on line %d: %s",
			     current_line_num, e.what() );
			break;

		case Debug::EX_BUG:
			msg( E_CRITICAL, E_USER, "Parser internal error on line %d: %s",
			     current_line_num, e.what() );
			break;
		}

		throw; // ...since we can just rethrow this one.
	}

	catch( std::exception& e ) {
		msg( E_CRITICAL, E_USER, "Unspecified error parsing line %d: %s",
		     current_line_num, e.what() );
		throw;
	}
}

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
